#include "PluginEditor.h"

using namespace juce;

static Font uiFont (float h, bool bold = true)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain));
}

static Font serifItalic (float h)
{
    return Font (FontOptions (Font::getDefaultSerifFontName(), h, Font::italic));
}

static Rectangle<float> circleRect (Point<float> c, float r)
{
    return Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
}

/** Text with a light lip below and dark shadow above → looks stamped into the panel. */
static void engravedText (Graphics& g, const String& t, Rectangle<float> r, Justification j, Colour c)
{
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawText (t, r.translated (0.0f, -1.0f), j, false);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawText (t, r.translated (0.0f, 1.0f), j, false);
    g.setColour (c);
    g.drawText (t, r, j, false);
}

//==============================================================================
// Drawing helpers
//==============================================================================
namespace vpdraw
{
void glowStroke (Graphics& g, const Path& p, Colour c, float w)
{
    const auto j = PathStrokeType::curved;
    const auto e = PathStrokeType::rounded;
    g.setColour (c.withAlpha (0.07f)); g.strokePath (p, PathStrokeType (w * 6.0f, j, e));
    g.setColour (c.withAlpha (0.18f)); g.strokePath (p, PathStrokeType (w * 2.8f, j, e));
    g.setColour (c);                   g.strokePath (p, PathStrokeType (w, j, e));
}

void screw (Graphics& g, Point<float> c, float rad, float angle)
{
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (circleRect (c.translated (0.5f, 1.5f), rad + 1.0f));
    g.setGradientFill (ColourGradient (Colour (0xffe9ecee), c.x - rad, c.y - rad, Colour (0xff5d6369), c.x + rad, c.y + rad, false));
    g.fillEllipse (circleRect (c, rad));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (circleRect (c, rad), 0.8f);

    // Phillips head
    for (float a : { angle, angle + MathConstants<float>::halfPi })
    {
        const auto d = Point<float> (std::cos (a), std::sin (a)) * (rad * 0.62f);
        g.setColour (Colour (0xff23272a));
        g.drawLine ({ c - d, c + d }, jmax (1.2f, rad * 0.22f));
    }
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawEllipse (circleRect (c, rad * 0.8f).translated (-0.4f, -0.4f), 0.5f);
}

void jewelLamp (Graphics& g, Point<float> c, float r, Colour col, bool on)
{
    if (on)
    {
        g.setGradientFill (ColourGradient (col.withAlpha (0.55f), c.x, c.y, col.withAlpha (0.0f), c.x + r * 2.8f, c.y, true));
        g.fillEllipse (circleRect (c, r * 2.8f));
    }
    // chrome bezel
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (circleRect (c.translated (0.0f, 2.0f), r + 4.0f));
    g.setGradientFill (ColourGradient (Colour (0xfff2f3f4), c.x, c.y - r - 4.0f, Colour (0xff50565b), c.x, c.y + r + 4.0f, false));
    g.fillEllipse (circleRect (c, r + 4.0f));
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillEllipse (circleRect (c, r + 0.8f));

    // glass
    const auto base = on ? col : col.withMultipliedBrightness (0.35f).withMultipliedSaturation (0.7f);
    ColourGradient glass (on ? base.brighter (0.9f) : base.brighter (0.3f), c.x - r * 0.2f, c.y - r * 0.25f,
                          base.darker (on ? 0.35f : 0.6f), c.x + r, c.y + r, true);
    glass.addColour (0.45, base);
    g.setGradientFill (glass);
    g.fillEllipse (circleRect (c, r));
    // facets + highlight
    g.setColour (Colours::white.withAlpha (on ? 0.85f : 0.4f));
    g.fillEllipse (Rectangle<float> (r * 0.8f, r * 0.5f).withCentre ({ c.x - r * 0.28f, c.y - r * 0.45f }));
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawEllipse (circleRect (c, r * 0.65f), 0.6f);
}

void plaque (Graphics& g, Rectangle<float> r, Colour fill, bool lit)
{
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (1.0f), 3.5f);
    if (lit)
    {
        g.setColour (fill.withAlpha (0.35f));
        g.fillRoundedRectangle (r.expanded (4.0f), 6.0f);
    }
    g.setGradientFill (ColourGradient (fill.brighter (lit ? 0.45f : 0.35f), 0.0f, r.getY(),
                                       fill.darker (0.35f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Colours::white.withAlpha (lit ? 0.45f : 0.22f));
    g.fillRect (r.getX() + 3.0f, r.getY() + 1.0f, r.getWidth() - 6.0f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.9f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

void brushedMetal (Graphics& g, Rectangle<float> r, bool vertical, Colour light, Colour dark, int seed)
{
    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (r.getSmallestIntegerContainer());
    if (vertical)
    {
        ColourGradient grad (dark, r.getX(), 0.0f, dark.darker (0.2f), r.getRight(), 0.0f, false);
        grad.addColour (0.35, light);
        grad.addColour (0.6, light.darker (0.08f));
        g.setGradientFill (grad);
    }
    else
    {
        ColourGradient grad (light, 0.0f, r.getY(), dark, 0.0f, r.getBottom(), false);
        grad.addColour (0.5, light.darker (0.1f));
        g.setGradientFill (grad);
    }
    g.fillRect (r);

    Random rnd (seed);
    const int n = (int) ((vertical ? r.getWidth() : r.getHeight()) * 3.0f);
    for (int i = 0; i < n; ++i)
    {
        const bool lightLine = rnd.nextBool();
        g.setColour ((lightLine ? Colours::white : Colours::black).withAlpha (0.03f + rnd.nextFloat() * 0.07f));
        if (vertical)
        {
            const float x = r.getX() + rnd.nextFloat() * r.getWidth();
            const float y0 = r.getY() + rnd.nextFloat() * r.getHeight();
            g.fillRect (x, y0, 0.6f, 30.0f + rnd.nextFloat() * r.getHeight() * 0.6f);
        }
        else
        {
            const float y = r.getY() + rnd.nextFloat() * r.getHeight();
            const float x0 = r.getX() + rnd.nextFloat() * r.getWidth();
            g.fillRect (x0, y, 30.0f + rnd.nextFloat() * r.getWidth() * 0.6f, 0.6f);
        }
    }
}
} // namespace vpdraw

//==============================================================================
// Look & feel
//==============================================================================
VPLookAndFeel::VPLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, vpcol::panelBot);
    setColour (Label::textColourId, vpcol::text);
    setColour (Slider::textBoxTextColourId, vpcol::text);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, vpcol::amber.withAlpha (0.4f));
    setColour (TextButton::buttonColourId, vpcol::plaque);
    setColour (TextButton::textColourOffId, vpcol::text);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (TextEditor::backgroundColourId, Colour (0xff1d2328));
    setColour (TextEditor::textColourId, vpcol::text);
    setColour (TextEditor::highlightColourId, vpcol::amber.withAlpha (0.4f));
    setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    setColour (TextEditor::focusedOutlineColourId, vpcol::cream.withAlpha (0.6f));
    setColour (CaretComponent::caretColourId, vpcol::cream);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff1d2124));
    setColour (TooltipWindow::textColourId, vpcol::text);
    setColour (TooltipWindow::outlineColourId, vpcol::cream.withAlpha (0.4f));
}

