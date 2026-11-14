#include "PluginEditor.h"

using namespace juce;

static Font uiFont (float h, bool bold = false)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain));
}

//==============================================================================
// Look & feel
//==============================================================================
VPLookAndFeel::VPLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, vpcol::bg);
    setColour (Label::textColourId, vpcol::text);
    setColour (Slider::textBoxTextColourId, vpcol::text);
    setColour (Slider::textBoxBackgroundColourId, vpcol::well);
    setColour (Slider::textBoxOutlineColourId, vpcol::panelEdge);
    setColour (Slider::textBoxHighlightColourId, vpcol::accent.withAlpha (0.4f));
    setColour (Slider::rotarySliderFillColourId, vpcol::accent);
    setColour (TextButton::buttonColourId, vpcol::well);
    setColour (TextButton::textColourOffId, vpcol::textDim);
    setColour (TextButton::textColourOnId, vpcol::bg);
    setColour (TextEditor::backgroundColourId, vpcol::well);
    setColour (TextEditor::textColourId, vpcol::text);
    setColour (TextEditor::highlightColourId, vpcol::accent.withAlpha (0.4f));
    setColour (CaretComponent::caretColourId, vpcol::accent);
}

void VPLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, Slider& s)
{
    const auto area   = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size  = jmin (area.getWidth(), area.getHeight());
    const auto centre = area.getCentre();
    const float r     = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto fill   = s.findColour (Slider::rotarySliderFillColourId);
    const float arcW  = jmax (2.5f, size * 0.045f);

    // track + value arc
    Path track;
    track.addCentredArc (centre.x, centre.y, r - arcW, r - arcW, 0.0f, startAngle, endAngle, true);
    g.setColour (vpcol::well);
    g.strokePath (track, PathStrokeType (arcW + 2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (pos > 0.001f)
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, r - arcW, r - arcW, 0.0f, startAngle, angle, true);
        g.setColour (fill.withAlpha (0.25f));
        g.strokePath (value, PathStrokeType (arcW + 5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (fill);
        g.strokePath (value, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // knob body
    const float br = r - arcW * 2.0f - 5.0f;
    auto body = Rectangle<float> (br * 2.0f, br * 2.0f).withCentre (centre);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (body.translated (0.0f, 3.0f).expanded (1.0f));
    g.setGradientFill (ColourGradient (Colour (0xff2c313b), body.getX(), body.getY(),
                                       Colour (0xff14171c), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Colour (0xff3a404b));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    // pointer
    const auto p1 = centre.getPointOnCircumference (br * 0.30f, angle);
    const auto p2 = centre.getPointOnCircumference (br * 0.82f, angle);
    g.setColour (vpcol::text);
    g.drawLine ({ p1, p2 }, jmax (2.0f, size * 0.035f));
}

void VPLookAndFeel::drawFaderCap (Graphics& g, Rectangle<float> r, Colour stripe)
{
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 3.0f);
    g.setGradientFill (ColourGradient (Colour (0xff3b414c), r.getX(), r.getY(),
                                       Colour (0xff1c1f25), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colour (0xff4a515e));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.35f));
    for (float dy : { -5.0f, 5.0f })
        g.fillRect (r.getX() + 4.0f, r.getCentreY() + dy, r.getWidth() - 8.0f, 1.0f);
    g.setColour (stripe);
    g.fillRect (r.getX() + 3.0f, r.getCentreY() - 1.0f, r.getWidth() - 6.0f, 2.0f);
}

static void drawDbScale (Graphics& g, Slider& s, float trackX, float labelRight)
{
    g.setFont (uiFont (10.0f));
    for (int v = -12; v <= 12; v += 3)
    {
        const float yy = (float) s.getPositionOfValue ((double) v);
        const bool major = v % 6 == 0;
        g.setColour (major ? vpcol::textDim : vpcol::panelEdge.brighter (0.2f));
        const float len = major ? 9.0f : 5.0f;
        g.fillRect (trackX - 6.0f - len, yy - 0.5f, len, 1.0f);
        g.fillRect (trackX + 6.0f, yy - 0.5f, len, 1.0f);
        if (major && labelRight > 0.0f)
            g.drawText (v > 0 ? "+" + String (v) : String (v),
                        Rectangle<float> (labelRight - 26.0f, yy - 6.0f, 22.0f, 12.0f), Justification::centredRight);
    }
}

void VPLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                                      Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearVertical && style != Slider::TwoValueVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
        return;
    }

    const float cx = (float) x + (float) w * 0.5f + 8.0f;
    const auto trackR = Rectangle<float> (cx - 2.5f, (float) y - 4.0f, 5.0f, (float) h + 8.0f);

    drawDbScale (g, s, cx, cx - 16.0f);

    g.setColour (vpcol::well);
    g.fillRoundedRectangle (trackR.expanded (1.0f), 3.0f);
    g.setColour (Colours::black);
    g.fillRoundedRectangle (trackR, 2.5f);

    const float capW = 34.0f, capH = 20.0f;

    if (style == Slider::TwoValueVertical)
    {
        auto band = Rectangle<float> (cx - 9.0f, maxPos, 18.0f, minPos - maxPos);
        g.setColour (vpcol::gain.withAlpha (0.18f));
        g.fillRect (band);
        g.setColour (vpcol::gain.withAlpha (0.6f));
        g.fillRect (band.withWidth (2.0f).withX (cx - 1.0f));

        drawFaderCap (g, Rectangle<float> (capW, capH * 0.8f).withCentre ({ cx, maxPos }), vpcol::gain);
        drawFaderCap (g, Rectangle<float> (capW, capH * 0.8f).withCentre ({ cx, minPos }), vpcol::gain);
    }
    else
    {
        const float zeroY = (float) s.getPositionOfValue (0.0);
        g.setColour (vpcol::accent.withAlpha (0.7f));
        g.fillRect (Rectangle<float> (cx - 1.0f, jmin (zeroY, pos), 2.0f, std::abs (zeroY - pos)));
        drawFaderCap (g, Rectangle<float> (capW, capH).withCentre ({ cx, pos }), vpcol::accent);
    }
}

void VPLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto onCol = b.findColour (TextButton::buttonOnColourId);

    if (on)
    {
        g.setColour (onCol.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (1.0f), 5.0f);
        g.setGradientFill (ColourGradient (onCol.brighter (0.15f), r.getX(), r.getY(),
                                           onCol.darker (0.25f), r.getX(), r.getBottom(), false));
    }
    else
    {
        g.setColour (vpcol::well.brighter (over ? 0.12f : 0.0f).darker (down ? 0.2f : 0.0f));
    }
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? onCol.brighter (0.3f) : vpcol::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

Font VPLookAndFeel::getTextButtonFont (TextButton&, int h) { return uiFont (jmin (12.0f, (float) h * 0.5f), true); }
Font VPLookAndFeel::getLabelFont (Label& l)                 { return l.getFont(); }

Label* VPLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.0f, true));
    l->setColour (Label::outlineColourId, vpcol::panelEdge);
    l->setColour (Label::backgroundColourId, vpcol::well);
    l->setColour (Label::textColourId, vpcol::text);
    l->setWantsKeyboardFocus (false); // otherwise it opens its editor when the window gets focus
    return l;
}

