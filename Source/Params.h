#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace tt
{
// ---------------------------------------------------------------------------
// The kit: a late-60s four-piece-plus-cymbals, the classic late-60s setup.
// ---------------------------------------------------------------------------
enum Piece { Kick = 0, Snare, RackTom, FloorTom, HiHat, Ride, Crash, Perc, NumPieces };

inline const char* pieceName (int p)
{
    static const char* n[] = { "Kick", "Snare", "Rack Tom", "Floor Tom", "Hi-Hat", "Ride", "Crash", "Percussion" };
    return n[p];
}
inline const char* pieceId (int p)
{
    static const char* n[] = { "kick", "snare", "rtom", "ftom", "hat", "ride", "crash", "perc" };
    return n[p];
}
inline const char* pieceNotes (int p)
{
    static const char* n[] = {
        "MIDI 35, 36",
        "MIDI 38 snare  |  40 rimshot  |  37 side-stick",
        "MIDI 50 high  |  48  |  47  |  45 low",
        "MIDI 43  |  41 low",
        "MIDI 42 closed  |  44 pedal  |  46 open",
        "MIDI 51, 59 ride  |  53 bell",
        "MIDI 49, 52, 55, 57",
        "MIDI 39 claps | 54 tambourine | 69 tamb shake | 70 shaker | 56 cowbell | 63 conga hi | 64 conga lo | 75 claves"
    };
    return n[p];
}

// Per-piece controls (the EDIT page)
enum PieceParam { Tune = 0, Damp, Decay, Tone, Vol, Pan, Room, NumPieceParams };

inline const char* pieceParamId (int k)
{
    static const char* n[] = { "tune", "damp", "decay", "tone", "vol", "pan", "room" };
    return n[k];
}
inline const char* pieceParamName (int k)
{
    static const char* n[] = { "Tune", "Towel", "Decay", "Tone", "Volume", "Pan", "Room" };
    return n[k];
}
inline juce::String pid (int piece, int k) { return juce::String (pieceId (piece)) + "_" + pieceParamId (k); }

// Master bus controls (the FX page)
enum MasterParam { Thud = 0, Squash, Tape, RoomLevel, RoomSize, Width, Output, Crunch, Vinyl, GateVerb, Smash, NumMasterParams };

inline const char* masterId (int k)
{
    static const char* n[] = { "m_thud", "m_squash", "m_tape", "m_roomlvl", "m_roomsize", "m_width", "m_out", "m_crunch", "m_vinyl", "m_gate", "m_smash" };
    return n[k];
}
inline const char* masterName (int k)
{
    static const char* n[] = { "Thud", "Squash", "Tape", "Room", "Room Size", "Width", "Output", "Crunch", "Vinyl", "Gate Verb", "Smash" };
    return n[k];
}

// Player (groove generator) parameter IDs
namespace PID
{
    inline constexpr const char* on = "p_on";
    inline constexpr const char* style = "p_style";
    inline constexpr const char* section = "p_section";
    inline constexpr const char* fillEvery = "p_fill";
    inline constexpr const char* intensity = "p_int";
    inline constexpr const char* looseness = "p_loose";
    inline constexpr const char* swing = "p_swing";
    inline constexpr const char* variation = "p_var";
}

// ---------------------------------------------------------------------------
// Factory kits. Values per piece: tune(st), towel, decay, tone, vol(dB), pan, room
// Master: thud, squash, tape, room level, room size, width, output(dB)
// ---------------------------------------------------------------------------
struct Preset
{
    const char* name;
    float piece[NumPieces][NumPieceParams];
    float master[NumMasterParams];
    int kitType = 0;          // 0 acoustic recordings, 1..3 the synthesized drum machines
};

// The kit's sound source
enum KitType { KitAcoustic = 0, KitBoomMachine, KitClubMachine, KitStudioMachine, NumKitTypes };
inline const char* kitTypeName (int k) { static const char* n[] = { "Acoustic (recorded)", "Boom Machine", "Club Machine", "Studio Machine '84" }; return n[k]; }

inline const std::array<Preset, 10>& presets()
{
    static const std::array<Preset, 10> p { {
        // tune, towel, decay, tone, vol, pan, room
        { "Studio '69 - Towels",
          { {  0.0f, 0.70f, 0.40f, 0.50f,  0.0f,  0.00f, 0.08f },
            { -1.5f, 0.65f, 0.40f, 0.50f, -1.0f, -0.08f, 0.15f },
            { -1.0f, 0.75f, 0.40f, 0.45f, -2.0f, -0.15f, 0.12f },
            { -1.5f, 0.75f, 0.45f, 0.45f, -2.0f,  0.35f, 0.12f },
            {  0.0f, 0.25f, 0.55f, 0.55f, -4.0f, -0.40f, 0.10f },
            {  0.0f, 0.30f, 0.45f, 0.50f,-7.0f,  0.45f, 0.12f },
            {  0.0f, 0.30f, 0.45f, 0.55f,-9.0f, -0.45f, 0.12f },
            {  0.0f, 0.20f, 0.55f, 0.60f, -5.0f,  0.30f, 0.10f } },
          { 0.45f, 0.55f, 0.40f, 0.18f, 0.20f, 0.70f, 0.0f } },

        { "Rooftop '69",
          { {  0.0f, 0.45f, 0.50f, 0.60f,  0.0f,  0.00f, 0.20f },
            { -1.0f, 0.40f, 0.50f, 0.60f, -1.0f, -0.08f, 0.28f },
            {  0.0f, 0.45f, 0.55f, 0.55f, -2.0f, -0.15f, 0.22f },
            { -1.0f, 0.45f, 0.60f, 0.55f, -2.0f,  0.35f, 0.22f },
            {  0.0f, 0.10f, 0.65f, 0.65f, -4.0f, -0.40f, 0.18f },
            {  0.0f, 0.15f, 0.60f, 0.60f,-7.0f,  0.45f, 0.22f },
            {  0.0f, 0.15f, 0.60f, 0.65f,-9.0f, -0.45f, 0.22f },
            {  0.0f, 0.10f, 0.60f, 0.65f, -5.0f,  0.30f, 0.20f } },
          { 0.30f, 0.40f, 0.30f, 0.32f, 0.45f, 0.90f, 0.0f } },

        { "Cellar Club '63 - Open",
          { {  0.5f, 0.10f, 0.70f, 0.80f,  0.0f,  0.00f, 0.25f },
            {  0.5f, 0.10f, 0.70f, 0.80f, -1.0f, -0.08f, 0.30f },
            {  0.5f, 0.10f, 0.75f, 0.75f, -2.0f, -0.15f, 0.25f },
            {  0.0f, 0.10f, 0.80f, 0.75f, -2.0f,  0.35f, 0.25f },
            {  0.0f, 0.00f, 0.80f, 0.80f, -3.0f, -0.40f, 0.25f },
            {  0.0f, 0.00f, 0.85f, 0.75f,-6.0f,  0.45f, 0.25f },
            {  0.0f, 0.00f, 0.85f, 0.80f,-8.0f, -0.45f, 0.25f },
            {  0.0f, 0.00f, 0.70f, 0.70f, -4.0f,  0.30f, 0.25f } },
          { 0.15f, 0.30f, 0.20f, 0.35f, 0.35f, 1.00f, 0.0f } },

        { "Full Towel",
          { { -1.0f, 1.00f, 0.30f, 0.35f,  0.0f,  0.00f, 0.04f },
            { -2.5f, 1.00f, 0.30f, 0.35f, -1.0f, -0.08f, 0.06f },
            { -2.0f, 1.00f, 0.30f, 0.30f, -2.0f, -0.15f, 0.05f },
            { -2.5f, 1.00f, 0.35f, 0.30f, -2.0f,  0.35f, 0.05f },
            {  0.0f, 0.45f, 0.45f, 0.40f, -5.0f, -0.40f, 0.04f },
            {  0.0f, 0.55f, 0.35f, 0.40f,-8.0f,  0.45f, 0.04f },
            {  0.0f, 0.55f, 0.35f, 0.45f,-10.0f, -0.45f, 0.04f },
            {  0.0f, 0.40f, 0.45f, 0.45f, -6.0f,  0.30f, 0.05f } },
          { 0.65f, 0.80f, 0.55f, 0.10f, 0.15f, 0.60f, 0.0f } },
        // ---- character kits
        { "Boom Bap Crunch",
          { {  0.0f, 0.45f, 0.55f, 0.45f,  0.0f,  0.00f, 0.06f },
            { -1.0f, 0.40f, 0.50f, 0.50f, -1.0f,  0.00f, 0.10f },
            { -1.0f, 0.50f, 0.45f, 0.45f, -3.0f, -0.15f, 0.08f },
            { -1.0f, 0.50f, 0.50f, 0.45f, -3.0f,  0.35f, 0.08f },
            {  0.0f, 0.20f, 0.50f, 0.50f, -5.0f, -0.30f, 0.06f },
            {  0.0f, 0.30f, 0.45f, 0.45f, -8.0f,  0.40f, 0.06f },
            {  0.0f, 0.30f, 0.45f, 0.50f, -9.0f, -0.40f, 0.06f },
            {  0.0f, 0.20f, 0.55f, 0.55f, -5.0f,  0.30f, 0.06f } },
          { 0.55f, 0.65f, 0.45f, 0.10f, 0.20f, 0.60f, 0.0f, 0.55f, 0.40f, 0.0f } },

        { "Gated '85",
          { {  0.0f, 0.15f, 0.60f, 0.70f,  0.0f,  0.00f, 0.15f },
            {  0.0f, 0.10f, 0.65f, 0.75f, -1.0f,  0.00f, 0.35f },
            {  0.0f, 0.10f, 0.70f, 0.70f, -2.0f, -0.25f, 0.30f },
            { -1.0f, 0.10f, 0.75f, 0.70f, -2.0f,  0.35f, 0.30f },
            {  0.0f, 0.00f, 0.60f, 0.75f, -5.0f, -0.35f, 0.15f },
            {  0.0f, 0.00f, 0.70f, 0.70f, -8.0f,  0.40f, 0.20f },
            {  0.0f, 0.00f, 0.80f, 0.80f, -8.0f, -0.40f, 0.20f },
            {  0.0f, 0.00f, 0.65f, 0.70f, -4.0f,  0.25f, 0.30f } },
          { 0.25f, 0.55f, 0.15f, 0.20f, 0.40f, 1.00f, 0.0f, 0.0f, 0.0f, 0.70f } },

        { "Boom Machine",
          { {  0.0f, 0.00f, 0.85f, 0.50f,  0.0f,  0.00f, 0.00f },
            {  0.0f, 0.00f, 0.55f, 0.65f, -3.0f,  0.00f, 0.08f },
            {  0.0f, 0.00f, 0.70f, 0.55f, -4.0f, -0.20f, 0.05f },
            {  0.0f, 0.00f, 0.75f, 0.50f, -4.0f,  0.30f, 0.05f },
            {  0.0f, 0.00f, 0.55f, 0.70f, -6.0f, -0.15f, 0.00f },
            {  0.0f, 0.00f, 0.60f, 0.65f, -9.0f,  0.30f, 0.05f },
            {  0.0f, 0.00f, 0.70f, 0.70f,-10.0f, -0.30f, 0.05f },
            {  0.0f, 0.00f, 0.60f, 0.65f, -4.0f,  0.15f, 0.08f } },
          { 0.20f, 0.35f, 0.25f, 0.10f, 0.25f, 0.80f, 0.0f, 0.0f, 0.0f, 0.0f }, 1 },

        { "Club Machine",
          { {  0.0f, 0.00f, 0.55f, 0.60f,  0.0f,  0.00f, 0.00f },
            {  0.0f, 0.00f, 0.50f, 0.70f, -3.0f,  0.00f, 0.10f },
            {  0.0f, 0.00f, 0.60f, 0.60f, -4.0f, -0.20f, 0.05f },
            {  0.0f, 0.00f, 0.65f, 0.55f, -4.0f,  0.30f, 0.05f },
            {  0.0f, 0.00f, 0.55f, 0.75f, -6.0f, -0.10f, 0.00f },
            {  0.0f, 0.00f, 0.65f, 0.70f, -8.0f,  0.25f, 0.05f },
            {  0.0f, 0.00f, 0.75f, 0.75f, -9.0f, -0.25f, 0.05f },
            {  0.0f, 0.00f, 0.55f, 0.70f, -4.0f,  0.10f, 0.10f } },
          { 0.15f, 0.30f, 0.20f, 0.10f, 0.25f, 0.90f, 0.0f, 0.0f, 0.0f, 0.0f }, 2 },

        { "Studio Machine '84",
          { {  0.0f, 0.00f, 0.55f, 0.60f,  0.0f,  0.00f, 0.10f },
            {  0.0f, 0.00f, 0.55f, 0.70f, -2.0f,  0.00f, 0.25f },
            {  0.0f, 0.00f, 0.65f, 0.65f, -3.0f, -0.25f, 0.20f },
            {  0.0f, 0.00f, 0.70f, 0.60f, -3.0f,  0.35f, 0.20f },
            {  0.0f, 0.00f, 0.55f, 0.75f, -6.0f, -0.25f, 0.05f },
            {  0.0f, 0.00f, 0.65f, 0.70f, -8.0f,  0.30f, 0.10f },
            {  0.0f, 0.00f, 0.75f, 0.75f, -9.0f, -0.30f, 0.10f },
            {  0.0f, 0.00f, 0.55f, 0.70f, -4.0f,  0.20f, 0.20f } },
          { 0.20f, 0.45f, 0.20f, 0.20f, 0.40f, 1.00f, 0.0f, 0.0f, 0.0f, 0.45f }, 3 },

        // the 90s psych-pop drum sound: a big open kit in a big room, crushed and blown out
        { "Blown Out '99",
          { { -1.0f, 0.20f, 0.70f, 0.55f,  0.0f,  0.00f, 0.35f },
            { -2.0f, 0.15f, 0.75f, 0.60f, -1.0f,  0.00f, 0.45f },
            { -2.0f, 0.20f, 0.75f, 0.50f, -2.0f, -0.30f, 0.40f },
            { -2.5f, 0.20f, 0.80f, 0.50f, -2.0f,  0.40f, 0.40f },
            {  0.0f, 0.10f, 0.60f, 0.55f, -6.0f, -0.35f, 0.25f },
            {  0.0f, 0.10f, 0.70f, 0.55f, -9.0f,  0.40f, 0.30f },
            {  0.0f, 0.10f, 0.80f, 0.60f,-10.0f, -0.40f, 0.30f },
            {  0.0f, 0.10f, 0.60f, 0.60f, -5.0f,  0.30f, 0.30f } },
          { 0.70f, 0.85f, 0.55f, 0.50f, 0.55f, 1.10f, 0.0f, 0.10f, 0.0f, 0.0f, 0.80f } },
    } };
    return p;
}
} // namespace tt
