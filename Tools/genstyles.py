# Generates Source/BandStyles.h from a readable table.
# keys: 0=C 2=D 4=E 5=F 7=G 9=A
D = dict(dFill=2, dInt=0.55, dLoose=0.3, dSwing=0.0, dVar=0.5, dRoom=-1, dRoomSize=-1,
         bReg=0, bHook=0.4, bBusy=0.55, bPush=0.2, bSwing=0.0, bLen=0.9, bComp=0.4, bGrit=0.2, bChorus=0.0, bFlanger=0.0, bBass=2, bMid=1.5, bTreble=2.5,
         lGen=0.0, lMood=0, lPads=0.6, lSpace=0.25, lSize=0.5, lShimmer=0.0, lCloud=0.0, lWobble=0.1, lGtr=0.25,
         tSync=3, tMix=0.0, tFb=0.2, tWow=0.15, tFlut=0.2, tAge=0.3, tAdt=0.0,
         sRole=0, sPattern=-1, lathe=2, chordBars=0, sectionBars=1, energy=0.55,
         pack='Rock', uBass=0, kOn=0, kInst=0, kFeel=1, kReg=1, kBusy=0.5, kColour=0.3, kMotion=0.3, kChorus=0.1, kLofi=0.0, kRoom=0.2,
         eRole=0, eInst=0, eSpace=0.4)
S = []
def st(name, blurb, bpm, key, prog, dStyle, dKit, bFeel, **kw):
    r = dict(D); r.update(kw); r.update(name=name, blurb=blurb, bpm=bpm, key=key, prog=prog, dStyle=dStyle, dKit=dKit, bFeel=bFeel); S.append(r)

st("60s Beat", "Bright, tight and tuneful. Jangly chords, ringing open strings, short tape slap.", 132, 4, (14,10,11), 0, 0, 4,
   bHook=0.55, bBusy=0.6, bGrit=0.05, tMix=0.3, tFb=0.1, tSync=0, lathe=4, lSpace=0.15, sRole=0)
st("Garage Rock", "Loud, trashy, fun. Fuzz guitar, barre chords, a little out of control.", 140, 9, (13,12,17), 17, 2, 0,
   dLoose=0.5, dInt=0.75, bGrit=0.55, bBusy=0.7, tMix=0.25, tFb=0.15, tSync=0, lathe=2)
st("Surf Rock", "Reverb-drenched lead lines, tremolo picking, drums all over the toms.", 160, 4, (16,0,6), 13, 2, 7,
   bGrit=0.1, bTreble=4, lSpace=0.55, lSize=0.35, lGtr=0.6, tSync=3, tMix=0.25, tFb=0.3, lathe=1)
st("Psychedelic Rock", "Droning chords, wah and fuzz, everything doubled and swirling.", 108, 4, (6,12,7), 0, 1, 1,
   dLoose=0.45, bHook=0.7, bChorus=0.3, lGen=0.25, lMood=1, lShimmer=0.3, lCloud=0.4, lWobble=0.4, lSpace=0.45,
   tSync=2, tMix=0.35, tFb=0.5, tWow=0.4, tAdt=0.6, lathe=1)
st("Krautrock", "The endless motorik pulse. One chord, hypnotic, building slowly.", 132, 2, (7,6,5), 1, 0, 5,
   bLen=0.8, bGrit=0.15, lGen=0.2, lMood=0, lSpace=0.35, tSync=2, tMix=0.3, tFb=0.45, lathe=1, sectionBars=2)
st("Classic Rock", "Big riffs, power chords, the stadium in the 70s.", 120, 9, (12,8,9), 4, 1, 0,
   dInt=0.7, bGrit=0.3, tSync=0, tMix=0.15, tFb=0.15, lathe=2)
st("Glam Rock", "Stomp-clap swagger, chunky chords, big chorus hooks.", 112, 7, (13,11,12), 12, 1, 5,
   dInt=0.75, dRoom=0.4, bGrit=0.4, tSync=0, tMix=0.2, tAdt=0.4, lathe=2)
st("Blues Rock", "Twelve-bar shuffle. Bend the strings, leave space, let it swing.", 96, 4, (18,18,11), 14, 2, 4,
   dSwing=0.75, bSwing=0.5, bHook=0.3, bGrit=0.2, tSync=0, tMix=0.2, lathe=1, sectionBars=3)
st("Rockabilly", "Fast twelve-bar train beat, slapback echo on everything.", 168, 4, (18,18,14), 14, 2, 4,
   dSwing=0.55, bSwing=0.35, bBusy=0.75, bGrit=0.0, tSync=0, tMix=0.4, tFb=0.1, lathe=4, sectionBars=3)
