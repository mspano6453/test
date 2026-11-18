#include "PluginEditor.h"

using namespace ttui;
using namespace tt;
using ttui::font;

namespace
{
const int kNormalNote[NumPieces] = { 36, 38, 48, 43, 42, 51, 49, 54 };
const int kAltNote[NumPieces]    = { 36, 37, 48, 43, 46, 53, 49, 39 };

void audition (TeaTowelProcessor& p, int piece, bool alt, float vel)
{
    const int note = alt ? kAltNote[piece] : kNormalNote[piece];
    p.keyboardState.noteOn (10, note, vel);
    p.keyboardState.noteOff (10, note, 0.0f);
}
} // namespace

// ===========================================================================
// KIT view - the drum kit you can click, like AD2's kit page
// ===========================================================================
KitView::KitView (TeaTowelProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

float KitView::damp (int p) const { return proc.apvts.getRawParameterValue (pid (p, Damp))->load(); }

void KitView::resized()
{
    const float W = (float) getWidth(), H = (float) getHeight();
    auto R = [&] (float cx, float cy, float w, float h)
    { return juce::Rectangle<float> (w * W, h * H).withCentre ({ cx * W, cy * H }); };

    floorY = H * 0.93f;
    kickR = H * 0.26f;
    kickCentre = { W * 0.50f, H * 0.68f };
    box[Kick]     = juce::Rectangle<float> (kickR * 2, kickR * 2).withCentre (kickCentre);
    box[Crash]    = R (0.22f, 0.17f, 0.28f, 0.14f);
    box[Ride]     = R (0.79f, 0.22f, 0.30f, 0.14f);
    box[HiHat]    = R (0.085f, 0.40f, 0.15f, 0.16f);
    box[RackTom]  = R (0.465f, 0.31f, 0.15f, 0.19f);
    box[FloorTom] = R (0.765f, 0.63f, 0.17f, 0.32f);
    box[Snare]    = R (0.255f, 0.57f, 0.17f, 0.16f);
    box[Perc]     = R (0.93f, 0.80f, 0.11f, 0.17f);
}

int KitView::pieceAt (juce::Point<float> pt) const
{
    static const int order[] = { Perc, RackTom, Snare, HiHat, FloorTom, Crash, Ride };
    for (int p : order)
        if (box[p].contains (pt)) return p;
    if (pt.getDistanceFrom (kickCentre) < kickR) return Kick;
    return -1;
}

void KitView::mouseDown (const juce::MouseEvent& e)
{
    const int p = pieceAt (e.position);
    if (p < 0) return;
    selected = p;
    if (onSelect) onSelect (p);
    const bool alt = e.mods.isPopupMenu() || e.mods.isAltDown();
    const float vel = e.mods.isShiftDown() ? 0.45f : 0.85f;
    audition (proc, p, alt, vel);
    repaint();
}

void KitView::mouseMove (const juce::MouseEvent& e)
{
    const int h = pieceAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void KitView::fileDragMove (const juce::StringArray&, int x, int y)
{
    const int t = pieceAt ({ (float) x, (float) y });
    if (t != dropTarget) { dropTarget = t; repaint(); }
}

void KitView::filesDropped (const juce::StringArray& files, int x, int y)
{
    dropTarget = -1;
    const int p = pieceAt ({ (float) x, (float) y });
    if (p >= 0 && files.size() > 0 && proc.loadSample (p, juce::File (files[0])))
    {
        selected = p;
        if (onSelect) onSelect (p);
        if (onSampleChanged) onSampleChanged();
    }
    repaint();
}

void KitView::timerCallback()
{
    bool any = false;
    for (int p = 0; p < NumPieces; ++p)
    {
        const float hit = proc.hitFlash[(size_t) p].exchange (0.0f);
        const float nf = juce::jmax (flash[p] * 0.82f, hit);
        if (nf > 0.01f || flash[p] > 0.01f) any = true;
        flash[p] = nf < 0.01f ? 0.0f : nf;
    }
    if (any) repaint();
}

void KitView::drawCymbal (juce::Graphics& g, juce::Rectangle<float> b, float relH, float standBottom)
{
    const auto c = b.getCentre();
    const float w = b.getWidth() * 0.92f, h = b.getHeight() * relH;

    // stand
    g.setColour (juce::Colour (0xff8d9096));
    g.drawLine (c.x, c.y, c.x, standBottom, 2.5f);
    g.drawLine (c.x, standBottom - 18.0f, c.x - 16.0f, standBottom, 2.0f);
    g.drawLine (c.x, standBottom - 18.0f, c.x + 16.0f, standBottom, 2.0f);

    const auto e = juce::Rectangle<float> (w, h).withCentre (c);
    juce::ColourGradient grad (juce::Colour (0xffe1bd6a), c.x - w * 0.15f, c.y - h * 0.4f,
                               juce::Colour (0xff6e4f1e), c.x + w * 0.5f, c.y + h * 0.5f, true);
    grad.addColour (0.5, juce::Colour (0xffb48a3f));
    g.setGradientFill (grad);
    g.fillEllipse (e);
    g.setColour (juce::Colour (0x30000000));
    for (float k = 0.35f; k < 1.0f; k += 0.2f)
        g.drawEllipse (juce::Rectangle<float> (w * k, h * k).withCentre (c), 0.8f);
    g.setColour (juce::Colour (0xffd8b46a));
    g.fillEllipse (juce::Rectangle<float> (w * 0.18f, h * 0.55f).withCentre (c.translated (0, -h * 0.12f)));
    g.setColour (juce::Colour (0x66000000));
    g.drawEllipse (e, 1.0f);
}

void KitView::drawDrum (juce::Graphics& g, juce::Rectangle<float> shell, float headH, float towel, bool snare)
{
    // Black oyster pearl shell
    juce::ColourGradient pearl (juce::Colour (0xff35353c), shell.getX(), 0,
                                juce::Colour (0xff2a2a30), shell.getRight(), 0, false);
    pearl.addColour (0.25, juce::Colour (0xff74747f));
    pearl.addColour (0.5, juce::Colour (0xff3e3e46));
    pearl.addColour (0.78, juce::Colour (0xff65656f));
    g.setGradientFill (pearl);
    juce::Path body;
    body.startNewSubPath (shell.getX(), shell.getY());
    body.lineTo (shell.getX(), shell.getBottom());
    body.addCentredArc (shell.getCentreX(), shell.getBottom(), shell.getWidth() * 0.5f, headH * 0.5f,
                        0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
    body.lineTo (shell.getRight(), shell.getY());
    body.closeSubPath();
    g.fillPath (body);

    // pearl swirl
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 0; i < 5; ++i)
    {
        const float fx = shell.getX() + shell.getWidth() * (0.1f + 0.18f * (float) i);
        g.drawLine (fx, shell.getY() + 4, fx + shell.getWidth() * 0.1f, shell.getBottom() - 2, 3.0f);
    }

    // lugs
    g.setColour (juce::Colour (0xffc4c8cc));
    const int lugs = 4;
    for (int i = 0; i < lugs; ++i)
    {
        const float lx = shell.getX() + shell.getWidth() * (0.14f + 0.72f * (float) i / (lugs - 1));
        g.fillRoundedRectangle (lx - 2.5f, shell.getY() + shell.getHeight() * 0.3f, 5.0f,
                                shell.getHeight() * (snare ? 0.4f : 0.3f), 2.0f);
    }

    // hoops
    g.setColour (juce::Colour (0xffd2d5d9));
    g.drawEllipse (shell.getX(), shell.getBottom() - headH * 0.5f, shell.getWidth(), headH, 2.5f);

    // batter head
    const auto head = juce::Rectangle<float> (shell.getX(), shell.getY() - headH * 0.5f, shell.getWidth(), headH);
    g.setColour (juce::Colour (0xffe8e1cf));
    g.fillEllipse (head);
    g.setColour (juce::Colour (0xffd2d5d9));
    g.drawEllipse (head, 2.5f);

    // the tea towel
    if (towel > 0.3f)
    {
        const float a = juce::jmap (towel, 0.3f, 1.0f, 0.35f, 1.0f);
        const float tw = shell.getWidth() * (0.35f + 0.3f * towel);
        const float tx = head.getCentreX() - tw * 0.55f;
        const float ty = head.getCentreY() - headH * 0.15f;
        const float th = headH * 0.6f + shell.getHeight() * 0.35f;
        juce::Path t;
        t.addRoundedRectangle (tx, ty, tw, th, 3.0f);
        g.setColour (juce::Colour (0xfff1ece0).withAlpha (a));
        g.fillPath (t);
        g.setColour (col::red.withAlpha (a));
        g.fillRect (tx, ty + th * 0.68f, tw, 2.5f);
        g.fillRect (tx, ty + th * 0.80f, tw, 2.5f);
        g.setColour (juce::Colours::black.withAlpha (0.15f * a));
        g.drawLine (tx, ty + headH * 0.45f, tx + tw, ty + headH * 0.45f, 1.0f);
    }
}

void KitView::drawKick (juce::Graphics& g, juce::Point<float> c, float r, float towel)
{
    // spurs
    g.setColour (juce::Colour (0xff8d9096));
    g.drawLine (c.x - r * 0.75f, c.y + r * 0.55f, c.x - r * 1.05f, floorY, 2.5f);
    g.drawLine (c.x + r * 0.75f, c.y + r * 0.55f, c.x + r * 1.05f, floorY, 2.5f);

    // shell edge + hoop
    juce::ColourGradient pearl (juce::Colour (0xff4c4c55), c.x - r, c.y - r,
                                juce::Colour (0xff18181b), c.x + r, c.y + r, false);
    g.setGradientFill (pearl);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colour (0xffcfc7b3));
    g.drawEllipse (c.x - r * 0.95f, c.y - r * 0.95f, r * 1.9f, r * 1.9f, 3.0f);

    // lugs around the hoop
    g.setColour (juce::Colour (0xffc4c8cc));
    for (int i = 0; i < 10; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 10.0f;
        const auto p = c.getPointOnCircumference (r * 0.975f, a);
        g.fillEllipse (p.x - 3.0f, p.y - 3.0f, 6.0f, 6.0f);
    }

    // resonant head
    const float hr = r * 0.88f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffefe8d6), c.x, c.y - hr,
                                             juce::Colour (0xffd6ccb3), c.x, c.y + hr, false));
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);

    // front-head logo
    g.setColour (col::bg);
    g.setFont (font (hr * 0.26f, true));
    g.drawText ("TEA TOWEL", juce::Rectangle<float> (hr * 2, hr * 0.4f).withCentre (c.translated (0, -hr * 0.05f)),
                juce::Justification::centred);
    g.setColour (col::red);
    g.fillRect (juce::Rectangle<float> (hr * 1.1f, 2.0f).withCentre (c.translated (0, hr * 0.2f)));
    g.setColour (col::muted);
    g.setFont (font (hr * 0.1f));
    g.drawText ("DEAD DRUMS", juce::Rectangle<float> (hr * 2, hr * 0.2f).withCentre (c.translated (0, hr * 0.36f)),
                juce::Justification::centred);

    // blanket peeking out of the bottom when heavily damped
    if (towel > 0.5f)
    {
        g.setColour (juce::Colour (0xff6a5a48).withAlpha (juce::jmap (towel, 0.5f, 1.0f, 0.3f, 0.9f)));
        juce::Path bl;
        bl.addCentredArc (c.x, c.y + hr * 0.92f, hr * 0.45f, hr * 0.12f, 0.0f,
                          -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.fillPath (bl);
    }
}

