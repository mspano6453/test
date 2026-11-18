# Generates Source/BandStyles.h from a readable table.
# keys: 0=C 2=D 4=E 5=F 7=G 9=A
D = dict(dFill=2, dInt=0.55, dLoose=0.3, dSwing=0.0, dVar=0.5, dRoom=-1, dRoomSize=-1,
         bReg=0, bHook=0.4, bBusy=0.55, bPush=0.2, bSwing=0.0, bLen=0.9, bComp=0.4, bGrit=0.2, bChorus=0.0, bFlanger=0.0, bBass=2, bMid=1.5, bTreble=2.5,
         lGen=0.0, lMood=0, lPads=0.6, lSpace=0.25, lSize=0.5, lShimmer=0.0, lCloud=0.0, lWobble=0.1, lGtr=0.25,
         tSync=3, tMix=0.0, tFb=0.2, tWow=0.15, tFlut=0.2, tAge=0.3, tAdt=0.0,
         sRole=0, sPattern=-1, lathe=2, chordBars=0, sectionBars=1, energy=0.55,
         pack='Rock', uBass=0, kOn=0, kInst=0, kFeel=1, kReg=1, kBusy=0.5, kColour=0.3, kMotion=0.3, kChorus=0.1, kLofi=0.0, kRoom=0.2,
         eRole=0, eInst=0, eSpace=0.4,
         cOn=0, cEns=0, cFeel=0, cBusy=0.5, cWarp=0.0, cTimp=0, cRoom=0.3, uBubble=0.0, dSmash=0.0, dOn=1, uOn=1,
         meter=0, gOn=0, gInst=0, gFeel=0, gBusy=0.5, vOn=0, vVoice=0, vFeel=0, vBusy=0.5)
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

# ---- the 60s melodic bass on the styles that want it (a hollow violin bass, played with a pick)
tweak("60s Beat", uBass=6, bFeel=17, bChorus=0.0)
tweak("Psychedelic Rock", uBass=6, bFeel=17, bChorus=0.0)
tweak("Sunshine Pop", uBass=6, bFeel=17, bChorus=0.0)
tweak("Tape Freakout", uBass=6, bFeel=17, bChorus=0.0)
tweak("Folk Rock", bChorus=0.0)

# ============================ 60s CHAMBER POP ============================
P='60s Chamber Pop'
st("String Octet Pop", "No drums, no bass: just a string octet sawing out driving staccato chords under the song.", 136, 4, (33,3,0), 0, 0, 17,
   pack=P, dOn=0, uOn=0, cOn=1, cEns=0, cFeel=1, cBusy=0.6, cRoom=0.15, lathe=4, lSpace=0.1, energy=0.6)
st("Quartet Ballad", "A tender string quartet holding the chords under an acoustic song. No drums.", 92, 5, (38,10,22), 5, 0, 1,
   pack=P, dOn=0, uOn=0, cOn=1, cEns=1, cFeel=0, cBusy=0.4, cRoom=0.25, lathe=3, energy=0.4)
st("Psychedelic Orchestra", "Heavy, strange and swaggering: a sawing cello riff, electric piano, timpani, a melodic bass.", 80, 9, (7,12,20), 0, 1, 17,
   pack=P, uBass=6, kOn=1, kInst=1, kFeel=2, kBusy=0.4, cOn=1, cEns=0, cFeel=7, cTimp=1, cRoom=0.3, tAdt=0.5, tMix=0.2, tSync=2, lathe=4)
st("Baroque Pop", "Harpsichord and strings over a gentle beat, with a melodic bass that sings its own tune.", 108, 7, (10,22,19), 0, 0, 17,
   pack=P, uBass=6, kOn=1, kInst=11, kFeel=5, kBusy=0.5, cOn=1, cEns=0, cFeel=0, cRoom=0.25, lathe=4)
st("Brass Band Pop", "Sunny and bouncing: tack piano, a high piccolo-trumpet line and a bass that won't sit still.", 112, 11, (8,10,14), 0, 2, 17,
   pack=P, uBass=6, bBusy=0.6, kOn=1, kInst=12, kFeel=2, cOn=1, cEns=2, cFeel=10, cBusy=0.4, cRoom=0.2, lathe=4)
st("Music Hall", "Old-time knees-up: swinging brushes, tack piano oom-pah, clarinets on the tune.", 116, 1, (14,22,10), 14, 0, 4,
   pack=P, dSwing=0.5, bSwing=0.4, uBass=6, kOn=1, kInst=12, kFeel=8, cOn=1, cEns=3, cFeel=2, cRoom=0.2, lathe=4)
st("Rising Finale", "A slow piano ballad where the whole orchestra climbs into chaos before every new section.", 76, 4, (9,10,3), 5, 1, 17,
   pack=P, uBass=6, kOn=1, kInst=0, kFeel=5, cOn=1, cEns=0, cFeel=6, cTimp=1, cRoom=0.35, lSpace=0.3, lathe=4, energy=0.5)

# ============================ 90s PSYCH POP ============================
P='90s Psych Pop'
st("Cosmic Orchestra Pop", "Huge smashed drums, warbly out-of-tune strings and timpani, fuzz bass, a gliding wand on top.", 84, 2, (19,9,20), 36, 9, 17,
   pack=P, dSmash=0.8, dInt=0.75, uBass=10, bGrit=0.1, uBubble=0.15, kOn=1, kInst=0, kFeel=5, kRoom=0.4,
   cOn=1, cEns=0, cFeel=0, cWarp=0.6, cTimp=1, cRoom=0.45, eRole=3, eInst=0, eSpace=0.6, tSync=2, tMix=0.25, lathe=6, energy=0.6)
st("Fuzz Lullaby", "Slow and dreamy: blown-out drums kept soft, glockenspiel, wobbling tremolo strings, fuzzy bass.", 72, 7, (19,10,13), 10, 9, 1,
   pack=P, dSmash=0.6, dInt=0.45, uBass=10, kOn=1, kInst=9, kFeel=10, kRoom=0.5, cOn=1, cEns=0, cFeel=5, cWarp=0.7, cRoom=0.5,
   lSpace=0.4, lathe=6, energy=0.4)
st("Blown Out Anthem", "A big singalong: crushed drums, a fuzz bass that pumps, tack piano, brass fanfares and radio swoops.", 116, 9, (8,13,9), 4, 9, 0,
   pack=P, dSmash=0.9, uBass=10, uBubble=0.4, kOn=1, kInst=12, kFeel=8, cOn=1, cEns=2, cFeel=8, cWarp=0.4, cTimp=1,
   eRole=3, eInst=5, eSpace=0.5, lathe=6, energy=0.75)
st("Space Bubble Pop", "Bouncy and odd: a bubbly wubbly bass, vibes, warbly woodwinds and a wand singing the tune.", 104, 4, (10,19,20), 0, 9, 17,
   pack=P, dSmash=0.5, uBass=1, uBubble=0.8, kOn=1, kInst=8, kFeel=1, cOn=1, cEns=3, cFeel=2, cWarp=0.35, eRole=1, eInst=0, eSpace=0.5, lathe=6)