//==============================================================================
// History view
//==============================================================================
HistoryView::HistoryView (VoxPilotProcessor& p) : proc (p)
{
    frames.assign (capacity, HistoryFrame { -100.0f, -100.0f, 0.0f, -18.0f, -45.0f, false });
    setOpaque (true);
}

void HistoryView::pull()
{
    int n;
    while ((n = proc.popHistory (scratch.data(), (int) scratch.size())) > 0)
        for (int i = 0; i < n; ++i)
        {
            frames[(size_t) writePos] = scratch[(size_t) i];
            writePos = (writePos + 1) % capacity;
        }
}

void HistoryView::resized() {}

Rectangle<float> HistoryView::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (34.0f).withTrimmedRight (34.0f).withTrimmedTop (8.0f).withTrimmedBottom (8.0f);
}

float HistoryView::levelToY (float db) const
{
    auto a = plotArea();
    const float h = a.getHeight() * 0.70f;
    return a.getY() + jmap (jlimit (minDb, maxDb, db), maxDb, minDb, 0.0f, h);
}

float HistoryView::yToLevel (float y) const
{
    auto a = plotArea();
    const float h = a.getHeight() * 0.70f;
    return jmap (y - a.getY(), 0.0f, h, maxDb, minDb);
}

float HistoryView::gainToY (float db) const
{
    auto a = plotArea();
    const float top = a.getY() + a.getHeight() * 0.70f + 10.0f;
    const float bottom = a.getBottom();
    const float lim = VoxPilotProcessor::gainLimitDb;
    return jmap (jlimit (-lim, lim, db), lim, -lim, top, bottom);
}