void KitView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();

    // studio wall + floor
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2622), 0, 0, juce::Colour (0xff171512), 0, H, false));
    g.fillAll();
    g.setColour (juce::Colour (0x0affffff));
    for (float x = 0; x < W; x += 24.0f) g.drawVerticalLine ((int) x, 0, H * 0.72f);   // acoustic panels
    g.setColour (juce::Colour (0xff15130f));
    g.fillRect (0.0f, H * 0.80f, W, H * 0.2f);
    g.setColour (juce::Colour (0xff4a2e22));
    g.fillEllipse (W * 0.12f, H * 0.82f, W * 0.76f, H * 0.16f);   // rug

    // cymbals (behind)
    drawCymbal (g, box[Crash], 0.32f, floorY);
    drawCymbal (g, box[Ride], 0.32f, floorY);

    // hi-hat (two cymbals)
    {
        const auto b = box[HiHat];
        const auto c = b.getCentre();
        g.setColour (juce::Colour (0xff8d9096));
        g.drawLine (c.x, c.y - b.getHeight() * 0.2f, c.x, floorY, 2.5f);
        g.drawLine (c.x, floorY - 22.0f, c.x - 18.0f, floorY, 2.0f);
        g.drawLine (c.x, floorY - 22.0f, c.x + 18.0f, floorY, 2.0f);
        drawCymbal (g, b.translated (0, 5.0f), 0.24f, c.y + 5.0f);
        drawCymbal (g, b.translated (0, -4.0f), 0.24f, c.y - 4.0f);
    }

    // kick
    drawKick (g, kickCentre, kickR, damp (Kick));

    // floor tom with legs
    {
        const auto b = box[FloorTom];
        const float headH = b.getWidth() * 0.32f;
        const auto shell = juce::Rectangle<float> (b.getX(), b.getY() + headH * 0.5f, b.getWidth(), b.getHeight() * 0.62f);
        g.setColour (juce::Colour (0xff8d9096));
        for (float fx : { 0.15f, 0.5f, 0.85f })
            g.drawLine (shell.getX() + shell.getWidth() * fx, shell.getBottom(), shell.getX() + shell.getWidth() * (fx * 1.2f - 0.1f), floorY, 2.5f);
        drawDrum (g, shell, headH, damp (FloorTom), false);
    }

    // rack tom (mounted on the kick)
    {
        const auto b = box[RackTom];
        const float headH = b.getWidth() * 0.32f;
        const auto shell = juce::Rectangle<float> (b.getX(), b.getY() + headH * 0.5f, b.getWidth(), b.getHeight() * 0.55f);
        g.setColour (juce::Colour (0xffb0b4b9));
        g.drawLine (shell.getCentreX(), shell.getBottom(), kickCentre.x, kickCentre.y - kickR * 0.95f, 4.0f);
        drawDrum (g, shell, headH, damp (RackTom), false);
    }

    // snare on stand
    {
        const auto b = box[Snare];
        const float headH = b.getWidth() * 0.32f;
        const auto shell = juce::Rectangle<float> (b.getX(), b.getY() + headH * 0.5f, b.getWidth(), b.getHeight() * 0.38f);
        const float sx = shell.getCentreX();
        g.setColour (juce::Colour (0xff8d9096));
        g.drawLine (sx, shell.getBottom(), sx, floorY - 20.0f, 2.5f);
        g.drawLine (sx, floorY - 20.0f, sx - 18.0f, floorY, 2.0f);
        g.drawLine (sx, floorY - 20.0f, sx + 18.0f, floorY, 2.0f);
        drawDrum (g, shell, headH, damp (Snare), true);
    }

    // percussion: a tambourine and a shaker on a little table
    {
        const auto b = box[Perc];
        g.setColour (juce::Colour (0xff3a2a20));
        g.fillRoundedRectangle (b.getX(), b.getBottom() - b.getHeight() * 0.22f, b.getWidth(), b.getHeight() * 0.12f, 2.0f);
        g.setColour (juce::Colour (0xff8d9096));
        g.drawLine (b.getCentreX(), b.getBottom() - b.getHeight() * 0.1f, b.getCentreX(), floorY, 2.0f);
        const auto t = juce::Rectangle<float> (b.getWidth() * 0.62f, b.getWidth() * 0.62f).withCentre ({ b.getX() + b.getWidth() * 0.38f, b.getY() + b.getHeight() * 0.42f });
        g.setColour (juce::Colour (0xffd9c9a3)); g.fillEllipse (t.reduced (t.getWidth() * 0.16f));
        g.setColour (juce::Colour (0xff6b4a2e)); g.drawEllipse (t.reduced (t.getWidth() * 0.08f), t.getWidth() * 0.14f);
        g.setColour (juce::Colour (0xffc8ccd2));
        for (int k = 0; k < 6; ++k)
        {
            const float a = (float) k * juce::MathConstants<float>::twoPi / 6.0f;
            const auto c = t.getCentre() + juce::Point<float> (std::cos (a), std::sin (a)) * t.getWidth() * 0.42f;
            g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
        }
        g.setColour (juce::Colour (0xffb84a32));
        g.fillRoundedRectangle (b.getRight() - b.getWidth() * 0.26f, b.getY() + b.getHeight() * 0.25f, b.getWidth() * 0.16f, b.getHeight() * 0.42f, 6.0f);
    }

    // selection, hover, hit flash, labels
    for (int p = 0; p < NumPieces; ++p)
    {
        auto b = box[p];
        const bool isSel = p == selected, isHover = p == hover, isDrop = p == dropTarget;
        if (flash[p] > 0.0f)
        {
            g.setColour (col::amber.withAlpha (0.28f * flash[p]));
            if (p == Kick) g.fillEllipse (b); else g.fillRoundedRectangle (b, 8.0f);
        }
        if (isSel || isHover || isDrop)
        {
            g.setColour (isDrop ? col::red : isSel ? col::amber : col::cream.withAlpha (0.35f));
            if (p == Kick) g.drawEllipse (b.reduced (1.0f), isSel ? 2.0f : 1.0f);
            else           g.drawRoundedRectangle (b, 8.0f, isSel ? 2.0f : 1.0f);
        }
        if (isSel || isHover)
        {
            g.setFont (font (12.0f, true));
            g.setColour (isSel ? col::amber : col::cream);
            const auto lb = p == Kick ? juce::Rectangle<float> (b.getX(), b.getBottom() - 4.0f, b.getWidth(), 16.0f)
                                      : juce::Rectangle<float> (b.getX(), b.getY() - 16.0f, b.getWidth(), 16.0f);
            g.drawText (juce::String (pieceName (p)).toUpperCase()
                            + (proc.getSampleName (p).isNotEmpty() ? "  (sample)" : ""),
                        lb, juce::Justification::centred);
        }
    }

    g.setColour (col::muted);
    g.setFont (font (11.5f));
    g.drawText ("Click to play  -  right-click: side-stick / open hat / ride bell  -  shift: soft  -  drop a sample on any drum",
                getLocalBounds().removeFromBottom (18).reduced (10, 0), juce::Justification::centredLeft);
}

