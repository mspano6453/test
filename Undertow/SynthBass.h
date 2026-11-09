#pragma once
#include "UndertowDSP.h"

// ---------------------------------------------------------------------------
// Synth bass for the electronic and 80s styles. One voice, like the real
// thing: a new note either restarts it, or (when it overlaps the last one)
// slides to the new pitch.
//   Sub    - a deep sine with a little punch at the start (hip-hop, trap)
//   Acid   - saw into a squelchy resonant filter with accents and slides
//   Reese  - two detuned saws growling against each other (drum & bass)
//   Pulse  - an 80s pulse-wave bass with a plucky filter
// ---------------------------------------------------------------------------
namespace ut
{
enum SynthModel { SMSub = 0, SMAcid, SMReese, SMPulse };

class SynthBass
{
public:
    void prepare (double s) { sr = s; reset(); }
    void reset() { active = false; amp = 0; ph1 = ph2 = ph3 = 0; lp1 = lp2 = lp3 = lp4 = 0; currentNote = -1; holdLeft = 0; }
    void setModel (int m) { model = m; }

    void noteOn (int midi, float vel, float lengthSec, bool slide)
    {
        const double f = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
        if (slide && active && amp > 0.05f)
        {
            target = f;                            // glide there, keep the envelope going
            glideK = 1.0 - std::exp (-1.0 / (0.055 * sr));
        }
        else
        {
            freq = target = f; glideK = 1.0;
            amp = 0; attackDone = false; filtEnv = 1.0f; age = 0;
            accent = vel > 0.85f ? 1.0f : 0.0f;
        }
        velocity = vel; active = true; released = false;
        holdLeft = lengthSec < 0 ? -1 : (int) (lengthSec * sr);
        currentNote = midi;
    }
    void noteOff() { released = true; currentNote = -1; holdLeft = 0; }
    void allOff() { reset(); }

    inline float render() noexcept
    {
        if (! active) return 0.0f;
        if (holdLeft > 0 && --holdLeft == 0) released = true;
        freq += (target - freq) * glideK;
        ++age;

        // amplitude envelope
        const float atk = 0.004f;
        if (! released)
        {
            if (! attackDone) { amp += (1.0f - amp) * (float) (1.0 - std::exp (-1.0 / (atk * sr))); if (amp > 0.98f) attackDone = true; }
            else if (model == SMSub) amp *= (float) std::exp (-1.0 / (2.5 * sr));          // a long, booming tail
            else amp += (0.85f - amp) * 0.0005f;
        }
        else
        {
            amp *= (float) std::exp (-1.0 / ((model == SMSub ? 0.09 : 0.03) * sr));
            if (amp < 1.0e-4f) { active = false; return 0.0f; }
        }

        const double dt = freq / sr;
        float y = 0;
        switch (model)
        {
            case SMSub:
            {
                // a little pitch punch at the start, then a pure low sine with a touch of harmonics
                const double punch = 1.0 + 0.5 * std::exp (-(double) age / (0.012 * sr));
                ph1 += dt * punch; if (ph1 >= 1) ph1 -= 1;
                const double s = std::sin (kTwoPi * ph1);
                y = (float) std::tanh (1.6 * s) * 0.9f;
                break;
            }
            case SMAcid:
            {
                ph1 += dt; if (ph1 >= 1) ph1 -= 1;
                const float saw = (float) (2.0 * ph1 - 1.0) - polyBlep (ph1, dt);
                filtEnv *= (float) std::exp (-1.0 / ((0.12 + 0.18 * (1.0 - accent)) * sr));
                const float cutoff = (float) juce::jlimit (60.0, sr * 0.4, 180.0 + (1400.0 + 2600.0 * accent) * filtEnv * (0.5 + 0.5 * velocity));
                y = ladder (saw, cutoff, 0.82f + 0.12f * accent) * 1.6f;
                break;
            }
            case SMReese:
            {
                const double drift = 1.0 + 0.0009 * std::sin (kTwoPi * 0.3 * (double) age / sr);
                ph1 += dt * 1.007 * drift; if (ph1 >= 1) ph1 -= 1;
                ph2 += dt * 0.993; if (ph2 >= 1) ph2 -= 1;
                const float a = (float) (2.0 * ph1 - 1.0) - polyBlep (ph1, dt * 1.007);
                const float b = (float) (2.0 * ph2 - 1.0) - polyBlep (ph2, dt * 0.993);
                y = ladder ((a + b) * 0.5f, 900.0f, 0.25f) * 1.4f + (float) std::sin (kTwoPi * ph1) * 0.35f;
                break;
            }
            default:  // SMPulse
            {
                ph1 += dt; if (ph1 >= 1) ph1 -= 1;
                const double w = 0.32;
                float p = (ph1 < w ? 1.0f : -1.0f) + polyBlep (ph1, dt) - polyBlep (std::fmod (ph1 + 1.0 - w, 1.0), dt);
                filtEnv *= (float) std::exp (-1.0 / (0.16 * sr));
                y = ladder (p * 0.7f, 250.0f + 2200.0f * filtEnv * (0.5f + 0.5f * velocity), 0.35f) * 1.3f;
                break;
            }
        }
        static const float trim[4] = { 0.5f, 0.8f, 0.7f, 0.6f };   // sit level with the real basses
        return y * amp * (0.55f + 0.45f * velocity) * trim[juce::jlimit (0, 3, model)];
    }
    int currentNote = -1;

private:
    static float polyBlep (double t, double dt)
    {
        if (t < dt) { const double x = t / dt; return (float) (x + x - x * x - 1.0); }
        if (t > 1.0 - dt) { const double x = (t - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
        return 0.0f;
    }
    // a simple 4-pole ladder-ish low-pass with resonance
    float ladder (float x, float cutoff, float res)
    {
        const float g = (float) (1.0 - std::exp (-kTwoPi * cutoff / sr));
        const float fb = res * 3.8f;
        const float in = std::tanh (x - fb * lp4);
        lp1 += g * (in - lp1); lp2 += g * (lp1 - lp2); lp3 += g * (lp2 - lp3); lp4 += g * (lp3 - lp4);
        return lp4 * (1.0f + res);
    }

    double sr = 44100, freq = 55, target = 55, glideK = 1, ph1 = 0, ph2 = 0, ph3 = 0;
    float amp = 0, filtEnv = 1, velocity = 0.8f, accent = 0, lp1 = 0, lp2 = 0, lp3 = 0, lp4 = 0;
    bool active = false, released = false, attackDone = false;
    int model = SMSub, holdLeft = 0;
    int64_t age = 0;
};
} // namespace ut