void HistoryView::paint (Graphics& g)
{
    g.fillAll (vpcol::well);
    const auto a = plotArea();
    const float levelBottom = levelToY (minDb);
    const float gainTop = gainToY (VoxPilotProcessor::gainLimitDb);

    // grid + axis labels
    g.setFont (uiFont (10.0f));
    for (int db = 0; db >= (int) minDb; db -= 6)
    {
        const float yy = levelToY ((float) db);
        g.setColour (vpcol::panelEdge.withAlpha (db % 12 == 0 ? 0.9f : 0.45f));
        g.drawHorizontalLine ((int) yy, a.getX(), a.getRight());
        if (db % 12 == 0)
        {
            g.setColour (vpcol::textDim);
            g.drawText (String (db), Rectangle<float> (0.0f, yy - 6.0f, 30.0f, 12.0f), Justification::centredRight);
        }
    }
    for (int db : { -12, -6, 0, 6, 12 })
    {
        const float yy = gainToY ((float) db);
        g.setColour (vpcol::panelEdge.withAlpha (db == 0 ? 1.0f : 0.5f));
        g.drawHorizontalLine ((int) yy, a.getX(), a.getRight());
        g.setColour (db == 0 ? vpcol::gain.withAlpha (0.8f) : vpcol::textDim);
        g.drawText (db > 0 ? "+" + String (db) : String (db),
                    Rectangle<float> (a.getRight() + 4.0f, yy - 6.0f, 28.0f, 12.0f), Justification::centredLeft);
    }
    g.setColour (vpcol::panelEdge);
    g.drawRect (Rectangle<float> (a.getX(), a.getY(), a.getWidth(), levelBottom - a.getY()), 1.0f);
    g.drawRect (Rectangle<float> (a.getX(), gainTop, a.getWidth(), a.getBottom() - gainTop), 1.0f);

    const float dx = a.getWidth() / (float) (capacity - 1);
    auto frameAt = [this] (int i) -> const HistoryFrame& { return frames[(size_t) ((writePos + i) % capacity)]; };

    // riding regions
    {
        g.setColour (vpcol::accent.withAlpha (0.06f));
        int start = -1;
        for (int i = 0; i <= capacity; ++i)
        {
            const bool act = i < capacity && frameAt (i).active;
            if (act && start < 0) start = i;
            if (! act && start >= 0)
            {
                g.fillRect (Rectangle<float> (a.getX() + (float) start * dx, a.getY(), (float) (i - start) * dx, levelBottom - a.getY()));
                start = -1;
            }
        }
    }

    Path vocalFill, vocalLine, outLine, targetLine, thrLine, gainLine;
    for (int i = 0; i < capacity; ++i)
    {
        const auto& f = frameAt (i);
        const float xx = a.getX() + (float) i * dx;
        const float vy = levelToY (f.vocalDb), oy = levelToY (f.outDb);
        const float ty = levelToY (f.targetDb), hy = levelToY (f.thresholdDb), gy = gainToY (f.gainDb);
        if (i == 0)
        {
            vocalFill.startNewSubPath (xx, levelBottom);
            vocalFill.lineTo (xx, vy);
            vocalLine.startNewSubPath (xx, vy);
            outLine.startNewSubPath (xx, oy);
            targetLine.startNewSubPath (xx, ty);
            thrLine.startNewSubPath (xx, hy);
            gainLine.startNewSubPath (xx, gy);
        }
        else
        {
            vocalFill.lineTo (xx, vy);
            vocalLine.lineTo (xx, vy);
            outLine.lineTo (xx, oy);
            targetLine.lineTo (xx, ty);
            thrLine.lineTo (xx, hy);
            gainLine.lineTo (xx, gy);
        }
    }
    vocalFill.lineTo (a.getRight(), levelBottom);
    vocalFill.closeSubPath();

    g.setGradientFill (ColourGradient (vpcol::vocal.withAlpha (0.55f), 0.0f, a.getY(),
                                       vpcol::vocal.withAlpha (0.10f), 0.0f, levelBottom, false));
    g.fillPath (vocalFill);
    g.setColour (vpcol::vocal.brighter (0.3f));
    g.strokePath (vocalLine, PathStrokeType (1.0f));

    // threshold (dashed red) & target (dashed cyan)
    const float dashes[] = { 5.0f, 4.0f };
    Path dashed;
    PathStrokeType (1.2f).createDashedStroke (dashed, thrLine, dashes, 2);
    g.setColour (vpcol::detect.withAlpha (0.8f));
    g.fillPath (dashed);

    dashed.clear();
    PathStrokeType (draggingTarget ? 2.2f : 1.5f).createDashedStroke (dashed, targetLine, dashes, 2);
    g.setColour (Colours::white.withAlpha (0.75f));
    g.fillPath (dashed);

    g.setColour (vpcol::accent);
    g.strokePath (outLine, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (vpcol::gain);
    g.strokePath (gainLine, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // legend
    struct Item { const char* name; Colour c; };
    const Item items[] = { { "VOCAL IN", vpcol::vocal.brighter (0.4f) }, { "OUTPUT", vpcol::accent },
                           { "TARGET", Colours::white.withAlpha (0.8f) }, { "DETECT", vpcol::detect } };
    float lx = a.getX() + 8.0f;
    g.setFont (uiFont (10.0f, true));
    for (auto& it : items)
    {
        g.setColour (it.c);
        g.fillRect (lx, a.getY() + 9.0f, 10.0f, 3.0f);
        g.setColour (vpcol::textDim);
        g.drawText (it.name, Rectangle<float> (lx + 14.0f, a.getY() + 4.0f, 60.0f, 13.0f), Justification::centredLeft);
        lx += 74.0f;
    }
    g.setColour (vpcol::gain);
    g.drawText ("RIDER GAIN", Rectangle<float> (a.getX() + 8.0f, gainTop + 3.0f, 80.0f, 13.0f), Justification::centredLeft);

    // current target handle on the right edge
    const float ty = levelToY (frameAt (capacity - 1).targetDb);
    Path tri;
    tri.addTriangle (a.getRight() + 1.0f, ty, a.getRight() + 8.0f, ty - 5.0f, a.getRight() + 8.0f, ty + 5.0f);
    g.setColour (Colours::white.withAlpha (0.85f));
    g.fillPath (tri);
}

void HistoryView::mouseMove (const MouseEvent& e)
{
    const float ty = levelToY (proc.apvts.getRawParameterValue (ParamIDs::target)->load());
    setMouseCursor (std::abs (e.position.y - ty) < 6.0f ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
}

void HistoryView::mouseDown (const MouseEvent& e)
{
    const float ty = levelToY (proc.apvts.getRawParameterValue (ParamIDs::target)->load());
    if (std::abs (e.position.y - ty) < 6.0f)
    {
        draggingTarget = true;
        proc.apvts.getParameter (ParamIDs::target)->beginChangeGesture();
    }
}

void HistoryView::mouseDrag (const MouseEvent& e)
{
    if (! draggingTarget) return;
    auto* p = proc.apvts.getParameter (ParamIDs::target);
    p->setValueNotifyingHost (p->convertTo0to1 (jlimit (-40.0f, 0.0f, yToLevel (e.position.y))));
}

void HistoryView::mouseUp (const MouseEvent&)
{
    if (draggingTarget)
        proc.apvts.getParameter (ParamIDs::target)->endChangeGesture();
    draggingTarget = false;
}

//==============================================================================
// Rider fader
//==============================================================================
RiderFader::RiderFader (VoxPilotProcessor& p)
    : proc (p), attachment (*p.apvts.getParameter (ParamIDs::rider), [] (float) {}, nullptr)
{
}

Rectangle<float> RiderFader::track() const
{
    return getLocalBounds().toFloat().reduced (0.0f, 14.0f);
}

float RiderFader::valueToY (float db) const
{
    const auto t = track();
    const float lim = VoxPilotProcessor::gainLimitDb;
    return jmap (jlimit (-lim, lim, db), lim, -lim, t.getY(), t.getBottom());
}

float RiderFader::yToValue (float y) const
{
    const auto t = track();
    const float lim = VoxPilotProcessor::gainLimitDb;
    return jlimit (-lim, lim, jmap (y, t.getY(), t.getBottom(), lim, -lim));
}

void RiderFader::setDisplayedGain (float db, bool interactive)
{
    canDrag = interactive;
    setMouseCursor (interactive ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
    if (! dragging && std::abs (db - shownDb) > 0.01f)
    {
        shownDb = db;
        repaint();
    }
}

void RiderFader::paint (Graphics& g)
{
    const auto t = track();
    const float cx = t.getCentreX() + 8.0f;

    g.setFont (uiFont (10.0f));
    for (int v = -12; v <= 12; v += 3)
    {
        const float yy = valueToY ((float) v);
        const bool major = v % 6 == 0;
        g.setColour (major ? vpcol::textDim : vpcol::panelEdge.brighter (0.2f));
        const float len = major ? 9.0f : 5.0f;
        g.fillRect (cx - 6.0f - len, yy - 0.5f, len, 1.0f);
        g.fillRect (cx + 6.0f, yy - 0.5f, len, 1.0f);
        if (major)
            g.drawText (v > 0 ? "+" + String (v) : String (v),
                        Rectangle<float> (cx - 42.0f, yy - 6.0f, 22.0f, 12.0f), Justification::centredRight);
    }

    const auto tr = Rectangle<float> (cx - 2.5f, t.getY() - 4.0f, 5.0f, t.getHeight() + 8.0f);
    g.setColour (Colours::black);
    g.fillRoundedRectangle (tr, 2.5f);

    const float zeroY = valueToY (0.0f), y = valueToY (shownDb);
    g.setColour (vpcol::gain.withAlpha (0.25f));
    g.fillRect (Rectangle<float> (cx - 8.0f, jmin (zeroY, y), 16.0f, std::abs (zeroY - y)));
    g.setColour (vpcol::gain);
    g.fillRect (Rectangle<float> (cx - 1.5f, jmin (zeroY, y), 3.0f, std::abs (zeroY - y)));

    VPLookAndFeel::drawFaderCap (g, Rectangle<float> (40.0f, 26.0f).withCentre ({ cx, y }),
                                 canDrag ? vpcol::readCol : vpcol::gain);
}

void RiderFader::mouseDown (const MouseEvent& e)
{
    if (! canDrag) return;
    dragging = true;
    attachment.beginGesture();
    mouseDrag (e);
}

void RiderFader::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;
    shownDb = yToValue (e.position.y);
    attachment.setValueAsPartOfGesture (shownDb);
    repaint();
}

void RiderFader::mouseUp (const MouseEvent&)
{
    if (dragging) attachment.endGesture();
    dragging = false;
}

void RiderFader::mouseDoubleClick (const MouseEvent&)
{
    if (canDrag) attachment.setValueAsCompleteGesture (0.0f);
}

//==============================================================================
// Level meter
//==============================================================================
void LevelMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black);
    g.fillRoundedRectangle (r, 2.0f);
    const float norm = jlimit (0.0f, 1.0f, jmap (level, -60.0f, 0.0f, 0.0f, 1.0f));
    auto lit = r.reduced (1.0f).removeFromBottom ((r.getHeight() - 2.0f) * norm);
    g.setGradientFill (ColourGradient (colour.darker (0.6f), 0.0f, r.getBottom(), colour, 0.0f, r.getY(), false));
    g.fillRect (lit);
}

//==============================================================================
// Editor
//==============================================================================
static constexpr int kW = 1000, kH = 590, kHeader = 64;

VoxPilotEditor::VoxPilotEditor (VoxPilotProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      rangeLowAtt  (*p.apvts.getParameter (ParamIDs::rangeLow),
                    [this] (float v) { rangeSlider.setMinValue (v, dontSendNotification, true); }, nullptr),
      rangeHighAtt (*p.apvts.getParameter (ParamIDs::rangeHigh),
                    [this] (float v) { rangeSlider.setMaxValue (v, dontSendNotification, true); }, nullptr),
      riderFader (p), history (p)
{

    setupKnob (targetKnob, targetLabel, "TARGET", ParamIDs::target);
    setupKnob (detectKnob, detectLabel, "VOCAL DETECT", ParamIDs::detect);
    setupKnob (bleedKnob,  bleedLabel,  "BLEED GUARD", ParamIDs::bleed);
    setupKnob (musicKnob,  musicLabel,  "MUSIC FOLLOW", ParamIDs::musicFollow);
    targetAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::target, targetKnob);
    detectAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::detect, detectKnob);
    bleedAtt  = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::bleed,  bleedKnob);
    musicAtt  = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::musicFollow, musicKnob);
    targetKnob.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 22);
    detectKnob.setColour (Slider::rotarySliderFillColourId, vpcol::detect);
    bleedKnob.setColour  (Slider::rotarySliderFillColourId, vpcol::detect.interpolatedWith (vpcol::gain, 0.5f));
    musicKnob.setColour  (Slider::rotarySliderFillColourId, vpcol::music);

    targetKnob.setTooltip ("Desired vocal level. You can also drag the white dashed line in the display.");
    detectKnob.setTooltip ("Level above which the signal is treated as voice. Below it the rider freezes.");
    bleedKnob.setTooltip ("Ignores bleed/noise: follows the music side-chain, or the noise floor without side-chain.");
    musicKnob.setTooltip ("Requires side-chain: lifts the target when the music gets louder and lowers it in quiet parts.");

    // range (two-value)
    rangeSlider.setRange (-VoxPilotProcessor::gainLimitDb, VoxPilotProcessor::gainLimitDb, 0.1);
    rangeSlider.setSliderSnapsToMousePosition (false);
    rangeSlider.onDragStart = [this] { rangeLowAtt.beginGesture(); rangeHighAtt.beginGesture(); };
    rangeSlider.onDragEnd   = [this] { rangeLowAtt.endGesture();   rangeHighAtt.endGesture(); };
    rangeSlider.onValueChange = [this]
    {
        if (rangeSlider.getMinValue() > 0.0) rangeSlider.setMinValue (0.0, dontSendNotification, true);
        if (rangeSlider.getMaxValue() < 0.0) rangeSlider.setMaxValue (0.0, dontSendNotification, true);
        rangeLowAtt.setValueAsPartOfGesture ((float) rangeSlider.getMinValue());
        rangeHighAtt.setValueAsPartOfGesture ((float) rangeSlider.getMaxValue());
    };
    rangeLowAtt.sendInitialUpdate();
    rangeHighAtt.sendInitialUpdate();
    addAndMakeVisible (rangeSlider);

    outputAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::output, outputSlider);
    outputSlider.setSliderStyle (Slider::LinearVertical);
    outputSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 22);
    outputSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (outputSlider);

    auto setupCaption = [this] (Label& l, const String& t)
    {
        l.setText (t, dontSendNotification);
        l.setFont (uiFont (11.0f, true));
        l.setColour (Label::textColourId, vpcol::textDim);
        l.setJustificationType (Justification::centred);
        addAndMakeVisible (l);
    };
    setupCaption (rangeLabel, "RANGE");
    setupCaption (riderLabel, "RIDER");
    setupCaption (outputLabel, "OUTPUT");

    for (auto* l : { &rangeValue, &riderValue })
    {
        l->setFont (uiFont (12.0f, true));
        l->setJustificationType (Justification::centred);
        l->setColour (Label::backgroundColourId, vpcol::well);
        l->setColour (Label::outlineColourId, vpcol::panelEdge);
        addAndMakeVisible (*l);
    }
    riderValue.setColour (Label::textColourId, vpcol::gain);

    addAndMakeVisible (riderFader);
    addAndMakeVisible (outMeter);
    addAndMakeVisible (inMeter);
    inMeter.colour = vpcol::vocal.brighter (0.4f);
    inMeter.setTooltip ("Vocal input level");
    outMeter.setTooltip ("Output level");
    addAndMakeVisible (history);

    // buttons
    auto setupBtn = [this] (TextButton& b, Colour on, int group)
    {
        b.setClickingTogglesState (false);
        b.setRadioGroupId (group);
        b.setColour (TextButton::buttonOnColourId, on);
        addAndMakeVisible (b);
    };
    setupBtn (fastBtn, vpcol::accent, 1);
    setupBtn (slowBtn, vpcol::accent, 1);
    setupBtn (offBtn, Colour (0xffb9bec7), 2);
    setupBtn (writeBtn, vpcol::writeCol, 2);
    setupBtn (readBtn, vpcol::readCol, 2);
    fastBtn.onClick  = [this] { setChoice (ParamIDs::fast, 1.0f); };
    slowBtn.onClick  = [this] { setChoice (ParamIDs::fast, 0.0f); };
    offBtn.onClick   = [this] { setChoice (ParamIDs::autoMode, 0.0f); };
    writeBtn.onClick = [this] { setChoice (ParamIDs::autoMode, 0.5f); };
    readBtn.onClick  = [this] { setChoice (ParamIDs::autoMode, 1.0f); };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = lookaheadBtns[(size_t) i];
        b.setButtonText (i == 0 ? String ("OFF") : String (roundToInt (lookaheadChoicesMs[i])));
        setupBtn (b, vpcol::gain, 3);
        b.onClick = [this, i] { setChoice (ParamIDs::lookahead, (float) i / 3.0f); };
        b.setTooltip ("Lookahead: the detector looks ahead so the gain is already right when a phrase starts. "
                      "Adds this much latency (Logic compensates automatically). Use OFF for live/recording.");
    }
    writeBtn.setTooltip ("Writes the rider moves to the 'Rider Gain' parameter. Put the track's automation in Latch or Touch in Logic.");
    readBtn.setTooltip ("Plays back the 'Rider Gain' automation (or lets you draw it by hand on the fader).");

    // set last so every child (and its text box) is rebuilt with our look & feel
    setLookAndFeel (&lnf);
    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
}