st("Tape Symphony", "Slow and enormous: tape-replay strings walking down under a piano, drums smashed in a big room.", 70, 5, (9,3,19), 10, 9, 1,
   pack=P, dSmash=0.75, uBass=6, kOn=1, kInst=0, kFeel=5, cOn=1, cEns=5, cFeel=3, cWarp=0.5, cRoom=0.4, lSpace=0.35, lathe=6, energy=0.5)

# ---- the rhythm guitar (Jangle) and the backing singers (Harmonia) where they belong
# Jangle guitars: 0 steel acoustic, 1 clean, 2 12-string, 3 hollow, 4 crunch, 5 fuzz, 6 palm-muted
# Jangle feels: 0 folk strum, 1 eighths, 2 power, 3 chug, 4 funk, 5 skank, 6 chops, 7 picked, 8 arpeggio, 9 ring, 10 wash
# Harmonia singers: 0 choir, 1 girl group, 2 the fellas, 3 close trio
# Harmonia feels: 0 oohs & aahs, 1 oohs, 2 aahs, 3 hum, 4 doo-wop, 5 ba ba ba, 6 answers, 7 gospel swells
for name, gi, gf, gb in [("60s Beat",2,1,0.55), ("Garage Rock",5,2,0.6), ("Surf Rock",1,1,0.6), ("Psychedelic Rock",2,9,0.5),
                         ("Classic Rock",4,2,0.55), ("Glam Rock",4,2,0.5), ("Blues Rock",4,3,0.5), ("Rockabilly",3,6,0.6),
                         ("Folk Rock",0,0,0.55), ("Punk",5,2,0.8), ("Post-Punk",1,1,0.45), ("Power Pop",4,1,0.6),
                         ("Shoegaze",5,10,0.6), ("Dream Pop",1,8,0.4), ("Grunge",5,2,0.55), ("Alt Rock",4,2,0.55),
                         ("Britpop",4,1,0.55), ("Indie Rock",1,1,0.55), ("Midwest Emo",1,8,0.6), ("Ragged Rock",5,9,0.5),
                         ("Slowcore",1,9,0.3), ("Post-Rock",1,8,0.45), ("80s Pop Rock",4,1,0.5), ("Hair Metal",5,3,0.65),
                         ("Detroit Soul",1,6,0.5), ("Philly Soul",1,6,0.55), ("Northern Soul",1,6,0.6), ("Funk",1,4,0.6),
                         ("Disco",1,4,0.6), ("Southern Soul",3,6,0.5), ("Psychedelic Soul",1,4,0.55), ("Sunshine Pop",0,0,0.55),
                         ("Baroque Pop",0,7,0.5), ("Brass Band Pop",0,0,0.5), ("Music Hall",3,6,0.5), ("Blown Out Anthem",5,2,0.55),
                         ("Space Bubble Pop",2,1,0.5), ("Cosmic Orchestra Pop",5,9,0.5)]:
    tweak(name, gOn=1, gInst=gi, gFeel=gf, gBusy=gb)
for name, vv, vf, vb in [("60s Beat",3,0,0.45), ("Folk Rock",3,0,0.4), ("Sunshine Pop",1,5,0.55), ("Power Pop",3,1,0.45),
                         ("Glam Rock",2,2,0.5), ("Britpop",2,0,0.4), ("Detroit Soul",1,0,0.5), ("Philly Soul",0,2,0.5),
                         ("Northern Soul",1,6,0.5), ("Southern Soul",0,6,0.5), ("Gospel Soul",0,7,0.6), ("Neo Soul",1,3,0.4),
                         ("Disco",1,2,0.5), ("80s Power Ballad",0,2,0.45), ("Glam Ballad",0,0,0.45), ("Dream Pop",1,1,0.4),
                         ("Baroque Pop",3,0,0.4), ("Rising Finale",0,2,0.4), ("Cosmic Orchestra Pop",0,2,0.45),
                         ("Blown Out Anthem",0,2,0.55), ("Space Bubble Pop",1,5,0.5), ("Tape Choir Ballad",0,1,0.4)]:
    tweak(name, vOn=1, vVoice=vv, vFeel=vf, vBusy=vb)

# ============================ THREE BEATS (3/4 and 6/8) ============================
# drum styles: 38 waltz, 39 jazz waltz, 40 folk waltz, 41 rock waltz, 42 6/8 soul ballad, 43 6/8 rock, 44 6/8 folk jig, 45 6/8 slow jam
P='Waltzes & 6/8'
st("Country Waltz", "A slow-dance waltz: brushed one-two-three, a strummed acoustic, tack piano and a bass on the one.", 96, 7, (13,14,11), 38, 0, 17,
   pack=P, meter=1, uBass=1, bHook=0.4, kOn=1, kInst=12, kFeel=1, kBusy=0.45, gOn=1, gInst=0, gFeel=0, gBusy=0.5, vOn=1, vVoice=3, vFeel=1, vBusy=0.35,
   dInt=0.45, lathe=4, energy=0.45)
st("Jazz Waltz", "Ride cymbal in three, a walking upright, electric piano and a hollow-body comping on two and three.", 138, 2, (24,23,31), 39, 0, 4,
   pack=P, meter=1, uBass=8, dSwing=0.55, bSwing=0.5, kOn=1, kInst=1, kFeel=7, kColour=0.7, gOn=1, gInst=3, gFeel=6, gBusy=0.4,
   dInt=0.4, dLoose=0.35, lathe=3, energy=0.45)
st("Folk Waltz", "Fingerpicked acoustic in three, floor-tom and tambourine, a string quartet and a close trio on top.", 104, 2, (14,10,11), 40, 0, 17,
   pack=P, meter=1, uBass=1, gOn=1, gInst=0, gFeel=7, gBusy=0.5, cOn=1, cEns=1, cFeel=0, cBusy=0.4, cRoom=0.3, vOn=1, vVoice=3, vFeel=0, vBusy=0.4,
   dInt=0.4, lathe=3, energy=0.45)
st("Psych Waltz", "A dreamy three-four that thunders in the chorus: organ, a melodic bass, ringing 12-string, big rock-waltz drums.", 112, 9, (19,8,20), 41, 1, 17,
   pack=P, meter=1, uBass=6, kOn=1, kInst=3, kFeel=0, kMotion=0.6, gOn=1, gInst=2, gFeel=9, gBusy=0.5, vOn=1, vVoice=0, vFeel=0, vBusy=0.4,
   tAdt=0.5, tMix=0.2, tSync=2, lGen=0.2, lMood=1, lathe=4, energy=0.55)
st("Soul Ballad 6/8", "The slow-dance 6/8: piano triplets, cross-stick on four, a girl group answering, strings to lift the chorus.", 72, 9, (10,8,9), 42, 0, 17,
   pack=P, meter=2, uBass=1, kOn=1, kInst=0, kFeel=1, kBusy=0.55, gOn=1, gInst=1, gFeel=8, gBusy=0.4, vOn=1, vVoice=1, vFeel=0, vBusy=0.45,
   cOn=1, cEns=0, cFeel=0, cRoom=0.35, dInt=0.45, lathe=4, energy=0.45)