// ===========================================================================
// Mixer strip row under the kit
// ===========================================================================
MixerView::MixerView (TeaTowelProcessor& p, juce::Component* popupParent)
{
    for (int i = 0; i < NumPieces; ++i)
    {
        auto& s = strips[i];
        s.name = std::make_unique<juce::TextButton> (juce::String (pieceName (i)).toUpperCase());
        s.name->setClickingTogglesState (false);
        s.name->onClick = [this, i] { if (onSelect) onSelect (i); };
        addAndMakeVisible (*s.name);

        s.pan = std::make_unique<Knob> ("PAN", true, popupParent);
        s.pan->slider.getProperties().set ("bipolar", true);
        s.pan->attach (p.apvts, pid (i, Pan));
        addAndMakeVisible (*s.pan);

        s.room = std::make_unique<Knob> ("ROOM", true, popupParent);
        s.room->attach (p.apvts, pid (i, Room));
        addAndMakeVisible (*s.room);

        s.fader = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::NoTextBox);
        s.fader->setPopupDisplayEnabled (true, true, popupParent);
        s.faderAtt = std::make_unique<SliderAttachment> (p.apvts, pid (i, Vol), *s.fader);
        addAndMakeVisible (*s.fader);
    }
    setSelected (0);
}

void MixerView::setSelected (int p)
{
    selected = p;
    for (int i = 0; i < NumPieces; ++i) strips[i].name->setToggleState (i == p, juce::dontSendNotification);
    repaint();
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (col::panel);
    g.setColour (col::line);
    g.drawHorizontalLine (0, 0, (float) getWidth());
    const float w = (float) getWidth() / NumPieces;
    for (int i = 1; i < NumPieces; ++i) g.drawVerticalLine ((int) (w * (float) i), 6.0f, (float) getHeight() - 6.0f);
}

