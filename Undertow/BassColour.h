#pragma once
#include "UndertowDSP.h"

// ---------------------------------------------------------------------------
// What turns the recorded basses into other basses, and the BUBBLE knob.
//
//  Violin Bass '60s - the small hollow-body bass played with a pick: round and
//                     woody, a bump in the low mids, little top end, and notes
//                     that thump and die away fast (old flatwound strings).
//  Flatwound Muted  - the 60s soul session sound: fingers, flatwounds and a
//                     foam mute under the strings. Short, deep, round.
//  Fretless         - the "mwah": a vocal resonance that blooms after the note.
//  Fuzz Bass        - a 60s fuzz pedal, all square-wave grind.
//
//  BUBBLE: 0 = the plain bass. Turn it up and an envelope filter starts to
//  quack on every note (bubbly); past halfway the filter also wobbles in time
//  with the song (wubbly), and at the top it squelches.
// ---------------------------------------------------------------------------
namespace ut
{
enum BassType { BPicked = 0, BFingers, BSynthSub, BSynthAcid, BSynthReese, BSynthPulse, BViolin, BFlatMuted, BUpright, BFretless, BFuzz, NumBassTypes };
inline juce::StringArray bassTypeNames()
{
    return { "Picked", "Fingers", "Synth Sub", "Synth Acid", "Synth Reese", "Synth 80s Pulse",
             "Violin Bass '60s", "Flatwound Muted", "Upright", "Fretless", "Fuzz Bass" };
}
inline bool bassIsSynth (int t) { return t >= BSynthSub && t <= BSynthPulse; }
// which recorded bass each type is made from: 0 picked, 1 fingers, 2 upright
inline int bassBank (int t) { return (t == BFingers || t == BFlatMuted || t == BFretless) ? 1 : (t == BUpright ? 2 : 0); }

// a state-variable filter (Chamberlin): low-pass and band-pass at once
struct Svf
{
    float lp = 0, bp = 0;
    inline void process (float x, float f, float q) { const float hp = x - lp - q * bp; bp += f * hp; lp += f * bp; if (! std::isfinite (lp)) lp = bp = 0; }
};

class BassColour
{
public:
    void prepare (double s)
    {
        sr = s;
        hp.set (Biquad::HP, sr, 38, 0.707);
        setType (-1);
        env = 0; noteT = 1 << 30; lfo = 0; fret = {}; bub = {};
    }
    void setType (int t)
    {
        if (t == type) return;
        type = t;
        switch (t)
        {
            case BViolin:    a.set (Biquad::Peak, sr, 170, 0.9, 5.0); b.set (Biquad::LP, sr, 1900, 0.7); c.set (Biquad::Peak, sr, 650, 1.0, -3.0); tau = 0.75f; break;
            case BFlatMuted: a.set (Biquad::Peak, sr, 105, 0.8, 3.5); b.set (Biquad::LP, sr, 1300, 0.7); c.set (Biquad::Peak, sr, 900, 1.0, -2.0); tau = 0.32f; break;
            case BUpright:   a.set (Biquad::Peak, sr, 120, 0.8, 1.5); b.set (Biquad::LP, sr, 3800, 0.7); c.set (Biquad::Peak, sr, 2200, 1.0, -2.0); tau = 0; break;
            case BFretless:  a.set (Biquad::Peak, sr, 90, 0.8, 1.0);  b.set (Biquad::LP, sr, 4200, 0.7); c.set (Biquad::Peak, sr, 1000, 1.0, 0.0); tau = 0; break;
            case BFuzz:      a.set (Biquad::Peak, sr, 140, 0.8, 2.0); b.set (Biquad::LP, sr, 2600, 0.7); c.set (Biquad::Peak, sr, 800, 1.0, 3.0); tau = 0; break;
            default: tau = 0; break;
        }
    }
    void noteOn() { noteT = 0; }
    float bubble = 0;          // 0..1
    double bpm = 120;

    inline float process (float x) noexcept
    {
        if (noteT < (1 << 30)) ++noteT;
        if (type >= BViolin)
        {
            x = hp.process (x);
            x = c.process (b.process (a.process (x)));
            // old flatwounds and a mute: the note thumps, then dies away quickly
            if (tau > 0) x *= std::exp (-(float) noteT / (float) (tau * sr)) * 0.85f + 0.15f * std::exp (-(float) noteT / (float) (tau * 3.0f * sr));
            if (type == BFretless)
            {
                // the mwah: a resonance that slides up as the note blooms
                const float tt = juce::jmin (1.0f, (float) noteT / (float) (0.28 * sr));
                const float f = 2.0f * std::sin (juce::MathConstants<float>::pi * (380.0f + 800.0f * tt) / (float) sr);
                fret.process (x, f, 0.35f);
                x += fret.bp * 0.55f;
            }
            if (type == BFuzz) x = std::tanh (x * 22.0f) * 0.11f + x * 0.06f;
            x *= type == BViolin ? 1.3f : (type == BFlatMuted ? 1.3f : (type == BUpright ? 0.8f : (type == BFretless ? 0.85f : 1.0f)));
        }
        if (bubble > 0.001f)
        {
            // envelope follower: how hard the note was hit opens the filter
            const float ax = std::abs (x);
            env += (ax > env ? 0.02f : 0.0004f) * (ax - env);
            const float b2 = bubble * bubble;
            float cut = 160.0f + juce::jmin (1.0f, env * 6.0f) * (600.0f + 3200.0f * bubble);
            if (bubble > 0.5f)
            {
                // wubbly: the filter wobbles in eighth notes with the song
                lfo += (bpm / 60.0 * 2.0) / sr; if (lfo >= 1) lfo -= 1;
                const float w = (bubble - 0.5f) * 2.0f;
                cut *= 1.0f + 0.85f * w * (float) std::sin (juce::MathConstants<double>::twoPi * lfo);
            }
            cut = juce::jlimit (60.0f, (float) sr * 0.2f, cut);
            const float f = 2.0f * std::sin (juce::MathConstants<float>::pi * cut / (float) sr);
            const float q = 1.0f - 0.82f * juce::jmin (1.0f, bubble * 1.2f);      // lower q = more resonance
            bub.process (x, f, q);
            const float wet = bub.lp * 0.8f + bub.bp * (0.4f + 0.8f * b2);
            const float mix = juce::jmin (1.0f, bubble * 1.6f);
            x = x * (1.0f - mix) + std::tanh (wet * 1.3f) * mix;
        }
        return x;
    }
private:
    double sr = 44100;
    int type = -2;
    float tau = 0, env = 0;
    int noteT = 1 << 30;
    double lfo = 0;
    Biquad hp, a, b, c;
    Svf fret, bub;
};
} // namespace ut