st("Folk Rock", "Strummed acoustic and jangly electric, harmonies, an autoharp shimmer.", 112, 7, (14,10,11), 15, 0, 1,
   bHook=0.45, bGrit=0.0, tAdt=0.5, tMix=0.15, tSync=3, lSpace=0.3, sRole=2, lathe=3)
st("Punk", "Fast, loud, three chords. Downstrokes on every eighth.", 176, 9, (13,8,11), 6, 2, 5,
   dInt=0.85, dLoose=0.4, bBusy=0.9, bLen=0.75, bGrit=0.6, bTreble=4, tMix=0.0, lathe=2, energy=0.75)
st("Post-Punk", "Chorused bass up front, angular clean guitar, floor-tom pulse.", 138, 9, (5,0,7), 7, 0, 0,
   bHook=0.65, bChorus=0.55, bGrit=0.15, lGen=0.15, lMood=3, lSpace=0.35, tSync=3, tMix=0.25, tFb=0.35, lathe=1, dRoom=0.25)
st("Gothic Rock", "Dark and slow-burning. Flanged bass, echoing guitar, big toms.", 112, 4, (4,0,16), 7, 1, 2,
   bChorus=0.6, bFlanger=0.4, bHook=0.5, lGen=0.35, lMood=3, lPads=0.6, lSpace=0.5, lShimmer=0.1, tSync=1, tMix=0.3, tFb=0.45, lathe=1)
st("New Wave", "Four on the floor, choppy guitar, synth pads, nervous energy.", 132, 2, (8,9,11), 11, 1, 3,
   bGrit=0.1, tAdt=0.4, tMix=0.15, sRole=1, lathe=0)
st("Synth Rock", "Guitars over a drum machine and synths. Cold, catchy, mechanical.", 120, 9, (0,2,5), 11, 0, 3,
   bGrit=0.2, sRole=3, sPattern=5, tSync=3, tMix=0.2, lathe=0)
st("Power Pop", "Crunchy and sweet: big chords, bright hooks, handclap energy.", 140, 4, (8,10,9), 4, 1, 0,
   bHook=0.6, bGrit=0.25, tAdt=0.6, tMix=0.1, lathe=2)
st("Shoegaze", "A wall of fuzz and reverb. Strum hard and lean on the tremolo arm; the band melts around you.", 110, 4, (19,20,7), 8, 1, 5,
   dLoose=0.5, dRoom=0.45, dRoomSize=0.8, dInt=0.65, bLen=1.25, bGrit=0.45, bChorus=0.3, bTreble=0,
   lGen=0.55, lMood=1, lPads=0.8, lSpace=0.75, lSize=0.95, lShimmer=0.5, lCloud=0.75, lWobble=0.45, lGtr=0.8,
   tSync=2, tMix=0.35, tFb=0.45, tWow=0.55, tFlut=0.3, tAdt=0.5, chordBars=1, lathe=1)
st("Dream Pop", "Soft, hazy and floating. Chorused clean guitar, breathy and slow.", 92, 2, (19,8,11), 5, 0, 1,
   bHook=0.6, bChorus=0.7, bGrit=0.0, lGen=0.45, lMood=1, lSpace=0.6, lSize=0.85, lShimmer=0.45, lCloud=0.3, lWobble=0.3, lGtr=0.6,
   tSync=2, tMix=0.25, tFb=0.35, tAdt=0.4, sRole=2, chordBars=1, lathe=3)
st("Grunge", "Quiet verse, loud chorus. Drop-tuned sludge and a chorused bass.", 104, 4, (21,15,16), 9, 2, 5,
   dLoose=0.45, dInt=0.7, dRoom=0.3, bGrit=0.7, bChorus=0.25, bBusy=0.6, tMix=0.0, lathe=2, energy=0.65)
st("Alt Rock", "90s college radio: loud-quiet dynamics, crunchy chords, singalong chorus.", 120, 9, (9,8,4), 4, 2, 0,
   bGrit=0.35, tSync=3, tMix=0.12, lathe=2)
st("Britpop", "Swagger and big melodies. Jangle in the verse, wall of guitars in the chorus.", 126, 7, (10,12,3), 0, 1, 1,
   bHook=0.55, bGrit=0.2, tAdt=0.5, tMix=0.12, lathe=2)
st("Indie Rock", "Wiry guitars, driving drums, a bit of echo and a lot of heart.", 128, 2, (8,9,5), 4, 0, 0,
   bHook=0.6, bGrit=0.2, tSync=3, tMix=0.2, tFb=0.3, lSpace=0.3, lathe=0)
st("Noise Rock", "Feedback, dissonance and repetition. Hit it hard and let it howl.", 150, 4, (7,17,21), 9, 2, 5,
   dInt=0.85, dLoose=0.55, bGrit=0.9, bFlanger=0.3, bBusy=0.8, tSync=3, tMix=0.4, tFb=0.85, tAge=0.6, lathe=2, energy=0.8)