void MixerView::resized()
{
    const int w = getWidth() / NumPieces;
    for (int i = 0; i < NumPieces; ++i)
    {
        auto r = juce::Rectangle<int> (i * w, 0, w, getHeight()).reduced (8, 6);
        auto& s = strips[i];
        s.name->setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        auto knobs = r.removeFromTop (48);
        s.pan->setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
        s.room->setBounds (knobs);
        s.fader->setBounds (r.withSizeKeepingCentre (40, r.getHeight()));
    }
}

// ===========================================================================
// EDIT page - one drum at a time
// ===========================================================================
EditView::EditView (TeaTowelProcessor& p, juce::Component* popupParent) : proc (p)
{
    for (int i = 0; i < NumPieces; ++i)
    {
        pieceButtons[i] = std::make_unique<juce::TextButton> (juce::String (pieceName (i)).toUpperCase());
        pieceButtons[i]->onClick = [this, i] { if (onSelect) onSelect (i); };
        addAndMakeVisible (*pieceButtons[i]);
    }
    for (int k = 0; k < NumPieceParams; ++k)
    {
        knobs[k] = std::make_unique<Knob> (juce::String (pieceParamName (k)).toUpperCase(), false, popupParent);
        if (k == Tune || k == Pan) knobs[k]->slider.getProperties().set ("bipolar", true);
        addAndMakeVisible (*knobs[k]);
    }

    sampleLabel.setFont (font (14.0f, true));
    sampleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (sampleLabel);

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a sample for the " + juce::String (pieceName (piece)),
                                                       juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile() && ! proc.loadSample (piece, f))
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                              "Tea Towel", "Couldn't read that audio file.");
                                  refreshSample();
                              });
    };
    clearButton.onClick = [this] { proc.clearSample (piece); refreshSample(); };
    playButton.onClick = [this] { audition (proc, piece, false, 0.85f); };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (playButton);

    setPiece (0);
}