void VPLookAndFeel::setScale (Slider& s, const String& spec)
{
    s.getProperties().set ("vpScale", spec);
}

void VPLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, Slider& s)
{
    const auto area  = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c     = area.getCentre();
    const float R    = jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const bool big   = R > 80.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // --- printed scale: ticks + numbers -----------------------------------------
    const float tickIn = R * 0.70f, tickOut = R * 0.77f;
    const int ticks = big ? 31 : 21;
    g.setColour (vpcol::text.withAlpha (0.85f));
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
        g.drawLine ({ c.getPointOnCircumference (tickIn, a), c.getPointOnCircumference (tickOut, a) }, 1.4f);
    }

    const auto spec = s.getProperties().getWithDefault ("vpScale", String()).toString();
    if (spec.isNotEmpty())
    {
        g.setFont (uiFont (jlimit (11.0f, 15.0f, R * 0.14f)));
        for (auto& item : StringArray::fromTokens (spec, ",", ""))
        {
            const auto text  = item.upToFirstOccurrenceOf (":", false, false);
            const auto value = item.containsChar (':') ? item.fromFirstOccurrenceOf (":", false, false).getDoubleValue()
                                                       : text.getDoubleValue();
            const float p = (float) s.valueToProportionOfLength (value);
            const float a = startAngle + p * (endAngle - startAngle);
            g.setColour (vpcol::text);
            g.drawLine ({ c.getPointOnCircumference (tickIn - 2.0f, a), c.getPointOnCircumference (tickOut + 3.0f, a) }, 2.0f);
            const auto tp = c.getPointOnCircumference (R * 0.90f, a);
            engravedText (g, text, Rectangle<float> (40.0f, 16.0f).withCentre (tp), Justification::centred, vpcol::text);
        }
    }

    // --- chrome skirt ------------------------------------------------------------
    const float rs = R * 0.62f;
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (circleRect (c.translated (0.0f, R * 0.06f), rs * 1.03f));
    g.setGradientFill (ColourGradient (Colour (0xfff4f5f6), c.x - rs * 0.6f, c.y - rs, Colour (0xff6f757b), c.x + rs * 0.6f, c.y + rs, false));
    g.fillEllipse (circleRect (c, rs));

    // spun-metal look: fine radial strokes + soft "anisotropic" highlights
    Random rnd (7);
    for (int i = 0; i < 160; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / 160.0f;
        const bool lightLine = rnd.nextBool();
        g.setColour ((lightLine ? Colours::white : Colours::black).withAlpha (0.04f + rnd.nextFloat() * 0.07f));
        g.drawLine ({ c.getPointOnCircumference (rs * 0.55f, a), c.getPointOnCircumference (rs * 0.97f, a) }, 0.8f);
    }
    for (float a : { -0.6f, MathConstants<float>::pi - 0.6f })
    {
        Path sheen;
        sheen.addPieSegment (circleRect (c, rs * 0.97f), a - 0.25f, a + 0.25f, 0.55f);
        g.setColour (Colours::white.withAlpha (0.22f));
        g.fillPath (sheen);
    }
    // knurled rim
    const int teeth = big ? 90 : 64;
    for (int i = 0; i < teeth; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / (float) teeth;
        g.setColour (Colours::black.withAlpha (0.28f));
        g.drawLine ({ c.getPointOnCircumference (rs * 0.91f, a), c.getPointOnCircumference (rs * 0.995f, a) }, 0.9f);
    }
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (circleRect (c, rs), 1.0f);

    // --- black bakelite cap ------------------------------------------------------
    const float rc = big ? R * 0.47f : R * 0.40f;
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (circleRect (c.translated (0.0f, R * 0.05f), rc * 1.05f));

    Path cap;
    if (big)
    {
        const int flutes = 14;
        for (int i = 0; i <= 180; ++i)
        {
            const float t = MathConstants<float>::twoPi * (float) i / 180.0f;
            const float rr = rc * (1.0f - 0.045f * (1.0f - std::cos ((float) flutes * t)));
            const auto p = c.getPointOnCircumference (rr, t);
            if (i == 0) cap.startNewSubPath (p); else cap.lineTo (p);
        }
        cap.closeSubPath();
    }
    else
    {
        cap.addEllipse (circleRect (c, rc));
    }
    g.setGradientFill (ColourGradient (Colour (0xff4a4a4a), c.x, c.y - rc, Colour (0xff050505), c.x, c.y + rc, false));
    g.fillPath (cap);
    g.setColour (Colours::black);
    g.strokePath (cap, PathStrokeType (1.0f));

    // cap top face
    const float rt = rc * (big ? 0.80f : 0.84f);
    g.setGradientFill (ColourGradient (Colour (0xff2e2e2e), c.x - rt * 0.4f, c.y - rt, Colour (0xff0d0d0d), c.x + rt * 0.4f, c.y + rt, false));
    g.fillEllipse (circleRect (c, rt));
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), c.x, c.y - rt,
                                       Colours::white.withAlpha (0.0f), c.x, c.y - rt * 0.1f, false));
    g.fillEllipse (circleRect (c, rt).reduced (rt * 0.12f, 0.0f).withTrimmedBottom (rt));

    // cream pointer line
    Path ptr;
    ptr.startNewSubPath (c.getPointOnCircumference (rt * 0.15f, angle));
    ptr.lineTo (c.getPointOnCircumference (rc * 0.93f, angle));
    const float pw = jmax (3.0f, R * 0.055f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.strokePath (ptr, PathStrokeType (pw + 2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (vpcol::cream);
    g.strokePath (ptr, PathStrokeType (pw, PathStrokeType::curved, PathStrokeType::rounded));
}

void VPLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (4.0f, 4.0f);
    if (down) r = r.translated (0.0f, 1.0f);
    const bool on = b.getToggleState();
    vpdraw::plaque (g, r, on ? b.findColour (TextButton::buttonOnColourId)
                             : vpcol::plaque.brighter (over ? 0.12f : 0.0f), on);
}

void VPLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (4.0f).translated (0.0f, down ? 1.0f : 0.0f);
    const bool on = b.getToggleState();
    g.setFont (uiFont (15.0f));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawText (b.getButtonText(), r.translated (0.0f, 1.0f), Justification::centred, false);
    g.setColour (b.findColour (on ? TextButton::textColourOnId : TextButton::textColourOffId));
    g.drawText (b.getButtonText(), r, Justification::centred, false);
}

