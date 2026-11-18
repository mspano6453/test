#include "LatheEditor.h"

using namespace ltui;
using namespace lt;
using ttui::font;
namespace col = ttui::col;

// ===========================================================================
// ChainView
// ===========================================================================
ChainView::ChainView (LatheProcessor& p) : proc (p) {}

juce::Rectangle<float> ChainView::tileRect (int slot) const
{
    return { 10.0f, 40.0f + 62.0f * (float) slot, (float) getWidth() - 20.0f, 54.0f };
}
juce::Rectangle<float> ChainView::dotRect (int slot) const
{
    auto t = tileRect (slot);
    return { t.getRight() - 30.0f, t.getCentreY() - 9.0f, 18.0f, 18.0f };
}

void ChainView::paint (juce::Graphics& g)
{
    g.fillAll (col::panel);
    g.setColour (col::muted);
    g.setFont (font (11.0f, true));
    g.drawText ("SIGNAL CHAIN  (drag to reorder)", 12, 12, getWidth() - 20, 16, juce::Justification::centredLeft);

    for (int s = 0; s < NumModules; ++s)
    {
        const int m = proc.order[(size_t) s].load();
        auto t = tileRect (s);
        if (s == dragSlot) t = t.withY (dragY - 27.0f);
        const bool on = proc.apvts.getRawParameterValue (modOnId (m))->load() > 0.5f;
        g.setColour (m == selected ? col::panel2.brighter (0.15f) : col::panel2);
        g.fillRoundedRectangle (t, 6.0f);
        g.setColour (m == selected ? col::amber : col::line);
        g.drawRoundedRectangle (t, 6.0f, m == selected ? 2.0f : 1.0f);
        g.setColour (on ? col::cream : col::muted);
        g.setFont (font (14.0f, true));
        g.drawText (juce::String (s + 1) + "  " + moduleName (m), t.reduced (12, 0).withTrimmedRight (30), juce::Justification::centredLeft);
        auto d = dotRect (s); if (s == dragSlot) d = d.withY (t.getCentreY() - 9.0f);
        g.setColour (on ? col::amber : col::line.brighter (0.2f));
        g.fillEllipse (d);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (d, 1.0f);
        if (dragTo == s && dragSlot >= 0 && dragTo != dragSlot)
        {
            g.setColour (col::amber);
            g.fillRect (t.getX(), (dragTo > dragSlot ? tileRect (s).getBottom() + 3.0f : tileRect (s).getY() - 5.0f), t.getWidth(), 2.0f);
        }
    }
    g.setColour (col::muted);
    g.setFont (font (12.0f));
    g.drawFittedText ("Click a module to edit it. The amber dot turns it on and off.", 12, getHeight() - 60, getWidth() - 24, 50,
                      juce::Justification::topLeft, 4, 1.0f);
}

void ChainView::mouseDown (const juce::MouseEvent& e)
{
    for (int s = 0; s < NumModules; ++s)
    {
        const int m = proc.order[(size_t) s].load();
        if (dotRect (s).expanded (4).contains (e.position))
        {
            if (auto* p = proc.apvts.getParameter (modOnId (m)))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
                p->endChangeGesture();
            }
            repaint();
            return;
        }
        if (tileRect (s).contains (e.position))
        {
            selected = m;
            if (onSelect) onSelect (m);
            dragSlot = s; dragTo = s; dragY = e.position.y;
            repaint();
            return;
        }
    }
}

void ChainView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragSlot < 0 || e.getDistanceFromDragStart() < 6) return;
    dragY = e.position.y;
    dragTo = juce::jlimit (0, NumModules - 1, (int) ((dragY - 40.0f) / 62.0f));
    repaint();
}

void ChainView::mouseUp (const juce::MouseEvent&)
{
    if (dragSlot >= 0 && dragTo >= 0 && dragTo != dragSlot)
    {
        std::array<int, NumModules> o {};
        for (int i = 0; i < NumModules; ++i) o[(size_t) i] = proc.order[(size_t) i].load();
        const int m = o[(size_t) dragSlot];
        std::vector<int> v (o.begin(), o.end());
        v.erase (v.begin() + dragSlot);
        v.insert (v.begin() + dragTo, m);
        for (int i = 0; i < NumModules; ++i) o[(size_t) i] = v[(size_t) i];
        proc.setOrder (o);
    }
    dragSlot = dragTo = -1;
    repaint();
}

