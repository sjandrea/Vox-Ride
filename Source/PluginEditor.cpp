#include "PluginEditor.h"

using namespace juce;

static Font uiFont (float h, bool bold = false)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain));
}

static String dbLabel (int v) { return v > 0 ? "+" + String (v) : String (v); }

//==============================================================================
// Drawing helpers
//==============================================================================
namespace vpdraw
{
void glowStroke (Graphics& g, const Path& p, Colour c, float w)
{
    const PathStrokeType::JointStyle j = PathStrokeType::curved;
    const PathStrokeType::EndCapStyle e = PathStrokeType::rounded;
    g.setColour (c.withAlpha (0.08f));  g.strokePath (p, PathStrokeType (w * 5.5f, j, e));
    g.setColour (c.withAlpha (0.18f));  g.strokePath (p, PathStrokeType (w * 2.8f, j, e));
    g.setColour (c);                    g.strokePath (p, PathStrokeType (w, j, e));
    g.setColour (Colours::white.withAlpha (0.35f));
    g.strokePath (p, PathStrokeType (jmax (0.6f, w * 0.3f), j, e));
}

void recess (Graphics& g, Rectangle<float> r, float rad, Colour fill)
{
    // light lip below, dark lip above → reads as cut into the metal
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.translated (0.0f, 1.0f).expanded (0.5f), rad + 0.5f, 1.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, rad);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.6f), 0.0f, r.getY(),
                                       Colours::transparentBlack, 0.0f, r.getY() + jmin (12.0f, r.getHeight() * 0.5f), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (r, rad, 1.0f);
}

void bevelPanel (Graphics& g, Rectangle<float> r, float rad)
{
    // soft drop shadow
    for (int i = 4; i >= 1; --i)
    {
        g.setColour (Colours::black.withAlpha (0.10f));
        g.fillRoundedRectangle (r.translated (0.0f, (float) i * 1.2f).expanded ((float) i), rad + (float) i);
    }
    g.setGradientFill (ColourGradient (vpcol::panelTop, 0.0f, r.getY(), vpcol::panelBot, 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);

    // inner bevel: bright top edge fading out, dark bottom edge
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.20f), 0.0f, r.getY(),
                                       Colours::white.withAlpha (0.0f), 0.0f, r.getY() + 40.0f, false));
    g.drawRoundedRectangle (r.reduced (1.5f), rad - 1.0f, 1.2f);
    g.setColour (Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (r, rad, 1.5f);
}

void screw (Graphics& g, Point<float> c, float rad, float angle)
{
    auto circle = [] (Point<float> p, float r) { return Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p); };
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (circle (c.translated (0.0f, 1.0f), rad + 1.0f));
    g.setGradientFill (ColourGradient (Colour (0xffb7bec4), c.x - rad, c.y - rad, Colour (0xff2d3439), c.x + rad, c.y + rad, false));
    g.fillEllipse (circle (c, rad));
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawEllipse (circle (c, rad), 0.8f);

    const auto d = Point<float> (std::cos (angle), std::sin (angle)) * (rad * 0.75f);
    g.setColour (Colour (0xff15191c));
    g.drawLine ({ c - d, c + d }, 1.6f);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawLine ({ (c - d).translated (0.0f, 1.0f), (c + d).translated (0.0f, 1.0f) }, 0.6f);
}

void led (Graphics& g, Point<float> c, float r, Colour col, bool on)
{
    auto circle = [] (Point<float> p, float rr) { return Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (p); };
    if (on)
    {
        g.setGradientFill (ColourGradient (col.withAlpha (0.65f), c.x, c.y, col.withAlpha (0.0f), c.x + r * 2.8f, c.y, true));
        g.fillEllipse (circle (c, r * 2.8f));
    }
    // housing
    g.setGradientFill (ColourGradient (Colour (0xff0b0f12), c.x, c.y - r, Colour (0xff4a5560), c.x, c.y + r + 2.0f, false));
    g.fillEllipse (circle (c, r + 2.0f));
    // lens
    const auto base = on ? col : col.withMultipliedBrightness (0.25f).withMultipliedSaturation (0.6f);
    g.setGradientFill (on ? ColourGradient (base.brighter (0.6f), c.x - r * 0.2f, c.y - r * 0.3f,
                                            base.withMultipliedSaturation (1.2f), c.x + r, c.y + r, true)
                          : ColourGradient (base.brighter (0.2f), c.x - r * 0.3f, c.y - r * 0.4f,
                                            base.darker (0.6f), c.x + r, c.y + r, true));
    g.fillEllipse (circle (c, r));
    // specular
    g.setColour (Colours::white.withAlpha (on ? 0.7f : 0.25f));
    g.fillEllipse (Rectangle<float> (r * 0.9f, r * 0.6f).withCentre ({ c.x - r * 0.25f, c.y - r * 0.45f }));
}