Font VPLookAndFeel::getLabelFont (Label& l) { return l.getFont(); }

void VPLookAndFeel::makePlaque (Label& l, Colour fill)
{
    l.getProperties().set ("vpPlaque", (int64) fill.getARGB());
    l.setFont (uiFont (15.5f));
    l.setJustificationType (Justification::centred);
    l.setColour (Label::textColourId, Colours::white);
    l.setInterceptsMouseClicks (false, false);
}

void VPLookAndFeel::drawLabel (Graphics& g, Label& l)
{
    const auto prop = l.getProperties()["vpPlaque"];
    if (prop.isVoid())
    {
        LookAndFeel_V4::drawLabel (g, l);
        return;
    }
    const auto fill = Colour ((uint32) (int64) prop);
    auto r = l.getLocalBounds().toFloat().reduced (3.0f, 3.0f);
    vpdraw::plaque (g, r, fill, fill != vpcol::plaque);
    g.setFont (l.getFont());
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawText (l.getText(), r.translated (0.0f, 1.0f), Justification::centred, false);
    g.setColour (l.findColour (Label::textColourId));
    g.drawText (l.getText(), r, Justification::centred, false);
}

Label* VPLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (17.0f, false));
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::textColourId, vpcol::text);
    l->setWantsKeyboardFocus (false); // otherwise it opens its editor when the window gets focus
    return l;
}

//==============================================================================
// History "scope"
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