void EditView::setPiece (int p)
{
    piece = p;
    for (int i = 0; i < NumPieces; ++i) pieceButtons[i]->setToggleState (i == p, juce::dontSendNotification);
    for (int k = 0; k < NumPieceParams; ++k) knobs[k]->attach (proc.apvts, pid (p, k));
    refreshSample();
    repaint();
}

void EditView::refreshSample()
{
    const auto name = proc.getSampleName (piece);
    sampleLabel.setText (name.isNotEmpty() ? "Your sample: " + name : "Built-in: " + TeaTowelProcessor::builtInName (piece),
                         juce::dontSendNotification);
    sampleLabel.setColour (juce::Label::textColourId, name.isNotEmpty() ? col::amber : col::cream);
    clearButton.setEnabled (name.isNotEmpty());
}

void EditView::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.size() > 0) proc.loadSample (piece, juce::File (files[0]));
    refreshSample();
}

void EditView::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (24, 0);
    r.removeFromTop (52);

    g.setColour (col::amber);
    g.setFont (font (34.0f, true));
    g.drawText (juce::String (pieceName (piece)).toUpperCase(), r.removeFromTop (46), juce::Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (font (13.0f));
    g.drawText (pieceNotes (piece), r.removeFromTop (20), juce::Justification::centredLeft);

    // knob panel
    auto panel = getLocalBounds().reduced (24, 0).withTop (140).withHeight (200);
    g.setColour (col::panel);
    g.fillRoundedRectangle (panel.toFloat(), 8.0f);

    // sample panel
    auto sp = getLocalBounds().reduced (24, 0).withTop (356).withHeight (130);
    g.setColour (col::panel);
    g.fillRoundedRectangle (sp.toFloat(), 8.0f);
    g.setColour (col::muted);
    g.setFont (font (12.0f));
    g.drawFittedText ("Every drum is a real recording (5 velocity layers, AVL Drumkits by Glen MacArthur, CC BY-SA 3.0). "
                      "Load or drag in your own one-shot (WAV, AIFF, FLAC) to replace it. Towel = how much cloth is on the head: "
                      "the ring gets choked off sooner and the head sounds duller.",
                      sp.reduced (20, 12).withTrimmedTop (50), juce::Justification::topLeft, 4, 1.0f);
}

void EditView::resized()
{
    auto r = getLocalBounds().reduced (24, 0);
    auto top = r.removeFromTop (44).withTrimmedTop (10);
    const int bw = top.getWidth() / NumPieces;
    for (int i = 0; i < NumPieces; ++i) pieceButtons[i]->setBounds (top.removeFromLeft (bw).reduced (3, 0));

    auto panel = getLocalBounds().reduced (24, 0).withTop (140).withHeight (200).reduced (12, 18);
    const int kw = panel.getWidth() / NumPieceParams;
    for (int k = 0; k < NumPieceParams; ++k) knobs[k]->setBounds (panel.removeFromLeft (kw).reduced (6, 0));

    auto sp = getLocalBounds().reduced (24, 0).withTop (356).withHeight (130).reduced (16, 12);
    auto row = sp.removeFromTop (34);
    playButton.setBounds (row.removeFromRight (80));
    row.removeFromRight (8);
    clearButton.setBounds (row.removeFromRight (110));
    row.removeFromRight (8);
    loadButton.setBounds (row.removeFromRight (130));
    sampleLabel.setBounds (row);
}

// ===========================================================================
// FX page - the master bus
// ===========================================================================
FxView::FxView (TeaTowelProcessor& p, juce::Component* popupParent) : proc (p)
{
    for (int k = 0; k < NumMasterParams; ++k)
    {
        knobs[k] = std::make_unique<Knob> (juce::String (masterName (k)).toUpperCase(), false, popupParent);
        knobs[k]->attach (p.apvts, masterId (k));
        addAndMakeVisible (*knobs[k]);
    }
    for (int i = 0; i < NumKitTypes; ++i) kitTypeBox.addItem (kitTypeName (i), i + 1);
    kitTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "kit_type", kitTypeBox);
    addAndMakeVisible (kitTypeBox);
    startTimerHz (30);
}

void FxView::timerCallback()
{
    const float gr = -proc.gainReductionDb.load();
    grShown = juce::jmax (gr, grShown * 0.9f);
    repaint (meterArea);
}