// ===========================================================================
// GenericPanel
// ===========================================================================
GenericPanel::GenericPanel (LatheProcessor& p, juce::String t, juce::String b, std::vector<Group> gs, int extraW, juce::Component* popupParent)
    : proc (p), title (std::move (t)), blurb (std::move (b)), groups (std::move (gs)), extraWidth (extraW)
{
    for (auto& g : groups)
    {
        groupComps.emplace_back();
        for (auto& c : g.ctls)
        {
            juce::Component* comp = nullptr;
            if (c.kind == Ctl::Knob)
            {
                auto* k = new ttui::Knob (c.label, false, popupParent);
                k->attach (proc.apvts, c.id);
                comp = k;
            }
            else if (c.kind == Ctl::Combo)
            {
                auto* cb = new juce::ComboBox();
                if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (c.id)))
                    cb->addItemList (ch->choices, 1);
                cAtt.add (new APVTS::ComboBoxAttachment (proc.apvts, c.id, *cb));
                comp = cb;
            }
            else
            {
                auto* tb = new juce::ToggleButton (c.label);
                tb->setColour (juce::ToggleButton::textColourId, col::cream);
                tb->setColour (juce::ToggleButton::tickColourId, col::amber);
                tb->setColour (juce::ToggleButton::tickDisabledColourId, col::muted);
                bAtt.add (new APVTS::ButtonAttachment (proc.apvts, c.id, *tb));
                comp = tb;
            }
            owned.add (comp);
            addAndMakeVisible (comp);
            groupComps.back().push_back (comp);
        }
    }
}

void GenericPanel::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (20, 14);
    g.setColour (col::amber);
    g.setFont (font (24.0f, true));
    g.drawText (title.toUpperCase(), r.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (font (13.0f));
    g.drawFittedText (blurb, r.removeFromTop (36), juce::Justification::topLeft, 2, 1.0f);

    for (size_t i = 0; i < groupAreas.size(); ++i)
    {
        const auto a = groupAreas[i];
        g.setColour (col::panel);
        g.fillRoundedRectangle (a.toFloat(), 8.0f);
        g.setColour (col::amber);
        g.setFont (font (14.0f, true));
        auto tr = a.reduced (12, 8).removeFromTop (18);
        g.drawText (groups[i].title, tr, juce::Justification::centredLeft);
        if (groupSuffix)
        {
            g.setColour (col::cream);
            g.setFont (font (12.0f));
            g.drawText (groupSuffix ((int) i), tr, juce::Justification::centredRight);
        }
    }
    if (paintExtra && ! extraArea.isEmpty())
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (extraArea.toFloat(), 8.0f);
        paintExtra (g, extraArea);
    }
}

void GenericPanel::resized()
{
    auto r = getLocalBounds().reduced (20, 14);
    r.removeFromTop (74);
    extraArea = {};
    if (extraWidth > 0) { extraArea = r.removeFromRight (extraWidth); r.removeFromRight (12); }
    groupAreas.clear();
    const int n = (int) groups.size();
    const int gw = (r.getWidth() - (n - 1) * 12) / juce::jmax (1, n);
    for (int i = 0; i < n; ++i)
    {
        auto a = r.removeFromLeft (gw);
        r.removeFromLeft (12);
        int knobs = 0;
        for (auto& c : groups[(size_t) i].ctls) if (c.kind == Ctl::Knob) ++knobs;
        const int perRow = (knobs >= 3 && gw >= 170) ? 2 : 1;
        const int rows = (knobs + perRow - 1) / perRow;
        const int others = (int) groups[(size_t) i].ctls.size() - knobs;
        const int h = juce::jmin (a.getHeight(), 40 + rows * 108 + others * 34 + 10);
        groupAreas.push_back (a.withHeight (h));

        auto inner = a.withHeight (h).reduced (10, 8).withTrimmedTop (28);
        int k = 0;
        juce::Rectangle<int> row;
        auto& comps = groupComps[(size_t) i];
        auto& ctls = groups[(size_t) i].ctls;
        // toggles/combos first, then the knob grid
        for (size_t c = 0; c < ctls.size(); ++c)
            if (ctls[c].kind != Ctl::Knob) { comps[c]->setBounds (inner.removeFromTop (28)); inner.removeFromTop (6); }
        for (size_t c = 0; c < ctls.size(); ++c)
        {
            if (ctls[c].kind != Ctl::Knob) continue;
            if (k % perRow == 0) row = inner.removeFromTop (108);
            const int cw = row.getWidth() / perRow;
            comps[c]->setBounds (row.removeFromLeft (cw).reduced (4, 2));
            ++k;
        }
    }
}