st("Doo-Wop 6/8", "Street-corner harmony: the fellas singing doo-wop, piano pounding eighths, a gentle triplet beat.", 70, 0, (10,10,9), 42, 0, 17,
   pack=P, meter=2, uBass=8, kOn=1, kInst=0, kFeel=1, kBusy=0.6, vOn=1, vVoice=2, vFeel=4, vBusy=0.5, gOn=1, gInst=3, gFeel=6, gBusy=0.4,
   dInt=0.4, lathe=4, energy=0.45, tMix=0.2, tSync=0)
st("Rocking 6/8", "A minor-key rocker in six: arpeggiated electric, an organ swelling under it, drums that roll like a ship.", 100, 9, (4,16,0), 43, 1, 0,
   pack=P, meter=2, uBass=0, kOn=1, kInst=3, kFeel=0, kMotion=0.5, gOn=1, gInst=1, gFeel=8, gBusy=0.5, vOn=1, vVoice=2, vFeel=2, vBusy=0.4,
   dInt=0.65, lathe=2, energy=0.6)
st("Folk Jig", "A lively 6/8 jig: bodhran-style tambourine, strummed acoustic, the flute and clarinets on the tune.", 116, 2, (14,11,17), 44, 0, 0,
   pack=P, meter=2, uBass=1, gOn=1, gInst=0, gFeel=0, gBusy=0.6, cOn=1, cEns=3, cFeel=9, cBusy=0.55, cRoom=0.25, dInt=0.55, lathe=3, energy=0.6)
st("Slow Jam 6/8", "Late-night R&B in six: claps on four, a sub bass, electric piano sevenths and a choir of aahs.", 64, 5, (23,26,38), 45, 7, 11,
   pack=P, meter=2, uBass=2, kOn=1, kInst=1, kFeel=7, kColour=0.8, vOn=1, vVoice=0, vFeel=2, vBusy=0.35, gOn=1, gInst=1, gFeel=9, gBusy=0.3,
   dInt=0.5, lathe=0, energy=0.45)
st("Gospel 6/8", "Church on Sunday: organ, piano triplets, the choir swelling on every chord, tambourine on the backbeat.", 68, 5, (10,22,8), 42, 1, 17,
   pack=P, meter=2, uBass=1, kOn=1, kInst=3, kFeel=0, kMotion=0.5, vOn=1, vVoice=0, vFeel=7, vBusy=0.6, gOn=0, dInt=0.55, dRoom=0.35, lathe=4, energy=0.55)

# ============================ 100 MORE SONGS: psychedelia first, 60s to 90s ============================
def M(*ds, **kw):
    r = {}
    for d in ds: r.update(d)
    r.update(kw); return r
VB   = dict(uBass=6, bHook=0.65, bChorus=0.0)                       # the hollow violin bass, melodic
ADT  = dict(tAdt=0.6, tMix=0.2, tSync=2, tWow=0.35)                 # double-tracked, tape slap
ECHO = dict(tSync=2, tMix=0.35, tFb=0.55, tWow=0.4)
ORG  = dict(kOn=1, kInst=3, kFeel=0, kMotion=0.7)
PNO  = dict(kOn=1, kInst=0, kFeel=1, kBusy=0.45)
TACK = dict(kOn=1, kInst=12, kFeel=8)
HARP = dict(kOn=1, kInst=11, kFeel=5)
EP   = dict(kOn=1, kInst=1, kFeel=1, kMotion=0.5, kColour=0.5)
PAD  = dict(kOn=1, kInst=7, kFeel=0)
VIB  = dict(kOn=1, kInst=8, kFeel=1, kColour=0.6)
GLK  = dict(kOn=1, kInst=9, kFeel=10, kRoom=0.5)
MTRN = dict(cOn=1, cEns=5, cFeel=0, cWarp=0.45, cRoom=0.3)          # tape strings
FLT  = dict(cOn=1, cEns=6, cFeel=2, cWarp=0.4, cRoom=0.3)           # tape flutes
STR  = dict(cOn=1, cEns=0, cFeel=0, cRoom=0.3)
BRS  = dict(cOn=1, cEns=2, cFeel=8, cRoom=0.25)
WND  = dict(cOn=1, cEns=3, cFeel=2, cRoom=0.25)
HAZE = dict(lGen=0.35, lMood=1, lCloud=0.5, lWobble=0.4, lSpace=0.5, lShimmer=0.3)
SMASH= dict(dSmash=0.8, lathe=6)
def G(i, f, b=0.5): return dict(gOn=1, gInst=i, gFeel=f, gBusy=b)
def V(v, f, b=0.45): return dict(vOn=1, vVoice=v, vFeel=f, vBusy=b)
def E(role, inst, sp=0.5): return dict(eRole=role, eInst=inst, eSpace=sp)
def many(pack, rows):
    for name, blurb, bpm, key, prog, ds, kit, bf, kw in rows:
        st(name, blurb, bpm, key, prog, ds, kit, bf, **M(kw, pack=pack))