void faderCap (Graphics& g, Rectangle<float> r, Colour stripe)
{
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (1.0f), 3.5f);

    ColourGradient metal (Colour (0xffd6dadf), 0.0f, r.getY(), Colour (0xff4f565d), 0.0f, r.getBottom(), false);
    metal.addColour (0.45, Colour (0xff9aa1a8));
    metal.addColour (0.55, Colour (0xff767d85));
    g.setGradientFill (metal);
    g.fillRoundedRectangle (r, 3.0f);

    g.setColour (Colours::black.withAlpha (0.25f));
    for (float f : { 0.2f, 0.32f, 0.68f, 0.8f })
        g.fillRect (r.getX() + 3.0f, r.getY() + r.getHeight() * f, r.getWidth() - 6.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    // glowing index line
    auto line = Rectangle<float> (r.getX() + 3.0f, r.getCentreY() - 1.5f, r.getWidth() - 6.0f, 3.0f);
    g.setColour (stripe.withAlpha (0.35f));
    g.fillRoundedRectangle (line.expanded (1.5f), 2.0f);
    g.setColour (stripe);
    g.fillRoundedRectangle (line, 1.0f);
}

void faderSlot (Graphics& g, float cx, float top, float bottom)
{
    recess (g, Rectangle<float> (cx - 4.5f, top - 8.0f, 9.0f, bottom - top + 16.0f), 4.0f, Colour (0xff030506));
}

void glowBar (Graphics& g, Rectangle<float> r, Colour c)
{
    if (r.getHeight() < 0.5f) return;
    g.setColour (c.withAlpha (0.10f)); g.fillRoundedRectangle (r.expanded (6.0f, 2.0f), 5.0f);
    g.setColour (c.withAlpha (0.22f)); g.fillRoundedRectangle (r.expanded (3.0f, 1.0f), 3.0f);
    g.setGradientFill (ColourGradient (c.darker (0.5f), r.getX(), 0.0f, c.darker (0.5f), r.getRight(), 0.0f, false));
    g.fillRect (r);
    ColourGradient core (c.darker (0.3f), r.getX(), 0.0f, c.darker (0.3f), r.getRight(), 0.0f, false);
    core.addColour (0.5, c.brighter (0.5f));
    g.setGradientFill (core);
    g.fillRect (r.reduced (r.getWidth() * 0.15f, 0.0f));
}

void dbScale (Graphics& g, const std::function<float (float)>& valueToY, float cx, bool labels)
{
    g.setFont (uiFont (11.5f, true));
    for (int v = -12; v <= 12; v += 3)
    {
        const float yy = valueToY ((float) v);
        const bool major = v % 6 == 0;
        g.setColour (major ? vpcol::text.withAlpha (0.85f) : vpcol::textDim.withAlpha (0.6f));
        const float len = major ? 10.0f : 6.0f;
        g.fillRect (cx - 9.0f - len, yy - 0.5f, len, 1.2f);
        g.fillRect (cx + 9.0f, yy - 0.5f, len, 1.2f);
        if (major && labels)
            g.drawText (dbLabel (v), Rectangle<float> (cx - 50.0f, yy - 7.0f, 28.0f, 14.0f), Justification::centredRight);
    }
}
} // namespace vpdraw

//==============================================================================
// Look & feel
//==============================================================================
VPLookAndFeel::VPLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, vpcol::bgBottom);
    setColour (Label::textColourId, vpcol::text);
    setColour (Slider::textBoxTextColourId, vpcol::text);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, vpcol::accent.withAlpha (0.4f));
    setColour (Slider::rotarySliderFillColourId, vpcol::accent);
    setColour (TextButton::buttonColourId, vpcol::well);
    setColour (TextButton::textColourOffId, vpcol::text);
    setColour (TextButton::textColourOnId, vpcol::darkText);
    setColour (TextEditor::backgroundColourId, vpcol::well);
    setColour (TextEditor::textColourId, vpcol::text);
    setColour (TextEditor::highlightColourId, vpcol::accent.withAlpha (0.4f));
    setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    setColour (TextEditor::focusedOutlineColourId, vpcol::accent.withAlpha (0.6f));
    setColour (CaretComponent::caretColourId, vpcol::accent);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff111a1f));
    setColour (TooltipWindow::textColourId, vpcol::text);
    setColour (TooltipWindow::outlineColourId, vpcol::accent.withAlpha (0.4f));
}

void VPLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, Slider& s)
{
    const auto area   = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float size  = jmin (area.getWidth(), area.getHeight());
    const auto c      = area.getCentre();
    const float r     = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto fill   = s.findColour (Slider::rotarySliderFillColourId);
    const float arcW  = jmax (3.5f, size * 0.055f);
    const float arcR  = r - arcW * 1.6f;
    auto circle = [] (Point<float> p, float rr) { return Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (p); };

    // arc channel + glowing value arc
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::black.withAlpha (0.65f));
    g.strokePath (track, PathStrokeType (arcW + 3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (pos > 0.002f)
    {
        Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        vpdraw::glowStroke (g, value, fill, arcW);
    }

    // bezel ring with knurling
    const float bezelR = arcR - arcW - 4.0f;
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (circle (c.translated (0.0f, 3.0f), bezelR + 1.5f));
    g.setGradientFill (ColourGradient (Colour (0xff59646e), c.x, c.y - bezelR, Colour (0xff0b1013), c.x, c.y + bezelR, false));
    g.fillEllipse (circle (c, bezelR));
    const int knurls = jlimit (36, 90, (int) (bezelR * 1.6f));
    for (int i = 0; i < knurls; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / (float) knurls;
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawLine ({ c.getPointOnCircumference (bezelR * 0.87f, a), c.getPointOnCircumference (bezelR * 0.99f, a) }, 1.0f);
    }
    g.setColour (Colours::black.withAlpha (0.9f));
    g.drawEllipse (circle (c, bezelR), 1.0f);

    // knob cap
    const float bodyR = bezelR * 0.80f;
    g.setGradientFill (ColourGradient (Colour (0xff4a525a), c.x - bodyR * 0.35f, c.y - bodyR * 0.45f,
                                       Colour (0xff0f1215), c.x + bodyR * 0.7f, c.y + bodyR * 0.9f, true));
    g.fillEllipse (circle (c, bodyR));
    // warm rim light along the lower edge
    Path rim;
    rim.addCentredArc (c.x, c.y, bodyR - 1.0f, bodyR - 1.0f, 0.0f, MathConstants<float>::pi * 0.62f, MathConstants<float>::pi * 1.38f, true);
    g.setColour (vpcol::gain.withAlpha (0.18f));
    g.strokePath (rim, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (vpcol::gain.withAlpha (0.55f));
    g.strokePath (rim, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
    // top sheen
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), c.x, c.y - bodyR,
                                       Colours::white.withAlpha (0.0f), c.x, c.y, false));
    g.fillEllipse (circle (c, bodyR).reduced (bodyR * 0.08f, 0.0f).withTrimmedBottom (bodyR));
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawEllipse (circle (c, bodyR), 1.0f);

    // pointer
    Path pointer;
    pointer.startNewSubPath (c.getPointOnCircumference (bodyR * 0.28f, angle));
    pointer.lineTo (c.getPointOnCircumference (bodyR * 0.9f, angle));
    const float pw = jmax (2.5f, size * 0.035f);
    g.setColour (Colours::white.withAlpha (0.2f));
    g.strokePath (pointer, PathStrokeType (pw * 2.5f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white);
    g.strokePath (pointer, PathStrokeType (pw, PathStrokeType::curved, PathStrokeType::rounded));
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
    vpdraw::dbScale (g, [&s] (float v) { return (float) s.getPositionOfValue ((double) v); }, cx, true);
    vpdraw::faderSlot (g, cx, (float) y, (float) (y + h));

    if (style == Slider::TwoValueVertical)
    {
        vpdraw::glowBar (g, Rectangle<float> (cx - 4.0f, maxPos, 8.0f, minPos - maxPos), vpcol::gain);
        vpdraw::faderCap (g, Rectangle<float> (38.0f, 18.0f).withCentre ({ cx, maxPos }), vpcol::gain);
        vpdraw::faderCap (g, Rectangle<float> (38.0f, 18.0f).withCentre ({ cx, minPos }), vpcol::gain);
    }
    else
    {
        vpdraw::faderCap (g, Rectangle<float> (40.0f, 22.0f).withCentre ({ cx, pos }), vpcol::accent);
    }
}

void VPLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const auto onCol = b.findColour (TextButton::buttonOnColourId);

    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f).expanded (1.0f), 5.0f);

    if (on)
    {
        g.setColour (onCol.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (2.5f), 6.5f);
        g.setGradientFill (ColourGradient (onCol.brighter (0.45f), 0.0f, r.getY(), onCol.darker (0.25f), 0.0f, r.getBottom(), false));
    }
    else
    {
        const float lift = over ? 0.08f : 0.0f;
        g.setGradientFill (ColourGradient (Colour (0xff3a4852).brighter (lift), 0.0f, r.getY(),
                                           Colour (0xff141b20).brighter (lift), 0.0f, r.getBottom(), false));
    }
    g.fillRoundedRectangle (down ? r.translated (0.0f, 1.0f) : r, 4.5f);

    g.setColour (Colours::white.withAlpha (on ? 0.5f : 0.16f));
    g.drawLine (r.getX() + 4.0f, r.getY() + 1.0f, r.getRight() - 4.0f, r.getY() + 1.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (r, 4.5f, 1.0f);
}

Font VPLookAndFeel::getTextButtonFont (TextButton&, int h) { return uiFont (jmin (15.0f, (float) h * 0.5f), true); }
Font VPLookAndFeel::getLabelFont (Label& l)                 { return l.getFont(); }

void VPLookAndFeel::makeValueDisplay (Label& l)
{
    l.getProperties().set ("vpValue", true);
    l.setFont (uiFont (15.0f, true));
    l.setJustificationType (Justification::centred);
    l.setColour (Label::backgroundColourId, Colours::transparentBlack);
    l.setColour (Label::outlineColourId, Colours::transparentBlack);
}

void VPLookAndFeel::drawLabel (Graphics& g, Label& l)
{
    if (! (bool) l.getProperties().getWithDefault ("vpValue", false))
    {
        LookAndFeel_V4::drawLabel (g, l);
        return;
    }

    auto r = l.getLocalBounds().toFloat().reduced (1.0f, 1.5f);
    vpdraw::recess (g, r, 4.0f, Colour (0xff070b0d));
    if (! l.isBeingEdited())
    {
        g.setColour (l.findColour (Label::textColourId));
        g.setFont (l.getFont());
        g.drawText (l.getText(), r, Justification::centred, false);
    }
}