Rectangle<float> HistoryView::screen() const    { return getLocalBounds().toFloat().reduced (22.0f, 18.0f); }
Rectangle<float> HistoryView::levelPlot() const
{
    auto s = screen().reduced (8.0f, 8.0f).withTrimmedLeft (24.0f).withTrimmedRight (24.0f);
    return s.withHeight (s.getHeight() * 0.64f);
}
Rectangle<float> HistoryView::gainPlot() const
{
    auto s = screen().reduced (8.0f, 8.0f).withTrimmedLeft (24.0f).withTrimmedRight (24.0f);
    return s.withTop (levelPlot().getBottom() + 8.0f);
}

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
    const auto b = getLocalBounds().toFloat();

    // black bezel with screws
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b.translated (0.0f, 4.0f).reduced (2.0f, 0.0f), 10.0f);
    g.setGradientFill (ColourGradient (Colour (0xff34373a), 0.0f, b.getY(), Colour (0xff0c0d0e), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (b.reduced (1.5f), 9.0f, 1.0f);
    for (auto p : { Point<float> (b.getX() + 11.0f, b.getY() + 10.0f), Point<float> (b.getRight() - 11.0f, b.getY() + 10.0f),
                    Point<float> (b.getX() + 11.0f, b.getBottom() - 10.0f), Point<float> (b.getRight() - 11.0f, b.getBottom() - 10.0f) })
        vpdraw::screw (g, p, 4.0f, 0.5f);

    g.setFont (uiFont (10.0f));
    g.setColour (vpcol::text.withAlpha (0.6f));
    g.drawText ("RIDE HISTORY", Rectangle<float> (b.getX() + 30.0f, b.getY() + 3.0f, 120.0f, 14.0f), Justification::centredLeft);

    // phosphor screen
    const auto s = screen();
    g.setColour (Colours::black);
    g.fillRoundedRectangle (s.expanded (2.0f), 7.0f);
    g.setGradientFill (ColourGradient (Colour (0xff142019), s.getCentreX(), s.getCentreY(), vpcol::scopeBg, s.getX(), s.getY(), true));
    g.fillRoundedRectangle (s, 6.0f);

    const auto la = levelPlot(), ga = gainPlot();
    const auto grid = vpcol::trace.withAlpha (0.08f);
    g.setColour (grid);
    for (int i = 0; i <= 10; ++i)
    {
        const float xx = la.getX() + la.getWidth() * (float) i / 10.0f;
        g.fillRect (xx, la.getY(), 1.0f, la.getHeight());
        g.fillRect (xx, ga.getY(), 1.0f, ga.getHeight());
    }
    g.setFont (uiFont (10.0f, false));
    for (int db = 0; db >= (int) minDb; db -= 12)
    {
        const float yy = levelToY ((float) db);
        g.setColour (grid);
        g.fillRect (la.getX(), yy - 0.5f, la.getWidth(), 1.0f);
        g.setColour (vpcol::trace.withAlpha (0.5f));
        g.drawText (String (db), Rectangle<float> (s.getX() + 2.0f, yy - 6.0f, 26.0f, 12.0f), Justification::centredRight);
    }
    for (int db : { -12, 0, 12 })
    {
        const float yy = gainToY ((float) db);
        g.setColour (db == 0 ? vpcol::gainTrace.withAlpha (0.25f) : grid);
        g.fillRect (ga.getX(), yy - 0.5f, ga.getWidth(), 1.0f);
        g.setColour (vpcol::gainTrace.withAlpha (0.6f));
        g.drawText (db > 0 ? "+" + String (db) : String (db),
                    Rectangle<float> (ga.getRight() + 2.0f, yy - 6.0f, 26.0f, 12.0f), Justification::centredLeft);
    }

    const float dx = la.getWidth() / (float) (capacity - 1);
    auto frameAt = [this] (int i) -> const HistoryFrame& { return frames[(size_t) ((writePos + i) % capacity)]; };

    Path vocalFill, outLine, targetLine, thrLine, gainLine;
    for (int i = 0; i < capacity; ++i)
    {
        const auto& f = frameAt (i);
        const float xx = la.getX() + (float) i * dx;
        const float vy = levelToY (f.vocalDb), oy = levelToY (f.outDb);
        const float ty = levelToY (f.targetDb), hy = levelToY (f.thresholdDb), gy = gainToY (f.gainDb);
        if (i == 0)
        {
            vocalFill.startNewSubPath (xx, la.getBottom());
            vocalFill.lineTo (xx, vy);
            outLine.startNewSubPath (xx, oy);
            targetLine.startNewSubPath (xx, ty);
            thrLine.startNewSubPath (xx, hy);
            gainLine.startNewSubPath (xx, gy);
        }
        else
        {
            vocalFill.lineTo (xx, vy);
            outLine.lineTo (xx, oy);
            targetLine.lineTo (xx, ty);
            thrLine.lineTo (xx, hy);
            gainLine.lineTo (xx, gy);
        }
    }
    vocalFill.lineTo (la.getRight(), la.getBottom());
    vocalFill.closeSubPath();

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (la.toNearestInt());
        g.setColour (vpcol::vocal.withAlpha (0.35f));
        g.fillPath (vocalFill);

        const float dashes[] = { 5.0f, 4.0f };
        Path dashed;
        PathStrokeType (1.2f).createDashedStroke (dashed, thrLine, dashes, 2);
        g.setColour (vpcol::detect.withAlpha (0.85f));
        g.fillPath (dashed);

        dashed.clear();
        PathStrokeType (draggingTarget ? 2.2f : 1.6f).createDashedStroke (dashed, targetLine, dashes, 2);
        g.setColour (vpcol::cream.withAlpha (0.9f));
        g.fillPath (dashed);

        vpdraw::glowStroke (g, outLine, vpcol::trace, 1.4f);
    }
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (ga.toNearestInt());
        vpdraw::glowStroke (g, gainLine, vpcol::gainTrace, 1.6f);
    }

    // legend
    struct Item { const char* name; Colour c; };
    const Item items[] = { { "VOCAL", vpcol::vocal.brighter (0.6f) }, { "OUTPUT", vpcol::trace }, { "TARGET", vpcol::cream },
                           { "DETECT", vpcol::detect }, { "GAIN", vpcol::gainTrace } };
    float lx = la.getRight() - 5.0f * 64.0f;
    g.setFont (uiFont (10.0f));
    for (auto& it : items)
    {
        g.setColour (it.c);
        g.fillRect (lx, la.getY() + 6.0f, 9.0f, 2.5f);
        g.drawText (it.name, Rectangle<float> (lx + 12.0f, la.getY() + 1.0f, 52.0f, 12.0f), Justification::centredLeft);
        lx += 64.0f;
    }

    // target handle
    const float ty = levelToY (frameAt (capacity - 1).targetDb);
    Path tri;
    tri.addTriangle (la.getRight() + 2.0f, ty, la.getRight() + 10.0f, ty - 5.0f, la.getRight() + 10.0f, ty + 5.0f);
    g.setColour (vpcol::cream);
    g.fillPath (tri);

    // CRT vignette + glass
    g.setGradientFill (ColourGradient (Colours::transparentBlack, s.getCentreX(), s.getCentreY(),
                                       Colours::black.withAlpha (0.45f), s.getX(), s.getY(), true));
    g.fillRoundedRectangle (s, 6.0f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), 0.0f, s.getY(),
                                       Colours::white.withAlpha (0.0f), 0.0f, s.getCentreY(), false));
    g.fillRoundedRectangle (s, 6.0f);
}