st("Stoner / Doom", "Slow, heavy and huge. Detuned riffs, fuzz to the floor.", 72, 2, (21,16,7), 10, 3, 6,
   dRoom=0.35, dRoomSize=0.7, dInt=0.75, bGrit=0.8, bBass=5, bTreble=-3, bLen=1.2, tSync=1, tMix=0.2, tFb=0.3, lathe=2)
st("Space Rock", "Long droning jams, echoes stacking up, synth swirls.", 116, 4, (7,6,5), 1, 1, 5,
   bGrit=0.3, lGen=0.4, lMood=4, lShimmer=0.6, lCloud=0.6, lSpace=0.6, lWobble=0.35, tSync=2, tMix=0.45, tFb=0.6, tWow=0.35, sRole=2, lathe=1, sectionBars=2)
st("Post-Rock", "Instrumental crescendos: quiet picking that builds to a roaring climax.", 96, 9, (8,20,11), 16, 1, 1,
   bHook=0.4, bLen=1.2, lGen=0.5, lMood=2, lSpace=0.7, lSize=0.9, lShimmer=0.5, lCloud=0.4, lGtr=0.6, tSync=2, tMix=0.35, tFb=0.5,
   chordBars=1, sectionBars=2, lathe=1)
st("Slowcore", "Very slow, very quiet, very sad. Every note rings out.", 68, 5, (11,9,5), 10, 0, 5,
   dInt=0.3, dLoose=0.2, bLen=1.3, bGrit=0.05, bBusy=0.3, lGen=0.2, lSpace=0.5, tSync=1, tMix=0.2, tFb=0.3, chordBars=1, lathe=3, energy=0.35)
st("Midwest Emo", "Twinkly arpeggios, busy drums, heart-on-sleeve chorus.", 150, 2, (19,8,9), 4, 0, 1,
   dVar=0.8, bHook=0.8, bBusy=0.7, bGrit=0.15, tAdt=0.3, lathe=2)
st("Ragged Rock", "Loose, loud and long. Big distorted chords that wander and feed back.", 104, 7, (12,11,6), 3, 2, 5,
   dLoose=0.55, bGrit=0.45, bLen=1.0, tSync=0, tMix=0.2, tFb=0.2, lathe=2)

# ---- give some rock styles keys / a different bass
def tweak(name, **kw):
    for r in S:
        if r['name'] == name: r.update(kw)
tweak("60s Beat", kOn=1, kInst=3, kFeel=1, kMotion=0.2, kBusy=0.45)
tweak("Psychedelic Rock", kOn=1, kInst=3, kFeel=0, kMotion=0.75)
tweak("Classic Rock", kOn=1, kInst=3, kFeel=0, kMotion=0.5, uBass=1)
tweak("Glam Rock", kOn=1, kInst=0, kFeel=8, kBusy=0.6)
tweak("Blues Rock", kOn=1, kInst=0, kFeel=8, uBass=1)
tweak("Rockabilly", kOn=1, kInst=0, kFeel=8, uBass=1)
tweak("Folk Rock", uBass=1)
tweak("New Wave", kOn=1, kInst=7, kFeel=1, kBusy=0.4)
tweak("Synth Rock", kOn=1, kInst=7, kFeel=6)
tweak("Krautrock", kOn=1, kInst=3, kFeel=0, kMotion=0.4, kBusy=0.3)
tweak("Space Rock", eRole=3, eInst=5, eSpace=0.6)
tweak("Post-Rock", kOn=1, kInst=9, kFeel=10, kRoom=0.5)
tweak("Dream Pop", uBass=1, eRole=2, eInst=3, eSpace=0.6)
tweak("Slowcore", uBass=1)

# ============================ 80s ============================
P='80s'
st("Synth-Pop", "Drum machine, bubbling arpeggios, a pulse bass in octaves. Bright and catchy.", 118, 9, (8,9,10), 26, 8, 3,
   pack=P, uBass=5, bGrit=0.05, bChorus=0.2, kOn=1, kInst=7, kFeel=6, kBusy=0.6, kChorus=0.4, sRole=0, tSync=3, tMix=0.15, lathe=0, dRoom=0.3)
st("Synthwave", "Neon-night drive: gated snare, warm pads, octave bass, lots of echo.", 100, 4, (0,4,16), 26, 5, 3,
   pack=P, uBass=5, bGrit=0.1, kOn=1, kInst=7, kFeel=0, kChorus=0.5, kRoom=0.4, tSync=3, tMix=0.25, tFb=0.4, lGen=0.2, lMood=1, lSpace=0.35, lathe=0)
st("80s Power Ballad", "Slow, huge and heartfelt. Gated drums, a digital piano, big chorus.", 72, 7, (9,10,13), 36, 5, 5,
   pack=P, uBass=1, bLen=1.1, kOn=1, kInst=2, kFeel=5, kRoom=0.4, kChorus=0.3, lSpace=0.4, tSync=1, tMix=0.15, lathe=2, energy=0.45)