VoxPilotEditor::~VoxPilotEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoxPilotEditor::setupKnob (Slider& s, Label& l, const String& name, const char*)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 72, 20);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setVelocityBasedMode (false);
    s.setMouseDragSensitivity (220);
    addAndMakeVisible (s);

    l.setText (name, dontSendNotification);
    l.setFont (uiFont (11.0f, true));
    l.setColour (Label::textColourId, vpcol::textDim);
    l.setJustificationType (Justification::centred);
    addAndMakeVisible (l);
}

void VoxPilotEditor::setChoice (const char* id, float normalised)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (normalised);
        p->endChangeGesture();
    }
}

void VoxPilotEditor::drawPanel (Graphics& g, Rectangle<int> r, const String& title)
{
    auto rf = r.toFloat();
    g.setColour (vpcol::panel);
    g.fillRoundedRectangle (rf, 8.0f);
    g.setColour (vpcol::panelEdge);
    g.drawRoundedRectangle (rf.reduced (0.5f), 8.0f, 1.0f);
    g.setColour (vpcol::textDim);
    g.setFont (uiFont (10.5f, true));
    g.drawText (title, r.withHeight (24).withTrimmedLeft (12), Justification::centredLeft);
}

static void drawLed (Graphics& g, Point<float> c, Colour col, bool on)
{
    if (on)
    {
        g.setColour (col.withAlpha (0.3f));
        g.fillEllipse (Rectangle<float> (18.0f, 18.0f).withCentre (c));
    }
    g.setColour (on ? col : col.withAlpha (0.18f));
    g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (c));
}

