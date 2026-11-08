# Generates Source/BandStyles.h from a readable table.
# keys: 0=C 2=D 4=E 5=F 7=G 9=A
D = dict(dFill=2, dInt=0.55, dLoose=0.3, dSwing=0.0, dVar=0.5, dRoom=-1, dRoomSize=-1,
         bReg=0, bHook=0.4, bBusy=0.55, bPush=0.2, bSwing=0.0, bLen=0.9, bComp=0.4, bGrit=0.2, bChorus=0.0, bFlanger=0.0, bBass=2, bMid=1.5, bTreble=2.5,
         lGen=0.0, lMood=0, lPads=0.6, lSpace=0.25, lSize=0.5, lShimmer=0.0, lCloud=0.0, lWobble=0.1, lGtr=0.25,
         tSync=3, tMix=0.0, tFb=0.2, tWow=0.15, tFlut=0.2, tAge=0.3, tAdt=0.0,
         sRole=0, sPattern=-1, lathe=2, chordBars=0, sectionBars=1, energy=0.55)
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
    out.append('          %d, %d, %d },' % (r['sRole'], r['sPattern'], r['lathe']))
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