// ===========================================================================
// EqPanel
// ===========================================================================
EqPanel::EqPanel (LatheProcessor& p, juce::Component* popupParent) : proc (p)
{
    for (int b = 0; b < NumEqBands; ++b)
    {
        bandButtons[b].setButtonText (eqBandName (b));
        bandButtons[b].onClick = [this, b] { selectBand (b); };
        addAndMakeVisible (bandButtons[b]);
    }
    for (auto* t : { &onToggle, &steepToggle })
    {
        t->setColour (juce::ToggleButton::textColourId, col::cream);
        t->setColour (juce::ToggleButton::tickColourId, col::amber);
        t->setColour (juce::ToggleButton::tickDisabledColourId, col::muted);
        addAndMakeVisible (*t);
    }
    steepAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "eq_steep", steepToggle);
    freq = std::make_unique<ttui::Knob> ("FREQUENCY", false, popupParent);
    gain = std::make_unique<ttui::Knob> ("GAIN", false, popupParent);
    q    = std::make_unique<ttui::Knob> ("Q", false, popupParent);
    gain->slider.getProperties().set ("bipolar", true);
    addAndMakeVisible (*freq); addAndMakeVisible (*gain); addAndMakeVisible (*q);
    selectBand (band);
}

void EqPanel::selectBand (int b)
{
    band = b;
    for (int i = 0; i < NumEqBands; ++i) bandButtons[i].setToggleState (i == b, juce::dontSendNotification);
    onAtt.reset();
    onAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, eqId (b, "on"), onToggle);
    freq->attach (proc.apvts, eqId (b, "f"));
    gain->attach (proc.apvts, eqId (b, "g"));
    q->attach (proc.apvts, eqId (b, "q"));
    gain->setEnabled (b != EqLowCut && b != EqHighCut);
    repaint();
}

void EqPanel::setP (const juce::String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

float EqPanel::xForFreq (float f) const
{
    return (float) graph.getX() + (float) graph.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
}
float EqPanel::freqForX (float x) const
{
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - (float) graph.getX()) / (float) graph.getWidth()));
}
float EqPanel::yForDb (float db) const { return (float) graph.getCentreY() - db / 15.0f * (float) graph.getHeight() * 0.5f; }
float EqPanel::dbForY (float y) const { return ((float) graph.getCentreY() - y) / ((float) graph.getHeight() * 0.5f) * 15.0f; }

juce::Point<float> EqPanel::nodePos (int b) const
{
    const float f = pv (eqId (b, "f"));
    const bool cut = b == EqLowCut || b == EqHighCut;
    return { xForFreq (f), yForDb (cut ? 0.0f : pv (eqId (b, "g"))) };
}

