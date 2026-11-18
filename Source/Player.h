#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include "BandBus.h"

// ---------------------------------------------------------------------------
// PLAYER: a drummer that follows Live's transport and plays the kit.
// Styles x sections, with fills, endings, intensity, looseness and swing.
// Patterns are 16 steps (sixteenth notes) of one 4/4 bar.
// Velocity characters: X x o m - g  ('.' = rest)
// ---------------------------------------------------------------------------
namespace tt
{
enum PlayerStyle { StSixties = 0, StMotorik, StKraut, StCrazyHorse, StPowerPop, StBallad,
                   StPunk, StPostPunk, StShoegaze, StGrunge, StHalfTime, StFourFloor, StGlam, StSurf, StShuffle, StFolk, StPostRock, StGarage,
                   StBoomBap, StTrap, StLofi, StGFunk, StElectro, StHouse, StTechno, StDnB, StSynthwave, StHairMetal,
                   StNewJack, StFreestyle, StHiNRG, StDetroitSoul, StPhillySoul, StFunk, StDisco, StTripHop, StPowerBallad, StNorthernSoul,
                   // three-beat styles (12 steps a bar): 3/4 ...
                   StWaltz, StJazzWaltz, StFolkWaltz, StRockWaltz,
                   // ... and 6/8
                   St68Ballad, St68Rock, St68Folk, St68SlowJam,
                   NumStyles };
constexpr int kFirstTriple = StWaltz;
inline bool styleIs34 (int s) { return s >= StWaltz && s <= StRockWaltz; }
inline bool styleIs68 (int s) { return s >= St68Ballad && s <= St68SlowJam; }
// the style that actually plays in this meter: a 4/4 style asked to play in 3/4 or 6/8
// becomes its nearest three-beat cousin (and the other way round)
inline int styleForMeter (int s, int meter)
{
    if (meter == band::M34)
    {
        if (styleIs34 (s)) return s;
        if (styleIs68 (s)) return StWaltz + (s - St68Ballad == 0 ? 0 : s - St68Ballad == 1 ? 3 : s - St68Ballad == 2 ? 2 : 1);
        switch (s)
        {
            case StSixties: case StBallad: case StDetroitSoul: case StPhillySoul: case StNorthernSoul: case StDisco: return StWaltz;
            case StFolk: case StShuffle: case StSurf: return StFolkWaltz;
            case StBoomBap: case StTrap: case StLofi: case StGFunk: case StElectro: case StHouse: case StTechno: case StDnB: case StSynthwave:
            case StNewJack: case StFreestyle: case StHiNRG: case StFunk: case StTripHop: return StJazzWaltz;
            default: return StRockWaltz;
        }
    }
    if (meter == band::M68)
    {
        if (styleIs68 (s)) return s;
        if (styleIs34 (s)) return St68Ballad + (s == StWaltz ? 0 : s == StRockWaltz ? 1 : s == StFolkWaltz ? 2 : 3);
        switch (s)
        {
            case StSixties: case StBallad: case StDetroitSoul: case StPhillySoul: case StNorthernSoul: case StDisco: case StPowerBallad: return St68Ballad;
            case StFolk: case StShuffle: case StSurf: return St68Folk;
            case StBoomBap: case StTrap: case StLofi: case StGFunk: case StElectro: case StHouse: case StTechno: case StDnB: case StSynthwave:
            case StNewJack: case StFreestyle: case StHiNRG: case StFunk: case StTripHop: return St68SlowJam;
            default: return St68Rock;
        }
    }
    if (s < kFirstTriple) return s;
    static const int back[] = { StBallad, StLofi, StFolk, StPowerBallad, StBallad, StHalfTime, StFolk, StTripHop };
    return back[s - kFirstTriple];
}
enum PlayerSection { SecVerse = 0, SecChorus, SecBridge, NumSections };
enum FillEvery { FillOff = 0, Fill4, Fill8, Fill16 };

inline const char* styleName (int s)
{
    static const char* n[] = { "Sixties Pop", "Motorik", "Kraut Funk", "Ragged Rock", "Power Pop", "Ballad",
                               "Punk", "Post-Punk", "Shoegaze Wash", "Grunge", "Half-Time Heavy", "Four on the Floor", "Glam Stomp",
                               "Surf", "Shuffle", "Folk Rock", "Post-Rock Build", "Garage Bash",
                               "Boom Bap", "Trap", "Lo-fi", "West Coast Funk", "Electro", "House", "Techno", "Drum & Bass", "Synthwave",
                               "Hair Metal", "New Jack Swing", "Freestyle", "Hi-NRG", "Detroit Soul", "Philly Soul", "Funk", "Disco",
                               "Trip-Hop", "Power Ballad", "Northern Soul",
                               "Waltz (3/4)", "Jazz Waltz (3/4)", "Folk Waltz (3/4)", "Rock Waltz (3/4)",
                               "Soul Ballad (6/8)", "Rock (6/8)", "Folk Jig (6/8)", "Slow Jam (6/8)" };
    return n[s];
}
inline const char* sectionName (int s)
{
    static const char* n[] = { "Verse", "Chorus", "Bridge" };
    return n[s];
}

// Key switches on any MIDI channel (an octave above the drums)
constexpr int kKeyVerse = 60, kKeyChorus = 62, kKeyBridge = 64, kKeyFill = 65, kKeyEnding = 67;

enum Lane { LK = 0, LS, LSS, LH, LO, LRD, LB, LC, LT1, LT2, LT3, LF,
            LCL, LTB, LTS, LSH, LCB, LCGH, LCGL, NumLanes };   // + claps, tambourine, tamb shake, shaker, cowbell, congas
constexpr int laneNote[NumLanes] = { 36, 38, 37, 42, 46, 51, 53, 49, 50, 48, 45, 43, 39, 54, 69, 70, 56, 63, 64 };

struct Groove
{
    const char* lane[NumLanes] {};
};

struct StyleDef
{
    Groove sec[NumSections];
    Groove extra[NumSections];   // ghost notes / extra kicks added as Intensity rises
    Groove fill[2];              // second half of a bar (steps 8-15 are used)
};

inline Groove G (std::initializer_list<std::pair<Lane, const char*>> l)
{
    Groove g;
    for (auto& p : l) g.lane[p.first] = p.second;
    return g;
}

inline const std::array<StyleDef, NumStyles>& styles()
{
    static const std::array<StyleDef, NumStyles> s = []
    {
        std::array<StyleDef, NumStyles> d {};
        const char* H8 = "x.o.x.o.x.o.x.o.";
        const char* BB = "....x.......x...";
        const char* BBX = "....X.......X...";

        d[StSixties].sec[SecVerse]  = G ({ { LH, H8 }, { LK, "x.....x.x......." }, { LS, BB } });
        d[StSixties].sec[SecChorus] = G ({ { LO, H8 }, { LK, "x.....x.x.x....." }, { LS, BB } });
        d[StSixties].sec[SecBridge] = G ({ { LRD, H8 }, { LB, "x.......x......." }, { LK, "x.......x......." }, { LS, BB } });
        d[StSixties].extra[SecVerse]  = G ({ { LS, "..........g....g" }, { LK, "...........x...." } });
        d[StSixties].extra[SecChorus] = G ({ { LS, ".......g......g." }, { LK, "...x............" } });
        d[StSixties].extra[SecBridge] = G ({ { LS, "..............g." } });
        d[StSixties].fill[0] = G ({ { LS, "........x.xx...." }, { LT1, "............x..." }, { LT2, ".............x.." }, { LF, "..............xx" }, { LK, "........x......." } });
        d[StSixties].fill[1] = G ({ { LS, "........xoxoxoxx" }, { LK, "........x......." } });

        d[StMotorik].sec[SecVerse]  = G ({ { LH, H8 }, { LK, "x.......x.x....." }, { LS, BB } });
        d[StMotorik].sec[SecChorus] = G ({ { LH, "x...x...x...x..." }, { LO, "..o...o...o...o." }, { LK, "x.......x.x....." }, { LS, BB } });
        d[StMotorik].sec[SecBridge] = G ({ { LRD, H8 }, { LK, "x...x...x...x..." }, { LS, BB } });
        d[StMotorik].extra[SecVerse]  = G ({ { LH, ".-.-.-.-.-.-.-.-" } });
        d[StMotorik].extra[SecChorus] = G ({ { LK, "..........x....." }, { LS, "..............g." } });
        d[StMotorik].extra[SecBridge] = G ({ { LS, "..............g." } });
        d[StMotorik].fill[0] = G ({ { LS, "........xoxoxoxx" }, { LK, "........x......." } });
        d[StMotorik].fill[1] = G ({ { LS, "........xo......" }, { LT1, "..........xo...." }, { LT2, "............xo.." }, { LF, "..............xx" }, { LK, "........x......." } });

        d[StKraut].sec[SecVerse]  = G ({ { LH, "x-o-x-o-x-o-x-o-" }, { LK, "x..x..x...x..x.." }, { LS, "....x.......x..g" } });
        d[StKraut].sec[SecChorus] = G ({ { LH, "x-o-x-o-x-o-x-.-" }, { LO, "..............o." }, { LK, "x..x..x...x..x.." }, { LS, "....x..g....x.g." } });
        d[StKraut].sec[SecBridge] = G ({ { LF, "x..x..x...x..x.." }, { LT2, "..x...x.x...x..." }, { LK, "x.......x......." }, { LS, "....o.......o..." }, { LH, "..-...-...-...-." } });
        d[StKraut].extra[SecVerse]  = G ({ { LS, ".g.....g.g......" } });
        d[StKraut].extra[SecChorus] = G ({ { LS, ".g.......g......" }, { LK, ".......x........" } });
        d[StKraut].extra[SecBridge] = G ({ { LS, ".g....g....g...." } });
        d[StKraut].fill[0] = G ({ { LS, "........xgxgxoxx" }, { LT2, "..............x." }, { LK, "........x..x...." } });
        d[StKraut].fill[1] = G ({ { LF, "........x.xx.x.x" }, { LT2, ".........x..x.x." }, { LK, "........x......." } });

        d[StCrazyHorse].sec[SecVerse]  = G ({ { LRD, H8 }, { LK, "x.......x......." }, { LS, BBX } });
        d[StCrazyHorse].sec[SecChorus] = G ({ { LO, H8 }, { LK, "x.....x.x......." }, { LS, BBX } });
        d[StCrazyHorse].sec[SecBridge] = G ({ { LO, ".o.ox.o.x.o.x.o." }, { LC, "x..............." }, { LK, "x.....x.x.x....." }, { LS, BBX } });
        d[StCrazyHorse].extra[SecVerse]  = G ({ { LK, "..........x....." }, { LS, "...............g" } });
        d[StCrazyHorse].extra[SecChorus] = G ({ { LK, "...........x...." }, { LC, "........x......." } });
        d[StCrazyHorse].extra[SecBridge] = G ({ { LS, "...............g" } });
        d[StCrazyHorse].fill[0] = G ({ { LS, "........x.o.x..." }, { LT2, "..........x....." }, { LT3, "...........x...." }, { LF, "............x.xx" }, { LK, "........x......." } });
        d[StCrazyHorse].fill[1] = G ({ { LS, "........X...X.xx" }, { LF, "..........x...x." }, { LC, "........x......." }, { LK, "........x...x..." } });

        d[StPowerPop].sec[SecVerse]  = G ({ { LH, H8 }, { LK, "x.x.....x.x....." }, { LS, BB } });
        d[StPowerPop].sec[SecChorus] = G ({ { LO, H8 }, { LK, "x.x...x.x.x....." }, { LS, BB } });
        d[StPowerPop].sec[SecBridge] = G ({ { LF, H8 }, { LK, "x.......x......." }, { LS, "............x..." } });
        d[StPowerPop].extra[SecVerse]  = G ({ { LK, "...x.......x...." }, { LS, ".......g.......g" } });
        d[StPowerPop].extra[SecChorus] = G ({ { LS, ".......g.......g" } });
        d[StPowerPop].extra[SecBridge] = G ({ { LS, "....g.......g..." } });
        d[StPowerPop].fill[0] = G ({ { LS, "........xoxoxoxo" }, { LK, "........x...x..." } });
        d[StPowerPop].fill[1] = G ({ { LS, "........xo......" }, { LT1, "..........xo...." }, { LT2, "............xo.." }, { LF, "..............xx" }, { LK, "........x......." } });

        d[StBallad].sec[SecVerse]  = G ({ { LH, "o.-.o.-.o.-.o.-." }, { LK, "x.........x....." }, { LSS, "....o.......o..." } });
        d[StBallad].sec[SecChorus] = G ({ { LRD, H8 }, { LK, "x.........x....." }, { LS, BB } });
        d[StBallad].sec[SecBridge] = G ({ { LF, "x.......x......." }, { LK, "x.......x......." }, { LB, "x...x...x...x..." } });
        d[StBallad].extra[SecVerse]  = G ({ { LK, ".......x........" } });
        d[StBallad].extra[SecChorus] = G ({ { LK, ".......x........" }, { LS, "..............g." } });
        d[StBallad].extra[SecBridge] = G ({ { LF, "......o.......o." } });
        d[StBallad].fill[0] = G ({ { LS, "........x.xx.xxx" }, { LT1, "...........x...." }, { LF, "..............x." } });
        d[StBallad].fill[1] = G ({ { LS, "........x...xoxx" }, { LF, "..........x....." }, { LK, "........x......." } });
        // ---- more rock: punk, post-punk, shoegaze, grunge, half-time, four-on-the-floor, glam, surf, shuffle, folk, post-rock, garage
        const char* H16 = "x-o-x-o-x-o-x-o-";
        const char* Q = "x...x...x...x...";
        const char* RF = "........xoxoxoxx";   // a standard snare run
        const char* TOMDOWN_S = "........xo......"; const char* TOMDOWN_1 = "..........xo...."; const char* TOMDOWN_2 = "............xo.."; const char* TOMDOWN_F = "..............xx";
        auto tomFill = [&] { return G ({ { LS, TOMDOWN_S }, { LT1, TOMDOWN_1 }, { LT2, TOMDOWN_2 }, { LF, TOMDOWN_F }, { LK, "........x......." } }); };

        d[StPunk].sec[SecVerse]  = G ({ { LH, "x.x.x.x.x.x.x.x." }, { LK, "x.....x.x......." }, { LS, BBX } });
        d[StPunk].sec[SecChorus] = G ({ { LC, "x.......x......." }, { LRD, "..x.x.x...x.x.x." }, { LK, "x...x...x...x..." }, { LS, "..X...X...X...X." } });   // two-beat
        d[StPunk].sec[SecBridge] = G ({ { LF, "x.x.x.x.x.x.x.x." }, { LK, "x...x...x...x..." }, { LS, BBX } });
        d[StPunk].extra[SecVerse]  = G ({ { LK, "..........x....." } });
        d[StPunk].extra[SecChorus] = G ({ { LC, "....x.......x..." } });
        d[StPunk].extra[SecBridge] = G ({ { LS, "..............x." } });
        d[StPunk].fill[0] = G ({ { LS, "........XxXxXxXx" }, { LK, "........x...x..." } });
        d[StPunk].fill[1] = tomFill();

        d[StPostPunk].sec[SecVerse]  = G ({ { LH, "x.x.x.x.x.x.x.x." }, { LF, "o...o...o...o..." }, { LK, "x.......x.x....." }, { LS, BB } });
        d[StPostPunk].sec[SecChorus] = G ({ { LO, "..o...o...o...o." }, { LH, "x...x...x...x..." }, { LK, Q }, { LS, BBX } });
        d[StPostPunk].sec[SecBridge] = G ({ { LF, "x..x..x.x..x..x." }, { LT2, "..x...x...x...x." }, { LK, "x.......x......." }, { LS, "............o..." } });
        d[StPostPunk].extra[SecVerse]  = G ({ { LT2, "...........o...." } });
        d[StPostPunk].extra[SecChorus] = G ({ { LS, "..............g." } });
        d[StPostPunk].extra[SecBridge] = G ({ { LT1, ".......o........" } });
        d[StPostPunk].fill[0] = G ({ { LF, "........x.x.x.x." }, { LT2, ".........x.x.x.x" }, { LK, "........x......." } });
        d[StPostPunk].fill[1] = G ({ { LS, RF }, { LK, "........x......." } });

        d[StShoegaze].sec[SecVerse]  = G ({ { LRD, "x.o.x.o.x.o.x.o." }, { LK, "x.....x.x......." }, { LS, BB } });
        d[StShoegaze].sec[SecChorus] = G ({ { LC, "x...x...x...x..." }, { LRD, "..o...o...o...o." }, { LK, "x.....x.x.x....." }, { LS, BBX } });    // the wash
        d[StShoegaze].sec[SecBridge] = G ({ { LF, "x.o.x.o.x.o.x.o." }, { LK, Q }, { LS, "............x..." } });
        d[StShoegaze].extra[SecVerse]  = G ({ { LK, "...........x...." }, { LS, "..............g." } });
        d[StShoegaze].extra[SecChorus] = G ({ { LK, "...x............" } });
        d[StShoegaze].extra[SecBridge] = G ({ { LC, "x..............." } });
        d[StShoegaze].fill[0] = G ({ { LS, "........x.x.x.xx" }, { LF, "..........x...x." }, { LC, "..............x." }, { LK, "........x......." } });
        d[StShoegaze].fill[1] = tomFill();

        d[StGrunge].sec[SecVerse]  = G ({ { LH, "x.o.x.o.x.o.x.o." }, { LK, "x..x..x.x......." }, { LS, BBX } });
        d[StGrunge].sec[SecChorus] = G ({ { LC, "x.......x......." }, { LO, "..o.o.o...o.o.o." }, { LK, "x.x...x.x.x....." }, { LS, BBX } });
        d[StGrunge].sec[SecBridge] = G ({ { LRD, "x.o.x.o.x.o.x.o." }, { LK, "x.......x.x....." }, { LS, "........X......." } });                   // half-time
        d[StGrunge].extra[SecVerse]  = G ({ { LK, "...........x...." }, { LS, "...............g" } });
        d[StGrunge].extra[SecChorus] = G ({ { LK, "...x............" } });
        d[StGrunge].extra[SecBridge] = G ({ { LK, "..x............." } });
        d[StGrunge].fill[0] = G ({ { LS, "........X.X.X.XX" }, { LF, "............x.x." }, { LK, "........x...x..." } });
        d[StGrunge].fill[1] = tomFill();

        d[StHalfTime].sec[SecVerse]  = G ({ { LH, "x.o.x.o.x.o.x.o." }, { LK, "x.........x....." }, { LS, "........X......." } });
        d[StHalfTime].sec[SecChorus] = G ({ { LC, "x.......x......." }, { LRD, "..x.x.x...x.x.x." }, { LK, "x.....x...x....." }, { LS, "........X......." } });
        d[StHalfTime].sec[SecBridge] = G ({ { LF, "x...x...x...x..." }, { LK, "x.......x......." }, { LS, "........o......." } });
        d[StHalfTime].extra[SecVerse]  = G ({ { LK, "..............x." } });
        d[StHalfTime].extra[SecChorus] = G ({ { LK, "...x............" } });
        d[StHalfTime].extra[SecBridge] = G ({ { LT2, "......o........." } });
        d[StHalfTime].fill[0] = G ({ { LS, "........x...x.x." }, { LF, "..........x...x." }, { LK, "........x......." } });
        d[StHalfTime].fill[1] = G ({ { LT2, "........x.x....." }, { LF, "............x.xx" }, { LK, "........x......." } });

        d[StFourFloor].sec[SecVerse]  = G ({ { LH, "x.x.x.x.x.x.x.x." }, { LK, Q }, { LS, BB } });
        d[StFourFloor].sec[SecChorus] = G ({ { LH, H16 }, { LO, "..o...o...o...o." }, { LK, Q }, { LS, BBX } });
        d[StFourFloor].sec[SecBridge] = G ({ { LRD, "x.o.x.o.x.o.x.o." }, { LK, Q }, { LT2, "......x.......x." } });
        d[StFourFloor].extra[SecVerse]  = G ({ { LO, "..............o." } });
        d[StFourFloor].extra[SecChorus] = G ({ { LS, "...............g" } });
        d[StFourFloor].extra[SecBridge] = G ({ { LS, "....o.......o..." } });
        d[StFourFloor].fill[0] = G ({ { LS, RF }, { LK, "........x...x..." } });
        d[StFourFloor].fill[1] = tomFill();

        d[StGlam].sec[SecVerse]  = G ({ { LK, "x...x...x...x..." }, { LF, "x...x...x...x..." }, { LS, BBX }, { LH, "..o...o...o...o." } });   // stomp
        d[StGlam].sec[SecChorus] = G ({ { LK, "x.....x.x.....x." }, { LS, BBX }, { LC, "x..............." }, { LO, "x.o.x.o.x.o.x.o." } });
        d[StGlam].sec[SecBridge] = G ({ { LT1, "x.x.x.x........." }, { LF, "........x.x.x.x." }, { LK, Q }, { LS, BBX } });
        d[StGlam].extra[SecVerse]  = G ({ { LK, "......x........." } });
        d[StGlam].extra[SecChorus] = G ({ { LC, "........x......." } });
        d[StGlam].extra[SecBridge] = G ({ { LS, "...............o" } });
        d[StGlam].fill[0] = G ({ { LS, "........X.X.XXXX" }, { LK, "........x...x..." } });
        d[StGlam].fill[1] = tomFill();

        d[StSurf].sec[SecVerse]  = G ({ { LRD, "x.x.x.x.x.x.x.x." }, { LK, "x.....x.x.....x." }, { LS, BB } });
        d[StSurf].sec[SecChorus] = G ({ { LT2, "xoxo....xoxo...." }, { LF, "....xoxo....xoxo" }, { LK, Q } });                                     // the tom gallop
        d[StSurf].sec[SecBridge] = G ({ { LS, "x-o-x-o-x-o-x-o-" }, { LK, "x.......x......." } });
        d[StSurf].extra[SecVerse]  = G ({ { LS, ".......g.......g" } });
        d[StSurf].extra[SecChorus] = G ({ { LS, "....o.......o..." } });
        d[StSurf].extra[SecBridge] = G ({ { LC, "x..............." } });
        d[StSurf].fill[0] = G ({ { LT2, "........xoxoxo.." }, { LF, "..............xx" }, { LK, "........x......." } });
        d[StSurf].fill[1] = G ({ { LS, "........xoxoxoxo" }, { LK, "........x...x..." } });

        // shuffle: hits on the beat and the last 16th, pushed toward a triplet by Swing
        d[StShuffle].sec[SecVerse]  = G ({ { LH, "x..ox..ox..ox..o" }, { LK, "x.......x......." }, { LS, BB } });
        d[StShuffle].sec[SecChorus] = G ({ { LRD, "x..ox..ox..ox..o" }, { LK, "x.....x.x......." }, { LS, BBX } });
        d[StShuffle].sec[SecBridge] = G ({ { LH, "x..ox..ox..ox..o" }, { LK, "x..ox..ox..ox..o" }, { LS, BB } });        // train beat
        d[StShuffle].extra[SecVerse]  = G ({ { LS, "...g...g...g...g" } });
        d[StShuffle].extra[SecChorus] = G ({ { LK, "...........o...." } });
        d[StShuffle].extra[SecBridge] = G ({ { LS, "...g.......g...." } });
        d[StShuffle].fill[0] = G ({ { LS, "........x..xx..x" }, { LF, "..............x." }, { LK, "........x......." } });
        d[StShuffle].fill[1] = G ({ { LS, "........x..x...." }, { LT1, "............x..x" }, { LF, "..............x." }, { LK, "........x......." } });

        d[StFolk].sec[SecVerse]  = G ({ { LH, "-.-.-.-.-.-.-.-." }, { LK, "x.....x.x......." }, { LSS, "....o.......o..." } });
        d[StFolk].sec[SecChorus] = G ({ { LO, "o.o.o.o.o.o.o.o." }, { LK, "x.....x.x......." }, { LS, BB } });
        d[StFolk].sec[SecBridge] = G ({ { LRD, "o.-.o.-.o.-.o.-." }, { LK, "x.......x......." }, { LSS, "....o.......o..." } });
        d[StFolk].extra[SecVerse]  = G ({ { LK, "...........o...." } });
        d[StFolk].extra[SecChorus] = G ({ { LS, "..............g." } });
        d[StFolk].extra[SecBridge] = G ({ { LK, "..............o." } });
        d[StFolk].fill[0] = G ({ { LS, "........o.o.o.xx" }, { LK, "........x......." } });
        d[StFolk].fill[1] = G ({ { LS, "........o...o..." }, { LT1, "..........o....." }, { LF, "..............x." } });

        d[StPostRock].sec[SecVerse]  = G ({ { LRD, "o.-.o.-.o.-.o.-." }, { LK, "x.......x......." }, { LSS, "....o.......o..." } });
        d[StPostRock].sec[SecChorus] = G ({ { LC, "x.x.x.x.x.x.x.x." }, { LK, "x.x.x.x.x.x.x.x." }, { LS, BBX } });                                    // the climax
        d[StPostRock].sec[SecBridge] = G ({ { LF, "x.x.x.x.x.x.x.x." }, { LT2, "...x.......x...." }, { LK, "x.......x......." } });                     // the build
        d[StPostRock].extra[SecVerse]  = G ({ { LT2, "..........o....." } });
        d[StPostRock].extra[SecChorus] = G ({ { LS, "...............x" } });
        d[StPostRock].extra[SecBridge] = G ({ { LS, "....o.......o..." } });
        d[StPostRock].fill[0] = G ({ { LF, "........xxxxxxxx" }, { LS, "............XXXX" }, { LK, "........x......." } });
        d[StPostRock].fill[1] = tomFill();

        d[StGarage].sec[SecVerse]  = G ({ { LO, "o.o.o.o.o.o.o.o." }, { LK, "x.x...x.x.x....." }, { LS, BBX } });
        d[StGarage].sec[SecChorus] = G ({ { LC, "x.x.x.x.x.x.x.x." }, { LK, "x.x...x.x.x....." }, { LS, BBX } });
        d[StGarage].sec[SecBridge] = G ({ { LF, "x.x.x.x.x.x.x.x." }, { LK, "x.......x......." }, { LS, BBX } });
        d[StGarage].extra[SecVerse]  = G ({ { LS, ".......o.......o" } });
        d[StGarage].extra[SecChorus] = G ({ { LK, "...x.......x...." } });
        d[StGarage].extra[SecBridge] = G ({ { LS, "..............o." } });
        d[StGarage].fill[0] = G ({ { LS, "........XoXoXoXX" }, { LK, "........x......." } });
        d[StGarage].fill[1] = tomFill();
        // ---- hip-hop, electronic, 80s, soul
        const char* BB2 = "....x.......x...";
        const char* H16b = "x-x-x-x-x-x-x-x-";
        const char* OFF = "..o...o...o...o.";
        const char* QQ = "x...x...x...x...";
        const char* RUN = "........xoxoxoxx";
        auto tomRun = [&] { return G ({ { LT1, "........xx......" }, { LT2, "..........xx...." }, { LF, "............xxxx" }, { LK, "........x......." } }); };
        auto set = [&] (int st, Groove v, Groove c, Groove b, Groove ev, Groove ec, Groove eb, Groove f0, Groove f1)
        { d[st].sec[SecVerse] = v; d[st].sec[SecChorus] = c; d[st].sec[SecBridge] = b; d[st].extra[SecVerse] = ev; d[st].extra[SecChorus] = ec; d[st].extra[SecBridge] = eb; d[st].fill[0] = f0; d[st].fill[1] = f1; };

        set (StBoomBap,
             G ({ { LH, "x.x.x.x.x.x.x.x." }, { LK, "x......x.x......" }, { LS, BB2 } }),
             G ({ { LH, "x.x.x.x.x.x.x.o." }, { LK, "x......x.x...x.." }, { LS, BB2 } }),
             G ({ { LRD, "x.x.x.x.x.x.x.x." }, { LK, "x.........x....." }, { LS, BB2 } }),
             G ({ { LS, "......g.......g." } }), G ({ { LK, "...........x...." } }), G ({ { LS, "...............g" } }),
             G ({ { LK, "........x..x.x.." }, { LS, "............x..." } }), G ({ { LS, "........x...x.xx" } }));
        set (StTrap,
             G ({ { LH, "x.x.x.x.x.x.r.x." }, { LK, "x.......x..x...." }, { LS, "........x......." }, { LCL, "........x......." } }),
             G ({ { LH, "x-x-x-x-xrx-x-tq" }, { LK, "x..x....x.x....." }, { LS, "........x......." }, { LCL, "........x......." }, { LO, "......o........." } }),
             G ({ { LH, "x...x...x...x..." }, { LK, "x..............." }, { LCL, "........x......." } }),
             G ({ { LK, "...............x" } }), G ({ { LH, "..............rr" } }), G ({ { LK, "..........x....." } }),
             G ({ { LH, "........tqtqrrrr" }, { LS, "............x.xx" } }), G ({ { LK, "........x.x.x.xx" }, { LCL, "............x..." } }));
        set (StLofi,
             G ({ { LH, "-.-.-.-.-.-.-.-." }, { LK, "x.......x.x....." }, { LS, "....o.......o..." } }),
             G ({ { LH, "x.-.x.-.x.-.x.-." }, { LK, "x......x..x....." }, { LS, "....o.......o..." } }),
             G ({ { LRD, "-.-.-.-.-.-.-.-." }, { LK, "x.........x....." }, { LSS, "....o.......o..." } }),
             G ({ { LS, "..........g....." } }), G ({ { LK, "...x............" } }), G ({ { LSH, "..-...-...-...-." } }),
             G ({ { LS, "........o..o.o.." } }), G ({ { LK, "........x...x..." } }));
        set (StGFunk,
             G ({ { LH, H16b }, { LK, "x.....x.x.x....." }, { LS, BB2 }, { LCL, BB2 } }),
             G ({ { LH, "x-x-x-x-x-x-x-x-" }, { LO, "......o.......o." }, { LK, "x.....x.x.x..x.." }, { LS, BB2 }, { LCL, BB2 } }),
             G ({ { LRD, "x.x.x.x.x.x.x.x." }, { LK, "x.......x......." }, { LS, BB2 } }),
             G ({ { LS, ".......g......g." } }), G ({ { LK, "...........x...." } }), G ({ { LSH, "x-x-x-x-x-x-x-x-" } }),
             G ({ { LS, RUN } }), tomRun());
        set (StElectro,
             G ({ { LH, H16b }, { LK, "x.....x...x....." }, { LCL, BB2 } }),
             G ({ { LH, H16b }, { LK, "x.....x...x...x." }, { LCL, BB2 }, { LCB, "..x...x...x....x" } }),
             G ({ { LH, "x...x...x...x..." }, { LK, "x.....x...x....." }, { LT2, "......x.......x." } }),
             G ({ { LCL, "..............x." } }), G ({ { LK, "...x............" } }), G ({ { LCB, "x..............." } }),
             tomRun(), G ({ { LCL, "........x.x.xxxx" } }));
        set (StHouse,
             G ({ { LK, QQ }, { LO, OFF }, { LCL, BB2 } }),
             G ({ { LK, QQ }, { LO, OFF }, { LH, "x-x-x-x-x-x-x-x-" }, { LCL, BB2 }, { LSH, "x-x-x-x-x-x-x-x-" } }),
             G ({ { LK, "x.......x......." }, { LO, OFF }, { LCGH, "..x..x....x..x.." } }),
             G ({ { LH, "x...x...x...x..." } }), G ({ { LCGL, "...x.......x...." } }), G ({ { LCL, "............x..." } }),
             G ({ { LCL, "........x.x.xxxx" }, { LK, "........x...x..." } }), G ({ { LS, "........xxxxxxxx" }, { LK, "........x...x..." } }));
        set (StTechno,
             G ({ { LK, QQ }, { LO, OFF }, { LSS, BB2 } }),
             G ({ { LK, QQ }, { LO, OFF }, { LH, "xxxxxxxxxxxxxxxx" }, { LCL, BB2 } }),
             G ({ { LK, QQ }, { LT2, "...x......x....." }, { LRD, "x.x.x.x.x.x.x.x." } }),
             G ({ { LH, "..x...x...x...x." } }), G ({ { LSS, "...............x" } }), G ({ { LCB, "..x.......x....." } }),
             G ({ { LCL, "........xxxxxxxx" }, { LK, "........x...x..." } }), tomRun());
        set (StDnB,
             G ({ { LH, "x.x.x.x.x.x.x.x." }, { LK, "x.........x....." }, { LS, BB2 } }),
             G ({ { LRD, "x.x.x.x.x.x.x.x." }, { LK, "x.........xx...." }, { LS, BB2 }, { LO, "..............o." } }),
             G ({ { LH, "x...x...x...x..." }, { LK, "x.........x....." }, { LS, "............x..." } }),
             G ({ { LS, "......g..g.....g" } }), G ({ { LS, ".......g..g...g." } }), G ({ { LK, "..x............." } }),
             G ({ { LS, "........x.xxx.xx" }, { LK, "........x.....x." } }), tomRun());
        set (StSynthwave,
             G ({ { LH, H16b }, { LK, QQ }, { LS, BB2 } }),
             G ({ { LH, H16b }, { LO, OFF }, { LK, QQ }, { LS, "....X.......X..." } }),
             G ({ { LRD, "x.x.x.x.x.x.x.x." }, { LK, "x.......x......." }, { LS, "............x..." } }),
             G ({ { LK, "..........x....." } }), G ({ { LCL, BB2 } }), G ({ { LT2, "..............x." } }),
             tomRun(), G ({ { LS, "........x.x.xxxx" }, { LK, "........x......." } }));
        set (StHairMetal,
             G ({ { LO, "o.o.o.o.o.o.o.o." }, { LK, "x.x...x.x.x....." }, { LS, "....X.......X..." } }),
             G ({ { LC, "x.x.x.x.x.x.x.x." }, { LK, "x.x.x.x.x.x.x.x." }, { LS, "....X.......X..." } }),
             G ({ { LRD, "x.o.x.o.x.o.x.o." }, { LK, "x.......x.x....." }, { LS, "........X......." } }),
             G ({ { LK, "...x.......x...." } }), G ({ { LC, "x..............." } }), G ({ { LK, "..x............." } }),
             tomRun(), G ({ { LS, "........XxXxXxXX" }, { LK, "........x.x.x.x." } }));
        set (StNewJack,
             G ({ { LH, H16b }, { LK, "x..x..x...x..x.." }, { LS, "....X.......X..." }, { LCL, BB2 } }),
             G ({ { LH, H16b }, { LK, "x..x..x...x..x.." }, { LS, "....X.......X..." }, { LCL, BB2 }, { LO, "..............o." } }),
             G ({ { LH, "x.x.x.x.x.x.x.x." }, { LK, "x.....x...x....." }, { LS, BB2 } }),
             G ({ { LS, "..g.......g....." } }), G ({ { LK, ".........x......" } }), G ({ { LCL, "..............x." } }),
             G ({ { LS, "........X.X.XXXX" }, { LCL, "............x..." } }), tomRun());
        set (StFreestyle,
             G ({ { LH, H16b }, { LK, "x..x..x.x..x..x." }, { LCL, BB2 } }),
             G ({ { LH, H16b }, { LK, "x..x..x.x..x..x." }, { LCL, BB2 }, { LS, BB2 }, { LCB, "x..x..x...x..x.." } }),
             G ({ { LH, "x...x...x...x..." }, { LK, "x..x..x........." }, { LT2, "........x..x..x." } }),
             G ({ { LCL, "...............x" } }), G ({ { LO, "......o.......o." } }), G ({ { LCL, "............x.x." } }),
             tomRun(), G ({ { LCL, "........x.xxx.xx" } }));
        set (StHiNRG,
             G ({ { LK, QQ }, { LO, OFF }, { LH, "x-x-x-x-x-x-x-x-" }, { LCL, BB2 } }),
             G ({ { LK, QQ }, { LO, OFF }, { LH, "xxxxxxxxxxxxxxxx" }, { LCL, BB2 }, { LS, BB2 } }),
             G ({ { LK, QQ }, { LO, OFF }, { LT1, "......x.......x." } }),
             G ({ { LTS, "x-x-x-x-x-x-x-x-" } }), G ({ { LC, "x..............." } }), G ({ { LCL, "..............x." } }),
             tomRun(), G ({ { LS, "........xxxxxxxx" }, { LK, "........x...x..." } }));
        set (StDetroitSoul,   // snare on every beat, tambourine riding the backbeat
             G ({ { LS, "x...x...x...x..." }, { LTB, BB2 }, { LK, "x.x...x.x.x....." }, { LH, "x.x.x.x.x.x.x.x." } }),
             G ({ { LS, "x...x...x...x..." }, { LTB, QQ }, { LCL, BB2 }, { LK, "x.x...x.x.x....." }, { LO, "o.o.o.o.o.o.o.o." } }),
             G ({ { LF, "x...x...x...x..." }, { LTB, BB2 }, { LK, "x.......x......." } }),
             G ({ { LTB, "..............x." } }), G ({ { LTB, "..x.......x....." } }), G ({ { LS, "..............g." } }),
             G ({ { LS, "........x.x.x.xx" }, { LTB, "........x...x..." } }), G ({ { LS, "........x...xxxx" }, { LF, "..........x....." } }));
        set (StPhillySoul,    // four on the floor, sizzling hats, tambourine and congas
             G ({ { LK, QQ }, { LO, OFF }, { LH, "x-x-x-x-x-x-x-x-" }, { LS, BB2 }, { LTB, BB2 } }),
             G ({ { LK, QQ }, { LO, OFF }, { LH, "x-x-x-x-x-x-x-x-" }, { LS, BB2 }, { LTS, "x-x-x-x-x-x-x-x-" }, { LCGH, "..x..x....x..x.." }, { LCGL, "x.......x......." } }),
             G ({ { LK, QQ }, { LRD, "x.x.x.x.x.x.x.x." }, { LCGH, "..x..x....x..x.." }, { LS, BB2 } }),
             G ({ { LCGH, "..x.......x....." } }), G ({ { LS, "..............g." } }), G ({ { LTB, BB2 } }),
             G ({ { LS, RUN }, { LCGH, "........x.x....." }, { LCGL, "............x.x." } }), tomRun());
        set (StFunk,
             G ({ { LH, H16b }, { LK, "x.....x...x..x.." }, { LS, BB2 } }),
             G ({ { LH, H16b }, { LK, "x.x...x...x..x.." }, { LS, BB2 }, { LO, ".......o........" } }),
             G ({ { LRD, "x.x.x.x.x.x.x.x." }, { LK, "x.....x...x....." }, { LS, BB2 }, { LCB, "x...x...x...x..." } }),
             G ({ { LS, "..g..g.g.g..g.g." } }), G ({ { LS, ".g.....g..g....g" } }), G ({ { LCGH, "..x..x....x..x.." } }),
             G ({ { LS, "........xgxgxgxx" }, { LK, "........x....x.." } }), tomRun());
        set (StDisco,
             G ({ { LK, QQ }, { LO, OFF }, { LH, "x.x.x.x.x.x.x.x." }, { LS, BB2 } }),
             G ({ { LK, QQ }, { LO, OFF }, { LH, "x-x-x-x-x-x-x-x-" }, { LS, BB2 }, { LCL, BB2 }, { LTS, "x-x-x-x-x-x-x-x-" } }),
             G ({ { LK, QQ }, { LO, OFF }, { LCB, "..x...x...x...x." } }),
             G ({ { LTB, BB2 } }), G ({ { LCGH, "..x..x....x..x.." } }), G ({ { LS, "..............g." } }),
             G ({ { LS, RUN } }), tomRun());
        set (StTripHop,
             G ({ { LH, "x..ox..ox..ox..o" }, { LK, "x.......x.x....." }, { LS, BB2 } }),
             G ({ { LH, "x..ox..ox..ox..o" }, { LK, "x......xx.x....." }, { LS, "....X.......X..." }, { LSH, "x-x-x-x-x-x-x-x-" } }),
             G ({ { LRD, "x.-.x.-.x.-.x.-." }, { LK, "x.........x....." }, { LSS, BB2 } }),
             G ({ { LS, "......g........g" } }), G ({ { LK, "...........x...." } }), G ({ { LCB, "..............x." } }),
             G ({ { LS, "........x..x.x.." } }), G ({ { LK, "........x..xx..." } }));
        set (StPowerBallad,
             G ({ { LH, "x.o.x.o.x.o.x.o." }, { LK, "x.......x.x....." }, { LS, "....X.......X..." } }),
             G ({ { LC, "x...x...x...x..." }, { LO, "..o...o...o...o." }, { LK, "x.....x.x.x....." }, { LS, "....X.......X..." } }),
             G ({ { LRD, "x.o.x.o.x.o.x.o." }, { LK, "x.......x......." }, { LS, "............X..." } }),
             G ({ { LK, "...........x...." } }), G ({ { LC, "........x......." } }), G ({ { LT2, "..............x." } }),
             tomRun(), G ({ { LS, "........X.X.XXXX" }, { LK, "........x......." } }));
        set (StNorthernSoul,
             G ({ { LS, "x...x...x...x..." }, { LTB, QQ }, { LK, QQ }, { LO, OFF } }),
             G ({ { LS, "x...x...x...x..." }, { LTB, QQ }, { LCL, BB2 }, { LK, QQ }, { LO, "o.o.o.o.o.o.o.o." } }),
             G ({ { LF, "x...x...x...x..." }, { LTB, BB2 }, { LK, QQ } }),
             G ({ { LS, "..............g." } }), G ({ { LTB, "..x...x...x...x." } }), G ({ { LCL, "............x..." } }),
             G ({ { LS, "........x.x.xxxx" }, { LTB, "........x...x..." } }), tomRun());

        // ---- three beats to the bar: 12 sixteenths. 3/4 beats fall on 0, 4, 8; 6/8 eighths on every other step, beats on 0 and 6
        auto tomRun3 = [&] { return G ({ { LT1, "......xx...." }, { LT2, "........xx.." }, { LF, "..........xx" }, { LK, "......x....." } }); };
        set (StWaltz,
             G ({ { LK, "x..........." }, { LH, "x...o...o..." }, { LSS, "....o...o..." } }),
             G ({ { LK, "x.......x..." }, { LRD, "x.o.x.o.x.o." }, { LS, "....x...x..." } }),
             G ({ { LF, "x..........." }, { LK, "x..........." }, { LRD, "x...x...x..." }, { LS, "........o..." } }),
             G ({ { LK, "......x....." } }), G ({ { LS, "..........g." } }), G ({ { LS, ".......g...." } }),
             G ({ { LS, "......x.x.xx" }, { LK, "......x....." } }), tomRun3());
        set (StJazzWaltz,
             G ({ { LRD, "x...x.x.x..." }, { LH, "....-...-..." }, { LK, "m..........." }, { LS, "......g....." } }),
             G ({ { LRD, "x...x.x.x.x." }, { LH, "....-...-..." }, { LK, "x.......m..." }, { LS, "....g...o..." } }),
             G ({ { LRD, "x...x...x..." }, { LK, "m..........." }, { LSS, "....o...o..." } }),
             G ({ { LS, "..g.......g." } }), G ({ { LS, "..g......g.." } }), G ({ { LS, ".........g.." } }),
             G ({ { LS, "......xoxo.x" }, { LK, "......x....." } }), G ({ { LS, "......x....." }, { LT1, "........x..." }, { LF, "..........x." } }));
        set (StFolkWaltz,
             G ({ { LF, "x..........." }, { LTB, "....x...x..." }, { LK, "x..........." } }),
             G ({ { LK, "x..........." }, { LS, "....x...x..." }, { LTB, "x.x.x.x.x.x." } }),
             G ({ { LF, "x...x...x..." }, { LTB, "x...x...x..." }, { LK, "x..........." } }),
             G ({ { LK, "........x..." } }), G ({ { LK, "......x....." } }), G ({ { LTS, "..-...-...-." } }),
             G ({ { LS, "......x.x.xx" }, { LTB, "......x...x." } }), tomRun3());
        set (StRockWaltz,
             G ({ { LH, "x.o.x.o.x.o." }, { LK, "x.......x..." }, { LS, "....X......." } }),
             G ({ { LC, "x..........." }, { LO, "..o.x.o.x.o." }, { LK, "x...x...x..." }, { LS, "....X...X..." } }),
             G ({ { LRD, "x.o.x.o.x.o." }, { LK, "x.....x....." }, { LS, "........X..." } }),
             G ({ { LK, "..........x." } }), G ({ { LS, "...........g" } }), G ({ { LK, "..x........." } }),
             G ({ { LS, "......X.X.XX" }, { LK, "......x....." } }), tomRun3());
        set (St68Ballad,
             G ({ { LH, "x.x.x.x.x.x." }, { LK, "x.........x." }, { LSS, "......o....." } }),
             G ({ { LRD, "x.x.x.x.x.x." }, { LK, "x.......x..." }, { LS, "......x....." } }),
             G ({ { LF, "o.-.-.o.-.-." }, { LK, "x.....x....." }, { LS, "......o....." } }),
             G ({ { LK, "....x......." } }), G ({ { LK, "....x.....x." } }), G ({ { LS, "..........g." } }),
             G ({ { LS, "......x.x..." }, { LT1, "..........x." }, { LF, "...........x" } }), G ({ { LS, "......xoxoxx" } }));
        set (St68Rock,
             G ({ { LH, "x.x.x.x.x.x." }, { LK, "x.....x.x..." }, { LS, "......X....." } }),
             G ({ { LC, "x..........." }, { LO, "x.x.x.x.x.x." }, { LK, "x...x.x....." }, { LS, "......X....." } }),
             G ({ { LRD, "x.x.x.x.x.x." }, { LK, "x.....x....." }, { LS, "......X....." }, { LF, "......x....." } }),
             G ({ { LK, "..........x." } }), G ({ { LK, "..........x." } }), G ({ { LT2, "..........o." } }),
             G ({ { LS, "......X.X.XX" }, { LK, "......x....." } }), tomRun3());
        set (St68Folk,
             G ({ { LTB, "X.o.o.X.o.o." }, { LK, "x.....x....." } }),
             G ({ { LK, "x.....x....." }, { LS, "......x....." }, { LTB, "X.o.o.X.o.o." }, { LH, "x.x.x.x.x.x." } }),
             G ({ { LF, "x.o.o.x.o.o." }, { LK, "x.....x....." } }),
             G ({ { LS, "..........g." } }), G ({ { LK, "....x......." } }), G ({ { LTB, "......x....." } }),
             G ({ { LS, "......x.x.x." }, { LTB, "......x.x.x." } }), G ({ { LF, "......x.x.xx" }, { LK, "......x....." } }));
        set (St68SlowJam,
             G ({ { LH, "x.x.x.x.x.x." }, { LK, "x...x......." }, { LS, "......x....." }, { LCL, "......x....." } }),
             G ({ { LH, "x-x-x-x-x-x-" }, { LK, "x...x...x..." }, { LS, "......X....." }, { LCL, "......x....." } }),
             G ({ { LRD, "x.x.x.x.x.x." }, { LK, "x..........." }, { LSS, "......o....." } }),
             G ({ { LK, "..........x." } }), G ({ { LK, "...........x" } }), G ({ { LK, "....x......." } }),
             G ({ { LS, "......x..x.x" } }), G ({ { LS, "......xoxoxx" }, { LK, "......x....." } }));
        return d;
    }();
    return s;
}

class GroovePlayer
{
public:
    struct Settings
    {
        bool on = false;
        int style = 0, section = 0, fillEvery = Fill8;
        float intensity = 0.5f, looseness = 0.3f, swing = 0.0f, variation = 0.5f;
        int meter = band::M44;                // 4/4, 3/4 or 6/8 (12 sixteenths a bar)
        const band::Leader* link = nullptr;   // set when a Bandleader is running the song
        int swells = 1;                       // backwards cymbals: off, into choruses, into every section
    };
    double swellSec = 0;                      // the length of the swell just sent (read by the sound engine)