void HistoryView::mouseMove (const MouseEvent& e)
{
    const float ty = levelToY (proc.apvts.getRawParameterValue (ParamIDs::target)->load());
    setMouseCursor (std::abs (e.position.y - ty) < 6.0f && screen().contains (e.position)
                        ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
}

void HistoryView::mouseDown (const MouseEvent& e)
{
    const float ty = levelToY (proc.apvts.getRawParameterValue (ParamIDs::target)->load());
    if (std::abs (e.position.y - ty) < 6.0f && screen().contains (e.position))
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
// Rider needle meter
//==============================================================================
RiderMeter::RiderMeter (VoxPilotProcessor& p)
    : proc (p), attachment (*p.apvts.getParameter (ParamIDs::rider), [] (float) {}, nullptr)
{
}

Rectangle<float> RiderMeter::face() const { return getLocalBounds().toFloat().reduced (24.0f, 22.0f); }

Point<float> RiderMeter::pivot() const
{
    const auto f = face();
    return { f.getCentreX(), f.getBottom() + f.getHeight() * 0.30f };
}

float RiderMeter::gainToAngle (float db) const
{
    const float lim = VoxPilotProcessor::gainLimitDb;
    return jmap (jlimit (-lim, lim, db), -lim, lim, -0.62f, 0.62f);
}

void RiderMeter::update (float gainDb, bool interactive, AutoMode mode)
{
    if (interactive != canDrag || mode != currentMode)
    {
        canDrag = interactive;
        currentMode = mode;
        setMouseCursor (interactive ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
        repaint();
    }
    if (! dragging) targetDb = gainDb;
    // simple needle ballistics
    const float next = needleDb + (targetDb - needleDb) * 0.4f;
    if (std::abs (next - needleDb) > 0.005f)
    {
        needleDb = next;
        repaint();
    }
}

void RiderMeter::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // black frame
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b.translated (0.0f, 5.0f).reduced (2.0f, 0.0f), 12.0f);
    g.setGradientFill (ColourGradient (Colour (0xff3a3c3e), 0.0f, b.getY(), Colour (0xff0b0b0c), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (Colours::white.withAlpha (0.22f));
    g.drawRoundedRectangle (b.reduced (1.5f), 11.0f, 1.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (b.reduced (9.0f), 8.0f, 2.0f);
    for (auto p : { Point<float> (b.getX() + 12.0f, b.getY() + 11.0f), Point<float> (b.getRight() - 12.0f, b.getY() + 11.0f),
                    Point<float> (b.getX() + 12.0f, b.getBottom() - 11.0f), Point<float> (b.getRight() - 12.0f, b.getBottom() - 11.0f) })
        vpdraw::screw (g, p, 4.5f, 0.7f);

    const auto f = face();
    Graphics::ScopedSaveState save (g);
    Path facePath;
    facePath.addRoundedRectangle (f, 6.0f);
    g.reduceClipRegion (facePath);

    // backlit cream face
    g.setGradientFill (ColourGradient (Colour (0xfffff3cc), f.getCentreX(), f.getY() + f.getHeight() * 0.25f,
                                       vpcol::vuEdge, f.getX(), f.getBottom(), true));
    g.fillRect (f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.45f), f.getCentreX(), f.getY(),
                                       Colours::white.withAlpha (0.0f), f.getCentreX(), f.getY() + f.getHeight() * 0.6f, true));
    g.fillRect (f);

    // scale
    const auto pv = pivot();
    const float arcR = pv.y - (f.getY() + f.getHeight() * 0.40f);
    auto pt = [&] (float db, float r) { const float a = gainToAngle (db); return Point<float> (pv.x + std::sin (a) * r, pv.y - std::cos (a) * r); };

    Path inkArc, redArc;
    inkArc.addCentredArc (pv.x, pv.y, arcR, arcR, 0.0f, gainToAngle (-12.0f), gainToAngle (0.0f), true);
    redArc.addCentredArc (pv.x, pv.y, arcR + 4.0f, arcR + 4.0f, 0.0f, gainToAngle (0.0f), gainToAngle (12.0f), true);
    g.setColour (vpcol::vuInk);
    g.strokePath (inkArc, PathStrokeType (2.0f));
    g.setColour (vpcol::vuRed);
    g.strokePath (redArc, PathStrokeType (6.0f));
    Path thinRed;
    thinRed.addCentredArc (pv.x, pv.y, arcR, arcR, 0.0f, gainToAngle (0.0f), gainToAngle (12.0f), true);
    g.strokePath (thinRed, PathStrokeType (2.0f));

    g.setFont (serifItalic (jlimit (13.0f, 19.0f, f.getHeight() * 0.10f)));
    for (int db = -12; db <= 12; ++db)
    {
        const bool major = db % 3 == 0;
        const auto ink = db > 0 ? vpcol::vuRed : vpcol::vuInk;
        g.setColour (ink);
        g.drawLine ({ pt ((float) db, arcR), pt ((float) db, arcR + (major ? 13.0f : 7.0f)) }, major ? 1.8f : 1.1f);
        if (major)
        {
            const auto lp = pt ((float) db, arcR + 26.0f);
            g.drawText (db > 0 ? "+" + String (db) : String (db), Rectangle<float> (36.0f, 18.0f).withCentre (lp), Justification::centred);
        }
    }

    g.setColour (vpcol::vuInk);
    g.setFont (serifItalic (f.getHeight() * 0.20f));
    g.drawText ("dB", Rectangle<float> (f.getCentreX() - 50.0f, f.getY() + f.getHeight() * 0.52f, 100.0f, f.getHeight() * 0.22f),
                Justification::centred);
    g.setFont (uiFont (10.0f));
    g.setColour (vpcol::vuInk.withAlpha (0.65f));
    g.drawText ("CUT", Rectangle<float> (f.getX() + 14.0f, f.getY() + f.getHeight() * 0.66f, 60.0f, 12.0f), Justification::centredLeft);
    g.drawText ("BOOST", Rectangle<float> (f.getRight() - 74.0f, f.getY() + f.getHeight() * 0.66f, 60.0f, 12.0f), Justification::centredRight);
    g.setFont (uiFont (9.0f));
    g.drawText (String::fromUTF8 ("SB AUDIO \xe2\x80\x93 RIDER GAIN"), Rectangle<float> (f.getX() + 10.0f, f.getBottom() - 16.0f, 180.0f, 12.0f), Justification::centredLeft);

    g.setFont (serifItalic (15.0f));
    g.setColour (vpcol::vuInk);
    g.drawText ((targetDb >= 0.0f ? "+" : "") + String (targetDb, 1) + " dB",
                Rectangle<float> (f.getRight() - 100.0f, f.getBottom() - 20.0f, 90.0f, 16.0f), Justification::centredRight);

    if (currentMode != AutoMode::off)
    {
        const bool w = currentMode == AutoMode::write;
        g.setColour (w ? vpcol::vuRed : Colour (0xff1f7a37));
        g.fillEllipse (circleRect ({ f.getCentreX() - 26.0f, f.getBottom() - 10.0f }, 3.5f));
        g.setFont (uiFont (10.0f));
        g.drawText (w ? "WRITE" : "READ", Rectangle<float> (f.getCentreX() - 18.0f, f.getBottom() - 16.0f, 60.0f, 12.0f), Justification::centredLeft);
    }

    // needle (with soft shadow)
    const float na = gainToAngle (needleDb);
    const auto tip = Point<float> (pv.x + std::sin (na) * (arcR + 14.0f), pv.y - std::cos (na) * (arcR + 14.0f));
    g.setColour (Colours::black.withAlpha (0.2f));
    g.drawLine ({ pv.translated (5.0f, 2.0f), tip.translated (5.0f, 2.0f) }, 2.5f);
    g.setColour (Colour (0xff121212));
    g.drawLine ({ pv, tip }, 2.0f);

    // glass glare and inner shadow
    Path glare;
    glare.startNewSubPath (f.getX(), f.getY());
    glare.lineTo (f.getX() + f.getWidth() * 0.62f, f.getY());
    glare.lineTo (f.getX() + f.getWidth() * 0.30f, f.getBottom());
    glare.lineTo (f.getX(), f.getBottom());
    glare.closeSubPath();
    g.setColour (Colours::white.withAlpha (0.10f));
    g.fillPath (glare);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.4f), 0.0f, f.getY(), Colours::transparentBlack, 0.0f, f.getY() + 16.0f, false));
    g.fillRect (f);
}