void EqPanel::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (20, 14);
    g.setColour (col::amber);
    g.setFont (font (24.0f, true));
    g.drawText ("EQUALIZER", r.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (font (13.0f));
    g.drawText ("Drag a node to move it, scroll to change its width, double-click to switch it on or off.",
                r.removeFromTop (22), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff141210));
    g.fillRoundedRectangle (graph.toFloat().expanded (4), 6.0f);

    // grid
    g.setFont (font (10.0f));
    for (float f : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        const float x = xForFreq (f);
        g.setColour (col::line);
        g.drawVerticalLine ((int) x, (float) graph.getY(), (float) graph.getBottom());
        g.setColour (col::muted);
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) x - 20, graph.getBottom() - 14, 40, 12, juce::Justification::centred);
    }
    for (float db : { -12.f, -6.f, 0.f, 6.f, 12.f })
    {
        const float y = yForDb (db);
        g.setColour (db == 0 ? col::line.brighter (0.3f) : col::line);
        g.drawHorizontalLine ((int) y, (float) graph.getX(), (float) graph.getRight());
        g.setColour (col::muted);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), graph.getX() + 4, (int) y - 12, 30, 12, juce::Justification::left);
    }

    // spectrum (output)
    if (! spectrum.empty())
    {
        const double sr = proc.getSampleRateSafe();
        const int bins = (int) spectrum.size();
        juce::Path sp;
        bool started = false;
        for (int x = graph.getX(); x <= graph.getRight(); x += 2)
        {
            const float f = freqForX ((float) x);
            const int k = juce::jlimit (1, bins - 1, (int) (f / (sr / 2.0) * (bins - 1)));
            const float db = spectrum[(size_t) k] + 4.5f * std::log2 (f / 1000.0f);   // tilt so a typical mix looks flat
            const float y = juce::jmap (juce::jlimit (-90.0f, 0.0f, db), -90.0f, 0.0f, (float) graph.getBottom(), (float) graph.getY());
            if (! started) { sp.startNewSubPath ((float) x, y); started = true; } else sp.lineTo ((float) x, y);
        }
        juce::Path fill (sp);
        fill.lineTo ((float) graph.getRight(), (float) graph.getBottom());
        fill.lineTo ((float) graph.getX(), (float) graph.getBottom());
        fill.closeSubPath();
        g.setColour (col::muted.withAlpha (0.22f));
        g.fillPath (fill);
        g.setColour (col::muted.withAlpha (0.6f));
        g.strokePath (sp, juce::PathStrokeType (1.0f));
    }

    // EQ curve
    Equalizer e;
    e.sr = proc.getSampleRateSafe();
    e.steepCuts = pv ("eq_steep") > 0.5f;
    for (int b = 0; b < NumEqBands; ++b)
        e.bands[b] = { pv (eqId (b, "on")) > 0.5f, pv (eqId (b, "f")), pv (eqId (b, "g")), pv (eqId (b, "q")) };
    juce::Path curve;
    for (int x = graph.getX(); x <= graph.getRight(); x += 2)
    {
        const float y = yForDb ((float) juce::jlimit (-15.0, 15.0, e.responseDb (freqForX ((float) x))));
        if (x == graph.getX()) curve.startNewSubPath ((float) x, y); else curve.lineTo ((float) x, y);
    }
    const bool modOn = pv (modOnId (MEq)) > 0.5f;
    g.setColour (modOn ? col::amber : col::muted);
    g.strokePath (curve, juce::PathStrokeType (2.5f));

    // nodes
    for (int b = 0; b < NumEqBands; ++b)
    {
        const auto p = nodePos (b);
        const bool on = pv (eqId (b, "on")) > 0.5f;
        const float rr = b == band ? 9.0f : 7.0f;
        g.setColour (on ? (b == band ? col::amber : col::cream) : col::panel2);
        g.fillEllipse (p.x - rr, p.y - rr, rr * 2, rr * 2);
        g.setColour (b == band ? col::cream : col::line.brighter (0.4f));
        g.drawEllipse (p.x - rr, p.y - rr, rr * 2, rr * 2, 1.5f);
        g.setColour (on ? col::bg : col::muted);
        g.setFont (font (10.0f, true));
        const char* tag[] = { "LC", "LS", "1", "2", "3", "4", "HS", "HC" };
        g.drawText (tag[b], juce::Rectangle<float> (rr * 2, rr * 2).withCentre (p), juce::Justification::centred);
    }

    if (! modOn)
    {
        g.setColour (col::muted);
        g.setFont (font (13.0f, true));
        g.drawText ("Equalizer is off - click its dot in the chain to turn it on", graph.reduced (10), juce::Justification::topRight);
    }
}

void EqPanel::resized()
{
    auto r = getLocalBounds().reduced (20, 14);
    r.removeFromTop (56);
    graph = r.removeFromTop (juce::jmax (200, r.getHeight() - 200)).reduced (4);
    r.removeFromTop (12);
    auto row = r.removeFromTop (30);
    const int bw = row.getWidth() / NumEqBands;
    for (int b = 0; b < NumEqBands; ++b) bandButtons[b].setBounds (row.removeFromLeft (bw).reduced (2, 0));
    r.removeFromTop (8);
    auto ctl = r;
    auto left = ctl.removeFromLeft (180);
    onToggle.setBounds (left.removeFromTop (30));
    steepToggle.setBounds (left.removeFromTop (30));
    freq->setBounds (ctl.removeFromLeft (120));
    gain->setBounds (ctl.removeFromLeft (120));
    q->setBounds (ctl.removeFromLeft (120));
}

void EqPanel::mouseDown (const juce::MouseEvent& e)
{
    dragBand = -1;
    float best = 14.0f;
    for (int b = 0; b < NumEqBands; ++b)
    {
        const float d = nodePos (b).getDistanceFrom (e.position);
        if (d < best) { best = d; dragBand = b; }
    }
    if (dragBand >= 0)
    {
        selectBand (dragBand);
        for (auto* id : { "f", "g" })
            if (auto* p = proc.apvts.getParameter (eqId (dragBand, id))) p->beginChangeGesture();
        if (pv (eqId (dragBand, "on")) < 0.5f) setP (eqId (dragBand, "on"), 1);
    }
}

void EqPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;
    setP (eqId (dragBand, "f"), freqForX (e.position.x));
    if (dragBand != EqLowCut && dragBand != EqHighCut)
        setP (eqId (dragBand, "g"), juce::jlimit (-12.0f, 12.0f, dbForY (e.position.y)));
    repaint();
}

void EqPanel::mouseUp (const juce::MouseEvent&)
{
    if (dragBand >= 0)
        for (auto* id : { "f", "g" })
            if (auto* p = proc.apvts.getParameter (eqId (dragBand, id))) p->endChangeGesture();
    dragBand = -1;
}

void EqPanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (int b = 0; b < NumEqBands; ++b)
        if (nodePos (b).getDistanceFrom (e.position) < 14.0f)
        {
            setP (eqId (b, "on"), pv (eqId (b, "on")) > 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
}

void EqPanel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float qv = pv (eqId (band, "q"));
    setP (eqId (band, "q"), juce::jlimit (0.2f, 8.0f, qv * std::pow (1.25f, w.deltaY * 4.0f)));
    repaint();
}

// ===========================================================================
// Meters
// ===========================================================================
Meters::Meters (LatheProcessor& p, juce::Component* popupParent) : proc (p)
{
    resetButton.onClick = [this] { proc.resetLoudness = true; tpHold = 0; };
    addAndMakeVisible (resetButton);
    inGain = std::make_unique<ttui::Knob> ("IN", true, popupParent);
    outGain = std::make_unique<ttui::Knob> ("OUT", true, popupParent);
    inGain->attach (p.apvts, "in_gain");
    outGain->attach (p.apvts, "out_gain");
    inGain->slider.getProperties().set ("bipolar", true);
    outGain->slider.getProperties().set ("bipolar", true);
    addAndMakeVisible (*inGain); addAndMakeVisible (*outGain);
    dither.addItemList ({ "Dither off", "Dither 16-bit", "Dither 24-bit" }, 1);
    ditherAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "dither", dither);
    addAndMakeVisible (dither);
    recordCut.addItemList ({ "Record cut: off", "60s mono single", "Worn vinyl", "Cassette", "45 rpm (a 33 sped up)" }, 1);
    recordCutAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "record_cut", recordCut);
    recordCut.setTooltip ("Hear the master as a 60s mono single, a worn record, a cassette, or a 33 played at 45. Turn it off before you bounce, or leave it on for the effect.");
    addAndMakeVisible (recordCut);
}

void Meters::tick()
{
    inPk = juce::jmax (proc.inPeak.load(), inPk * 0.92f);
    outPk = juce::jmax (proc.outPeak.load(), outPk * 0.92f);
    tpHold = juce::jmax (tpHold, proc.outTruePeak.load());
    repaint();
}

void Meters::paint (juce::Graphics& g)
{
    g.fillAll (col::panel);
    auto r = getLocalBounds().reduced (12, 10);
    g.setColour (col::muted);
    g.setFont (font (11.0f, true));
    g.drawText ("LEVELS", r.removeFromTop (16), juce::Justification::centredLeft);

    auto bars = r.removeFromTop (200);
    auto drawBar = [&] (juce::Rectangle<int> a, float pk, const char* name)
    {
        g.setColour (col::muted);
        g.setFont (font (10.0f, true));
        g.drawText (name, a.removeFromBottom (14), juce::Justification::centred);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (a.toFloat(), 3.0f);
        const float db = gainToDb (pk);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto f = a.toFloat().withTop ((float) a.getBottom() - frac * (float) a.getHeight());
        g.setColour (db > -0.3f ? col::red : col::amber);
        g.fillRoundedRectangle (f, 3.0f);
    };
    auto b1 = bars.removeFromLeft (bars.getWidth() / 2).reduced (12, 0);
    drawBar (b1, inPk, "IN");
    drawBar (bars.reduced (12, 0), outPk, "OUT");

    r.removeFromTop (12);
    auto readout = [&] (const juce::String& label, float v, const juce::String& unit, bool big)
    {
        auto row = r.removeFromTop (big ? 44 : 30);
        g.setColour (col::muted);
        g.setFont (font (11.0f, true));
        g.drawText (label, row.removeFromTop (14), juce::Justification::centredLeft);
        g.setColour (big ? col::amber : col::cream);
        g.setFont (font (big ? 24.0f : 15.0f, true));
        g.drawText (v <= -69.9f ? juce::String ("--") : juce::String (v, 1) + " " + unit, row, juce::Justification::centredLeft);
    };
    readout ("INTEGRATED", proc.outLoud.integrated.load(), "LUFS", true);
    readout ("SHORT-TERM", proc.outLoud.shortTerm.load(), "LUFS", false);
    readout ("MOMENTARY", proc.outLoud.momentary.load(), "LUFS", false);
    readout ("TRUE PEAK (max)", gainToDb (tpHold), "dBTP", false);

    // per-section loudness (when a Bandleader runs the song)
    auto b = getLocalBounds().reduced (12, 10);
    b.removeFromTop (16 + 200 + 12 + 44 + 30 * 3 + 26 + 10 + 78 + 6 + 26 + 16);
    if (b.getHeight() < 60) return;
    g.setColour (col::muted); g.setFont (font (11.0f, true));
    g.drawText ("BY SECTION (Bandleader)", b.removeFromTop (16), juce::Justification::centredLeft);
    const char* names[3] = { "Verse", "Chorus", "Bridge" };
    const float verse = proc.uiSecLufs[0].load();
    for (int i = 0; i < 3; ++i)
    {
        auto row = b.removeFromTop (18);
        const float v = proc.uiSecLufs[i].load();
        g.setColour (col::muted); g.setFont (font (12.0f));
        g.drawText (names[i], row.removeFromLeft (60), juce::Justification::centredLeft);
        g.setColour (col::cream);
        juce::String t = v <= -69.9f ? juce::String ("--") : juce::String (v, 1) + " LUFS";
        if (i > 0 && v > -69.9f && verse > -69.9f) t << "  (" << (v >= verse ? "+" : "") << juce::String (v - verse, 1) << ")";
        g.drawText (t, row, juce::Justification::centredLeft);
    }
    if (! proc.uiLinked.load() && verse <= -69.9f)
    {
        g.setColour (col::muted); g.setFont (font (10.0f));
        g.drawFittedText ("Add Bandleader to the set to see each section's loudness.", b.removeFromTop (28), juce::Justification::topLeft, 2);
    }
}

