# Tea Towel — dead drum instrument

A VST3 / AU drum instrument for that towel-on-the-heads, late-60s studio sound: thuddy toms, a fat low snare with short wires, a kick with no boom, and dark cymbals — all squashed through a valve-style compressor and tape.

The workflow is laid out in pages: a Kit page with mixer, a per-drum Edit page, and an FX page for the drum bus.

## The sounds

Every drum is a real recording — nothing is synthesised. The kit is built from the free **AVL Drumkits** by Glen MacArthur (CC BY-SA 3.0), with 5 velocity layers per hit:

| Drum | Recording |
|---|---|
| Kick | 26" kick |
| Snare | Snare (plus rimshot and side-stick) |
| Rack tom | 14" tom |
| Floor toms | 16" (note 43) and 18" (note 41) |
| Hi-hat | Closed, pedal, open |
| Ride | 24" ride (plus bell) |
| Crash | 20" crash |

The samples are embedded in the plugin, so there's nothing extra to install. Credits and license are in `Samples/CREDITS.txt`; keep them with the samples if you ever share the plugin.

## Pages

**KIT** — Click a drum to play it (right-click for side-stick, open hat or ride bell; shift-click plays softer). Drop a WAV/AIFF onto any drum to replace it with your sample. The mixer under the kit has pan, room send and volume for each drum. Drums light up as MIDI hits them.

**EDIT** — One drum at a time:

| Knob | What it does |
|---|---|
| Tune | ±12 semitones |
| Towel | How much cloth is on the head. More = the ring is choked off sooner and the head sounds duller |
| Decay | How much of the recording's natural ring is let through |
| Tone | Brightness. Softer hits are automatically a little darker |
| Volume / Pan / Room | Same as the mixer |

The **Load sample…** button (or drag-and-drop) swaps in your own one-shot; Tune, Towel, Decay and Tone still shape it. **Use built-in** goes back to the kit's recording. Sample paths are saved with your Live set.

**FX** — the drum bus:

- **Thud** — low-end bump around 95 Hz
- **Squash** — a slow, leveling compressor in the spirit of the valve limiters used on 60s drum sub-mixes (meter shows gain reduction)
- **Tape** — saturation plus top-end roll-off
- **Room / Room Size** — a small, dark, damped room fed by each drum's Room knob
- **Width / Output**

## Kits (presets menu, top right)

- **Studio '69 – Towels** — the default; damped and squashed
- **Rooftop '69** — a bit more open and roomy
- **Cellar Club '63 – Open** — no towels, brighter, livelier
- **Full Towel** — maximum deadness

## MIDI map (works with Ableton's standard Drum Rack layout)

| Drum | Notes |
|---|---|
| Kick | 35, 36 |
| Snare | 38 snare · 40 rimshot · 37 side-stick |
| Rack tom | 50 · 48 · 47 · 45 (high → low) |
| Floor tom | 43 · 41 |
| Hi-hat | 42 closed · 44 pedal · 46 open (closed/pedal choke the open hat) |
| Ride | 51, 59 · 53 bell |
| Crash | 49, 52, 55, 57 |

The `Grooves` folder has five MIDI clips (straight eighths, shuffle, floor-tom groove, ride with a tom fill, half-time). Drag them onto the Tea Towel track in Live.

## Undertow — post-punk bass player

A virtual bass player in the late-70s/80s post-punk style: picked bass, pumping eighths, high melodic hooks,
octave pops, descending gothic lines. Real samples (Karoryfer "Black And Blue Basses", CC0, the picked bass).

* **Bass Player** mode: put Undertow on a MIDI track, set its Sidechain to your guitar track and press play.
  It hears your chords and plays along to Live's tempo (or follows chords from a MIDI clip).
  VERSE / CHORUS / BRIDGE, FILL, NEW LINE, plus Variation, Fills, Hook, Busy, Push, Human, Swing, Length.
  Keyswitches (Live note names): C-1 verse, C#-1 chorus, D-1 bridge, D#-1 fill. The bass line also comes out as MIDI.
* **Play It**: play the bass yourself from a keyboard.
* **Guitar to Bass: Octave** — your guitar an octave down through the bass rig.
* **Guitar to Bass: Tracked** — play single notes on guitar and the real bass plays them an octave down (~35 ms behind).
* **The rig**: compressor, grit, chorus, flanger, amp EQ with a bright cab.

## Bandleader — runs the band

Put Bandleader on its own MIDI track (it makes no sound) and press play. It holds the song — key, chords per
section, the song form (verse/chorus/verse/chorus/bridge/chorus), section changes, fills, drop-outs and energy —
and shares it with the other plugins on this computer. Everything follows it automatically:
Tea Towel (sections, fills, drop-outs), Undertow (chords; locks to Tea Towel's kick), Loon (key and chords,
swells in choruses, dips under the kick), Schaltwerk (Strumchord plays the chords, drum box drops out),
Carousel (REC waits for the next section and records exactly one section; slots can follow sections),
Tape Twin (echo throw on the last beat before each new section) and Lathe (loudness per section). Footswitch: C2 next section, D2 fill, E2 drop out,
F2 all in, G2 verse, A2 chorus, B2 bridge (or CC 90-96). Each plugin still works on its own without it.

**Styles:** pick a genre in Bandleader's Style menu (29 rock styles, from 60s Beat to Shoegaze to Stoner/Doom) and every
plugin in the set moves its knobs to suit it. APPLY AGAIN resets them. Reopening a set keeps your tweaks.

## Getting the plugin built

The plugin has to be compiled for your machine. The easiest way needs no tools installed:

1. Create a new GitHub repository and upload everything in this folder (including the hidden `.github` folder).
2. Open the **Actions** tab. The "Build Tea Towel" workflow runs automatically (or press *Run workflow*). It takes about 10 minutes.
3. When it finishes, download **TeaTowel-macOS** or **TeaTowel-Windows** from the run's *Artifacts* section and unzip it (on Mac, unzip the inner zip too).

### Build it yourself instead

You need CMake 3.22+ and Xcode (Mac) or Visual Studio 2022 with C++ (Windows).

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The plugin appears in `build/TeaTowel_artefacts/Release/VST3/` (and `AU/` on Mac).

## Installing in Ableton Live 12

**Mac**
1. Copy `Tea Towel.vst3` to `/Library/Audio/Plug-Ins/VST3/` (and optionally `Tea Towel.component` to `/Library/Audio/Plug-Ins/Components/`).
2. Because the build isn't notarised, clear the download quarantine once in Terminal:
   `sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Tea Towel.vst3"`
   (same for the `.component` if you copied it).

**Windows**
1. Copy the `Tea Towel.vst3` folder to `C:\Program Files\Common Files\VST3\`.

**Then in Live:** Settings → Plug-Ins → turn on *Use VST3 Plug-In System Folders* → **Rescan**. Tea Towel shows up under Plug-Ins → Tea Towel Audio. Drop it on a MIDI track and play.

## Offline test tool

`cmake -B build -DTEATOWEL_RENDER_DEMO=ON` also builds `RenderDemo`, which renders every drum plus an 8-bar groove to a WAV and checks the output for clipping and NaNs:
`RenderDemo demo.wav [kit number 0-3]`

## License note

Built with JUCE 8, which is free for personal projects under its AGPLv3 / starter terms. If you ever sell it, check JUCE's licensing.
