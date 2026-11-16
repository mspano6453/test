#pragma once
#include "HarmoniaDSP.h"
#include "../Source/BandBus.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// The backing singers' arranger: close harmony under a top line that moves as
// little as it can, and the parts sung the way backing vocals go: held oohs
// and aahs, a hum, doo-wop, "ba ba ba", answers at the ends of phrases, gospel
// swells.
// ---------------------------------------------------------------------------
namespace hm
{
enum Feel { FOohAah = 0, FOohs, FAahs, FHum, FDooWop, FBaBa, FAnswers, FGospel, NumFeels };
inline const char* feelName (int f)
{
    static const char* n[] = { "Oohs & Aahs", "Oohs", "Aahs", "Hum", "Doo-Wop", "Ba Ba Ba", "Answers", "Gospel Swells" };
    return (f >= 0 && f < NumFeels) ? n[f] : "";
}
enum Group { GChoir = 0, GGirls, GMen, GTrio, NumGroups };
inline const char* groupName (int g)
{
    static const char* n[] = { "Choir (mixed)", "Girl Group", "The Fellas", "Close Trio" };
    return (g >= 0 && g < NumGroups) ? n[g] : "";
}
inline bool partIsFemale (int group, int part, int parts)
{
    if (group == GGirls) return true;
    if (group == GMen) return false;
    return part < (parts + 1) / 2;     // the top half of the stack
}
inline int singersPerPart (int group) { return group == GTrio ? 1 : group == GChoir ? 3 : 2; }

// the vowel for this section
inline Syllable syllableFor (int feel, int section)
{
    switch (feel)
    {
        case FOohAah: return { section == band::Chorus ? VAh : section == band::Bridge ? VMm : VOo, ONone };
        case FOohs: return { VOo, ONone };
        case FAahs: case FGospel: return { VAh, ONone };
        case FHum: return { VMm, ONone };
        case FDooWop: return { VOo, OD };
        case FBaBa: return { VAh, OB };
        default: return { VOo, ONone };
    }
}

struct Hit { int step; float len; float vel; Syllable syl; bool swell = false; };   // len in 16ths; < 0 = hold to the next

inline void writeBar (int feel, int section, int barIn, float busy, int meter, Rng& r, std::vector<Hit>& out)
{
    out.clear();
    const int n = band::meterSteps (meter), beat = band::meterBeatSteps (meter);
    const bool six = meter == band::M68;
    const float b = juce::jlimit (0.0f, 1.0f, busy + (section == band::Chorus ? 0.15f : 0.0f));
    const Syllable base = syllableFor (feel, section);
    auto add = [&] (int s, float len, float vel, Syllable syl, bool swell = false) { out.push_back ({ s, len, vel, syl, swell }); };
    switch (feel)
    {
        case FDooWop:
            if (six) { for (int s = 0; s < n; s += 2) add (s, 1.7f, s % 6 == 0 ? 0.8f : 0.62f, { s % 6 == 0 ? VOo : VOo, s == 6 ? OWah : OD }); }
            else if (barIn % 2 == 0) { for (int s = 0; s < n; s += beat) add (s, 3.2f, s % 8 == 0 ? 0.8f : 0.68f, { VOo, OD }); }
            else { add (0, 3.2f, 0.8f, { VOo, OD }); add (4, 3.2f, 0.7f, { VOo, OD }); add (8, 7.5f, 0.82f, { VAh, OWah }); }
            break;
        case FBaBa:
        {
            static const int p4[] = { 0, 3, 6, 8, 10 }, p3[] = { 0, 4, 6, 8 }, p6[] = { 0, 2, 4, 6, 8, 10 };
            if (meter == band::M44) for (int s : p4) add (s, s == 10 ? 5.0f : 1.6f, s == 0 ? 0.82f : 0.7f, { VAh, OB });
            else if (six) for (int s : p6) add (s, 1.6f, s % 6 == 0 ? 0.8f : 0.66f, { VAh, OB });
            else for (int s : p3) add (s, s == 8 ? 3.5f : 1.6f, s == 0 ? 0.82f : 0.7f, { VAh, OB });
            if (b < 0.35f && ! out.empty()) out.resize (2);
            break;
        }
        case FAnswers:
            // quiet for most of the phrase; an "ooh" answer at the end of every other bar
            if (barIn % 2 == 1) { add (n / 2, (float) (beat * 0.9), 0.74f, { VOo, ONone }); add (n / 2 + beat, (float) (n / 2 - beat), 0.7f, { VOh, ONone }); }
            else if (b > 0.7f) add (n - beat, (float) beat, 0.6f, { VOo, ONone });
            break;
        case FGospel:
            add (0, -1.0f, 0.82f, base, true);
            if (b > 0.6f) add (n / 2, -1.0f, 0.78f, base, true);
            break;
        default:
            // held pads: one note a chord, sung again at the start of a phrase
            add (0, -1.0f, 0.72f, base, false);
            if (b > 0.7f && r.chance (b - 0.4f)) add (n / 2, -1.0f, 0.62f, base, false);
            break;
    }
}

// close harmony: the top part stays near where it was, the rest stack down the chord under it
inline void voiceParts (int root, int q, int parts, int center, const int* prev, bool havePrev, int* out)
{
    int t[4]; const int nt = band::qualityTones (q, t);
    auto isTone = [&] (int nn) { for (int i = 0; i < nt; ++i) if (((nn % 12) + 12) % 12 == (root + t[i]) % 12) return true; return false; };
    int top = center + 5;
    if (havePrev) top = prev[0];
    // the nearest chord tone to where the top was (stay inside a sensible range)
    int best = top, bd = 99;
    for (int d = -6; d <= 6; ++d) if (isTone (top + d) && std::abs (d) < bd && top + d >= center - 2 && top + d <= center + 12) { bd = std::abs (d); best = top + d; }
    out[0] = best;
    for (int p = 1; p < parts; ++p)
    {
        int nn = out[p - 1] - 1;
        while (! isTone (nn) && nn > 20) --nn;
        out[p] = nn;
    }
}
} // namespace hm