many('Summer of Love', [
 ("Paisley Jangle", "Chiming 12-string, a singing bass, tambourine and three-part harmonies.", 126, 2, (14,10,12), 0, 0, 17, M(VB, ADT, G(2,1), V(3,0), lathe=4)),
 ("Technicolor Dream", "Swirling organ and a wand melody over a loping beat; everything echoes.", 104, 4, (12,20,6), 0, 1, 17, M(VB, ORG, ECHO, HAZE, E(1,0,0.6), G(2,9), lathe=1)),
 ("Freakbeat", "Mod R&B gone strange: fuzz riff, pounding drums, organ stabs.", 142, 9, (15,12,16), 17, 2, 0, M(G(5,2,0.6), kOn=1, kInst=3, kFeel=3, bGrit=0.4, tAdt=0.4, dInt=0.75, lathe=2)),
 ("Sitar Sunrise", "A droning one-chord raga rocker with ringing strings and a choir hum.", 112, 2, (7,6,7), 15, 1, 5, M(G(2,9,0.6), E(2,3,0.6), V(2,3,0.3), HAZE, sRole=1, chordBars=2, sectionBars=2, lathe=1)),
 ("Kaleidoscope Ballad", "Slow, floating and strange: harpsichord, tape flutes, a bass that wanders.", 88, 9, (19,12,20), 5, 0, 17, M(VB, HARP, FLT, ADT, E(3,0,0.6), lathe=4, energy=0.45)),
 ("Flower Shop Pop", "Bouncy toytown psych: tack piano, woodwinds and a marching snare.", 118, 0, (10,22,14), 0, 0, 17, M(VB, TACK, WND, V(3,5), ADT, lathe=4)),
 ("Light Show Jam", "A long ballroom jam: two chords, organ swells, guitars answering each other.", 120, 4, (6,12,6), 0, 1, 1, M(ORG, ECHO, HAZE, G(1,8,0.6), bHook=0.7, sectionBars=2, lathe=1)),
 ("West Coast Harmony", "Sunny folk-rock with stacked voices, acoustic strum and a warm bass.", 116, 7, (8,10,12), 15, 0, 1, M(G(0,0), V(0,0,0.5), PNO, uBass=1, tAdt=0.4, lathe=3)),
 ("Acid Folk", "Fingerpicked and eerie: a ribbon melody, hand drums, a drone underneath.", 96, 2, (5,6,3), 15, 0, 5, M(G(0,7), E(1,1,0.6), HAZE, uBass=8, dInt=0.35, lathe=3, energy=0.4)),
 ("Garage Psych", "Cheap organ, fuzz and attitude. Two minutes, three chords, one scream.", 150, 4, (13,12,15), 17, 2, 0, M(G(5,2,0.7), kOn=1, kInst=3, kFeel=1, kMotion=0.2, bGrit=0.45, dLoose=0.5, tMix=0.2, tSync=0, lathe=2)),
 ("Backwards Afternoon", "Dreamy mid-tempo with tape-reversed swells and a lazy melodic bass.", 92, 5, (12,19,20), 0, 0, 17, M(VB, ADT, MTRN, E(3,5,0.6), G(1,9,0.4), tFb=0.6, lathe=4)),
 ("Swinging London", "Brassy, cheeky pop with a walking bass and a go-go beat.", 132, 7, (10,14,22), 0, 2, 4, M(BRS, PNO, V(1,5), uBass=7, tAdt=0.4, lathe=4)),
 ("Mellow Mushroom", "Laid-back groove, electric piano, flute lines and soft congas.", 100, 9, (27,23,26), 33, 0, 10, M(EP, FLT, uBass=1, dInt=0.4, lSpace=0.35, lathe=3, energy=0.45)),
 ("Electric Raga", "Fast modal burn: droning bass, tumbling toms, a buzzing wire lead.", 138, 4, (7,6,7), 13, 1, 5, M(G(5,9,0.6), E(1,2,0.5), ECHO, dInt=0.7, sectionBars=2, lathe=1)),
 ("Dandelion Waltz", "A whimsical 3/4: harpsichord, clarinets and a music-box glockenspiel.", 108, 7, (10,19,22), 38, 0, 17, M(VB, HARP, WND, V(3,1,0.35), meter=1, lathe=4)),
 ("Mind Garden", "Hazy 6/8 ballad with tremolo guitar, tape strings and hushed oohs.", 66, 4, (9,19,3), 42, 0, 1, M(G(1,8,0.4), MTRN, V(0,1,0.35), HAZE, uBass=6, meter=2, lathe=3, energy=0.4)),
 ("Strobe Light", "Pounding four-on-the-floor psych with phased drums and organ stabs.", 134, 9, (6,5,0), 11, 1, 3, M(dict(kOn=1, kInst=3, kFeel=3, kMotion=0.9), G(5,1,0.6), ECHO, bGrit=0.3, lathe=1)),
 ("Incense & Tambourines", "Soft-psych with vibes, a ribbon melody and a girl-group chorus.", 110, 5, (23,10,24), 0, 0, 1, M(VIB, E(1,1,0.45), V(1,0), uBass=7, tAdt=0.5, lathe=4)),
 ("Hall of Mirrors", "Minor-key and mysterious: descending strings, harpsichord, a gothic bass.", 98, 9, (16,0,3), 0, 1, 2, M(HARP, dict(cOn=1, cEns=0, cFeel=3, cRoom=0.35), E(2,3,0.6), ADT, uBass=6, lathe=1)),
 ("Proto-Heavy Thunder", "The loudest band in town: fuzz bass, fuzz guitar, drums like falling furniture.", 100, 4, (15,12,6), 9, 2, 0, M(SMASH, G(5,2,0.6), uBass=10, bGrit=0.5, dInt=0.85, energy=0.75)),
])