Label* VPLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    makeValueDisplay (*l);
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

Rectangle<float> HistoryView::levelBox() const
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    return b.withHeight (b.getHeight() * 0.67f);
}

Rectangle<float> HistoryView::gainBox() const
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    return b.withTop (levelBox().getBottom() + 12.0f);
}

Rectangle<float> HistoryView::levelPlot() const { return levelBox().withTrimmedLeft (44.0f).withTrimmedRight (14.0f).reduced (0.0f, 12.0f); }
Rectangle<float> HistoryView::gainPlot() const  { return gainBox().withTrimmedLeft (14.0f).withTrimmedRight (40.0f).reduced (0.0f, 10.0f); }

float HistoryView::levelToY (float db) const
{
    const auto a = levelPlot();
    return jmap (jlimit (minDb, maxDb, db), maxDb, minDb, a.getY(), a.getBottom());
}

float HistoryView::yToLevel (float y) const
{
    const auto a = levelPlot();
    return jmap (y, a.getY(), a.getBottom(), maxDb, minDb);
}

float HistoryView::gainToY (float db) const
{
    const auto a = gainPlot();
    const float lim = VoxPilotProcessor::gainLimitDb;
    return jmap (jlimit (-lim, lim, db), lim, -lim, a.getY(), a.getBottom());
}

void HistoryView::paint (Graphics& g)
{
    const auto lb = levelBox(), gb = gainBox();
    const auto la = levelPlot(), ga = gainPlot();

    // recessed bezels
    vpdraw::recess (g, lb, 7.0f, Colour (0xff060c0f));
    vpdraw::recess (g, gb, 7.0f, Colour (0xff060c0f));

    // plot screens
    for (auto a : { la, ga })
    {
        g.setGradientFill (ColourGradient (vpcol::plotTop, 0.0f, a.getY(), vpcol::plotBot, 0.0f, a.getBottom(), false));
        g.fillRect (a);
    }

    // grid
    g.setFont (uiFont (12.0f, true));
    for (int db = 0; db >= (int) minDb; db -= 6)
    {
        const float yy = levelToY ((float) db);
        g.setColour (vpcol::accent.withAlpha (db % 12 == 0 ? 0.13f : 0.06f));
        g.fillRect (la.getX(), yy - 0.5f, la.getWidth(), 1.0f);
        if (db % 12 == 0)
        {
            g.setColour (vpcol::text.withAlpha (0.8f));
            g.drawText (String (db), Rectangle<float> (lb.getX(), yy - 7.0f, 36.0f, 14.0f), Justification::centredRight);
        }
    }
    for (int i = 1; i < 8; ++i)
    {
        g.setColour (vpcol::accent.withAlpha (0.05f));
        g.fillRect (la.getX() + la.getWidth() * (float) i / 8.0f, la.getY(), 1.0f, la.getHeight());
        g.fillRect (ga.getX() + ga.getWidth() * (float) i / 8.0f, ga.getY(), 1.0f, ga.getHeight());
    }
    g.setFont (uiFont (11.5f, true));
    for (int db : { -12, -6, 0, 6, 12 })
    {
        const float yy = gainToY ((float) db);
        g.setColour (db == 0 ? vpcol::gain.withAlpha (0.25f) : vpcol::accent.withAlpha (0.06f));
        g.fillRect (ga.getX(), yy - 0.5f, ga.getWidth(), 1.0f);
        g.setColour (db == 0 ? vpcol::gain : vpcol::text.withAlpha (0.8f));
        g.drawText (dbLabel (db), Rectangle<float> (ga.getRight() + 4.0f, yy - 7.0f, 30.0f, 14.0f), Justification::centredLeft);
    }
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawRect (la, 1.0f);
    g.drawRect (ga, 1.0f);

    const float dx = la.getWidth() / (float) (capacity - 1);
    const float gdx = ga.getWidth() / (float) (capacity - 1);
    auto frameAt = [this] (int i) -> const HistoryFrame& { return frames[(size_t) ((writePos + i) % capacity)]; };

    // riding regions
    {
        g.setColour (vpcol::accent.withAlpha (0.045f));
        int start = -1;
        for (int i = 0; i <= capacity; ++i)
        {
            const bool act = i < capacity && frameAt (i).active;
            if (act && start < 0) start = i;
            if (! act && start >= 0)
            {
                g.fillRect (Rectangle<float> (la.getX() + (float) start * dx, la.getY(), (float) (i - start) * dx, la.getHeight()));
                start = -1;
            }
        }
    }

    Path vocalFill, vocalLine, outLine, targetLine, thrLine, gainLine;
    for (int i = 0; i < capacity; ++i)
    {
        const auto& f = frameAt (i);
        const float xx = la.getX() + (float) i * dx, gx = ga.getX() + (float) i * gdx;
        const float vy = levelToY (f.vocalDb), oy = levelToY (f.outDb);
        const float ty = levelToY (f.targetDb), hy = levelToY (f.thresholdDb), gy = gainToY (f.gainDb);
        if (i == 0)
        {
            vocalFill.startNewSubPath (xx, la.getBottom());
            vocalFill.lineTo (xx, vy);
            vocalLine.startNewSubPath (xx, vy);
            outLine.startNewSubPath (xx, oy);
            targetLine.startNewSubPath (xx, ty);
            thrLine.startNewSubPath (xx, hy);
            gainLine.startNewSubPath (gx, gy);
        }
        else
        {
            vocalFill.lineTo (xx, vy);
            vocalLine.lineTo (xx, vy);
            outLine.lineTo (xx, oy);
            targetLine.lineTo (xx, ty);
            thrLine.lineTo (xx, hy);
            gainLine.lineTo (gx, gy);
        }
    }
    vocalFill.lineTo (la.getRight(), la.getBottom());
    vocalFill.closeSubPath();

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (la.toNearestInt());
        g.setGradientFill (ColourGradient (vpcol::vocal.withAlpha (0.55f), 0.0f, la.getY() + la.getHeight() * 0.15f,
                                           vpcol::vocal.withAlpha (0.08f), 0.0f, la.getBottom(), false));
        g.fillPath (vocalFill);
        g.setColour (vpcol::vocal.brighter (0.5f));
        g.strokePath (vocalLine, PathStrokeType (1.0f));

        const float dashes[] = { 6.0f, 4.0f };
        Path dashed;
        PathStrokeType (1.4f).createDashedStroke (dashed, thrLine, dashes, 2);
        g.setColour (vpcol::detect.withAlpha (0.9f));
        g.fillPath (dashed);

        dashed.clear();
        PathStrokeType (draggingTarget ? 2.6f : 2.0f).createDashedStroke (dashed, targetLine, dashes, 2);
        g.setColour (Colours::white.withAlpha (0.9f));
        g.fillPath (dashed);

        vpdraw::glowStroke (g, outLine, vpcol::accent, 1.8f);
    }
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (ga.toNearestInt());
        vpdraw::glowStroke (g, gainLine, vpcol::gain, 2.0f);
    }

    // legend
    struct Item { const char* name; Colour c; };
    const Item items[] = { { "VOCAL IN", vpcol::vocal.brighter (0.5f) }, { "OUTPUT", vpcol::accent },
                           { "TARGET", Colours::white }, { "DETECT", vpcol::detect } };
    float lx = la.getX() + 12.0f;
    g.setFont (uiFont (12.5f, true));
    for (auto& it : items)
    {
        g.setColour (it.c);
        g.fillRoundedRectangle (lx, la.getY() + 11.0f, 14.0f, 3.5f, 1.5f);
        g.setColour (vpcol::text.withAlpha (0.9f));
        g.drawText (it.name, Rectangle<float> (lx + 19.0f, la.getY() + 5.0f, 80.0f, 16.0f), Justification::centredLeft);
        lx += jmax (86.0f, la.getWidth() / 4.2f);
    }
    g.setColour (vpcol::gain);
    g.setFont (uiFont (13.0f, true));
    g.drawText ("RIDER GAIN", Rectangle<float> (ga.getX() + 10.0f, ga.getY() + 5.0f, 120.0f, 16.0f), Justification::centredLeft);

    // current target handle on the right edge
    const float ty = levelToY (frameAt (capacity - 1).targetDb);
    Path tri;
    tri.addTriangle (la.getRight() + 1.0f, ty, la.getRight() + 9.0f, ty - 6.0f, la.getRight() + 9.0f, ty + 6.0f);
    g.setColour (Colours::white);
    g.fillPath (tri);
}