void FxView::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (24, 20);

    auto drawGroup = [&] (juce::Rectangle<int> a, const juce::String& title, const juce::String& sub)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (a.toFloat(), 8.0f);
        g.setColour (col::amber);
        g.setFont (font (15.0f, true));
        g.drawText (title, a.reduced (16, 10).removeFromTop (20), juce::Justification::topLeft);
        g.setColour (col::muted);
        g.setFont (font (12.0f));
        g.drawFittedText (sub, a.reduced (16, 10).withTrimmedTop (22).removeFromTop (34), juce::Justification::topLeft, 2);
    };

    auto top = r.removeFromTop (250);
    const int gw = top.getWidth();
    drawGroup (top.removeFromLeft ((int) (gw * 0.56f)).reduced (0, 0).withTrimmedRight (8), "DRUM BUS",
               "Low-end bump, a valve-style leveling compressor, and tape saturation.");
    drawGroup (top.withTrimmedLeft (8), "ROOM", "A small, dead, carpeted space. Each drum's Room knob sends into it.");

    auto bottom = r.withTrimmedTop (16).withHeight (230);
    drawGroup (bottom.removeFromLeft ((int) (bottom.getWidth() * 0.36f)).withTrimmedRight (8), "OUTPUT", "Stereo width and final level.");
    drawGroup (bottom.withTrimmedLeft (8), "CHARACTER", "Kit sound, old-sampler crunch, vinyl, and an 80s gated reverb on snare and toms.");

    // squash meter
    g.setColour (col::muted);
    g.setFont (font (11.0f, true));
    g.drawText ("GR", meterArea.withHeight (14).translated (0, -16), juce::Justification::centred);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, grShown / 20.0f);
    g.setColour (col::amber);
    g.fillRoundedRectangle (meterArea.toFloat().withHeight ((float) meterArea.getHeight() * frac), 3.0f);
    g.setColour (col::cream);
    g.setFont (font (11.0f));
    g.drawText (juce::String (grShown, 1), meterArea.withY (meterArea.getBottom() + 2).withHeight (14),
                juce::Justification::centred);
}

void FxView::resized()
{
    auto r = getLocalBounds().reduced (24, 20);
    auto top = r.removeFromTop (250);
    const int gw = top.getWidth();
    auto bus = top.removeFromLeft ((int) (gw * 0.56f)).withTrimmedRight (8).reduced (16, 10).withTrimmedTop (60);
    auto room = top.withTrimmedLeft (8).reduced (16, 10).withTrimmedTop (60);

    meterArea = bus.removeFromRight (22).withTrimmedTop (16).withTrimmedBottom (20);
    bus.removeFromRight (10);
    const int bw = bus.getWidth() / 3;
    knobs[Thud]->setBounds (bus.removeFromLeft (bw).reduced (6, 0));
    knobs[Squash]->setBounds (bus.removeFromLeft (bw).reduced (6, 0));
    knobs[Tape]->setBounds (bus.reduced (6, 0));

    const int rw = room.getWidth() / 2;
    knobs[RoomLevel]->setBounds (room.removeFromLeft (rw).reduced (6, 0));
    knobs[RoomSize]->setBounds (room.reduced (6, 0));

    auto bottom = r.withTrimmedTop (16).withHeight (230);
    auto out = bottom.removeFromLeft ((int) (bottom.getWidth() * 0.36f)).withTrimmedRight (8).reduced (16, 10).withTrimmedTop (56);
    const int ow = out.getWidth() / 2;
    knobs[Width]->setBounds (out.removeFromLeft (ow).reduced (6, 0));
    knobs[Output]->setBounds (out.reduced (6, 0));
    auto ch = bottom.withTrimmedLeft (8).reduced (16, 10).withTrimmedTop (56);
    kitTypeBox.setBounds (ch.removeFromLeft (190).withSizeKeepingCentre (180, 26));
    const int cw = ch.getWidth() / 4;
    knobs[Crunch]->setBounds (ch.removeFromLeft (cw).reduced (6, 0));
    knobs[Vinyl]->setBounds (ch.removeFromLeft (cw).reduced (6, 0));
    knobs[GateVerb]->setBounds (ch.removeFromLeft (cw).reduced (6, 0));
    knobs[Smash]->setBounds (ch.reduced (6, 0));
}

// ===========================================================================
// PLAYER page - a drummer that follows Live's transport
// ===========================================================================
PlayerView::PlayerView (TeaTowelProcessor& p, juce::Component* popupParent) : proc (p)
{
    onButton.setColour (juce::ToggleButton::textColourId, col::cream);
    onButton.setColour (juce::ToggleButton::tickColourId, col::amber);
    onButton.setColour (juce::ToggleButton::tickDisabledColourId, col::muted);
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, PID::on, onButton);
    addAndMakeVisible (onButton);

    for (int i = 0; i < NumStyles; ++i) styleBox.addItem (styleName (i), i + 1);
    styleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, PID::style, styleBox);
    addAndMakeVisible (styleBox);

    fillBox.addItemList ({ "Off", "4 bars", "8 bars", "16 bars" }, 1);
    fillAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, PID::fillEvery, fillBox);
    addAndMakeVisible (fillBox);
    swellBox.addItemList ({ "Off", "Into choruses", "Into every section" }, 1);
    swellAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "swells", swellBox);
    addAndMakeVisible (swellBox);
    if (auto* lp = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter ("loop"))) loopBox.addItemList (lp->choices, 1);
    loopAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "loop", loopBox);
    addAndMakeVisible (loopBox);
    loopKitBox.addItemList ({ "Loop + kit", "Loop only" }, 1);
    loopKitAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "loop_kit", loopKitBox);
    addAndMakeVisible (loopKitBox);

    for (int i = 0; i < NumSections; ++i)
    {
        sectionButtons[i].setButtonText (juce::String (sectionName (i)).toUpperCase());
        sectionButtons[i].onClick = [this, i] { setSection (i); };
        addAndMakeVisible (sectionButtons[i]);
    }
    fillButton.onClick = [this] { proc.player.requestFill = true; };
    endingButton.onClick = [this] { proc.player.requestEnding = true; };
    addAndMakeVisible (fillButton);
    addAndMakeVisible (endingButton);

    intensity = std::make_unique<Knob> ("INTENSITY", false, popupParent);
    looseness = std::make_unique<Knob> ("LOOSENESS", false, popupParent);
    swing     = std::make_unique<Knob> ("SWING", false, popupParent);
    variation = std::make_unique<Knob> ("VARIATION", false, popupParent);
    variation->attach (p.apvts, PID::variation);
    addAndMakeVisible (*variation);
    intensity->attach (p.apvts, PID::intensity);
    looseness->attach (p.apvts, PID::looseness);
    swing->attach (p.apvts, PID::swing);
    addAndMakeVisible (*intensity);
    addAndMakeVisible (*looseness);
    addAndMakeVisible (*swing);

    startTimerHz (30);
}