    // UI / key-switch requests (any thread)
    std::atomic<bool> requestFill { false }, requestEnding { false };
    // Read by the UI
    std::atomic<int> uiSection { 0 }, uiStep { -1 }, uiBar { 0 };
    std::atomic<bool> uiFillComing { false }, uiEnded { false }, uiRunning { false };
    bool fillingNow() const { return fillThisBar; }

    void prepare (double newSr) { sr = newSr; reset(); }

    void reset()
    {
        pendingCount = 0;
        lastStep = -1;
        section = -1;
        ended = false;
        fillThisBar = false;
        fillNextBar = false;
        endingStage = 0;
        barInSection = 0;
        uiStep = -1;
        uiRunning = false;
    }

    // Handles key switches in `midi`, then adds the drummer's hits for this block to `midi`
    // (for the kit) and `out` (MIDI output, for recording). `now` = absolute sample time of block start.
    void process (juce::MidiBuffer& midi, juce::MidiBuffer& out, int numSamples, int64_t now,
                  const Settings& s, juce::AudioPlayHead* playHead)
    {
        // key switches
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            if (! msg.isNoteOn()) continue;
            switch (msg.getNoteNumber())
            {
                case kKeyVerse:  keySection = SecVerse;  sectionFromKey = true; break;
                case kKeyChorus: keySection = SecChorus; sectionFromKey = true; break;
                case kKeyBridge: keySection = SecBridge; sectionFromKey = true; break;
                case kKeyFill:   requestFill = true; break;
                case kKeyEnding: requestEnding = true; break;
                default: break;
            }
        }
        if (s.section != lastParamSection) { lastParamSection = s.section; sectionFromKey = false; }
        const int wanted = sectionFromKey ? keySection : s.section;