void HistoryView::mouseMove (const MouseEvent& e)
{
    const float ty = levelToY (proc.apvts.getRawParameterValue (ParamIDs::target)->load());
    setMouseCursor (std::abs (e.position.y - ty) < 6.0f && levelBox().contains (e.position)
                        ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
}

void HistoryView::mouseDown (const MouseEvent& e)
{
    const float ty = levelToY (proc.apvts.getRawParameterValue (ParamIDs::target)->load());
    if (std::abs (e.position.y - ty) < 6.0f && levelBox().contains (e.position))
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
    return getLocalBounds().toFloat().reduced (0.0f, 16.0f);
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
    if (interactive != canDrag)
    {
        canDrag = interactive;
        setMouseCursor (interactive ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
        repaint();
    }
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

    vpdraw::dbScale (g, [this] (float v) { return valueToY (v); }, cx, true);
    vpdraw::faderSlot (g, cx, t.getY(), t.getBottom());

    const float zeroY = valueToY (0.0f), y = valueToY (shownDb);
    vpdraw::glowBar (g, Rectangle<float> (cx - 4.0f, jmin (zeroY, y), 8.0f, std::abs (zeroY - y)), vpcol::gain);
    vpdraw::faderCap (g, Rectangle<float> (44.0f, 28.0f).withCentre ({ cx, y }), canDrag ? vpcol::readCol : vpcol::gain);
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
    auto r = getLocalBounds().toFloat().reduced (3.0f, 2.0f);
    vpdraw::recess (g, r, 3.0f, Colour (0xff030506));
    const float norm = jlimit (0.0f, 1.0f, jmap (level, -60.0f, 0.0f, 0.0f, 1.0f));
    auto lit = r.reduced (2.0f).removeFromBottom ((r.getHeight() - 4.0f) * norm);
    vpdraw::glowBar (g, lit, colour);
}

//==============================================================================
// Editor
//==============================================================================
static constexpr int kW = 1000, kH = 600, kHeader = 74;

static Rectangle<int> levelPanelBounds()   { return { 12, kHeader + 10, 240, kH - kHeader - 22 }; }
static Rectangle<int> historyPanelBounds() { return { 262, kHeader + 10, kW - 262 - 312, kH - kHeader - 22 }; }
static Rectangle<int> gainPanelBounds()    { return { kW - 302, kHeader + 10, 290, kH - kHeader - 22 }; }

VoxPilotEditor::VoxPilotEditor (VoxPilotProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      rangeLowAtt  (*p.apvts.getParameter (ParamIDs::rangeLow),
                    [this] (float v) { rangeSlider.setMinValue (v, dontSendNotification, true); }, nullptr),
      rangeHighAtt (*p.apvts.getParameter (ParamIDs::rangeHigh),
                    [this] (float v) { rangeSlider.setMaxValue (v, dontSendNotification, true); }, nullptr),
      riderFader (p), history (p)
{
    setupKnob (targetKnob, targetLabel, "TARGET");
    setupKnob (detectKnob, detectLabel, "VOCAL DETECT");
    setupKnob (bleedKnob,  bleedLabel,  "BLEED GUARD");
    setupKnob (musicKnob,  musicLabel,  "MUSIC FOLLOW");
    targetAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::target, targetKnob);
    detectAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::detect, detectKnob);
    bleedAtt  = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::bleed,  bleedKnob);
    musicAtt  = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::musicFollow, musicKnob);
    targetKnob.setTextBoxStyle (Slider::TextBoxBelow, false, 96, 30);
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
    rangeSlider.setTooltip ("Maximum cut (lower handle) and boost (upper handle) the rider may apply.");
    addAndMakeVisible (rangeSlider);

    outputAtt = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::output, outputSlider);
    outputSlider.setSliderStyle (Slider::LinearVertical);
    outputSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 30);
    outputSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (outputSlider);

    auto setupCaption = [this] (Label& l, const String& t)
    {
        l.setText (t, dontSendNotification);
        l.setFont (uiFont (15.0f, true));
        l.setColour (Label::textColourId, vpcol::text);
        l.setJustificationType (Justification::centred);
        addAndMakeVisible (l);
    };
    setupCaption (rangeLabel, "RANGE");
    setupCaption (riderLabel, "RIDER");
    setupCaption (outputLabel, "OUTPUT");

    for (auto* l : { &rangeValue, &riderValue })
    {
        VPLookAndFeel::makeValueDisplay (*l);
        l->setColour (Label::textColourId, vpcol::text);
        addAndMakeVisible (*l);
    }
    riderValue.setColour (Label::textColourId, vpcol::gain);

    addAndMakeVisible (riderFader);
    addAndMakeVisible (outMeter);
    addAndMakeVisible (inMeter);
    inMeter.colour = vpcol::vocal.brighter (0.3f);
    inMeter.setTooltip ("Vocal input level");
    outMeter.setTooltip ("Output level");
    addAndMakeVisible (history);

    // buttons
    auto setupBtn = [this] (TextButton& b, Colour on, Colour onText, int group)
    {
        b.setClickingTogglesState (false);
        b.setRadioGroupId (group);
        b.setColour (TextButton::buttonOnColourId, on);
        b.setColour (TextButton::textColourOnId, onText);
        addAndMakeVisible (b);
    };
    setupBtn (fastBtn,  vpcol::accent,   vpcol::darkText, 1);
    setupBtn (slowBtn,  vpcol::accent,   vpcol::darkText, 1);
    setupBtn (offBtn,   vpcol::silver,   vpcol::darkText, 2);
    setupBtn (writeBtn, vpcol::writeCol, Colours::white,  2);
    setupBtn (readBtn,  vpcol::readCol,  vpcol::darkText, 2);
    fastBtn.onClick  = [this] { setChoice (ParamIDs::fast, 1.0f); };
    slowBtn.onClick  = [this] { setChoice (ParamIDs::fast, 0.0f); };
    offBtn.onClick   = [this] { setChoice (ParamIDs::autoMode, 0.0f); };
    writeBtn.onClick = [this] { setChoice (ParamIDs::autoMode, 0.5f); };
    readBtn.onClick  = [this] { setChoice (ParamIDs::autoMode, 1.0f); };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = lookaheadBtns[(size_t) i];
        b.setButtonText (i == 0 ? String ("OFF") : String (roundToInt (lookaheadChoicesMs[i])));
        setupBtn (b, vpcol::gain, vpcol::darkText, 3);
        b.onClick = [this, i] { setChoice (ParamIDs::lookahead, (float) i / 3.0f); };
        b.setTooltip ("Lookahead: the detector looks ahead so the gain is already right when a phrase starts. "
                      "Adds this much latency (Logic compensates automatically). Use OFF for live/recording.");
    }
    writeBtn.setTooltip ("Writes the rider moves to the 'Rider Gain' parameter. Put the track's automation in Latch or Touch in Logic.");
    readBtn.setTooltip ("Plays back the 'Rider Gain' automation (or lets you draw it by hand on the fader).");

    // set last so every child (and its text box) is rebuilt with our look & feel
    setLookAndFeel (&lnf);
    setOpaque (true);
    setSize (kW, kH);
    timerCallback();
    startTimerHz (30);
}