void Meters::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    r.removeFromTop (16 + 200 + 12 + 44 + 30 * 3);
    resetButton.setBounds (r.removeFromTop (26));
    r.removeFromTop (10);
    auto knobs = r.removeFromTop (78);
    inGain->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
    outGain->setBounds (knobs);
    r.removeFromTop (6);
    dither.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    recordCut.setBounds (r.removeFromTop (26));
}

// ===========================================================================
// Editor
// ===========================================================================
LatheEditor::LatheEditor (LatheProcessor& p)
    : AudioProcessorEditor (&p), proc (p), chain (p), meters (p, this)
{
    setLookAndFeel (&lnf);

    for (int i = 0; i < p.getNumPrograms(); ++i) presetBox.addItem (p.getProgramName (i), i + 1);
    presetBox.setSelectedId (p.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); repaint(); };
    addAndMakeVisible (presetBox);

    for (int t = 0; t < 3; ++t) targetBox.addItem (targetName (t), t + 1);
    targetBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (targetBox);
    assistButton.onClick = [this] { proc.startAssistant (targetBox.getSelectedId() - 1); };
    addAndMakeVisible (assistButton);

    chain.onSelect = [this] (int m) { select (m); };
    addAndMakeVisible (chain);
    addAndMakeVisible (meters);

    eqPanel = std::make_unique<EqPanel> (p, this);
    addChildComponent (*eqPanel);

    using K = Ctl;
    const juce::String bn[3] = { "Low", "Mid", "High" };
    panels[MDynEq] = std::make_unique<GenericPanel> (p, "Dynamic EQ",
        "Bells that only cut when their band gets too loud - tame boomy notes or harsh peaks without dulling the whole mix.",
        std::vector<Group> {
            { "Band 1", { { K::Toggle, deId (0, "on"), "On" }, { K::Knob, deId (0, "f"), "FREQ" }, { K::Knob, deId (0, "q"), "Q" },
                          { K::Knob, deId (0, "thr"), "THRESHOLD" }, { K::Knob, deId (0, "range"), "MAX CUT" } } },
            { "Band 2", { { K::Toggle, deId (1, "on"), "On" }, { K::Knob, deId (1, "f"), "FREQ" }, { K::Knob, deId (1, "q"), "Q" },
                          { K::Knob, deId (1, "thr"), "THRESHOLD" }, { K::Knob, deId (1, "range"), "MAX CUT" } } } }, 0, this);
    panels[MDynEq]->groupSuffix = [this] (int g) { return "cutting " + juce::String (proc.dynEqCut[g].load(), 1) + " dB"; };

    panels[MTape] = std::make_unique<GenericPanel> (p, "Vintage Tape",
        "Run the mix through a tape machine: soft saturation, a low-end head bump and a gently rounded top.",
        std::vector<Group> { { "Tape machine", { { K::Combo, "tp_speed", "Speed" }, { K::Knob, "tp_drive", "DRIVE" }, { K::Knob, "tp_bump", "LOW BUMP" },
                                                  { K::Knob, "tp_warmth", "WARMTH" }, { K::Knob, "tp_mix", "MIX" } } } }, 0, this);

    std::vector<Group> dg { { "Crossovers", { { K::Knob, "dy_x1", "LOW / MID" }, { K::Knob, "dy_x2", "MID / HIGH" } } } };
    for (int b = 0; b < 3; ++b)
        dg.push_back ({ bn[b], { { K::Knob, dyId (b, "thr"), "THRESHOLD" }, { K::Knob, dyId (b, "ratio"), "RATIO" }, { K::Knob, dyId (b, "att"), "ATTACK" },
                                 { K::Knob, dyId (b, "rel"), "RELEASE" }, { K::Knob, dyId (b, "gain"), "GAIN" } } });
    panels[MDyn] = std::make_unique<GenericPanel> (p, "Dynamics",
        "Three-band compressor: control the low end, the body and the top separately.", dg, 0, this);
    panels[MDyn]->groupSuffix = [this] (int g) { return g == 0 ? juce::String() : "GR " + juce::String (proc.dynGr[g - 1].load(), 1) + " dB"; };

    std::vector<Group> eg { { "Crossovers", { { K::Knob, "ex_x1", "LOW / MID" }, { K::Knob, "ex_x2", "MID / HIGH" }, { K::Knob, "ex_mix", "MIX" } } } };
    for (int b = 0; b < 3; ++b) eg.push_back ({ bn[b], { { K::Combo, exId (b, "mode"), "Mode" }, { K::Knob, exId (b, "amt"), "AMOUNT" } } });
    panels[MExciter] = std::make_unique<GenericPanel> (p, "Exciter",
        "Adds harmonics band by band for presence and sparkle. Tape is smooth, Tube is warm and even, Warm is gentle, Retro is gritty.", eg, 0, this);

    std::vector<Group> ig { { "Crossovers", { { K::Knob, "im_x1", "LOW / MID" }, { K::Knob, "im_x2", "MID / HIGH" }, { K::Knob, "im_stereoize", "STEREOIZE" } } } };
    for (int b = 0; b < 3; ++b) ig.push_back ({ bn[b], { { K::Knob, imId (b), "WIDTH" } } });
    panels[MImager] = std::make_unique<GenericPanel> (p, "Imager",
        "Width per band: keep the bass centred (low width 0-60%) and open up the top. Stereoize adds width to mono parts.", ig, 220, this);
    panels[MImager]->paintExtra = [this] (juce::Graphics& g, juce::Rectangle<int> a)
    {
        auto sq = a.reduced (14).withSizeKeepingCentre (juce::jmin (a.getWidth(), a.getHeight()) - 28, juce::jmin (a.getWidth(), a.getHeight()) - 28);
        const auto c = sq.getCentre().toFloat();
        const float rad = (float) sq.getWidth() * 0.5f;
        g.setColour (col::line);
        g.drawEllipse (sq.toFloat(), 1.0f);
        g.drawLine (c.x, c.y - rad, c.x, c.y + rad, 1.0f);
        g.drawLine (c.x - rad, c.y, c.x + rad, c.y, 1.0f);
        g.setColour (col::amber.withAlpha (0.55f));
        for (size_t i = 0; i + 1 < scope.size(); i += 2)
        {
            const float l = scope[i], r = scope[i + 1];
            const float x = (l - r) * 0.7071f, y = (l + r) * 0.7071f;
            g.fillRect (c.x + juce::jlimit (-1.0f, 1.0f, x) * rad, c.y - juce::jlimit (-1.0f, 1.0f, y) * rad, 1.5f, 1.5f);
        }
        g.setColour (col::muted);
        g.setFont (font (11.0f, true));
        g.drawText ("STEREO SCOPE", a.reduced (10, 6), juce::Justification::topLeft);
    };

    panels[MMax] = std::make_unique<GenericPanel> (p, "Maximizer",
        "Raises loudness without clipping. Push GAIN until the integrated LUFS reads your target; the ceiling keeps peaks safe for streaming.",
        std::vector<Group> { { "Maximizer", { { K::Toggle, "mx_tp", "True peak" }, { K::Knob, "mx_gain", "GAIN" }, { K::Knob, "mx_ceil", "CEILING" },
                                              { K::Knob, "mx_rel", "RELEASE" } } } }, 330, this);
    panels[MMax]->paintExtra = [this] (juce::Graphics& g, juce::Rectangle<int> a)
    {
        auto r = a.reduced (16, 12);
        g.setColour (col::muted);
        g.setFont (font (11.0f, true));
        g.drawText ("GAIN REDUCTION", r.removeFromTop (16), juce::Justification::centredLeft);
        auto bar = r.removeFromTop (26);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bar.toFloat(), 3.0f);
        const float gr = -proc.maxGr.load();
        g.setColour (col::amber);
        g.fillRoundedRectangle (bar.toFloat().withWidth ((float) bar.getWidth() * juce::jlimit (0.0f, 1.0f, gr / 12.0f)), 3.0f);
        g.setColour (col::cream);
        g.setFont (font (13.0f, true));
        g.drawText (juce::String (-gr, 1) + " dB", bar, juce::Justification::centred);
        r.removeFromTop (18);
        g.setColour (col::amber);
        g.setFont (font (30.0f, true));
        const float lu = proc.outLoud.integrated.load();
        g.drawText (lu <= -69.9f ? juce::String ("-- LUFS") : juce::String (lu, 1) + " LUFS", r.removeFromTop (40), juce::Justification::centredLeft);
        g.setColour (col::muted);
        g.setFont (font (12.0f));
        g.drawFittedText ("Targets: streaming services about -14 LUFS, rock and pop masters -11 to -9. "
                          "Keep the ceiling at -1 dB for streaming. Press Reset (right) after changes to re-measure.",
                          r.removeFromTop (70), juce::Justification::topLeft, 4, 1.0f);
    };

    for (auto& pnl : panels) if (pnl) addChildComponent (*pnl);

    fft = std::make_unique<juce::dsp::FFT> (12);
    ring.assign (4096, 0.0f);
    fftData.assign (8192, 0.0f);
    smooth.assign (2049, -100.0f);

    setSize (1180, 720);
    select (MEq);
    startTimerHz (30);
}