st("New Jack Swing", "Swung machine beats, stabbing brass synths, a punchy bass that bounces.", 104, 2, (23,31,24), 28, 8, 16,
   pack=P, uBass=5, dSwing=0.4, bSwing=0.35, bGrit=0.05, kOn=1, kInst=6, kFeel=3, kColour=0.6, lathe=2)
st("Freestyle", "Latin-flavoured club pop: busy machine drums, synth stabs, octave bass.", 118, 9, (0,3,16), 29, 8, 3,
   pack=P, uBass=5, kOn=1, kInst=7, kFeel=3, kBusy=0.6, lathe=0)
st("Hi-NRG", "Fast, glossy dance floor. Galloping octave bass, four on the floor.", 128, 4, (0,4,6), 30, 8, 3,
   pack=P, uBass=5, bBusy=0.8, kOn=1, kInst=6, kFeel=3, kBusy=0.5, lathe=2, energy=0.7)
st("Hair Metal", "Teased hair and big choruses: gated toms, crunchy picked bass.", 126, 4, (12,13,9), 27, 5, 0,
   pack=P, bGrit=0.5, dInt=0.8, lathe=2, energy=0.7)
st("80s Pop Rock", "Arena-sized pop rock: gated drums, chorused guitars, synth pads under the chorus.", 122, 2, (8,9,10), 4, 5, 0,
   pack=P, bChorus=0.25, kOn=1, kInst=7, kFeel=0, kBusy=0.35, tAdt=0.4, tMix=0.12, lathe=2)
st("Smooth 80s", "Glossy and laid back: major sevenths, chorused electric piano, a busy fingered bass.", 96, 5, (23,26,25), 4, 0, 8,
   pack=P, uBass=1, bBusy=0.5, bChorus=0.15, kOn=1, kInst=1, kFeel=1, kColour=0.75, kChorus=0.35, kMotion=0.25, lathe=3, energy=0.45)

# ============================ GLAM ============================
P='Glam'
st("Glam Stomp", "Boots on the floor: stomp-clap beat, pounding piano, fat chorus.", 116, 7, (13,11,12), 12, 3, 5,
   pack=P, dInt=0.8, bGrit=0.45, kOn=1, kInst=0, kFeel=8, kBusy=0.6, tAdt=0.4, tMix=0.15, lathe=2)
st("Glitter Boogie", "Swinging boogie with a pout: shuffle beat, walking bass, barrelhouse piano.", 132, 9, (18,18,13), 14, 1, 4,
   pack=P, dSwing=0.55, bSwing=0.4, bGrit=0.3, kOn=1, kInst=0, kFeel=8, sectionBars=3, tSync=0, tMix=0.25, lathe=2)
st("Art Glam", "Theatrical and spacey: strings, a melodic bass, a ribbon instrument singing on top.", 108, 0, (19,10,20), 12, 1, 1,
   pack=P, bHook=0.65, uBass=1, kOn=1, kInst=5, kFeel=0, kRoom=0.35, eRole=1, eInst=1, eSpace=0.5, tAdt=0.5, tMix=0.15, lathe=1)
st("Glam Ballad", "Lighters up: piano arpeggios, a slow stomp, everything soaring.", 76, 2, (9,19,10), 5, 0, 1,
   pack=P, uBass=1, bLen=1.15, kOn=1, kInst=0, kFeel=5, kRoom=0.35, lSpace=0.35, tSync=1, tMix=0.12, lathe=3, energy=0.45)
st("Glam Metal", "Glitter with distortion: gated drums, galloping riffs, sky-high chorus.", 132, 4, (12,13,16), 27, 5, 7,
   pack=P, bGrit=0.55, dInt=0.8, lathe=2, energy=0.75)

# ============================ HIP-HOP ============================
P='Hip-Hop'
st("Boom Bap", "Dusty crunchy drums, a lazy swing, jazzy electric piano and a fat fingered bass.", 90, 2, (24,27,23), 18, 4, 12,
   pack=P, dSwing=0.3, bSwing=0.25, uBass=1, bLen=1.0, bGrit=0.1, kOn=1, kInst=1, kFeel=7, kColour=0.8, kLofi=0.25, chordBars=1, lathe=2)
st("Trap", "Booming sub-bass that slides, rattling hi-hat rolls, dark glockenspiel melody.", 140, 4, (0,3,33), 19, 6, 11,
   pack=P, uBass=2, bGrit=0.0, bChorus=0.0, kOn=1, kInst=9, kFeel=10, kRoom=0.4, chordBars=1, lathe=2, energy=0.6)