        bool playing = false;
        double ppq = 0, bpm = 120;
        if (playHead != nullptr)
            if (auto pos = playHead->getPosition())
            {
                playing = pos->getIsPlaying();
                if (auto p = pos->getPpqPosition()) ppq = *p;
                if (auto b = pos->getBpm()) bpm = *b;
            }

        if (! s.on || ! playing)
        {
            if (uiRunning) reset();
            flush (midi, out, numSamples, now);
            return;
        }
        uiRunning = true;

        const double ppqPerSample = bpm / 60.0 / sr;
        const double ppqEnd = ppq + ppqPerSample * numSamples;
        const double stepLenSamples = 0.25 / ppqPerSample;

        // a jump in the timeline (loop or relocate) restarts the phrase
        if (lastStep >= 0 && (std::floor (ppq * 4.0) < lastStep || std::floor (ppq * 4.0) > lastStep + 2))
        {
            barInSection = 0;
            fillThisBar = fillNextBar = false;
        }

        const int ns = band::meterSteps (s.meter);
        if (ns != lastNs) { lastNs = ns; barInSection = 0; fillThisBar = fillNextBar = false; }
        for (int64_t k = (int64_t) std::ceil (ppq * 4.0 - 1.0e-9); (double) k * 0.25 < ppqEnd; ++k)
        {
            const double stepPpq = (double) k * 0.25;
            const int offset = juce::jlimit (0, numSamples - 1, (int) ((stepPpq - ppq) / ppqPerSample));
            const int step = (int) (((k % ns) + ns) % ns);
            const int64_t bar = (k - step) / ns;
            lastStep = k;
            int w = wanted;
            // following the Bandleader: this bar's section on the downbeat, the coming one after it
            if (s.link != nullptr) w = s.link->sectionAt (step == 0 ? bar : bar + 1);
            playStep (step, now + offset, stepLenSamples, s, w, bar);
        }