many('Studio Wizardry', [
 ("Brass Band Parade", "A brass band marches through a pop song: tack piano, fanfares, a bass that sings.", 112, 7, (10,14,22), 0, 0, 17, M(VB, TACK, BRS, V(3,0), ADT, cTimp=1, lathe=4)),
 ("Orchestral Daydream", "Dreamy piano verse, then the whole orchestra climbs into chaos.", 78, 7, (9,3,19), 5, 0, 17, M(VB, dict(kOn=1, kInst=0, kFeel=5), dict(cOn=1, cEns=0, cFeel=6, cTimp=1, cRoom=0.4), ADT, lathe=4, energy=0.5)),
 ("Circus Poster", "A swirling fairground waltz: calliope organ, tape loops, oom-pah bass.", 116, 2, (16,3,0), 38, 0, 4, M(dict(kOn=1, kInst=3, kFeel=8, kMotion=0.9), FLT, E(3,5,0.5), ECHO, uBass=6, meter=1, lathe=4)),
 ("Floating Waltz", "Slow waltz verse feel with a floating organ, wand swoops and a drowsy bass.", 84, 9, (12,19,13), 38, 0, 17, M(VB, ORG, E(3,0,0.6), V(3,1,0.35), ADT, meter=1, lathe=4, energy=0.45)),
 ("String Quartet Lament", "A lonely story song carried by a sawing string quartet. No drums.", 132, 4, (33,0,3), 0, 0, 17, M(dict(cOn=1, cEns=1, cFeel=1, cBusy=0.6, cRoom=0.15), V(3,6,0.4), dOn=0, uOn=0, lathe=4)),
 ("Piccolo Trumpet Lane", "Bright suburban pop: pounding piano, a high baroque trumpet, a busy bass.", 114, 11, (8,10,23), 0, 2, 17, M(VB, dict(kOn=1, kInst=0, kFeel=2), dict(cOn=1, cEns=2, cFeel=10, cBusy=0.4), V(3,1,0.35), bBusy=0.65, lathe=4)),
 ("One-Chord Tape Loop", "One chord, a thundering drum loop, tape-loop seagulls and a drone.", 124, 0, (30,30,30), 1, 1, 5, M(SMASH, E(3,5,0.7), dict(cOn=1, cEns=5, cFeel=5, cWarp=0.7), HAZE, ECHO, uBass=6, dSmash=0.5, sectionBars=2, lathe=1)),
 ("Surreal Cello Stomp", "Lurching and surreal: sawing cellos, electric piano, a choir chanting nonsense.", 86, 9, (36,12,20), 0, 1, 17, M(VB, EP, dict(cOn=1, cEns=0, cFeel=7, cRoom=0.3), V(0,5,0.5), ADT, lathe=4)),
 ("Tape Flute Reverie", "Tape flutes open it; cellos and trumpets take over; nothing is quite in tune.", 96, 9, (19,12,20), 0, 0, 17, M(VB, FLT, dict(kOn=1, kInst=10, kFeel=5), E(3,5,0.5), ADT, cWarp=0.6, tWow=0.6, lathe=4)),
 ("Clarinet Vaudeville", "A fond old-time shuffle with clarinets and a soft-shoe beat.", 120, 0, (22,10,14), 14, 0, 4, M(TACK, WND, dSwing=0.5, bSwing=0.4, uBass=6, lathe=4)),
 ("Harpsichord Fugue", "Baroque runs over a rock beat: harpsichord, pizzicato strings, a walking bass.", 122, 9, (16,0,4), 0, 0, 4, M(HARP, dict(cOn=1, cEns=0, cFeel=10, cRoom=0.25), uBass=6, lathe=4)),
 ("Tea-Time Symphony", "Genteel and odd: pizzicato strings, woodwind answers, a tuba-like bass.", 104, 5, (10,22,37), 0, 0, 4, M(dict(cOn=1, cEns=0, cFeel=4), dict(kOn=1, kInst=9, kFeel=10), V(3,0,0.4), uBass=8, dInt=0.4, lathe=4)),
 ("Flanged Sunrise", "Everything through the tape flanger: phased drums, swooshing guitars.", 108, 4, (12,13,20), 0, 1, 17, M(VB, G(2,9,0.6), ORG, ADT, bFlanger=0.5, tWow=0.7, tFlut=0.5, lathe=1)),
 ("Teenage Symphony", "Teenage symphony: stacked harmonies, vibes, tack piano, sleigh bells and French horn.", 116, 5, (23,19,24), 0, 0, 17, M(dict(kOn=1, kInst=12, kFeel=1), dict(cOn=1, cEns=4, cFeel=2), V(0,0,0.6), E(1,0,0.4), uBass=7, bHook=0.7, lathe=4)),
 ("Pocket Symphony", "Cello triplets, a quivering wand lead, organ and a choir that keeps changing key.", 124, 3, (16,13,11), 0, 0, 17, M(dict(kOn=1, kInst=3, kFeel=1, kMotion=0.3), dict(cOn=1, cEns=0, cFeel=7), E(1,0,0.5), V(0,5,0.6), uBass=7, lathe=4)),
 ("English Pastoral", "Wistful English pop: acoustic strum, tape strings, music-hall piano.", 110, 7, (10,14,19), 0, 0, 17, M(VB, G(0,0), MTRN, dict(kOn=1, kInst=12, kFeel=1, kBusy=0.4), lathe=3)),
 ("Autumn Organ Pop", "Autumnal and jazzy: breathy harmonies, tape strings, an organ solo.", 106, 4, (25,23,26), 0, 0, 17, M(VB, dict(kOn=1, kInst=3, kFeel=1, kMotion=0.4, kColour=0.6), MTRN, V(3,1,0.5), lathe=3)),
 ("Organ Hymn", "A stately organ hymn over a slow soul beat with a descending bass.", 74, 0, (33,33,10), 5, 0, 1, M(dict(kOn=1, kInst=3, kFeel=0, kMotion=0.5), dict(cOn=1, cEns=0, cFeel=3), uBass=1, dRoom=0.35, lathe=3, energy=0.45)),
 ("Symphonic Ballad", "Symphonic ballad: tape strings, flute, a huge orchestral lift in the chorus.", 80, 4, (3,16,9), 44, 0, 1, M(MTRN, dict(kOn=1, kInst=0, kFeel=5), G(0,7,0.4), V(0,2,0.4), cTimp=1, uBass=1, meter=2, lathe=3, energy=0.5)),
 ("Toy Piano Lullaby", "Tiny and tender: glockenspiel, tape flutes, brushes and a hum.", 76, 0, (10,19,9), 5, 0, 1, M(GLK, FLT, V(3,3,0.35), uBass=8, dInt=0.3, lathe=3, energy=0.35)),
])

many('70s Cosmic', [
 ("Weightless Drift", "Slow, wide and weightless: electric piano, slide swoops, a heartbeat kick.", 66, 4, (30,28,3), 10, 1, 1, M(EP, PAD, E(3,0,0.7), V(1,2,0.35), HAZE, uBass=1, bLen=1.2, lSize=0.9, lathe=1, energy=0.4)),
 ("Space Rock Chug", "Relentless two-chord space rock: chugging riff, oscillator swoops, no brakes.", 136, 4, (6,6,5), 1, 2, 5, M(G(5,3,0.7), E(3,5,0.7), ECHO, bGrit=0.4, dInt=0.75, sectionBars=2, lathe=2)),
 ("Canterbury Tea", "Jazzy, whimsical prog: fuzz organ, a fretless bass, odd little flute tunes.", 118, 5, (25,26,34), 0, 0, 4, M(dict(kOn=1, kInst=3, kFeel=1, kColour=0.8, kMotion=0.4), FLT, uBass=9, bHook=0.7, lathe=3)),
 ("Mellotron Epic", "Grand and solemn: tape strings and choir, 12-string arpeggios, timpani.", 84, 4, (3,16,20), 5, 1, 1, M(MTRN, E(2,3,0.6), G(2,8,0.5), cTimp=1, uBass=0, dRoom=0.4, lathe=1)),
 ("Kosmische Glide", "Motorik beat, shimmering guitars and a pad that never resolves.", 144, 2, (11,28,7), 1, 0, 5, M(PAD, G(1,8,0.6), E(2,1,0.6), ECHO, HAZE, sectionBars=2, lathe=1)),
 ("Cluster Garden", "Gentle German electronics: a pulsing sequencer, vibes and a ribbon tune.", 108, 0, (28,11,28), 1, 8, 3, M(dict(kOn=1, kInst=7, kFeel=6), E(1,1,0.6), uBass=5, dInt=0.4, lGen=0.3, lMood=4, lathe=3, energy=0.45)),
 ("Glam Space Anthem", "Acoustic strum, strings and a rocket-ship chorus with handclaps.", 100, 5, (10,19,8), 12, 1, 1, M(G(0,0), STR, PNO, V(2,2,0.5), ADT, uBass=0, lathe=2)),
 ("Fuzz Boogie", "Boogie shuffle with a fat fuzz riff, congas and falsetto oohs.", 126, 4, (13,12,15), 14, 1, 0, M(G(4,3,0.6), V(1,1,0.5), STR, dSwing=0.5, bSwing=0.4, bGrit=0.3, tAdt=0.5, lathe=2)),
 ("Art Rock Stomp", "Arch and angular: oboe lines, treated piano, synth squiggles over a stomp.", 128, 9, (4,16,0), 12, 1, 3, M(PNO, dict(cOn=1, cEns=3, cFeel=2), E(3,5,0.5), G(4,1,0.5), lathe=2)),
 ("Berlin Instrumental", "Cold, beautiful and slow: pads, a wire drone, a treated drum machine.", 92, 9, (0,2,3), 11, 8, 3, M(PAD, E(2,2,0.7), dict(cOn=1, cEns=5, cFeel=0, cWarp=0.3), uBass=5, lGen=0.3, lMood=3, lathe=1, energy=0.45)),
 ("Doom Swing", "Slow, detuned and doomy with a swinging groove under the weight.", 76, 4, (15,6,5), 9, 2, 0, M(G(5,2,0.5), uBass=10, bGrit=0.5, dSwing=0.2, dInt=0.8, dRoom=0.4, lathe=2, energy=0.7)),
 ("Big Room Blues Stomp", "Enormous room drums, a heavy blues riff and an organ underneath.", 92, 4, (15,12,6), 9, 2, 0, M(G(4,2,0.6), ORG, dRoom=0.6, dRoomSize=0.9, dInt=0.85, bGrit=0.35, lathe=2, energy=0.75)),
 ("Southern Jam", "Twin-guitar boogie that stretches out: piano, organ and a rolling bass.", 124, 7, (12,13,11), 4, 1, 4, M(G(4,1,0.6), dict(kOn=1, kInst=0, kFeel=8), uBass=1, bBusy=0.7, sectionBars=2, lathe=2)),
 ("Laurel Canyon", "Warm and woody: acoustic guitars, electric piano, close harmonies.", 96, 2, (23,26,10), 5, 0, 1, M(G(0,7), EP, V(3,0,0.5), uBass=1, dInt=0.4, lathe=3, energy=0.45)),
 ("Yacht Sunset", "Smooth and expensive: major sevenths, clavinet, a slick fingered bass.", 104, 5, (26,23,35), 33, 0, 10, M(dict(kOn=1, kInst=1, kFeel=1, kColour=0.8, kChorus=0.5), G(1,4,0.4), V(1,2,0.4), uBass=1, lathe=0)),
 ("Blaxploitation Chase", "Wah guitar, flutes, strings and a bass that runs for its life.", 116, 4, (29,27,28), 33, 1, 10, M(G(1,4,0.7), dict(cOn=1, cEns=4, cFeel=1), dict(kOn=1, kInst=4, kFeel=3), uBass=1, bBusy=0.75, lathe=2)),
 ("Cosmic Funk", "Rubber bass, synth squelch and a choir from the mothership.", 104, 4, (29,27,29), 33, 1, 10, M(dict(kOn=1, kInst=4, kFeel=3), E(3,5,0.5), V(0,5,0.5), BRS, uBass=1, uBubble=0.7, lathe=2)),
 ("Proto-Punk Drone", "Two chords, viola drone, pounding floor tom. Sunglasses indoors.", 132, 2, (11,11,12), 7, 2, 5, M(G(4,1,0.7), dict(cOn=1, cEns=1, cFeel=5, cWarp=0.3), bGrit=0.3, dLoose=0.5, tAdt=0.3, lathe=2)),
 ("Pub Rock", "Sweaty R&B played fast with a honking piano.", 150, 9, (13,12,18), 4, 2, 4, M(G(4,6,0.6), dict(kOn=1, kInst=0, kFeel=8), bGrit=0.2, dInt=0.75, lathe=2)),
 ("Dub Chamber", "Deep and spacious: skanking guitar, a huge bass, echoes flying off the snare.", 76, 9, (5,6,5), 33, 0, 3, M(G(1,5,0.5), dict(kOn=1, kInst=3, kFeel=9), E(3,5,0.6), uBass=2, bBusy=0.4, tSync=1, tMix=0.45, tFb=0.65, dInt=0.45, lathe=1)),
])