st("Lo-fi Hip-Hop", "Study beats: crackly drums, warm seventh chords on a worn-out piano, soft bass.", 80, 5, (38,25,26), 20, 4, 12,
   pack=P, dSwing=0.35, bSwing=0.3, uBass=1, bBusy=0.35, bGrit=0.0, kOn=1, kInst=10, kFeel=7, kColour=0.9, kLofi=0.6, chordBars=1, lathe=3, energy=0.4)
st("West Coast Funk", "Laid-back bounce: synth bass, funky keys and a high whistling lead that glides.", 92, 7, (27,32,27), 21, 6, 10,
   pack=P, dSwing=0.25, bSwing=0.2, uBass=5, kOn=1, kInst=1, kFeel=1, kColour=0.6, eRole=1, eInst=0, eSpace=0.3, chordBars=1, lathe=2)
st("Electro", "Robotic breakdance: machine drums, synth stabs, bleeps and swoops from the radio studio.", 112, 9, (28,5,7), 22, 6, 3,
   pack=P, uBass=5, kOn=1, kInst=7, kFeel=3, eRole=3, eInst=5, eSpace=0.4, chordBars=1, lathe=0)
st("Old School Party", "Block-party rap over a live disco band: clav stabs, busy funky bass.", 108, 4, (29,32,24), 34, 2, 10,
   pack=P, uBass=1, kOn=1, kInst=4, kFeel=3, chordBars=1, lathe=2)
st("Jazz Rap", "Head-nodding boom bap with walking bass and mellow vibraphone sevenths.", 94, 9, (24,38,23), 18, 4, 4,
   pack=P, dSwing=0.35, bSwing=0.3, uBass=1, kOn=1, kInst=8, kFeel=1, kColour=0.85, kBusy=0.35, chordBars=1, lathe=3)
st("Dark Trap", "Minor-key menace: sliding sub, rolling hats, a tape choir hanging in the air.", 144, 2, (3,16,0), 19, 6, 11,
   pack=P, uBass=2, kOn=0, eRole=2, eInst=3, eSpace=0.6, chordBars=1, lathe=2, energy=0.6)

# ============================ ELECTRONIC ============================
P='Electronic'
st("House", "Four on the floor, offbeat hats, that classic piano and a bouncing bass.", 122, 0, (23,26,30), 23, 7, 13,
   pack=P, uBass=5, kOn=1, kInst=0, kFeel=4, kColour=0.6, kRoom=0.3, chordBars=1, lathe=2)
st("Deep House", "Warm and late-night: deep sub bass, jazzy electric-piano stabs, smooth groove.", 120, 9, (27,37,28), 23, 7, 13,
   pack=P, uBass=2, kOn=1, kInst=1, kFeel=4, kColour=0.85, kChorus=0.3, kMotion=0.3, chordBars=1, lathe=3)
st("Acid House", "One chord, a squelching acid bass line, and the drum machine pounding.", 124, 9, (28,28,27), 23, 7, 15,
   pack=P, uBass=3, kOn=0, eRole=3, eInst=5, eSpace=0.35, chordBars=1, lathe=2, energy=0.65)
st("Techno", "Hypnotic and relentless: rolling bass, machine drums, radio bleeps.", 130, 2, (28,7,21), 24, 7, 14,
   pack=P, uBass=5, kOn=0, eRole=3, eInst=5, eSpace=0.45, chordBars=2, sectionBars=2, lathe=2, energy=0.7)
st("Detroit Techno", "Machine soul: rolling bass, sweeping string-machine chords, futuristic.", 128, 5, (27,37,28), 24, 8, 14,
   pack=P, uBass=5, kOn=1, kInst=5, kFeel=0, kColour=0.6, chordBars=2, sectionBars=2, lathe=2)
st("Drum & Bass", "Fast breakbeats, a growling bass that holds and slides, airy pads.", 172, 4, (0,4,16), 25, 1, 11,
   pack=P, uBass=4, kOn=1, kInst=7, kFeel=0, kRoom=0.4, chordBars=1, lathe=2, energy=0.7)
st("Trip-Hop", "Slow and smoky: crunchy breaks, a deep bass, electric piano and a ghostly wand melody.", 84, 2, (3,0,27), 35, 4, 2,
   pack=P, uBass=1, bChorus=0.2, kOn=1, kInst=1, kFeel=7, kColour=0.7, kLofi=0.3, eRole=1, eInst=0, eSpace=0.6, lathe=3, energy=0.45)
st("Italo Disco", "Glittering machine disco: octave bass, arpeggiators everywhere, a sweet melody.", 120, 9, (0,8,16), 30, 8, 3,
   pack=P, uBass=5, kOn=1, kInst=7, kFeel=6, eRole=1, eInst=4, eSpace=0.3, lathe=0)