VoxPilotEditor::~VoxPilotEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoxPilotEditor::setupKnob (Slider& s, Label& l, const String& name)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 28);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setVelocityBasedMode (false);
    s.setMouseDragSensitivity (220);
    addAndMakeVisible (s);

    l.setText (name, dontSendNotification);
    l.setFont (uiFont (15.0f, true));
    l.setColour (Label::textColourId, vpcol::text);
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

void VoxPilotEditor::buildBackground()
{
    // Rendered once at 2x: brushed slate metal with vignette and a header band.
    const int scale = 2;
    background = Image (Image::RGB, kW * scale, kH * scale, true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale ((float) scale));

    g.setGradientFill (ColourGradient (vpcol::bgTop, 0.0f, 0.0f, vpcol::bgBottom, 0.0f, (float) kH, false));
    g.fillAll();

    Random rnd (20251114);
    for (int y = 0; y < kH * 2; ++y)
    {
        const float yy = (float) y * 0.5f;
        for (int k = 0; k < 4; ++k)
        {
            const float x0 = rnd.nextFloat() * (float) kW;
            const float len = 40.0f + rnd.nextFloat() * 500.0f;
            const bool light = rnd.nextBool();
            g.setColour ((light ? Colours::white : Colours::black).withAlpha (0.012f + rnd.nextFloat() * 0.03f));
            g.fillRect (x0, yy, len, 0.5f);
        }
    }

    // vignette
    g.setGradientFill (ColourGradient (Colours::transparentBlack, kW * 0.5f, kH * 0.45f,
                                       Colours::black.withAlpha (0.45f), 0.0f, 0.0f, true));
    g.fillAll();

    // header band
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.05f), 0.0f, 0.0f,
                                       Colours::black.withAlpha (0.2f), 0.0f, (float) kHeader, false));
    g.fillRect (0, 0, kW, kHeader);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillRect (0.0f, (float) kHeader - 2.0f, (float) kW, 1.5f);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.fillRect (0.0f, (float) kHeader - 0.5f, (float) kW, 1.0f);

    // outer frame
    g.setColour (Colours::black);
    g.drawRect (0, 0, kW, kH, 2);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRect (2, 2, kW - 4, kH - 4, 1);

    vpdraw::screw (g, { 9.0f, 9.0f }, 4.0f, 0.6f);
    vpdraw::screw (g, { (float) kW - 9.0f, 9.0f }, 4.0f, 2.1f);

    // panels
    auto panel = [&g] (Rectangle<int> r)
    {
        auto rf = r.toFloat();
        vpdraw::bevelPanel (g, rf, 9.0f);
        const float in = 13.0f;
        vpdraw::screw (g, { rf.getX() + in, rf.getY() + in }, 5.5f, 0.4f);
        vpdraw::screw (g, { rf.getRight() - in, rf.getY() + in }, 5.5f, 1.3f);
        vpdraw::screw (g, { rf.getX() + in, rf.getBottom() - in }, 5.5f, 2.4f);
        vpdraw::screw (g, { rf.getRight() - in, rf.getBottom() - in }, 5.5f, 0.9f);
    };
    panel (levelPanelBounds());
    panel (historyPanelBounds());
    panel (gainPanelBounds());
}