void RiderMeter::mouseDown (const MouseEvent&)
{
    if (! canDrag) return;
    dragging = true;
    dragStartDb = targetDb;
    attachment.beginGesture();
}

void RiderMeter::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;
    const float lim = VoxPilotProcessor::gainLimitDb;
    targetDb = jlimit (-lim, lim, dragStartDb + ((float) e.getDistanceFromDragStartX() - (float) e.getDistanceFromDragStartY()) * 0.06f);
    attachment.setValueAsPartOfGesture (targetDb);
}

void RiderMeter::mouseUp (const MouseEvent&)
{
    if (dragging) attachment.endGesture();
    dragging = false;
}

void RiderMeter::mouseDoubleClick (const MouseEvent&)
{
    if (canDrag) attachment.setValueAsCompleteGesture (0.0f);
}

//==============================================================================
// Toggle switch
//==============================================================================
ToggleSwitch::ToggleSwitch (String upText, String downText) : upLabel (std::move (upText)), downLabel (std::move (downText))
{
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void ToggleSwitch::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setFont (uiFont (15.0f));
    engravedText (g, upLabel, b.withHeight (18.0f), Justification::centred, up ? vpcol::text : vpcol::textDim.withAlpha (0.6f));
    engravedText (g, downLabel, b.withTrimmedTop (b.getHeight() - 18.0f), Justification::centred, up ? vpcol::textDim.withAlpha (0.6f) : vpcol::text);

    const auto c = b.getCentre();
    // mounting plate
    auto plate = Rectangle<float> (40.0f, 56.0f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (plate.translated (0.0f, 3.0f), 5.0f);
    vpdraw::brushedMetal (g, plate, false, Colour (0xffd9dcdf), Colour (0xff7d8389), 11);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (plate, 4.0f, 1.0f);

    // hex nut
    Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const auto p = c.getPointOnCircumference (14.0f, MathConstants<float>::pi / 6.0f + (float) i * MathConstants<float>::pi / 3.0f);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xfff5f6f7), c.x - 10.0f, c.y - 12.0f, Colour (0xff5a6066), c.x + 10.0f, c.y + 12.0f, false));
    g.fillPath (hex);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.strokePath (hex, PathStrokeType (1.0f));
    g.setColour (Colour (0xff1a1c1e));
    g.fillEllipse (circleRect (c, 6.5f));

    // bat lever
    const float dir = up ? -1.0f : 1.0f;
    const auto tip = c.translated (0.0f, dir * 36.0f);
    Path lever;
    lever.startNewSubPath (c.x - 4.5f, c.y);
    lever.lineTo (tip.x - 3.0f, tip.y);
    lever.lineTo (tip.x + 3.0f, tip.y);
    lever.lineTo (c.x + 4.5f, c.y);
    lever.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillPath (lever, AffineTransform::translation (3.0f, 3.0f));
    {
        ColourGradient chrome (Colour (0xff6a7076), c.x - 5.0f, 0.0f, Colour (0xff5b6167), c.x + 5.0f, 0.0f, false);
        chrome.addColour (0.4, Colour (0xfffafbfc));
        g.setGradientFill (chrome);
    }
    g.fillPath (lever);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.strokePath (lever, PathStrokeType (0.8f));
    g.setGradientFill (ColourGradient (Colours::white, tip.x - 3.0f, tip.y - 3.0f, Colour (0xff60666c), tip.x + 5.0f, tip.y + 5.0f, true));
    g.fillEllipse (circleRect (tip, 6.0f));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (circleRect (tip, 6.0f), 0.8f);
}

void ToggleSwitch::mouseDown (const MouseEvent& e)
{
    // click on a label selects it; click on the switch itself flips it
    bool flipTo = ! up;
    if (e.position.y < 20.0f) flipTo = true;
    else if (e.position.y > (float) getHeight() - 20.0f) flipTo = false;
    setUp (flipTo);
    if (onFlip) onFlip (flipTo);
}

//==============================================================================
// Editor
//==============================================================================
static constexpr int kW = 1100, kH = 740, kEar = 34;

static const Rectangle<float> nameplateBounds { 390.0f, 16.0f, 320.0f, 72.0f };

