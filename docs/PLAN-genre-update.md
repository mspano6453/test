# Genre update plan (one big update, requested 2026-11-08)

Mike's answers:
- First focus: hip-hop + electronic, 80s packs, glam packs, hip-hop packs, "weird early electronic instrument" plugin(s),
  psychedelic mode, Motown mode (call it "Detroit Soul" — Motown is a trademark), Philly sound mode ("Philly Soul").
- Keys player: real samples when easily possible.
- Electronic drums / synth bass may be synthesized; acoustic instruments use real samples.
- Delivery: one big update (send audio previews along the way).

## Samples (all CC0 unless noted) — check license before use
- VCSL (github sgossner/VCSL, CC0): Tambourine 1/2, Claps, Shaker small/large, Cowbells, Conga, Bongos, Agogo,
  Claves, Guiro, Vibraphone, Glockenspiel, Upright Piano (Yamaha), Grand Piano (Kawai/Steinway), Marimba, TX81Z FM Piano.
  Cloned blob-less at /tmp/claude-0/dl-vcsl/repo (git config gc.auto 0; never `ls-tree -l`).
- Karoryfer Black And Blue Basses (CC0): darkblack = fingered bass (for funk/soul/Detroit/Philly). /tmp/claude-0/dl-bbb/repo

## Work items
1. Harmony: chord qualities (maj, min, 7, maj7, m7, dim, sus4, m7b5) on the bus + 7th-chord progressions.
2. Tea Towel: percussion lanes (tambourine, claps, shaker, cowbell, conga hi/lo) from VCSL; synthesized machine kits
   ("Boom Machine" 808-like, "Club Machine" 909-like, "Studio Machine '84" Linn-like), "Boom Bap" crunch kit,
   80s gated reverb; ratchet glyphs for trap hat rolls; new drum styles per genre.
3. Undertow: bass type Picked / Fingered (darkblack samples) / Synth (sub w/ glide, acid, reese, 80s pulse);
   new feels: Motor City (busy melodic fingered), Philly Disco, Trap Sub, Boom Bap, House offbeat, Techno roll, Acid, New Jack.
4. New plugin: keys player (real piano + vibes + glock samples from VCSL; synthesized electric piano, organ w/ rotary,
   clav, string machine, synth brass, FM piano samples). Comping feels: stabs, soul 8ths, Detroit pounding quarters,
   Philly lush strings, house offbeat, 80s pad, arpeggio, lo-fi 7ths/9ths. Follows Bandleader; MIDI playable.
5. New plugin: early electronic instruments ("Ether"): theremin, ondes, trautonium, tape choir, stylus organ,
   radiophonic oscillators, tube clarinet; MIDI play + a melody player over Bandleader chords.
6. Bandleader: style packs with headings (Rock, 80s, Glam, Hip-hop, Electronic, Soul, Psychedelic, Early Electronic),
   ~45 new styles; style table drives new plugins too.
7. Tests (StyleTest over every style), pluginval for all plugins, installer (10 plugins), manual + itch page updates.

No real artist/brand names anywhere (no Motown, Roland/808/909/LinnDrum, Mellotron, Stylophone, Clavioline, Moog).