many('80s Neo-Psych', [
 ("Paisley Underground", "60s jangle through an 80s amp: 12-string, organ and a snare with reverb.", 128, 2, (12,8,14), 0, 1, 1, M(G(2,1,0.6), ORG, V(3,0,0.4), dRoom=0.35, tAdt=0.4, lathe=2)),
 ("Jangle College", "Bright arpeggios, a busy melodic bass and mumbled harmonies.", 138, 9, (9,8,11), 0, 0, 1, M(G(2,8,0.7), V(3,1,0.35), bHook=0.7, uBass=0, lathe=2)),
 ("Dream Jangle", "Chorused guitars and a sighing bass under a slow gated beat.", 104, 2, (19,8,9), 5, 1, 1, M(G(1,8,0.5), PAD, HAZE, bChorus=0.6, dRoom=0.4, lathe=3)),
 ("Ethereal Cathedral", "Glossy, glittering guitars over a drum machine; a voice like weather.", 96, 4, (9,3,19), 11, 8, 2, M(G(1,8,0.6), PAD, V(1,2,0.4), E(2,3,0.7), HAZE, bChorus=0.7, lSize=0.95, lShimmer=0.6, lathe=3)),
 ("Gloom Disco", "Six-string bass melody, synth strings and a danceable drum machine.", 126, 4, (0,4,1), 11, 8, 1, M(dict(kOn=1, kInst=5, kFeel=0), G(1,8,0.5), bHook=0.8, bChorus=0.6, lathe=0)),
 ("Neo-Psychedelic Swirl", "Flanged bass, backwards guitar and floor toms in a big dark room.", 120, 9, (1,0,16), 7, 1, 2, M(G(1,9,0.6), ECHO, HAZE, bFlanger=0.5, bChorus=0.5, dRoom=0.4, lathe=1)),
 ("Gospel Drone", "One chord, tremolo guitar, organ and gospel patience.", 108, 4, (7,7,11), 1, 1, 5, M(G(5,10,0.6), dict(kOn=1, kInst=3, kFeel=0, kMotion=0.8), V(0,7,0.4), ECHO, HAZE, sectionBars=2, chordBars=2, lathe=1)),
 ("Indie Dance Groove", "A funky drummer loop, wah guitar, organ and a bassline that dances.", 112, 9, (11,12,29), 33, 1, 10, M(G(1,4,0.6), dict(kOn=1, kInst=3, kFeel=3, kMotion=0.6), uBass=1, bBusy=0.7, tAdt=0.4, lathe=2)),
 ("Baggy Sunrise", "Loose and blissful: house piano, shuffling breakbeat, a chiming guitar.", 116, 0, (8,13,11), 33, 1, 10, M(dict(kOn=1, kInst=0, kFeel=4), G(2,8,0.5), V(1,2,0.4), uBass=1, lathe=0)),
 ("C86 Shamble", "Fast, shy and scrappy: trebly guitars and a tambourine doing the drummer's job.", 156, 7, (14,10,13), 6, 0, 5, M(G(1,1,0.8), bTreble=4, dLoose=0.5, dInt=0.6, lathe=2)),
 ("Feedback Candy", "A 60s girl-group beat buried under a blizzard of fuzz.", 124, 4, (10,13,14), 0, 1, 5, M(SMASH, G(5,10,0.7), V(1,1,0.4), dSmash=0.5, dRoom=0.5, lSpace=0.6, lGtr=0.7, lathe=1)),
 ("Synth Psych", "Analog arpeggios, a motorik machine and a wand lead from the future's past.", 124, 9, (0,2,5), 1, 8, 3, M(dict(kOn=1, kInst=7, kFeel=6), E(1,0,0.5), uBass=5, sRole=3, sPattern=1, lathe=0)),
 ("Sophisti-Pop", "Fretless bass, jazzy sevenths and a snare the size of a penthouse.", 100, 5, (23,25,35), 5, 1, 1, M(dict(kOn=1, kInst=2, kFeel=1, kColour=0.8), V(1,2,0.4), uBass=9, bHook=0.7, dRoom=0.4, lathe=0)),
 ("Big Music", "Widescreen and windswept: strummed acoustics, piano, a chorus you could plant a flag on.", 132, 2, (8,9,13), 4, 1, 0, M(G(0,1,0.7), PNO, STR, V(2,2,0.4), dRoom=0.45, lathe=2, energy=0.7)),
 ("Goth Western", "Twanging tremolo guitar, a galloping bass and tubular drama.", 140, 4, (0,16,4), 13, 1, 7, M(G(1,1,0.6), E(2,3,0.5), dict(cOn=1, cEns=2, cFeel=8), bChorus=0.4, tMix=0.3, tSync=0, lathe=1)),
 ("Cold Wave", "Minimal and frozen: a pulse bass, one-finger synth, a dry drum box.", 118, 2, (5,0,6), 11, 8, 3, M(dict(kOn=1, kInst=7, kFeel=0, kBusy=0.3), E(1,4,0.3), uBass=5, sRole=3, sPattern=5, lathe=0)),
 ("Hi-Fi Art Pop", "Fairlight-era studio pop: sampled strings, fretless bass, a choir in a box.", 110, 9, (4,3,16), 5, 1, 1, M(dict(kOn=1, kInst=2, kFeel=5), E(2,3,0.5), STR, uBass=9, dRoom=0.4, lathe=0)),
 ("Noise Pop Rush", "Three chords at full speed with the amps on fire and a tune underneath.", 164, 4, (8,13,11), 6, 2, 5, M(G(5,2,0.8), bGrit=0.5, dInt=0.85, lSpace=0.4, lathe=2, energy=0.8)),
 ("Space Blues", "A slow two-chord blues in orbit: slide swoops, organ, brushed drums.", 72, 4, (11,12,11), 10, 0, 5, M(G(1,9,0.4), dict(kOn=1, kInst=3, kFeel=0, kMotion=0.6), E(3,0,0.7), ECHO, HAZE, dInt=0.35, lathe=1, energy=0.4)),
 ("Second Summer", "Acid squelch meets tambourine: a 4/4 kick under psychedelic guitars.", 122, 9, (11,6,12), 11, 8, 15, M(G(2,9,0.5), dict(kOn=1, kInst=0, kFeel=4), E(3,5,0.5), uBass=3, lathe=0)),
])