void PlayerView::setSection (int s)
{
    if (auto* prm = proc.apvts.getParameter (PID::section))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) s));
        prm->endChangeGesture();
    }
}

void PlayerView::timerCallback()
{
    const bool running = proc.player.uiRunning.load();
    const int sec = running ? proc.player.uiSection.load()
                            : (int) proc.apvts.getRawParameterValue (PID::section)->load();
    for (int i = 0; i < NumSections; ++i) sectionButtons[i].setToggleState (i == sec, juce::dontSendNotification);
    fillButton.setToggleState (running && proc.player.uiFillComing.load(), juce::dontSendNotification);
    const int st = running ? proc.player.uiStep.load() : -1;
    if (st != shownStep || sec != shownSection)
    {
        shownStep = st; shownSection = sec;
        repaint (lightsArea.getUnion (statusArea));
    }
}

void PlayerView::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto r = getLocalBounds().reduced (24, 16);

    auto panel = [&] (juce::Rectangle<int> a, const juce::String& title)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (a.toFloat(), 8.0f);
        g.setColour (col::amber);
        g.setFont (font (15.0f, true));
        g.drawText (title, a.reduced (16, 10).removeFromTop (20), juce::Justification::topLeft);
    };
    panel ({ r.getX(), r.getY(), r.getWidth(), 120 }, "DRUMMER");
    panel ({ r.getX(), r.getY() + 136, r.getWidth(), 150 }, "SONG");
    panel ({ r.getX(), r.getY() + 302, r.getWidth(), 210 }, "FEEL");

    g.setColour (col::muted);
    g.setFont (font (12.0f));
    g.drawText ("Style", r.getX() + 16, r.getY() + 74, 60, 24, juce::Justification::centredLeft);
    g.drawText ("Auto fill", r.getX() + 16 + 470, r.getY() + 74, 80, 24, juce::Justification::centredLeft);
    g.drawText ("Backwards", r.getX() + 16 + 470, r.getY() + 40, 80, 24, juce::Justification::centredLeft);
    g.drawText ("Loop", r.getX() + 730, r.getY() + 40, 40, 24, juce::Justification::centredLeft);

    // step lights
    const bool running = proc.player.uiRunning.load();
    for (int i = 0; i < 16; ++i)
    {
        const float w = (float) lightsArea.getWidth() / 16.0f;
        auto c = juce::Rectangle<float> ((float) lightsArea.getX() + w * (float) i + 3.0f, (float) lightsArea.getY(), w - 6.0f, (float) lightsArea.getHeight());
        g.setColour (i == shownStep ? col::amber : (i % 4 == 0 ? col::line.brighter (0.25f) : col::line));
        g.fillRoundedRectangle (c, 3.0f);
    }

    g.setFont (font (13.0f, true));
    g.setColour (running ? col::cream : col::muted);
    juce::String status;
    if (proc.uiLinked.load())
    {
        if (proc.apvts.getRawParameterValue ("sit_out")->load() > 0.5f) status = "Sitting out this style (no drums). Pick another style in Bandleader to bring them back.";
        else status = running ? "Following Bandleader  -  " + juce::String (sectionName (proc.player.uiSection.load())) + "  -  bar " + juce::String (proc.player.uiBar.load())
                              : juce::String ("Following Bandleader - press play in Live.");
    }
    else if (! (bool) proc.apvts.getRawParameterValue (PID::on)->load())
        status = "Player is off - tick the box above, then press play in Live.";
    else if (! running)
        status = "Waiting for Live to play...";
    else if (proc.player.uiEnded.load())
        status = "Ended. Pick a section or press FILL to start again.";
    else
        status = juce::String (sectionName (proc.player.uiSection.load())) + "  -  bar " + juce::String (proc.player.uiBar.load())
                 + (proc.player.uiFillComing.load() ? "  -  fill coming" : "");
    g.drawText (status, statusArea, juce::Justification::centredLeft);

    g.setColour (col::muted);
    g.setFont (font (13.0f));
    g.drawFittedText ("Every hit is humanized. VARIATION sets how much the drummer improvises: ghost notes, extra kicks, open hats and made-up fills.\n\n"
                      "Section changes land on the next bar, with a fill leading in.\n\n"
                      "Script a whole song: automate \"Player Section\" in Live's Arrangement view.\n\n"
                      "Key switches from your keyboard: C3 verse, D3 chorus, E3 bridge, F3 fill, G3 ending.\n\n"
                      "Record what it plays: make a MIDI track, set MIDI From to this track / Tea Towel, arm and record.",
                      juce::Rectangle<int> (r.getX() + 500, r.getY() + 302 + 36, r.getWidth() - 516, 160), juce::Justification::topLeft, 12, 1.0f);
}