VoxPilotEditor::VoxPilotEditor (VoxPilotProcessor& p)
    : AudioProcessorEditor (&p), proc (p), meter (p), history (p)
{
    setupKnob (targetKnob, targetPlaque, "TARGET",       ParamIDs::target,      "-40,-30,-20,-10,0", vpcol::red);
    setupKnob (outputKnob, outputPlaque, "OUTPUT",       ParamIDs::output,      "-12,-6,0,+6:6,+12:12");
    setupKnob (detectKnob, detectPlaque, "VOCAL DETECT", ParamIDs::detect,      "-70,-60,-50,-40,-30,-20");
    setupKnob (bleedKnob,  bleedPlaque,  "BLEED GUARD",  ParamIDs::bleed,       "0:0,2:0.2,4:0.4,6:0.6,8:0.8,10:1");
    setupKnob (musicKnob,  musicPlaque,  "MUSIC FOLLOW", ParamIDs::musicFollow, "0:0,2:0.2,4:0.4,6:0.6,8:0.8,10:1");
    setupKnob (cutKnob,    cutPlaque,    "MAX CUT",      ParamIDs::rangeLow,    "-12,-9,-6,-3,0");
    setupKnob (boostKnob,  boostPlaque,  "MAX BOOST",    ParamIDs::rangeHigh,   "0,+3:3,+6:6,+9:9,+12:12");

    targetKnob.setTooltip ("Desired vocal level. You can also drag the dashed line in the history screen.");
    detectKnob.setTooltip ("Level above which the signal is treated as voice. Below it the rider freezes.");
    bleedKnob.setTooltip ("Ignores bleed/noise: follows the music side-chain, or the noise floor without side-chain.");
    musicKnob.setTooltip ("Requires side-chain: lifts the target when the music gets louder and lowers it in quiet parts.");
    cutKnob.setTooltip ("Maximum amount the rider may turn the vocal down.");
    boostKnob.setTooltip ("Maximum amount the rider may turn the vocal up.");
    outputKnob.setTooltip ("Output trim after the rider.");
    outputKnob.setDoubleClickReturnValue (true, 0.0);

    responsePlaque.setText ("RESPONSE", dontSendNotification);
    VPLookAndFeel::makePlaque (responsePlaque);
    addAndMakeVisible (responsePlaque);
    speedSwitch.onFlip = [this] (bool fastUp) { setChoice (ParamIDs::fast, fastUp ? 1.0f : 0.0f); };
    speedSwitch.setTooltip ("FAST follows words and phrases closely, SLOW rides smoothly.");
    addAndMakeVisible (speedSwitch);

    meter.setTooltip ("Rider gain. In READ mode you can drag the needle (double-click = 0 dB).");
    addAndMakeVisible (meter);
    addAndMakeVisible (history);

    // plaque push-buttons
    auto setupBtn = [this] (TextButton& b, Colour on, Colour onText, int group)
    {
        b.setClickingTogglesState (false);
        b.setRadioGroupId (group);
        b.setColour (TextButton::buttonOnColourId, on);
        b.setColour (TextButton::textColourOnId, onText);
        addAndMakeVisible (b);
    };
    setupBtn (offBtn,   Colour (0xffe8e2cf), vpcol::darkText, 2);
    setupBtn (writeBtn, vpcol::red,          Colours::white,  2);
    setupBtn (readBtn,  Colour (0xff2f9e4c), Colours::white,  2);
    offBtn.onClick   = [this] { setChoice (ParamIDs::autoMode, 0.0f); };
    writeBtn.onClick = [this] { setChoice (ParamIDs::autoMode, 0.5f); };
    readBtn.onClick  = [this] { setChoice (ParamIDs::autoMode, 1.0f); };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = lookaheadBtns[(size_t) i];
        b.setButtonText (i == 0 ? String ("OFF") : String (roundToInt (lookaheadChoicesMs[i])));
        setupBtn (b, Colour (0xffe0821a), vpcol::darkText, 3);
        b.onClick = [this, i] { setChoice (ParamIDs::lookahead, (float) i / 3.0f); };
        b.setTooltip ("Lookahead: the detector looks ahead so the gain is already right when a phrase starts. "
                      "Adds this much latency (Logic compensates automatically). Use OFF for live/recording.");
    }
    writeBtn.setTooltip ("Writes the rider moves to the 'Rider Gain' parameter. Put the track's automation in Latch or Touch in Logic.");
    readBtn.setTooltip ("Plays back the 'Rider Gain' automation (or drag the needle by hand).");

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

void VoxPilotEditor::setupKnob (Slider& s, Label& plaque, const String& name, const char* paramId,
                                const String& scale, Colour plaqueColour)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 100, 26);
    s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    s.setVelocityBasedMode (false);
    s.setMouseDragSensitivity (240);
    VPLookAndFeel::setScale (s, scale);
    addAndMakeVisible (s);
    attachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, paramId, s));

    plaque.setText (name, dontSendNotification);
    VPLookAndFeel::makePlaque (plaque, plaqueColour);
    addAndMakeVisible (plaque);
}

void VoxPilotEditor::placeKnob (Slider& s, Label& plaque, Point<int> c, int size)
{
    const int w = size + 30;
    s.setBounds (c.x - w / 2, c.y - size / 2, w, size + 26);
    const int pw = plaque.getText().length() * 10 + 26;
    plaque.setBounds (c.x - pw / 2, c.y - size / 2 - 38, pw, 36);
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
    const int scale = 2;
    background = Image (Image::RGB, kW * scale, kH * scale, true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale ((float) scale));

    // painted front plate
    const auto panel = Rectangle<float> ((float) kEar, 0.0f, (float) (kW - 2 * kEar), (float) kH);
    g.setGradientFill (ColourGradient (vpcol::panelTop, 0.0f, 0.0f, vpcol::panelBot, 0.0f, (float) kH, false));
    g.fillRect (panel);
    {
        Random rnd (4242);
        for (int i = 0; i < 9000; ++i)   // fine powder-coat speckle
        {
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.025f + rnd.nextFloat() * 0.03f));
            g.fillRect (panel.getX() + rnd.nextFloat() * panel.getWidth(), rnd.nextFloat() * (float) kH, 1.0f, 1.0f);
        }
    }
    g.setGradientFill (ColourGradient (Colours::transparentBlack, kW * 0.5f, kH * 0.42f,
                                       Colours::black.withAlpha (0.28f), (float) kEar, 0.0f, true));
    g.fillRect (panel);

    // rack ears
    for (float x : { 0.0f, (float) (kW - kEar) })
    {
        const auto ear = Rectangle<float> (x, 0.0f, (float) kEar, (float) kH);
        vpdraw::brushedMetal (g, ear, true, Colour (0xffd5d9dc), Colour (0xff7b8288), x < 1.0f ? 1 : 2);
        g.setColour (Colours::black.withAlpha (0.7f));
        g.fillRect (x < 1.0f ? ear.getRight() - 2.0f : ear.getX(), 0.0f, 2.0f, (float) kH);
        for (float yy : { 70.0f, (float) kH * 0.5f, (float) kH - 70.0f })
        {
            auto slot = Rectangle<float> (14.0f, 30.0f).withCentre ({ ear.getCentreX(), yy });
            g.setColour (Colours::white.withAlpha (0.35f));
            g.fillRoundedRectangle (slot.translated (0.0f, 1.0f), 7.0f);
            g.setColour (Colour (0xff15181a));
            g.fillRoundedRectangle (slot, 7.0f);
            vpdraw::screw (g, slot.getCentre(), 5.0f, 0.3f);
        }
    }

    // panel screws
    for (auto pt : { Point<float> (kEar + 22.0f, 22.0f), Point<float> (kW - kEar - 22.0f, 22.0f),
                     Point<float> (kEar + 22.0f, kH - 22.0f), Point<float> (kW - kEar - 22.0f, kH - 22.0f) })
        vpdraw::screw (g, pt, 6.5f, 0.6f);

    // brushed nameplate
    {
        const auto np = nameplateBounds;
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (np.translated (0.0f, 4.0f), 5.0f);
        g.setColour (Colour (0xff2a2e31));
        g.fillRoundedRectangle (np, 5.0f);
        const auto inner = np.reduced (4.0f);
        vpdraw::brushedMetal (g, inner, false, Colour (0xffeceeef), Colour (0xffa5abb0), 99);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (inner.reduced (3.0f), 3.0f, 1.2f);
        g.setColour (Colours::white.withAlpha (0.6f));
        g.drawRoundedRectangle (inner.reduced (4.2f), 2.0f, 0.6f);
        vpdraw::screw (g, { inner.getX() + 14.0f, inner.getCentreY() }, 5.0f, 0.2f);
        vpdraw::screw (g, { inner.getRight() - 14.0f, inner.getCentreY() }, 5.0f, 1.1f);

        auto title = uiFont (38.0f).withExtraKerningFactor (0.02f);
        g.setFont (title);
        g.setColour (Colours::white.withAlpha (0.7f));
        g.drawText ("VOXPILOT", inner.withHeight (44.0f).translated (0.0f, 4.0f), Justification::centred);
        g.setColour (Colour (0xff141618));
        g.drawText ("VOXPILOT", inner.withHeight (44.0f).translated (0.0f, 3.0f), Justification::centred);
        g.setFont (uiFont (12.0f).withExtraKerningFactor (0.08f));
        g.drawText (String::fromUTF8 ("VOCAL RIDER \xe2\x80\x93 MODEL VP-1"), inner.withTrimmedTop (42.0f).withHeight (18.0f), Justification::centred);
    }

    // divider
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRect ((float) kEar + 40.0f, 352.0f, (float) (kW - 2 * kEar) - 80.0f, 1.2f);
    g.setColour (Colours::white.withAlpha (0.14f));
    g.fillRect ((float) kEar + 40.0f, 353.2f, (float) (kW - 2 * kEar) - 80.0f, 1.0f);

    // section captions (engraved)
    g.setFont (uiFont (14.0f).withExtraKerningFactor (0.05f));
    engravedText (g, "LOOKAHEAD (MS)", { 740.0f, 580.0f, 300.0f, 18.0f }, Justification::centred, vpcol::text);
    engravedText (g, "AUTOMATION", { 740.0f, 650.0f, 300.0f, 18.0f }, Justification::centred, vpcol::text);
}

