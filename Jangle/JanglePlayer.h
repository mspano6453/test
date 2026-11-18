#pragma once
#include "JangleDSP.h"
#include "../Source/BandBus.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// The rhythm guitar player: real chord shapes on six strings (open chords,
// barre chords, power chords), strummed down and up with the timing and the
// accents of a real right hand, or picked string by string.
// ---------------------------------------------------------------------------
namespace jg
{
constexpr int kOpen[6] = { 40, 45, 50, 55, 59, 64 };   // E A D G B E

enum Feel { FFolk = 0, FEighths, FPower, FChug, FFunk, FSkank, FChops, FPicked, FArpeggio, FRing, FWash, NumFeels };
inline const char* feelName (int f)
{
    static const char* n[] = { "Folk Strum", "Eighths Down-Up", "Power Chord Eighths", "Palm-Muted Chug", "Funk Scratch", "Offbeat Skank",
                               "Backbeat Chops", "Fingerpicked", "Ringing Arpeggio", "Let It Ring", "Shoegaze Wash" };
    return (f >= 0 && f < NumFeels) ? n[f] : "";
}
inline bool feelUsesPower (int f) { return f == FPower || f == FChug; }

struct Shape { int fret[6] { -1, -1, -1, -1, -1, -1 }; bool valid() const { for (int f : fret) if (f >= 0) return true; return false; } };

// open-position chords a guitarist reaches for first (root pc, quality) -> frets low E .. high E
inline bool openShape (int root, int q, Shape& s)
{
    struct O { int r, q; int f[6]; };
    static const O o[] = {
        { 0, band::QMaj, { -1, 3, 2, 0, 1, 0 } }, { 0, band::QDom7, { -1, 3, 2, 3, 1, 0 } }, { 0, band::QMaj7, { -1, 3, 2, 0, 0, 0 } },
        { 2, band::QMaj, { -1, -1, 0, 2, 3, 2 } }, { 2, band::QMin, { -1, -1, 0, 2, 3, 1 } }, { 2, band::QDom7, { -1, -1, 0, 2, 1, 2 } },
        { 2, band::QMaj7, { -1, -1, 0, 2, 2, 2 } }, { 2, band::QMin7, { -1, -1, 0, 2, 1, 1 } }, { 2, band::QSus4, { -1, -1, 0, 2, 3, 3 } },
        { 4, band::QMaj, { 0, 2, 2, 1, 0, 0 } }, { 4, band::QMin, { 0, 2, 2, 0, 0, 0 } }, { 4, band::QDom7, { 0, 2, 0, 1, 0, 0 } },
        { 4, band::QMaj7, { 0, 2, 1, 1, 0, 0 } }, { 4, band::QMin7, { 0, 2, 0, 0, 0, 0 } }, { 4, band::QSus4, { 0, 2, 2, 2, 0, 0 } },
        { 5, band::QMaj7, { -1, -1, 3, 2, 1, 0 } },
        { 7, band::QMaj, { 3, 2, 0, 0, 0, 3 } }, { 7, band::QDom7, { 3, 2, 0, 0, 0, 1 } }, { 7, band::QMaj7, { 3, 2, 0, 0, 0, 2 } },
        { 9, band::QMaj, { -1, 0, 2, 2, 2, 0 } }, { 9, band::QMin, { -1, 0, 2, 2, 1, 0 } }, { 9, band::QDom7, { -1, 0, 2, 0, 2, 0 } },
        { 9, band::QMaj7, { -1, 0, 2, 1, 2, 0 } }, { 9, band::QMin7, { -1, 0, 2, 0, 1, 0 } }, { 9, band::QSus4, { -1, 0, 2, 2, 3, 0 } },
        { 11, band::QDom7, { -1, 2, 1, 2, 0, 2 } },
    };
    for (auto& x : o) if (x.r == root && x.q == q) { std::copy (x.f, x.f + 6, s.fret); return true; }
    return false;
}

// a barre chord: the E shape (root on the low E string) or the A shape (root on the A string)
inline Shape barreShape (int root, int q, bool aShape, int fret)
{
    Shape s;
    static const int E[band::NumQualities][6] = {
        { 0, 2, 2, 1, 0, 0 }, { 0, 2, 2, 0, 0, 0 }, { 0, 2, 0, 1, 0, 0 }, { 0, -9, 1, 1, 0, -9 },
        { 0, 2, 0, 0, 0, 0 }, { 0, -9, 2, 0, -1, -9 }, { 0, 2, 2, 2, 0, 0 }, { 0, -9, 0, 0, -1, -9 } };
    static const int A[band::NumQualities][6] = {
        { -9, 0, 2, 2, 2, 0 }, { -9, 0, 2, 2, 1, 0 }, { -9, 0, 2, 0, 2, 0 }, { -9, 0, 2, 1, 2, 0 },
        { -9, 0, 2, 0, 1, 0 }, { -9, 0, 1, 2, 1, -9 }, { -9, 0, 2, 2, 3, 0 }, { -9, 0, 1, 0, 1, -9 } };
    const auto& t = aShape ? A[q] : E[q];
    juce::ignoreUnused (root);
    for (int i = 0; i < 6; ++i) s.fret[i] = (t[i] <= -9 || fret + t[i] < 0) ? -1 : fret + t[i];
    return s;
}

// position: 0 open chords where they exist, 1 barre chords low on the neck, 2 up the neck
inline Shape chordShape (int root, int q, int position)
{
    root = ((root % 12) + 12) % 12;
    if (q < 0 || q >= band::NumQualities) q = 0;
    Shape s;
    if (position == 0 && openShape (root, q, s)) return s;
    const int fe = ((root - 4) % 12 + 12) % 12, fa = ((root - 9) % 12 + 12) % 12;
    if (position == 2)
    {
        const int e = fe < 5 ? fe + 12 : fe, a = fa < 5 ? fa + 12 : fa;
        return (std::abs (e - 7) <= std::abs (a - 7)) ? barreShape (root, q, false, e) : barreShape (root, q, true, a);
    }
    if (position == 0 && fe == 0) return barreShape (root, q, false, 0);
    if (position == 0 && fa == 0) return barreShape (root, q, true, 0);
    // the lower barre that sits nearest the first few frets
    const int e = fe == 0 ? 12 : fe, a = fa == 0 ? 12 : fa;
    return (e <= a + 1) ? barreShape (root, q, false, e) : barreShape (root, q, true, a);
}

// root, fifth and octave on the low strings
inline Shape powerShape (int root, int position)
{
    Shape s;
    const int fe = ((root - 4) % 12 + 12) % 12, fa = ((root - 9) % 12 + 12) % 12;
    const bool onA = position == 2 ? fa < fe : (fe > 7 && fa <= 7);
    if (! onA) { s.fret[0] = fe; s.fret[1] = fe + 2; s.fret[2] = fe + 2; }
    else { s.fret[1] = fa; s.fret[2] = fa + 2; s.fret[3] = fa + 2; }
    return s;
}

inline int stringNote (const Shape& s, int str) { return s.fret[str] < 0 ? -1 : kOpen[str] + s.fret[str]; }
inline int lowestString (const Shape& s) { for (int i = 0; i < 6; ++i) if (s.fret[i] >= 0) return i; return 0; }

// one stroke of the right hand
enum Stroke { SDown = 0, SUp, SScratch, SBass, SPick, SPinch, SLead };   // SLead: one lead note (arg = the MIDI note)
struct Hit { int step; int stroke; float vel; float len; int arg = 0; };   // len in 16ths (<0: let it ring); arg: the string for picks

// A lead guitar answer over the back half of a bar: a short pentatonic lick off the chord, ending on a chord tone
inline void writeLick (int root, int q, int ns, int beat, Rng& r, std::vector<Hit>& out)
{
    const int from = ns - (ns >= 16 ? 8 : 6);
    out.erase (std::remove_if (out.begin(), out.end(), [from] (const Hit& h) { return h.step >= from && h.step > 0; }), out.end());
    static const int majP[] = { 0, 2, 4, 7, 9 }, minP[] = { 0, 3, 5, 7, 10 };
    const int* sc = q == 1 ? minP : majP;
    // the scale around the top of the neck (E4 to E5 and a bit)
    int pool[16]; int np = 0;
    for (int n = 62; n <= 79 && np < 16; ++n) for (int i = 0; i < 5; ++i) if ((n - root - sc[i]) % 12 == 0) { pool[np++] = n; break; }
    if (np < 4) return;
    // the rhythm: a few shapes a guitarist reaches for
    static const int r1[] = { 0, 2, 4, 6 }, r2[] = { 0, 1, 2, 4, 6 }, r3[] = { 2, 3, 4, 6 }, r4[] = { 0, 2, 3, 4 };
    const int* rs[] = { r1, r2, r3, r4 }; const int rn[] = { 4, 5, 4, 4 };
    const int pick = r.i (4);
    int at = r.i (np - 3) + 2;
    const bool down = r.chance (0.65f);
    for (int k = 0; k < rn[pick]; ++k)
    {
        const int step = from + rs[pick][k] * (ns >= 16 ? 1 : 1);
        if (step >= ns) break;
        const bool lastNote = k == rn[pick] - 1;
        if (lastNote)
        {
            // home: the nearest root or fifth
            int best = pool[at], bd = 99;
            for (int i = 0; i < np; ++i) { const int iv = ((pool[i] - root) % 12 + 12) % 12; if ((iv == 0 || iv == 7) && std::abs (i - at) < bd) { bd = std::abs (i - at); best = pool[i]; } }
            out.push_back ({ step, SLead, 0.82f, (float) (ns - step) + (float) beat, best });
        }
        else
        {
            out.push_back ({ step, SLead, k == 0 ? 0.86f : 0.7f + 0.1f * r.u(), rs[pick][k + 1] - rs[pick][k] <= 1 ? 0.9f : 1.8f, pool[at] });
            at = juce::jlimit (0, np - 1, at + (down ? -1 : 1) * (r.chance (0.8f) ? 1 : 2));
        }
    }
    juce::ignoreUnused (beat);
}

// Writes one bar. steps: 16 (4/4) or 12 (3/4, 6/8)
inline void writeBar (int feel, int section, float busy, int meter, Rng& r, std::vector<Hit>& out)
{
    if (band::meterOdd (meter)) { writeBar (feel, section, busy, band::M44, r, out); band::oddRemap (out, meter); return; }
    out.clear();
    const bool chorus = section == band::Chorus, bridge = section == band::Bridge;
    const float b = juce::jlimit (0.0f, 1.0f, busy + (chorus ? 0.15f : 0.0f) - (bridge ? 0.15f : 0.0f));
    auto add = [&] (int s, int st, float v, float len, int arg = 0) { out.push_back ({ s, st, v, len, arg }); };
    if (meter == band::M44)
    {
        switch (feel)
        {
            case FFolk:      // D . D U . U D U  (the strings ring until the next stroke catches them)
                add (0, SDown, 0.9f, -1.0f); add (4, SDown, 0.78f, -1.0f); add (6, SUp, 0.58f, -1.0f);
                add (10, SUp, 0.6f, -1.0f); add (12, SDown, 0.8f, -1.0f); add (14, SUp, 0.58f, -1.0f);
                if (b > 0.7f) add (8, SDown, 0.5f, -1.0f);
                break;
            case FEighths:
                for (int s = 0; s < 16; s += 2) add (s, s % 4 == 0 ? SDown : SUp, s % 8 == 0 ? 0.88f : (s % 4 == 0 ? 0.76f : 0.56f), -1.0f);
                if (b > 0.75f) for (int s = 1; s < 16; s += 4) if (r.chance (b - 0.5f)) add (s, SUp, 0.45f, 0.9f);
                break;
            case FPower:
                for (int s = 0; s < 16; s += 2) add (s, SDown, s % 8 == 0 ? 0.92f : (s % 4 == 0 ? 0.84f : 0.74f), 1.8f);
                break;
            case FChug:
                for (int s = 0; s < 16; s += (b > 0.65f ? 1 : 2)) add (s, SDown, s % 4 == 0 ? 0.86f : 0.68f, 0.7f, 1);   // arg 1 = palm muted
                if (chorus) { out[0].arg = 0; out[0].len = 3.5f; }
                break;
            case FFunk:
            {
                static const char* pats[] = { "x.sxs.x.sxs.x.s.", "s.x.sxs.s.x.sxsx", "x.s.sxs.x.s.sxs." };
                const char* p = pats[r.i (3)];
                for (int s = 0; s < 16; ++s)
                {
                    if (p[s] == 'x') add (s, s % 2 ? SUp : SDown, 0.82f, 0.6f);
                    else if (p[s] == 's' && (b > 0.35f || s % 4 == 0)) add (s, SScratch, 0.6f, 0.3f);
                }
                break;
            }
            case FSkank:
                if (b < 0.5f) { add (4, SUp, 0.82f, 0.8f); add (12, SUp, 0.86f, 0.8f); }
                else for (int s = 2; s < 16; s += 4) add (s, SUp, s % 8 == 6 ? 0.84f : 0.76f, 0.7f);
                break;
            case FChops:
                add (4, SDown, 0.86f, 0.9f); add (12, SDown, 0.88f, 0.9f);
                if (b > 0.6f) { add (7, SScratch, 0.5f, 0.3f); add (15, SScratch, 0.5f, 0.3f); }
                break;
            case FPicked:    // Travis picking: an alternating thumb, fingers in between
                add (0, SBass, 0.82f, 4.0f, 0); add (4, SBass, 0.74f, 4.0f, 1); add (8, SBass, 0.8f, 4.0f, 0); add (12, SBass, 0.74f, 4.0f, 1);
                add (2, SPick, 0.6f, 3.0f, 4); add (6, SPinch, 0.62f, 3.0f, 5); add (10, SPick, 0.6f, 3.0f, 3); add (14, SPick, 0.58f, 3.0f, 4);
                if (b > 0.6f) { add (5, SPick, 0.5f, 2.0f, 3); add (13, SPick, 0.5f, 2.0f, 5); }
                break;
            case FArpeggio:
            {
                static const int up[8] = { 0, 2, 3, 4, 5, 4, 3, 2 };
                for (int i = 0; i < 8; ++i) add (i * 2, SPick, i == 0 ? 0.8f : 0.62f, -1.0f, up[i]);
                break;
            }
            case FRing:
                add (0, SDown, 0.86f, -1.0f);
                if (b > 0.55f) add (8, SDown, 0.66f, -1.0f);
                if (b > 0.8f) add (14, SUp, 0.5f, -1.0f);
                break;
            default:         // FWash
                add (0, SDown, 0.92f, -1.0f); add (6, SUp, 0.7f, -1.0f); add (8, SDown, 0.84f, -1.0f); add (14, SUp, 0.7f, -1.0f);
                if (b > 0.6f) { add (3, SUp, 0.55f, -1.0f); add (11, SUp, 0.55f, -1.0f); }
                break;
        }
        return;
    }
    const bool six = meter == band::M68;
    switch (feel)
    {
        case FPicked: case FArpeggio:
        {
            // 6/8: bass and up the strings (and back); 3/4: bass, then the chord picked through
            static const int a6[6] = { 0, 2, 3, 5, 3, 2 }, a3[6] = { 0, 3, 4, 5, 4, 3 };
            for (int i = 0; i < 6; ++i) add (i * 2, i == 0 ? SBass : SPick, i == 0 ? 0.82f : (i == 3 && six ? 0.7f : 0.6f), feel == FArpeggio ? -1.0f : 3.0f, six ? a6[i] : a3[i]);
            break;
        }
        case FPower: case FChug:
            for (int s = 0; s < 12; s += 2) add (s, SDown, s % (six ? 6 : 4) == 0 ? 0.9f : 0.72f, feel == FChug ? 0.7f : 1.8f, feel == FChug ? 1 : 0);
            break;
        case FSkank: case FChops:
            if (six) for (int s : { 2, 4, 8, 10 }) add (s, SUp, 0.78f, 0.7f);
            else { add (4, SDown, 0.84f, 0.9f); add (8, SDown, 0.8f, 0.9f); }
            break;
        case FRing: case FWash:
            add (0, SDown, 0.88f, -1.0f);
            if (b > 0.55f || feel == FWash) add (six ? 6 : 8, SDown, 0.66f, -1.0f);
            break;
        case FFunk:
            for (int s = 0; s < 12; ++s) { if (s % (six ? 6 : 4) == 0) add (s, SDown, 0.82f, 0.6f); else if (s % 2 == 0 && b > 0.35f) add (s, SScratch, 0.55f, 0.3f); }
            break;
        default:   // folk, eighths: the waltz strum (bass, chuck, chuck) or the 6/8 rock
            if (six)
            {
                add (0, SDown, 0.9f, -1.0f); add (2, SDown, 0.6f, -1.0f); add (4, SUp, 0.55f, -1.0f);
                add (6, SDown, 0.82f, -1.0f); add (8, SDown, 0.6f, -1.0f); add (10, SUp, 0.55f, -1.0f);
            }
            else
            {
                add (0, SBass, 0.86f, -1.0f, 0);
                add (4, SDown, 0.74f, 3.0f); add (8, SDown, 0.72f, 3.0f);
                if (b > 0.5f) add (10, SUp, 0.52f, 1.8f);
            }
            break;
    }
}

// the bar before a new section: a flourish on the last beat
inline void writeFill (int feel, int meter, Rng& r, std::vector<Hit>& out)
{
    const int n = band::meterSteps (meter);
    const int from = n - (meter == band::M68 ? 6 : 4);
    out.erase (std::remove_if (out.begin(), out.end(), [from] (const Hit& h) { return h.step >= from; }), out.end());
    if (feel == FRing || feel == FWash || feel == FArpeggio) { out.push_back ({ from, SDown, 0.8f, 2.0f }); out.push_back ({ from + 2, SUp, 0.7f, -1.0f }); return; }
    const bool scratchy = feel == FFunk || feel == FChops || r.chance (0.3f);
    for (int s = from, i = 0; s < n; ++s, ++i)
        out.push_back ({ s, scratchy && i % 2 == 1 ? SScratch : (i % 2 ? SUp : SDown), 0.66f + 0.06f * (float) i, 0.9f });
}
} // namespace jg