void VoxPilotEditor::drawPanel (Graphics& g, Rectangle<int> r, const String& title)
{
    g.setColour (Colours::black.withAlpha (0.6f));
    g.setFont (uiFont (17.0f, true));
    g.drawText (title, r.withHeight (28).withTrimmedLeft (30).translated (0, 5), Justification::centredLeft);
    g.setColour (vpcol::text);
    g.drawText (title, r.withHeight (28).withTrimmedLeft (30).translated (0, 4), Justification::centredLeft);
}

void VoxPilotEditor::paint (Graphics& g)
{
    if (background.isNull())
        buildBackground();
    g.drawImage (background, getLocalBounds().toFloat());

    // logo: waveform glyph + name
    {
        Path wave;
        const float x0 = 22.0f, cy = 36.0f;
        wave.startNewSubPath (x0, cy);
        for (int i = 1; i <= 30; ++i)
        {
            const float t = (float) i / 30.0f;
            const float env = std::sin (t * MathConstants<float>::pi);
            wave.lineTo (x0 + t * 40.0f, cy - std::sin (t * 24.0f) * 13.0f * env);
        }
        vpdraw::glowStroke (g, wave, vpcol::accent, 2.2f);
        g.setColour (vpcol::gain);
        g.fillRoundedRectangle (x0 + 2.0f, cy - 19.0f, 34.0f, 2.5f, 1.0f);

        g.setFont (uiFont (30.0f, true));
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawText ("VOX", 72, 8, 70, 32, Justification::centredLeft);
        g.setColour (vpcol::text);
        g.drawText ("VOX", 72, 7, 70, 32, Justification::centredLeft);
        g.setColour (vpcol::accent);
        g.drawText ("PILOT", 133, 7, 110, 32, Justification::centredLeft);
        g.setFont (uiFont (11.5f, true));
        g.setColour (vpcol::text.withAlpha (0.9f));
        g.drawText ("AUTOMATIC VOCAL LEVEL RIDER", 73, 37, 200, 14, Justification::centredLeft);
        g.setFont (uiFont (9.5f, true));
        g.setColour (vpcol::text.withAlpha (0.75f));
        g.drawText (String::fromUTF8 ("SB AUDIO \xc2\xb7 STUDIO SERIES"), 73, 53, 186, 12, Justification::centred);
        g.fillRect (60.0f, 59.0f, 40.0f, 1.0f);
        g.fillRect (232.0f, 59.0f, 40.0f, 1.0f);
    }

    // status LEDs
    g.setFont (uiFont (14.0f, true));
    vpdraw::led (g, { 292.0f, 26.0f }, 7.5f, vpcol::readCol, rideLed);
    g.setColour (rideLed ? vpcol::text : vpcol::textDim);
    g.drawText (rideLed ? "RIDING" : "IDLE", 307, 18, 85, 16, Justification::centredLeft);
    const bool scSignal = scLed && scLevel > -70.0f;
    vpdraw::led (g, { 292.0f, 52.0f }, 7.5f, vpcol::music, scSignal);
    g.setColour (scLed ? vpcol::text : vpcol::textDim);
    g.setFont (uiFont (scLed ? 14.0f : 12.0f, true));
    g.drawText (scLed ? "SIDECHAIN" : "NO SIDECHAIN", 307, 44, 88, 16, Justification::centredLeft);

    // group separators + titles
    auto separator = [&g] (float x)
    {
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillRect (x, 10.0f, 1.0f, (float) kHeader - 22.0f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillRect (x + 1.0f, 10.0f, 1.0f, (float) kHeader - 22.0f);
    };
    separator (394.0f);
    separator (733.0f);

    g.setFont (uiFont (12.5f, true));
    g.setColour (vpcol::text.withAlpha (0.9f));
    g.drawText ("RESPONSE", fastBtn.getX() + 2, 6, 120, 14, Justification::centredLeft);
    g.drawText ("LOOKAHEAD (MS)", lookaheadBtns[0].getX() + 2, 6, 170, 14, Justification::centredLeft);
    g.drawText ("AUTOMATION", offBtn.getX() + 2, 6, 200, 14, Justification::centredLeft);

    drawPanel (g, levelPanelBounds(), "LEVEL");
    drawPanel (g, historyPanelBounds(), "RIDE HISTORY");
    drawPanel (g, gainPanelBounds(), "GAIN");
}

void VoxPilotEditor::resized()
{
    // header controls
    fastBtn.setBounds (404, 24, 62, 38);
    slowBtn.setBounds (468, 24, 62, 38);
    for (int i = 0; i < 4; ++i)
        lookaheadBtns[(size_t) i].setBounds (542 + i * 45, 24, 43, 38);
    offBtn.setBounds   (742, 24, 78, 38);
    writeBtn.setBounds (821, 24, 78, 38);
    readBtn.setBounds  (900, 24, 78, 38);

    // left panel
    const auto lp = levelPanelBounds();
    const int lx = lp.getX(), ly = lp.getY();
    targetLabel.setBounds (lx, ly + 36, lp.getWidth(), 18);
    targetKnob.setBounds (lx + 38, ly + 54, 164, 168);

    detectLabel.setBounds (lx + 2, ly + 230, 118, 18);
    detectKnob.setBounds  (lx + 12, ly + 248, 98, 106);
    bleedLabel.setBounds  (lx + 120, ly + 230, 118, 18);
    bleedKnob.setBounds   (lx + 130, ly + 248, 98, 106);
    musicLabel.setBounds  (lx + 61, ly + 362, 118, 18);
    musicKnob.setBounds   (lx + 71, ly + 380, 98, 106);

    // centre
    const auto hp = historyPanelBounds();
    history.setBounds (hp.getX() + 16, hp.getY() + 40, hp.getWidth() - 32, hp.getHeight() - 40 - 28);

    // right panel: Range | Rider | Output + meters
    const auto gp = gainPanelBounds();
    const int rx = gp.getX(), top = gp.getY() + 40;
    const int faderTop = top + 24, faderH = 376;
    rangeLabel.setBounds (rx + 8, top, 84, 18);
    rangeSlider.setBounds (rx + 8, faderTop, 84, faderH);
    rangeValue.setBounds (rx + 12, faderTop + faderH + 12, 76, 30);

    riderLabel.setBounds (rx + 100, top, 84, 18);
    riderFader.setBounds (rx + 100, faderTop - 4, 84, faderH + 8);
    riderValue.setBounds (rx + 104, faderTop + faderH + 12, 76, 30);

    outputLabel.setBounds (rx + 186, top, 84, 18);
    outputSlider.setBounds (rx + 186, faderTop, 76, faderH + 42);
    inMeter.setBounds (rx + 258, faderTop + 2, 13, faderH - 36);
    outMeter.setBounds (rx + 272, faderTop + 2, 13, faderH - 36);
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
        repaint (276, 0, 116, kHeader);
    }
    scLevel = scl;
}