        flush (midi, out, numSamples, now);
    }

private:
    struct Pending { int64_t t; uint8_t note, vel; bool on; };
    static constexpr int kMaxPending = 512;
    Pending pending[kMaxPending];
    int pendingCount = 0;

    double sr = 44100.0;
    int64_t lastStep = -1;
    int lastNs = 16;
    int section = -1, keySection = 0, lastParamSection = -1;
    bool sectionFromKey = false;
    bool ended = false, fillThisBar = false, fillNextBar = false;
    int endingStage = 0;          // 0 none, 1 ending fill this bar, 2 final hit next bar
    int barInSection = 0;
    int fillAlt = 0;
    uint32_t rng = 0x1234567u;

    float rand01() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng & 0xffffff) / 16777216.0f; }
    float gauss() { return (rand01() + rand01() + rand01() - 1.5f) * 1.41f; }

    static float velOf (char c)
    {
        switch (c)
        {
            case 'X': return 120; case 'x': return 102; case 'o': return 82;
            case 'm': return 66;  case '-': return 52;  case 'g': return 34;
            case 'r': case 't': case 'q': return 74;   // rolls: 2, 3 or 4 hits in one 16th
            default: return 0;
        }
    }

    void push (int64_t t, int note, float vel)
    {
        if (pendingCount + 2 > kMaxPending) return;
        const uint8_t v = (uint8_t) juce::jlimit (1, 127, (int) std::lround (vel));
        pending[pendingCount++] = { t, (uint8_t) note, v, true };
        pending[pendingCount++] = { t + (int64_t) (0.05 * sr), (uint8_t) note, 0, false };
    }

    void flush (juce::MidiBuffer& midi, juce::MidiBuffer& out, int numSamples, int64_t now)
    {
        int w = 0;
        for (int i = 0; i < pendingCount; ++i)
        {
            const auto& p = pending[i];
            if (p.t < now + numSamples)
            {
                const int off = (int) juce::jlimit<int64_t> (0, numSamples - 1, p.t - now);
                const auto m = p.on ? juce::MidiMessage::noteOn (10, p.note, (juce::uint8) p.vel)
                                    : juce::MidiMessage::noteOff (10, p.note);
                if (p.on) midi.addEvent (m, off);
                out.addEvent (m, off);
            }
            else pending[w++] = p;
        }
        pendingCount = w;
    }

    void playStep (int step, int64_t t, double stepLen, const Settings& s, int wanted, int64_t bar)
    {
        uiStep = step;
        const int ns = band::meterSteps (s.meter);          // steps in this bar: 16, 12 for 3/4 and 6/8, 20 for 5/4, 14 for 7/8
        const int pns = band::patternSteps (s.meter);      // steps of the pattern it plays (5/4 and 7/8 play a 4/4 pattern through a map)
        const int ps = band::oddSrc (s.meter, step);
        const int half = ns / 2, last = ns - 1;
        const int beat = band::meterBeatSteps (s.meter);    // 4 (a quarter), or 6 in 6/8 (a dotted quarter)
        const int pulse = s.meter == band::M68 ? 2 : 4;     // where the hats count: quarters, or 6/8's eighths
        const auto& st = styles()[(size_t) juce::jlimit (0, NumStyles - 1, styleForMeter (s.style, s.meter))];
        const band::Leader* L = s.link;

        if (step == 0)
        {
            // new bar
            bool sectionStart = false;
            formBar = L != nullptr ? (L->learning ? 1 : L->isCountIn (bar) ? 2 : L->isFinalHit (bar) ? 3 : (L->isOver (bar) || L->isEnding (bar)) ? 4 : (! L->isOut (bar) && L->rests (bar, true)) ? 5 : 0) : 0;
            if (formBar == 3)
            {
                // the song's last chord: one big hit, then let it ring
                hit (laneNote[LC], 124, t, s, LC);
                hit (laneNote[LK], 124, t, s, LK);
                hit (laneNote[LF], 110, t, s, LF);
                fillThisBar = false; wasOut = true;
                return;
            }
            if (formBar != 0) { wasOut = true; fillThisBar = fillNextBar = false; if (formBar == 5 || formBar == 4) return; }
            if (section < 0) { section = wanted; barInSection = 0; sectionStart = true; ended = false; }
            if (endingStage == 2)
            {
                // the final hit, then silence until something changes
                hit (laneNote[LC], 124, t, s, LC);
                hit (laneNote[LK], 124, t, s, LK);
                hit (laneNote[LF], 110, t, s, LF);
                endingStage = 0;
                ended = true;
                uiEnded = true;
                fillThisBar = false;
                return;
            }
            if (ended)
            {
                if (wanted != section || requestFill.exchange (false)) { ended = false; uiEnded = false; section = wanted; barInSection = 0; sectionStart = true; }
                else return;
            }
            if (wanted != section) { section = wanted; barInSection = 0; sectionStart = true; }
            else if (! sectionStart) ++barInSection;
            if (L != nullptr && formBar == 0)
            {
                int64_t in = 0; L->sectionAt (bar, &in); barInSection = (int) in;
                const bool out = L->isOut (bar);
                if (wasOut && ! out) sectionStart = true;      // everybody back in: crash
                const bool intro = L->introStage (bar) != 0;
                if (wasIntro && ! intro) sectionStart = true;  // out of the intro into the song
                wasIntro = intro;
                wasOut = outBar = out;
                if (L->fillAt (bar)) fillNextBar = true;       // the band fills at the end of this bar
                halfBar = L->isHalf (bar);
                introBar = intro;
            }
            else if (formBar == 0) { outBar = wasOut = false; halfBar = introBar = false; }

            // a backwards cymbal over the second half of this bar when the next bar starts a new section
            swellThisBar = false;
            if (L != nullptr && formBar == 0 && s.swells > 0 && ! outBar && ! L->isHalf (bar))
            {
                int64_t inNext = 0;
                const int now = L->sectionAt (bar), next = L->sectionAt (bar + 1, &inNext);
                if (inNext == 0 && next != now && ! L->isOut (bar + 1) && ! L->isEnding (bar + 1) && ! L->isOver (bar + 1)
                    && (s.swells == 2 || next == 1) && rand01() < 0.85f)
                    swellThisBar = true;
            }
            fillThisBar = fillNextBar;
            const bool requested = fillNextBar;
            fillNextBar = false;
            const int every = s.fillEvery == Fill4 ? 4 : s.fillEvery == Fill8 ? 8 : s.fillEvery == Fill16 ? 16 : 0;
            const int phraseLen = every > 0 ? every : 8;
            barsLeftInPhrase = phraseLen - 1 - (barInSection % phraseLen);
            if (every > 0 && barsLeftInPhrase == 0 && formBar == 0) fillThisBar = true;
            if (endingStage == 1) { fillThisBar = true; endingStage = 2; }
            if (fillThisBar) chooseFill (s, ! requested && endingStage == 0, pns);

            // the drummer's feel drifts a little from bar to bar
            const float V = s.variation;
            energy += (1.0f - energy) * 0.25f + gauss() * 0.035f * V;
            energy = juce::jlimit (0.88f, 1.12f, energy);
            timeBiasMs = juce::jlimit (-3.0f, 3.0f, timeBiasMs * 0.85f + gauss() * 0.7f * (0.3f + V));
            hatAccent = 1.0f + (rand01() - 0.3f) * 0.35f * (0.3f + V);

            uiSection = section;
            uiBar = barInSection + 1;

            if (formBar == 0 && sectionStart && (barInSection == 0 || L != nullptr) && ! outBar && ! introBar)
                hit (laneNote[LC], 112 + 10 * s.intensity, t, s, LC);

            // tell the bass player where the kicks are (this bar, and a guess at the next)
            {
                auto mask = [&] (int sec, bool fill)
                {
                    uint16_t m = 0;
                    const Groove& gg = st.sec[juce::jlimit (0, NumSections - 1, sec)];
                    for (int i = 0; i < ns && i < 16; ++i)
                    {
                        const int si = band::oddSrc (s.meter, i);
                        const bool inF = fill && si >= fillStart;
                        const char* lane = inF ? (useGenFill ? nullptr : st.fill[fillAlt].lane[LK]) : gg.lane[LK];
                        const char c = inF && useGenFill ? genFill[LK][si] : (lane != nullptr ? lane[si] : '.');
                        if (c != '.') m |= (uint16_t) (1u << i);
                    }
                    return m;
                };
                const uint16_t thisBar = formBar != 0 ? (uint16_t) 0 : outBar ? (fillThisBar ? (uint16_t) (mask (section, true) & (0xFFFFu << juce::jmin (15, fillStart))) : (uint16_t) 0) : mask (section, fillThisBar);
                band::publishKicks (bar, thisBar);
                const int nextSec = L != nullptr ? L->sectionAt (bar + 1) : section;
                band::publishKicks (bar + 1, L != nullptr && L->rests (bar + 1, true) ? (uint16_t) 0 : mask (nextSec, false));
            }

            pendingSectionStart = sectionStart && formBar == 0;
        }

        // ---- the song form's own bars
        if (formBar == 1)
        {
            // Bandleader is learning the song: keep time, quietly, on the hi-hat
            if (step % pulse == 0) hit (laneNote[LH], step == 0 ? 70.0f : 52.0f, t, s, LH);
            return;
        }
        if (formBar == 2)
        {
            // the count-in: sticks on every beat (6/8: every eighth, leaning on 1 and 4)
            const int every = s.meter == band::M68 ? 2 : 4;
            if (step % every == 0) hit (laneNote[LSS], step % beat == 0 ? 112.0f : 84.0f, t, s, LSS);
            return;
        }
        if (formBar != 0) return;

        if (ended || section < 0) return;
        // the Bandleader called a fill after this bar had started: take it if there's room
        if (L != nullptr && step > 0 && step <= half && ! fillThisBar && L->fillAt (bar)) { fillThisBar = true; chooseFill (s, false, pns); }
        if (outBar && ! (fillThisBar && ps >= fillStart && L != nullptr && ! L->isOut (bar + 1))) return;   // dropped out (but play the fill that brings everyone back)

        // requests land on the second half of the current bar if there's time, otherwise the next bar
        if (requestFill.exchange (false)) { if (step < half) { fillThisBar = true; chooseFill (s, false, pns); } else fillNextBar = true; }
        if (requestEnding.exchange (false))
        {
            if (step < half) { fillThisBar = true; chooseFill (s, false, pns); endingStage = 2; }
            else             { endingStage = 1; }
        }
        // a section change coming at the next bar gets a fill if there's room
        if (step == half && wanted != section && ! fillThisBar) { fillThisBar = true; chooseFill (s, false, pns); }
        if (step == half && swellThisBar)
        {
            // timed to land exactly on the next downbeat, under the crash
            swellSec = (double) (ns - half) * stepLen / sr;
            push (t, 26, 70.0f + 30.0f * s.intensity);
            swellThisBar = false;
        }

        uiFillComing = fillThisBar || fillNextBar || endingStage > 0;

        const bool inFill = fillThisBar && ps >= fillStart;
        nowInFill = inFill;
        const Groove& g = st.sec[section];
        const Groove& ex = st.extra[section];
        const float V = s.variation;

        const float build = (barsLeftInPhrase == 0 || fillThisBar) ? 1.05f : 1.0f;
        const float velScale = (0.78f + 0.34f * s.intensity) * energy * build * (introBar ? 0.82f : 1.0f);
        const float swingDelay = (step % 2 == 1 && s.meter != band::M68) ? (float) (s.swing * 0.333 * stepLen) : 0.0f;
        const int halfSnare = s.meter == band::M68 ? 6 : 8;

        for (int l = 0; l < NumLanes; ++l)
        {
            if (step == 0 && pendingSectionStart && (l == LH || l == LO || l == LRD) && ! inFill) continue;  // crash replaces cymbal
            char c;
            int lane = l;
            if (inFill)
            {
                c = useGenFill ? genFill[l][ps] : (st.fill[fillAlt].lane[l] != nullptr ? st.fill[fillAlt].lane[l][ps] : '.');
                // keep time on the hats/ride through a short fill's first steps
                if (c == '.' && ps < pns / 2 && g.lane[l] != nullptr && (l == LH || l == LRD || l == LO)) c = g.lane[l][ps];
            }
            else
            {
                c = g.lane[l] != nullptr ? g.lane[l][ps] : '.';
                if (! halfBar && c == '.' && ex.lane[l] != nullptr && ex.lane[l][ps] != '.' && rand01() < (s.intensity - 0.35f) * 1.6f)
                    c = ex.lane[l][ps];

                // little things a real drummer does that a loop never would
                if (l == LH && c != '.' && step % 4 == 2 && rand01() < 0.03f * V) c = '.';                  // drops a weak hat
                if (l == LH && c != '.' && step == ns - 2 && rand01() < 0.07f * V) lane = LO;               // opens the hat
                if (l == LS && c == '.' && (step % 2 == 1) && rand01() < 0.05f * V * (0.4f + s.intensity)) c = 'g';   // ghost note
                if (l == LK && c == '.' && (step == 3 || step == 7 || step == 10 || (ns == 16 && step == 14))
                    && g.lane[LS] != nullptr && g.lane[LS][ps] == '.' && rand01() < 0.025f * V) c = 'o';  // extra kick
                if (l == LS && c == '.' && step == last && barsLeftInPhrase == 0 && rand01() < 0.4f * V) c = '-';  // pickup into the next phrase

                // the half-time breakdown: one big snare, the kick on one, the cymbals only on the beat
                if (halfBar)
                {
                    const bool snareBar = s.meter != band::M68 || (barInSection % 2 == 1);
                    if (l == LS || l == LSS || l == LCL) c = (step == halfSnare && snareBar && l == LS) ? 'X' : '.';
                    else if (l == LK) { if (c != '.' && step != 0 && step < halfSnare) c = '.'; if (step == 0) c = 'x'; }
                    else if ((l == LH || l == LO || l == LRD || l == LC || l == LTB || l == LTS || l == LSH) && step % (s.meter == band::M68 ? 6 : pulse) != 0) c = '.';
                }
            }
            if (c == '.') continue;
            float v = velOf (c) * velScale * (1.0f + gauss() * 0.04f);
            if (l == LH || l == LO || l == LRD) v *= (step % beat == 0) ? hatAccent : 1.0f / hatAccent;
            if (s.intensity < 0.25f && (l == LH || l == LRD) && (step % 4) == 2) v *= 0.7f;
            const int subs = c == 'r' ? 2 : c == 't' ? 3 : c == 'q' ? 4 : 1;
            if (subs == 1) hit (laneNote[lane], v, t + (int64_t) swingDelay, s, l);
            else
                for (int k = 0; k < subs; ++k)   // a roll: tight, getting a little louder
                    push (t + (int64_t) (swingDelay + stepLen * k / subs + 0.002 * sr), laneNote[lane], v * (0.75f + 0.25f * (float) k / (float) (subs - 1)));
        }
        if (step == last) pendingSectionStart = false;
    }

    int formBar = 0;              // 0 the song, 1 learning, 2 count-in, 3 final hit, 4 ringing out / over, 5 resting
    bool wasIntro = false, introBar = false, halfBar = false;
    bool pendingSectionStart = false, outBar = false, wasOut = false;
    bool nowInFill = false, useGenFill = false;
    int fillStart = 8, barsLeftInPhrase = 7;
    float energy = 1.0f, timeBiasMs = 0.0f, hatAccent = 1.0f;
    char genFill[NumLanes][20] {};

    // Pick this bar's fill: one of the style's own fills, or one made up on the spot.
    // Auto fills are sometimes just the last beat; requested ones always take half a bar.
    void chooseFill (const Settings& s, bool mayBeShort, int ns = 16)
    {
        const float V = s.variation;
        fillStart = (mayBeShort && rand01() < 0.35f * V) ? ns - ns / 4 : ns / 2;
        const float r = rand01();
        if (r < 1.0f - 0.6f * V) { useGenFill = false; fillAlt = rand01() < 0.5f ? 0 : 1; return; }

        useGenFill = true;
        for (auto& lane : genFill) for (auto& c : lane) c = '.';
        // snare -> rack tom(s) -> floor tom, sixteenths or eighths
        const int lanesSeq[] = { LS, LS, LT1, LT2, LT2, LT3, LF, LF };
        const bool sixteenths = rand01() < 0.45f + 0.4f * s.intensity;
        const int len = ns - fillStart;
        for (int i = 0; i < len; ++i)
        {
            const int step = fillStart + i;
            if (! sixteenths && (i % 2 == 1)) continue;
            if (sixteenths && i > 0 && i < len - 1 && rand01() < 0.15f * V) continue;   // leave a gap now and then
            const int lane = lanesSeq[juce::jlimit (0, 7, (int) ((float) i / (float) len * 8.0f + (rand01() - 0.5f) * 1.5f))];
            genFill[lane][step] = i >= len - 2 ? 'x' : (i < 2 ? 'o' : (rand01() < 0.5f ? 'x' : 'o'));
        }
        genFill[LK][fillStart] = 'x';
        if (rand01() < 0.5f) genFill[LK][ns - 1] = 'o';
    }

    float hash01 (int step, int lane) const
    {
        uint32_t h = (uint32_t) (step * 73856093) ^ (uint32_t) (lane * 19349663) ^ (uint32_t) (barInSection * 83492791);
        h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15;
        return (float) (h & 0xffff) / 65536.0f;
    }

    bool swellThisBar = false;

    void hit (int note, float vel, int64_t t, const Settings& s, int lane)
    {
        const float L = s.looseness;
        // a few ms behind the grid on average, drifting from bar to bar, rushing a touch in fills
        double lagMs = 3.0 + 8.0 * L + timeBiasMs;
        if (lane == LS) lagMs += 6.0 * L;
        if (lane == LC) lagMs += 4.0 * L;
        if (lane == LK) lagMs -= 1.5;
        if (nowInFill)  lagMs -= 2.5;
        lagMs += gauss() * (1.2 + 6.0 * L);
        lagMs = std::max (0.0, lagMs);
        vel += gauss() * (3.0f + 8.0f * L);
        const int64_t when = t + (int64_t) (lagMs * 0.001 * sr);
        // sloppy flam on hard backbeats when it's really loose
        if (lane == LS && vel > 100 && L > 0.6f && rand01() < L * 0.6f)
            push (when, note, vel * 0.45f), push (when + (int64_t) (0.018 * sr), note, vel);
        else
            push (when, note, vel);
    }
};
} // namespace tt
