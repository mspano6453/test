#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace tt
{
// ---------------------------------------------------------------------------
// The kit: a late-60s four-piece-plus-cymbals, the classic late-60s setup.
// ---------------------------------------------------------------------------
enum Piece { Kick = 0, Snare, RackTom, FloorTom, HiHat, Ride, Crash, NumPieces };

inline const char* pieceName (int p)
{
    static const char* n[] = { "Kick", "Snare", "Rack Tom", "Floor Tom", "Hi-Hat", "Ride", "Crash" };
    return n[p];
}
inline const char* pieceId (int p)
{
    static const char* n[] = { "kick", "snare", "rtom", "ftom", "hat", "ride", "crash" };
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
        "MIDI 49, 52, 55, 57"
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
enum MasterParam { Thud = 0, Squash, Tape, RoomLevel, RoomSize, Width, Output, NumMasterParams };

inline const char* masterId (int k)
{
    static const char* n[] = { "m_thud", "m_squash", "m_tape", "m_roomlvl", "m_roomsize", "m_width", "m_out" };
    return n[k];
}
inline const char* masterName (int k)
{
    static const char* n[] = { "Thud", "Squash", "Tape", "Room", "Room Size", "Width", "Output" };
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
};

inline const std::array<Preset, 4>& presets()
{
    static const std::array<Preset, 4> p { {
        // tune, towel, decay, tone, vol, pan, room
        { "Studio '69 - Towels",
          { {  0.0f, 0.70f, 0.40f, 0.50f,  0.0f,  0.00f, 0.08f },
            { -1.5f, 0.65f, 0.40f, 0.50f, -1.0f, -0.08f, 0.15f },
            { -1.0f, 0.75f, 0.40f, 0.45f, -2.0f, -0.15f, 0.12f },
            { -1.5f, 0.75f, 0.45f, 0.45f, -2.0f,  0.35f, 0.12f },
            {  0.0f, 0.25f, 0.55f, 0.55f, -4.0f, -0.40f, 0.10f },
            {  0.0f, 0.30f, 0.45f, 0.50f,-7.0f,  0.45f, 0.12f },
            {  0.0f, 0.30f, 0.45f, 0.55f,-9.0f, -0.45f, 0.12f } },
          { 0.45f, 0.55f, 0.40f, 0.18f, 0.20f, 0.70f, 0.0f } },

        { "Rooftop '69",
          { {  0.0f, 0.45f, 0.50f, 0.60f,  0.0f,  0.00f, 0.20f },
            { -1.0f, 0.40f, 0.50f, 0.60f, -1.0f, -0.08f, 0.28f },
            {  0.0f, 0.45f, 0.55f, 0.55f, -2.0f, -0.15f, 0.22f },
            { -1.0f, 0.45f, 0.60f, 0.55f, -2.0f,  0.35f, 0.22f },
            {  0.0f, 0.10f, 0.65f, 0.65f, -4.0f, -0.40f, 0.18f },
            {  0.0f, 0.15f, 0.60f, 0.60f,-7.0f,  0.45f, 0.22f },
            {  0.0f, 0.15f, 0.60f, 0.65f,-9.0f, -0.45f, 0.22f } },
          { 0.30f, 0.40f, 0.30f, 0.32f, 0.45f, 0.90f, 0.0f } },

        { "Cellar Club '63 - Open",
          { {  0.5f, 0.10f, 0.70f, 0.80f,  0.0f,  0.00f, 0.25f },
            {  0.5f, 0.10f, 0.70f, 0.80f, -1.0f, -0.08f, 0.30f },
            {  0.5f, 0.10f, 0.75f, 0.75f, -2.0f, -0.15f, 0.25f },
            {  0.0f, 0.10f, 0.80f, 0.75f, -2.0f,  0.35f, 0.25f },
            {  0.0f, 0.00f, 0.80f, 0.80f, -3.0f, -0.40f, 0.25f },
            {  0.0f, 0.00f, 0.85f, 0.75f,-6.0f,  0.45f, 0.25f },
            {  0.0f, 0.00f, 0.85f, 0.80f,-8.0f, -0.45f, 0.25f } },
          { 0.15f, 0.30f, 0.20f, 0.35f, 0.35f, 1.00f, 0.0f } },

        { "Full Towel",
          { { -1.0f, 1.00f, 0.30f, 0.35f,  0.0f,  0.00f, 0.04f },
            { -2.5f, 1.00f, 0.30f, 0.35f, -1.0f, -0.08f, 0.06f },
            { -2.0f, 1.00f, 0.30f, 0.30f, -2.0f, -0.15f, 0.05f },
            { -2.5f, 1.00f, 0.35f, 0.30f, -2.0f,  0.35f, 0.05f },
            {  0.0f, 0.45f, 0.45f, 0.40f, -5.0f, -0.40f, 0.04f },
            {  0.0f, 0.55f, 0.35f, 0.40f,-8.0f,  0.45f, 0.04f },
            {  0.0f, 0.55f, 0.35f, 0.45f,-10.0f, -0.45f, 0.04f } },
          { 0.65f, 0.80f, 0.55f, 0.10f, 0.15f, 0.60f, 0.0f } },
    } };
    return p;
}
} // namespace tt