many('90s Psych & Beyond', [
 ("Fuzz Folk March", "A strummed acoustic pushed into the red, singing saw and a marching-band brass section.", 132, 7, (8,10,13), 4, 9, 5, M(SMASH, G(0,1,0.8), BRS, E(1,0,0.5), uBass=10, dSmash=0.6, lathe=6)),
 ("Bedroom Symphony", "Four-track pop with too many ideas: toy piano, clarinets, sudden fuzz.", 120, 0, (10,19,14), 0, 0, 17, M(VB, GLK, WND, G(5,1,0.5), V(3,5,0.4), tWow=0.6, tAge=0.6, lathe=6)),
 ("Lo-fi Brass Parade", "A lo-fi brass band, singalong harmonies and a tape hiss halo.", 108, 5, (14,10,12), 0, 0, 17, M(VB, TACK, BRS, V(0,5,0.6), tAge=0.7, tWow=0.5, tMix=0.15, lathe=6)),
 ("Heartbroken Symphony", "Symphonic and heartbroken: warped strings, harp-like piano, cannon drums.", 78, 0, (9,19,3), 36, 9, 1, M(SMASH, dict(kOn=1, kInst=0, kFeel=5, kRoom=0.4), dict(cOn=1, cEns=0, cFeel=0, cWarp=0.6, cTimp=1, cRoom=0.45), V(0,2,0.4), E(3,0,0.6), uBass=10, bGrit=0.1)),
 ("Robot Acoustic Pop", "Acoustic strum over a squelchy synth bass and a programmed beat.", 96, 0, (10,8,19), 5, 8, 1, M(G(0,0,0.6), dict(kOn=1, kInst=7, kFeel=6, kBusy=0.4), E(3,5,0.5), uBass=3, uBubble=0.6, dSmash=0.4, lathe=6)),
 ("Deserter's Lullaby", "A frail waltz with bowed saw, pump organ and a faraway choir.", 92, 2, (19,10,9), 38, 0, 1, M(ORG, E(1,0,0.6), V(0,1,0.4), MTRN, uBass=8, meter=1, cWarp=0.6, dInt=0.35, lathe=3, energy=0.4)),
 ("Gospel Space Swell", "Gospel choir, horns and strings over one swelling chord progression.", 70, 7, (33,33,13), 42, 1, 1, M(dict(kOn=1, kInst=3, kFeel=0), V(0,7,0.6), dict(cOn=1, cEns=4, cFeel=0, cRoom=0.45), G(1,8,0.4), HAZE, meter=2, lathe=1)),
 ("Pitch-Bend Wash", "Guitars bent out of pitch, a buried voice, a snare like a pillow fight.", 96, 4, (19,20,12), 8, 1, 5, M(G(5,10,0.8), V(1,1,0.35), HAZE, bGrit=0.4, lCloud=0.8, lSpace=0.8, lSize=0.95, lGtr=0.85, tWow=0.6, chordBars=1, lathe=1)),
 ("Space Rock Revival", "Tremolo, drones and a slow build to white light.", 88, 9, (7,11,12), 10, 1, 5, M(G(5,10,0.5), ORG, V(0,7,0.4), STR, ECHO, HAZE, sectionBars=2, lathe=1)),
 ("Stereo Lounge", "Motorik bossa: vintage organ, vibes, 'ba ba ba' and a bubbling synth.", 132, 5, (26,23,34), 1, 0, 3, M(dict(kOn=1, kInst=3, kFeel=1, kMotion=0.2, kColour=0.7), VIB, V(1,5,0.6), E(3,5,0.4), uBass=1, lathe=3)),
 ("Trip-Hop Noir", "A dusty breakbeat, upright bass, tape strings and a wand melody.", 84, 9, (3,0,16), 35, 3, 12, M(MTRN, E(1,0,0.6), EP, uBass=8, kLofi=0.5, tAge=0.6, lathe=1)),
 ("Big Beat Psych", "A huge distorted break, a sitar-ish riff and sirens.", 124, 4, (6,7,5), 33, 9, 10, M(SMASH, G(2,9,0.6), E(3,5,0.6), uBass=3, bGrit=0.4, dSmash=0.9, energy=0.8)),
 ("Brit-Psych Anthem", "A sunny stomper with backwards guitar, tape strings and a terrace chorus.", 118, 9, (12,8,20), 0, 1, 17, M(VB, G(4,1,0.6), MTRN, V(2,2,0.5), ADT, lathe=2)),
 ("Dandy Jangle", "Tambourine-led 60s revival: droning 12-string and a bored, brilliant groove.", 122, 4, (12,11,13), 0, 1, 5, M(G(2,1,0.7), ORG, V(3,1,0.35), dLoose=0.45, tAdt=0.5, lathe=2)),
 ("Slacker Fuzz", "Wobbly, tuneful and half-asleep, then suddenly very loud.", 112, 2, (8,12,13), 9, 2, 0, M(G(5,1,0.6), bGrit=0.4, dLoose=0.55, uBass=0, lathe=2)),
 ("Lo-fi Hymn", "Four-track tape, a detuned piano, a drum kit down the hall.", 84, 5, (10,9,13), 5, 0, 1, M(dict(kOn=1, kInst=10, kFeel=5, kLofi=0.7), G(0,7,0.4), V(3,3,0.3), dRoom=0.6, tAge=0.8, tWow=0.6, tMix=0.15, lathe=3, energy=0.4)),
 ("Chamber Indie", "Strings, glockenspiel and brushed drums around a fingerpicked guitar.", 100, 2, (9,10,38), 15, 0, 1, M(G(0,7), dict(cOn=1, cEns=1, cFeel=2), GLK, uBass=8, dInt=0.4, lathe=3, energy=0.45)),
 ("Desert Psych", "A heavy riff rolling down an empty highway, fuzz bass in the back seat.", 104, 2, (15,6,12), 9, 2, 0, M(G(5,3,0.6), ECHO, uBass=10, bGrit=0.4, dInt=0.75, dRoom=0.35, tMix=0.15, lathe=2, energy=0.7)),
 ("Post-Rock Sunrise", "Glockenspiel and bowed guitar swelling into a cymbal-washed climax.", 80, 2, (9,3,8), 10, 1, 1, M(G(1,8,0.5), GLK, dict(cOn=1, cEns=0, cFeel=5, cRoom=0.5), HAZE, lSize=0.9, sectionBars=2, lathe=1)),
 ("Carnival Finale", "Everybody in: fuzz, brass, choir, timpani, the rise before each section. Then again, louder.", 126, 7, (8,13,33), 4, 9, 0, M(SMASH, G(5,2,0.7), dict(cOn=1, cEns=4, cFeel=6, cTimp=1, cWarp=0.4), V(0,2,0.7), TACK, E(3,5,0.6), uBass=10, uBubble=0.4, energy=0.85)),
])