st("Ambient Techno", "Soft machine pulse under drifting pads and a tape-choir drone.", 108, 2, (28,30,27), 24, 7, 5,
   pack=P, uBass=2, bBusy=0.3, dInt=0.4, kOn=1, kInst=7, kFeel=0, kRoom=0.5, eRole=2, eInst=3, eSpace=0.7,
   lGen=0.5, lMood=2, lSpace=0.6, lSize=0.9, lShimmer=0.4, chordBars=2, sectionBars=2, lathe=3, energy=0.35)

# ============================ SOUL ============================
P='Soul'
st("Detroit Soul", "The 60s hit factory: snare on every beat, tambourine, pounding piano, a bass that never stops moving.", 120, 0, (10,8,22), 31, 2, 8,
   pack=P, uBass=1, bBusy=0.65, bGrit=0.05, kOn=1, kInst=0, kFeel=2, kBusy=0.5, tSync=0, tMix=0.1, lathe=4)
st("Philly Soul", "Lush and sweeping: four on the floor, sizzling hats, strings and a bouncing octave bass.", 108, 5, (25,23,26), 32, 0, 9,
   pack=P, uBass=1, kOn=1, kInst=5, kFeel=11, kColour=0.55, kRoom=0.35, lathe=3)
st("Northern Soul", "Fast, stomping, made for the all-nighter: driving beat, busy bass, piano.", 132, 2, (8,10,14), 37, 2, 8,
   pack=P, uBass=1, bBusy=0.75, kOn=1, kInst=0, kFeel=2, lathe=4, energy=0.7)
st("Disco", "Four on the floor with open hats, octave bass, strings and a mirror ball.", 120, 9, (24,23,31), 34, 0, 9,
   pack=P, uBass=1, kOn=1, kInst=5, kFeel=11, kColour=0.6, chordBars=1, lathe=2, energy=0.65)
st("Funk", "Everything on the ONE: tight drums, popping bass, chicken-scratch clav stabs.", 104, 4, (29,32,27), 33, 1, 10,
   pack=P, uBass=1, bGrit=0.1, kOn=1, kInst=4, kFeel=3, kBusy=0.6, chordBars=2, lathe=2)
st("Southern Soul", "Sweaty and gritty: laid-back beat, walking bass, churchy organ.", 96, 7, (14,13,18), 0, 2, 4,
   pack=P, dSwing=0.2, uBass=1, kOn=1, kInst=3, kFeel=1, kMotion=0.5, tSync=0, tMix=0.15, lathe=4)
st("Neo Soul", "Behind the beat and warm: melodic bass, electric piano with rich ninths.", 88, 3, (38,25,23), 20, 4, 1,
   pack=P, dSwing=0.3, bSwing=0.25, uBass=1, bBusy=0.45, kOn=1, kInst=1, kFeel=7, kColour=0.95, kMotion=0.3, chordBars=1, lathe=3, energy=0.45)
st("Gospel Soul", "Slow and lifting: walking bass, swelling organ, the whole church singing.", 76, 5, (22,10,13), 5, 0, 4,
   pack=P, uBass=1, kOn=1, kInst=3, kFeel=0, kMotion=0.6, kColour=0.5, lSpace=0.35, lathe=3, energy=0.5)

# ============================ PSYCHEDELIC ============================
P='Psychedelic'
st("Psychedelic Soul", "Funk gets cosmic: fuzzy funk bass, swirling organ, phased-out swoops.", 112, 9, (27,29,28), 33, 1, 10,
   pack=P, uBass=1, bGrit=0.25, kOn=1, kInst=3, kFeel=1, kMotion=0.8, eRole=3, eInst=0, eSpace=0.5, tSync=2, tMix=0.25, tAdt=0.5, lathe=1)
st("Acid Rock Jam", "Long trippy jam: swirling organ, melodic bass, echoes stacking up.", 116, 4, (6,12,7), 0, 1, 1,
   pack=P, bHook=0.65, kOn=1, kInst=3, kFeel=0, kMotion=0.85, lGen=0.3, lMood=1, lCloud=0.5, lWobble=0.45, lSpace=0.45,
   tSync=2, tMix=0.35, tFb=0.55, tWow=0.4, tAdt=0.6, sectionBars=2, lathe=1)
st("Raga Drone", "One chord forever: a droning choir and strummed strings over a gentle pulse.", 92, 2, (7,7,28), 15, 0, 5,
   pack=P, bBusy=0.3, eRole=2, eInst=3, eSpace=0.6, sRole=1, lGen=0.4, lMood=1, lSpace=0.5, lShimmer=0.3, chordBars=2, sectionBars=2, lathe=1, energy=0.4)
st("Tape Freakout", "Backwards-sounding mid-60s studio trickery: tape-choir flutes, a melodic bass, double-tracked everything.", 100, 0, (12,19,20), 0, 0, 1,
   pack=P, bHook=0.7, eRole=1, eInst=3, eSpace=0.4, tSync=2, tMix=0.3, tFb=0.5, tWow=0.5, tAdt=0.7, lathe=4)