void VoxPilotEditor::paint (Graphics& g)
{
    if (background.isNull())
        buildBackground();
    g.drawImage (background, getLocalBounds().toFloat());

    // jewel lamps
    g.setFont (uiFont (17.0f));
    vpdraw::jewelLamp (g, { 132.0f, 52.0f }, 12.0f, vpcol::green, rideLed);
    engravedText (g, "RIDING", { 156.0f, 42.0f, 120.0f, 22.0f }, Justification::centredLeft, vpcol::text);
    vpdraw::jewelLamp (g, { 846.0f, 52.0f }, 12.0f, vpcol::amber, scLed);
    engravedText (g, "SIDECHAIN", { 870.0f, 42.0f, 140.0f, 22.0f }, Justification::centredLeft,
                  proc.sidechainConnected.load() ? vpcol::text : vpcol::textDim);
}

void VoxPilotEditor::resized()
{
    // row 1: target | meter | output
    placeKnob (targetKnob, targetPlaque, { 210, 222 }, 190);
    meter.setBounds (378, 104, 344, 236);
    placeKnob (outputKnob, outputPlaque, { 890, 226 }, 160);

    // row 2: response switch + five knobs
    responsePlaque.setBounds (62, 370, 128, 36);
    speedSwitch.setBounds (86, 412, 80, 136);
    const int y2 = 466, size2 = 128;
    placeKnob (detectKnob, detectPlaque, { 290, y2 }, size2);
    placeKnob (bleedKnob,  bleedPlaque,  { 446, y2 }, size2);
    placeKnob (musicKnob,  musicPlaque,  { 602, y2 }, size2);
    placeKnob (cutKnob,    cutPlaque,    { 758, y2 }, size2);
    placeKnob (boostKnob,  boostPlaque,  { 914, y2 }, size2);

    // row 3: history scope + button banks
    history.setBounds (66, 574, 650, 148);
    for (int i = 0; i < 4; ++i)
        lookaheadBtns[(size_t) i].setBounds (742 + i * 75, 600, 74, 42);
    offBtn.setBounds   (742, 670, 100, 42);
    writeBtn.setBounds (842, 670, 100, 42);
    readBtn.setBounds  (942, 670, 100, 42);
}

void VoxPilotEditor::timerCallback()
{
    history.pull();
    history.repaint();

    const auto mode = (AutoMode) roundToInt (proc.apvts.getRawParameterValue (ParamIDs::autoMode)->load());
    const bool fast = proc.apvts.getRawParameterValue (ParamIDs::fast)->load() > 0.5f;

    speedSwitch.setUp (fast);
    offBtn.setToggleState (mode == AutoMode::off, dontSendNotification);
    writeBtn.setToggleState (mode == AutoMode::write, dontSendNotification);
    readBtn.setToggleState (mode == AutoMode::read, dontSendNotification);
    const int la = roundToInt (proc.apvts.getRawParameterValue (ParamIDs::lookahead)->load());
    for (int i = 0; i < 4; ++i)
        lookaheadBtns[(size_t) i].setToggleState (i == la, dontSendNotification);

    meter.update (proc.meterGainDb.load(), mode == AutoMode::read, mode);

    const bool ride = proc.meterActive.load();
    const bool sc = proc.sidechainConnected.load() && proc.meterMusicDb.load() > -70.0f;
    if (ride != rideLed || sc != scLed)
    {
        rideLed = ride;
        scLed = sc;
        repaint (100, 20, 200, 64);
        repaint (812, 20, 230, 64);
    }
}