many('Odd Time (5/4 & 7/8)', [
 ("Cool Five", "A smoky jazz 5/4: ride cymbal, a vamping piano, a walking upright.", 168, 3, (24,23,31), 3, 0, 4, M(dict(kOn=1, kInst=0, kFeel=2, kColour=0.7), uBass=8, dSwing=0.5, bSwing=0.5, meter=3, lathe=3, energy=0.5)),
 ("Spy Theme Five", "Minor-key espionage in five: twangy guitar, brass stabs, bongos.", 140, 4, (16,0,3), 0, 1, 7, M(G(1,6,0.6), BRS, uBass=7, meter=3, tMix=0.25, tSync=0, lathe=2)),
 ("Raga Rock Five", "A droning one-chord 5/4 with ringing 12-string and a wire lead.", 120, 2, (7,6,7), 15, 1, 5, M(G(2,9,0.6), E(1,2,0.5), HAZE, meter=3, sectionBars=2, lathe=1)),
 ("Prog Seven", "Seven-eight riffing: fuzz organ, a busy bass, the drummer counting under his breath.", 132, 9, (16,4,0), 4, 2, 0, M(dict(kOn=1, kInst=3, kFeel=3, kMotion=0.5), G(4,2,0.6), uBass=0, bBusy=0.65, meter=4, lathe=2)),
 ("Seven Sisters", "A skipping 7/8 folk-psych tune: harpsichord, tape flutes, tambourine.", 116, 2, (14,10,12), 15, 0, 17, M(VB, HARP, FLT, G(0,0,0.5), meter=4, lathe=4)),
 ("Money Seven", "A lopsided seven-beat groove: a bass riff out front, electric piano, sax-like brass.", 120, 4, (11,12,11), 33, 1, 1, M(EP, BRS, uBass=1, bHook=0.7, meter=4, lathe=1)),
])

many('Raga & Tape Room', [
 ("Sitar Afternoon", "A real sitar trading lines with a twelve-string over a tanpura-style one-chord groove.", 104, 4, (7,6,7), 15, 1, 5, M(G(2,9,0.5), E(1,7,0.55), HAZE, sectionBars=2, lathe=1)),
 ("Drone Garden", "The tanpura drone under everything, hand percussion, a hum from the backing singers.", 84, 2, (7,7,6), 15, 0, 5, M(E(2,8,0.6), V(0,3,0.3), uBass=5, lathe=1, energy=0.35)),
 ("Within & Without", "Slow raga-rock: sitar melody, tape cellos swelling under it, tabla-ish toms.", 76, 9, (7,6,7), 15, 0, 6, M(E(1,7,0.6), dict(cOn=1, cEns=7, cFeel=0, cRoom=0.4), sectionBars=2, lathe=1, energy=0.4)),
 ("Brass Tapes", "A wonky tape-replay brass section stabbing over a bouncy music-hall beat.", 112, 7, (0,2,5), 17, 0, 1, M(dict(cOn=1, cEns=8, cFeel=1, cWarp=0.3, cRoom=0.25), PNO, lathe=4)),
 ("Woodwind Tapes", "Tape-replay woodwinds in a lazy waltz with a harpsichord and a walking bass.", 96, 2, (14,10,12), 15, 0, 17, M(dict(cOn=1, cEns=9, cFeel=2, cWarp=0.35, cRoom=0.3), HARP, meter=1, lathe=4)),
 ("Cello Tapes", "The tape-replay cellos on a slow minor song, a fuzz guitar out front, a long tape echo.", 72, 4, (16,0,3), 15, 1, 6, M(dict(cOn=1, cEns=7, cFeel=0, cRoom=0.35), G(4,7,0.4), ECHO, lathe=1, energy=0.45)),
])

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
    int cOn, cEns, cFeel; float cBusy, cWarp; int cTimp; float cRoom;   // Chamber (orchestra)
    float uBubble, dSmash;   // Undertow's BUBBLE, Tea Towel's SMASH
    int dOn, uOn;            // 0 = the drums / the bass sit this one out
    int meter;               // 0 4/4, 1 3/4, 2 6/8
    int gOn, gInst, gFeel; float gBusy;    // Jangle (rhythm guitar)
    int vOn, vVoice, vFeel; float vBusy;   // Harmonia (backing vocals)
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
    out.append('          %d, %d, %s,' % (r['eRole'], r['eInst'], f(float(r['eSpace']))))
    out.append('          %d, %d, %d, %s, %s, %d, %s, %s, %s },' % (r['cOn'], r['cEns'], r['cFeel'], f(float(r['cBusy'])), f(float(r['cWarp'])), r['cTimp'], f(float(r['cRoom'])), f(float(r['uBubble'])), f(float(r['dSmash']))))
    out[-1] = out[-1][:-3] + ','
    out.append('          %d, %d, %d, %d, %d, %d, %s, %d, %d, %d, %s },' % (r['dOn'], r['uOn'], r['meter'], r['gOn'], r['gInst'], r['gFeel'], f(float(r['gBusy'])), r['vOn'], r['vVoice'], r['vFeel'], f(float(r['vBusy']))))
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