st("Space Psych", "Motorik pulse into the void: pads, radio-studio swoops, shimmering echoes.", 104, 4, (7,6,5), 1, 1, 5,
   pack=P, kOn=1, kInst=7, kFeel=0, eRole=3, eInst=5, eSpace=0.7, lGen=0.4, lMood=4, lShimmer=0.6, lCloud=0.6, lSpace=0.6,
   tSync=2, tMix=0.4, tFb=0.6, sectionBars=2, lathe=1)
st("Sunshine Pop", "Sweet harmonies and bright chords: piano eighths, melodic bass, a singing ribbon melody.", 120, 7, (10,19,22), 0, 0, 1,
   pack=P, bHook=0.6, uBass=1, kOn=1, kInst=0, kFeel=1, kBusy=0.4, eRole=1, eInst=1, eSpace=0.35, tAdt=0.5, tMix=0.12, lathe=4)

# ============================ EARLY ELECTRONIC ============================
P='Early Electronic'
st("50s Sci-Fi", "Flying saucers over the drive-in: a wavering wand melody, soft vibes, brushed drums.", 80, 9, (3,16,0), 5, 0, 5,
   pack=P, uBass=1, bBusy=0.3, dInt=0.35, kOn=1, kInst=8, kFeel=0, kColour=0.6, eRole=1, eInst=0, eSpace=0.7, lSpace=0.4, lathe=3, energy=0.35)
st("Radiophonic", "The 60s radio workshop: galloping sub bass, bloops and oscillator melodies, machine pulse.", 112, 4, (21,0,7), 1, 8, 7,
   pack=P, uBass=2, kOn=0, eRole=1, eInst=5, eSpace=0.5, tSync=3, tMix=0.2, lathe=1)
st("Space Age Lounge", "Bachelor-pad exotica: swinging brushes, walking bass, vibes and a ribbon melody.", 104, 5, (23,24,25), 14, 0, 4,
   pack=P, dSwing=0.45, bSwing=0.35, uBass=1, kOn=1, kInst=8, kFeel=1, kColour=0.8, eRole=1, eInst=1, eSpace=0.45, lathe=3, energy=0.45)
st("Stylus Pop", "A cheeky pocket stylus organ playing the tune over a 60s beat and a combo organ.", 126, 2, (8,14,13), 0, 2, 0,
   pack=P, kOn=1, kInst=3, kFeel=1, kMotion=0.2, eRole=1, eInst=4, eSpace=0.25, lathe=4)
st("Satellite Pop", "Early-60s space-race instrumental: a wobbly valve-reed melody, surf drums, galloping bass.", 128, 9, (0,8,10), 13, 2, 7,
   pack=P, kOn=1, kInst=3, kFeel=0, kMotion=0.35, eRole=1, eInst=6, eSpace=0.5, tSync=0, tMix=0.3, lathe=4)
st("Wire Opera", "Eerie and slow: buzzing wire instrument with undertones, strings, gothic bass.", 70, 4, (3,4,16), 10, 0, 2,
   pack=P, uBass=1, bLen=1.2, kOn=1, kInst=5, kFeel=0, kRoom=0.5, eRole=1, eInst=2, eSpace=0.75, lathe=1, energy=0.4)
st("Tape Choir Ballad", "A slow song floating on a tape choir, piano arpeggios and a soft bass.", 74, 7, (9,19,10), 5, 0, 1,
   pack=P, uBass=1, kOn=1, kInst=0, kFeel=5, eRole=2, eInst=3, eSpace=0.55, lathe=3, energy=0.4)
st("Cosmic Sequencer", "German cosmic music: a pulsing sequencer, endless motorik beat, a buzzing wire drone.", 120, 2, (7,28,5), 1, 8, 5,
   pack=P, uBass=5, kOn=1, kInst=7, kFeel=6, eRole=2, eInst=2, eSpace=0.6, lGen=0.3, lMood=4, lSpace=0.4, sectionBars=2, lathe=1)