void VoxPilotEditor::paint (Graphics& g)
{
    g.fillAll (vpcol::bg);

    // header
    auto header = getLocalBounds().removeFromTop (kHeader);
    g.setGradientFill (ColourGradient (Colour (0xff1a1d23), 0.0f, 0.0f, Colour (0xff111318), 0.0f, (float) kHeader, false));
    g.fillRect (header);
    g.setColour (vpcol::panelEdge);
    g.drawHorizontalLine (kHeader - 1, 0.0f, (float) getWidth());

    // logo: a little waveform glyph + name
    {
        Path wave;
        const float x0 = 20.0f, cy = kHeader * 0.5f;
        wave.startNewSubPath (x0, cy);
        for (int i = 1; i <= 26; ++i)
        {
            const float t = (float) i / 26.0f;
            const float env = std::sin (t * MathConstants<float>::pi);
            wave.lineTo (x0 + t * 34.0f, cy - std::sin (t * 22.0f) * 12.0f * env);
        }
        g.setColour (vpcol::accent);
        g.strokePath (wave, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (vpcol::gain);
        g.drawLine (x0, cy - 15.0f, x0 + 34.0f, cy - 15.0f, 1.5f);

        g.setFont (uiFont (24.0f, true));
        g.setColour (vpcol::text);
        g.drawText ("VOX", 66, 12, 60, 26, Justification::centredLeft);
        g.setColour (vpcol::accent);
        g.drawText ("PILOT", 116, 12, 80, 26, Justification::centredLeft);
        g.setFont (uiFont (10.0f));
        g.setColour (vpcol::textDim);
        g.drawText ("AUTOMATIC VOCAL LEVEL RIDER", 67, 37, 200, 14, Justification::centredLeft);
    }

    // status LEDs
    g.setFont (uiFont (11.0f, true));
    drawLed (g, { 300.0f, 24.0f }, vpcol::readCol, rideLed);
    g.setColour (rideLed ? vpcol::text : vpcol::textDim);
    g.drawText (rideLed ? "RIDING" : "IDLE", 312, 17, 60, 14, Justification::centredLeft);
    const bool scSignal = scLed && scLevel > -70.0f;
    drawLed (g, { 300.0f, 42.0f }, vpcol::music, scSignal);
    g.setColour (scLed ? vpcol::text : vpcol::textDim);
    g.drawText (scLed ? "SIDECHAIN" : "NO SIDECHAIN", 312, 35, 100, 14, Justification::centredLeft);

    g.setFont (uiFont (10.0f, true));
    g.setColour (vpcol::textDim);
    g.drawText ("RESPONSE", fastBtn.getX(), 4, 120, 12, Justification::centredLeft);
    g.drawText ("LOOKAHEAD (MS)", lookaheadBtns[0].getX(), 4, 170, 12, Justification::centredLeft);
    g.drawText ("AUTOMATION", offBtn.getX(), 4, 200, 12, Justification::centredLeft);

    // panels
    drawPanel (g, { 14, kHeader + 12, 236, kH - kHeader - 26 }, "LEVEL");
    drawPanel (g, { 262, kHeader + 12, kW - 262 - 310, kH - kHeader - 26 }, "RIDE HISTORY");
    drawPanel (g, { kW - 298, kHeader + 12, 284, kH - kHeader - 26 }, "GAIN");
}

void VoxPilotEditor::resized()
{
    // header controls
    int x = 428;
    fastBtn.setBounds (x, 20, 58, 30);
    slowBtn.setBounds (x + 62, 20, 58, 30);
    x = 568;
    for (int i = 0; i < 4; ++i)
        lookaheadBtns[(size_t) i].setBounds (x + i * 44, 20, i == 0 ? 42 : 40, 30);
    x = kW - 14 - 3 * 76;
    offBtn.setBounds (x, 20, 72, 30);
    writeBtn.setBounds (x + 76, 20, 72, 30);
    readBtn.setBounds (x + 152, 20, 72, 30);

    // left panel
    const int lx = 14, ly = kHeader + 12;
    targetLabel.setBounds (lx, ly + 26, 236, 16);
    targetKnob.setBounds (lx + 38, ly + 44, 160, 172);

    detectLabel.setBounds (lx + 6, ly + 236, 110, 16);
    detectKnob.setBounds  (lx + 14, ly + 252, 94, 108);
    bleedLabel.setBounds  (lx + 120, ly + 236, 110, 16);
    bleedKnob.setBounds   (lx + 128, ly + 252, 94, 108);
    musicLabel.setBounds  (lx + 63, ly + 372, 110, 16);
    musicKnob.setBounds   (lx + 71, ly + 388, 94, 108);

    // centre
    history.setBounds (262 + 10, kHeader + 12 + 28, kW - 262 - 310 - 20, kH - kHeader - 26 - 40);

    // right panel: Range | Rider | Output + meters
    const int rx = kW - 298, top = kHeader + 12 + 30;
    const int faderTop = top + 20, faderH = 380;
    rangeLabel.setBounds (rx + 6, top, 80, 16);
    rangeSlider.setBounds (rx + 6, faderTop, 80, faderH);
    rangeValue.setBounds (rx + 12, faderTop + faderH + 10, 68, 22);

    riderLabel.setBounds (rx + 96, top, 80, 16);
    riderFader.setBounds (rx + 96, faderTop - 4, 80, faderH + 8);
    riderValue.setBounds (rx + 102, faderTop + faderH + 10, 68, 22);

    outputLabel.setBounds (rx + 186, top, 80, 16);
    outputSlider.setBounds (rx + 186, faderTop, 72, faderH + 32);
    inMeter.setBounds (rx + 260, faderTop + 10, 7, faderH - 40);
    outMeter.setBounds (rx + 270, faderTop + 10, 7, faderH - 40);
}

void VoxPilotEditor::timerCallback()
{
    history.pull();
    history.repaint();

    const auto mode = (AutoMode) roundToInt (proc.apvts.getRawParameterValue (ParamIDs::autoMode)->load());
    const bool fast = proc.apvts.getRawParameterValue (ParamIDs::fast)->load() > 0.5f;

    fastBtn.setToggleState (fast, dontSendNotification);
    slowBtn.setToggleState (! fast, dontSendNotification);
    offBtn.setToggleState (mode == AutoMode::off, dontSendNotification);
    writeBtn.setToggleState (mode == AutoMode::write, dontSendNotification);
    readBtn.setToggleState (mode == AutoMode::read, dontSendNotification);
    const int la = roundToInt (proc.apvts.getRawParameterValue (ParamIDs::lookahead)->load());
    for (int i = 0; i < 4; ++i)
        lookaheadBtns[(size_t) i].setToggleState (i == la, dontSendNotification);

    const float gainDb = proc.meterGainDb.load();
    riderFader.setDisplayedGain (gainDb, mode == AutoMode::read);
    riderValue.setText ((gainDb >= 0.0f ? "+" : "") + String (gainDb, 1) + " dB", dontSendNotification);
    rangeValue.setText (String (rangeSlider.getMinValue(), 1) + " / +" + String (rangeSlider.getMaxValue(), 1),
                        dontSendNotification);

    outMeter.setLevel (proc.meterOutDb.load());
    inMeter.setLevel (proc.meterVocalDb.load());

    const bool ride = proc.meterActive.load();
    const bool sc = proc.sidechainConnected.load();
    const float scl = proc.meterMusicDb.load();
    const bool scSigChanged = (scl > -70.0f) != (scLevel > -70.0f);
    if (ride != rideLed || sc != scLed || scSigChanged)
    {
        rideLed = ride; scLed = sc; scLevel = scl;
        repaint (0, 0, 420, kHeader);
    }
    scLevel = scl;
}