LatheEditor::~LatheEditor() { setLookAndFeel (nullptr); }

void LatheEditor::select (int m)
{
    chain.selected = m;
    eqPanel->setVisible (m == MEq);
    for (int i = 0; i < NumModules; ++i) if (panels[i]) panels[i]->setVisible (i == m);
    chain.repaint();
}

void LatheEditor::computeSpectrum()
{
    float tmp[2048];
    int got;
    while ((got = proc.spectrumFifo.pull (tmp, 2048)) > 0)
        for (int i = 0; i < got; ++i) { ring[(size_t) ringPos] = tmp[i]; ringPos = (ringPos + 1) % 4096; }
    for (int i = 0; i < 4096; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / 4095.0f);
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % 4096)] * w;
    }
    std::fill (fftData.begin() + 4096, fftData.end(), 0.0f);
    fft->performFrequencyOnlyForwardTransform (fftData.data());
    for (int k = 0; k <= 2048; ++k)
    {
        const float db = gainToDb (fftData[(size_t) k] / 1024.0f);
        smooth[(size_t) k] = db > smooth[(size_t) k] ? db : smooth[(size_t) k] * 0.85f + db * 0.15f;
    }
    eqPanel->setSpectrum (smooth);

    // scope points
    float st[1024];
    scope.clear();
    while ((got = proc.scopeFifo.pull (st, 1024)) > 0)
        for (int i = 0; i < got; ++i) scope.push_back (st[i]);
    if (scope.size() > 2048) scope.erase (scope.begin(), scope.end() - 2048);
}