def f(x): return ("%.3ff" % x) if isinstance(x, float) else str(x)
out = []
out.append('''#pragma once
// ---------------------------------------------------------------------------
// Genre styles for the whole band. Picking a Style in Bandleader moves the knobs
// on every plugin in the set, like loading a preset on each one; tweak away after.
// Generated by genstyles.py - edit the table there.
// ---------------------------------------------------------------------------
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "BandBus.h"

namespace band
{
struct StyleDef
{
    const char* name; const char* blurb; float bpm; int key; int prog[3]; int chordBars, sectionBars; float energy;
    int dStyle, dKit, dFill; float dInt, dLoose, dSwing, dVar, dRoom, dRoomSize;
    int bFeel, bReg; float bHook, bBusy, bPush, bSwing, bLen, bComp, bGrit, bChorus, bFlanger, bBass, bMid, bTreble;
    float lGen; int lMood; float lPads, lSpace, lSize, lShimmer, lCloud, lWobble, lGtr;
    int tSync; float tMix, tFb, tWow, tFlut, tAge, tAdt;
    int sRole, sPattern;     // Schaltwerk: 0 sits out, 1 Strumchord pad, 2 Strumchord strums, 3 drum machine + pad
    int lathe;               // Lathe preset
    const char* pack;        // the heading it sits under in Bandleader's menu
    int uBass;               // Undertow bass: 0 picked, 1 fingers, 2 sub, 3 acid, 4 reese, 5 80s pulse
    int kOn, kInst, kFeel, kReg; float kBusy, kColour, kMotion, kChorus, kLofi, kRoom;   // Parlour (keys)
    int eRole, eInst; float eSpace;   // Ether: 0 sits out, 1 melody, 2 drone/pad, 3 swoops + effects
};

inline const std::vector<StyleDef>& styles()
{
    static const std::vector<StyleDef> s = {''')
for r in S:
    out.append('        { "%s", "%s", %s, %d, { %d, %d, %d }, %d, %d, %s,' % (r['name'], r['blurb'], f(float(r['bpm'])), r['key'], *r['prog'], r['chordBars'], r['sectionBars'], f(r['energy'])))
    out.append('          %d, %d, %d, %s, %s, %s, %s, %s, %s,' % (r['dStyle'], r['dKit'], r['dFill'], f(r['dInt']), f(r['dLoose']), f(r['dSwing']), f(r['dVar']), f(float(r['dRoom'])), f(float(r['dRoomSize']))))
    out.append('          %d, %d, %s,' % (r['bFeel'], r['bReg'], ", ".join(f(float(r[k])) for k in ['bHook','bBusy','bPush','bSwing','bLen','bComp','bGrit','bChorus','bFlanger','bBass','bMid','bTreble'])))
    out.append('          %s, %d, %s,' % (f(r['lGen']), r['lMood'], ", ".join(f(float(r[k])) for k in ['lPads','lSpace','lSize','lShimmer','lCloud','lWobble','lGtr'])))
    out.append('          %d, %s,' % (r['tSync'], ", ".join(f(float(r[k])) for k in ['tMix','tFb','tWow','tFlut','tAge','tAdt'])))
    out.append('          %d, %d, %d,' % (r['sRole'], r['sPattern'], r['lathe']))
    out.append('          "%s", %d, %d, %d, %d, %d, %s,' % (r['pack'], r['uBass'], r['kOn'], r['kInst'], r['kFeel'], r['kReg'], ", ".join(f(float(r[k])) for k in ['kBusy','kColour','kMotion','kChorus','kLofi','kRoom'])))
    out.append('          %d, %d, %s },' % (r['eRole'], r['eInst'], f(float(r['eSpace']))))
out.append('''    };
    return s;
}

inline juce::StringArray styleNames()
{
    juce::StringArray a { "Custom (no style)" };
    for (auto& s : styles()) a.add (s.name);
    return a;
}

// set a parameter by its real value (choice params take the item index)
inline void setParam (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

// ---------------------------------------------------------------------------
// StyleFollower: lives in each plugin. On the message thread it watches the bus;
// when Bandleader applies a style this plugin hasn't taken yet, it calls apply().
// The last style taken is saved with the plugin, so reopening a set changes nothing.
// ---------------------------------------------------------------------------
class StyleFollower : private juce::Timer
{
public:
    StyleFollower (juce::AudioProcessorValueTreeState& s, std::function<void (const StyleDef&)> fn)
        : apvts (s), apply (std::move (fn)) { startTimerHz (4); }
    ~StyleFollower() override { stopTimer(); }

    void check()
    {
        if (auto* p = apvts.getRawParameterValue ("take_style")) if (p->load() < 0.5f) return;
        Leader L;
        if (! readLeader (L) || L.styleStamp == 0 || L.style < 0 || L.style >= (int) styles().size()) return;
        const auto mine = (uint32_t) (juce::int64) apvts.state.getProperty ("styleStamp", 0);
        if (mine == L.styleStamp) return;
        apvts.state.setProperty ("styleStamp", (juce::int64) L.styleStamp, nullptr);
        apply (styles()[(size_t) L.style]);
    }
private:
    void timerCallback() override { check(); }
    juce::AudioProcessorValueTreeState& apvts;
    std::function<void (const StyleDef&)> apply;
};
} // namespace band
''')
open(__import__('os').path.join(__import__('os').path.dirname(__file__), '..', 'Source', 'BandStyles.h'),'w').write("\n".join(out))
print(len(S), "styles")