void PlayerView::resized()
{
    auto r = getLocalBounds().reduced (24, 16);
    auto top = juce::Rectangle<int> (r.getX() + 16, r.getY() + 38, r.getWidth() - 32, 28);
    onButton.setBounds (top.removeFromLeft (360));
    styleBox.setBounds (r.getX() + 16 + 64, r.getY() + 74, 300, 26);
    fillBox.setBounds (r.getX() + 16 + 470 + 80, r.getY() + 74, 140, 26);
    swellBox.setBounds (r.getX() + 16 + 470 + 80, r.getY() + 40, 140, 26);
    loopBox.setBounds (r.getX() + 770, r.getY() + 40, r.getWidth() - 786, 26);
    loopKitBox.setBounds (r.getX() + 770, r.getY() + 74, r.getWidth() - 786, 26);

    auto song = juce::Rectangle<int> (r.getX() + 16, r.getY() + 136 + 40, r.getWidth() - 32, 44);
    const int bw = (song.getWidth() - 5 * 10) / 5;
    for (int i = 0; i < NumSections; ++i) { sectionButtons[i].setBounds (song.removeFromLeft (bw)); song.removeFromLeft (10); }
    song.removeFromLeft (14);
    fillButton.setBounds (song.removeFromLeft (bw - 7)); song.removeFromLeft (10);
    endingButton.setBounds (song.removeFromLeft (bw - 7));

    lightsArea = juce::Rectangle<int> (r.getX() + 16, r.getY() + 136 + 96, r.getWidth() - 32, 14);
    statusArea = juce::Rectangle<int> (r.getX() + 16, r.getY() + 136 + 114, r.getWidth() - 32, 20);

    auto feel = juce::Rectangle<int> (r.getX() + 16, r.getY() + 302 + 36, 4 * 116, 120);
    intensity->setBounds (feel.removeFromLeft (116).reduced (6, 0));
    looseness->setBounds (feel.removeFromLeft (116).reduced (6, 0));
    swing->setBounds (feel.removeFromLeft (116).reduced (6, 0));
    variation->setBounds (feel.removeFromLeft (116).reduced (6, 0));
}

// ===========================================================================
// Editor shell: header with tabs + preset menu, like AD2's top bar
// ===========================================================================
TeaTowelEditor::TeaTowelEditor (TeaTowelProcessor& p)
    : AudioProcessorEditor (&p), proc (p), kit (p), mixer (p, this), edit (p, this), fx (p, this), player (p, this)
{
    setLookAndFeel (&lnf);

    for (auto* t : { &tabKit, &tabEdit, &tabFx, &tabPlayer })
    {
        t->setRadioGroupId (1001);
        t->setClickingTogglesState (true);
        addAndMakeVisible (*t);
    }
    tabKit.onClick  = [this] { showPage (0); };
    tabEdit.onClick = [this] { showPage (1); };
    tabFx.onClick   = [this] { showPage (2); };
    tabPlayer.onClick = [this] { showPage (3); };

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        proc.setCurrentProgram (presetBox.getSelectedId() - 1);
        kit.repaint();
    };
    addAndMakeVisible (presetBox);

    kit.onSelect = [this] (int pc) { select (pc); };
    kit.onSampleChanged = [this] { edit.refreshSample(); };
    mixer.onSelect = [this] (int pc) { select (pc); audition (proc, pc, false, 0.8f); };
    edit.onSelect = [this] (int pc) { select (pc); };

    addAndMakeVisible (kit);
    addAndMakeVisible (mixer);
    addChildComponent (edit);
    addChildComponent (fx);
    addChildComponent (player);

    tabKit.setToggleState (true, juce::dontSendNotification);
    dragMidi = std::make_unique<mc::DragButton> (proc.capture, "Tea Towel", juce::Colour (0xffe0a33a));
    addAndMakeVisible (*dragMidi);
    setSize (1000, 660);
}

TeaTowelEditor::~TeaTowelEditor() { setLookAndFeel (nullptr); }

void TeaTowelEditor::select (int piece)
{
    kit.setSelected (piece);
    mixer.setSelected (piece);
    edit.setPiece (piece);
}

void TeaTowelEditor::showPage (int pg)
{
    page = pg;
    kit.setVisible (pg == 0);
    mixer.setVisible (pg == 0);
    edit.setVisible (pg == 1);
    fx.setVisible (pg == 2);
    player.setVisible (pg == 3);
    if (pg == 0) kit.repaint();
}

void TeaTowelEditor::paint (juce::Graphics& g)
{
    g.fillAll (ttui::col::bg);
    auto header = getLocalBounds().removeFromTop (56);
    g.setColour (ttui::col::panel);
    g.fillRect (header);
    g.setColour (ttui::col::red);
    g.fillRect (header.removeFromBottom (2));

    g.setColour (ttui::col::amber);
    g.setFont (font (26.0f, true));
    g.drawText ("TEA TOWEL", 20, 8, 200, 28, juce::Justification::centredLeft);
    g.setColour (ttui::col::muted);
    g.setFont (font (11.0f));
    g.drawText ("DEAD DRUM INSTRUMENT", 21, 34, 200, 14, juce::Justification::centredLeft);
}

void TeaTowelEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (56).reduced (16, 12);
    presetBox.setBounds (header.removeFromRight (240));
    header.removeFromRight (20);
    tabPlayer.setBounds (header.removeFromRight (84));
    header.removeFromRight (6);
    tabFx.setBounds (header.removeFromRight (70));
    header.removeFromRight (6);
    tabEdit.setBounds (header.removeFromRight (70));
    header.removeFromRight (6);
    tabKit.setBounds (header.removeFromRight (70));
    header.removeFromRight (14);
    dragMidi->setBounds (header.removeFromRight (170));

    edit.setBounds (r);
    fx.setBounds (r);
    player.setBounds (r);
    mixer.setBounds (r.removeFromBottom (215));
    kit.setBounds (r);
}