void LatheEditor::timerCallback()
{
    proc.assistantTick();
    computeSpectrum();
    meters.tick();
    chain.repaint();
    if (eqPanel->isVisible()) eqPanel->repaint();
    for (auto& pnl : panels) if (pnl && pnl->isVisible()) pnl->repaint();
    repaint (statusArea);
    assistButton.setEnabled (proc.assistPhase.load() == 0 || proc.assistPhase.load() == 3);
}

void LatheEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto header = getLocalBounds().removeFromTop (56);
    g.setColour (col::panel);
    g.fillRect (header);
    g.setColour (col::red);
    g.fillRect (header.removeFromBottom (2));
    g.setColour (col::amber);
    g.setFont (font (26.0f, true));
    g.drawText ("LATHE", 20, 8, 200, 28, juce::Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (font (11.0f));
    g.drawText ("MASTERING SUITE", 21, 34, 200, 14, juce::Justification::centredLeft);

    const int ph = proc.assistPhase.load();
    if (ph != 0)
    {
        auto a = statusArea;
        g.setColour (col::cream);
        g.setFont (font (12.0f));
        g.drawText (proc.assistMessage, a.removeFromTop (16), juce::Justification::centredLeft);
        auto bar = a.removeFromTop (6).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (col::amber);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.assistProgress.load()), 3.0f);
    }
}

void LatheEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (56).reduced (16, 12);
    presetBox.setBounds (header.removeFromRight (220));
    header.removeFromRight (20);
    header.removeFromLeft (180);
    assistButton.setBounds (header.removeFromLeft (120));
    header.removeFromLeft (8);
    targetBox.setBounds (header.removeFromLeft (190));
    header.removeFromLeft (12);
    statusArea = header.withTrimmedTop (4);

    chain.setBounds (r.removeFromLeft (210));
    meters.setBounds (r.removeFromRight (200));
    eqPanel->setBounds (r);
    for (auto& pnl : panels) if (pnl) pnl->setBounds (r);
}
