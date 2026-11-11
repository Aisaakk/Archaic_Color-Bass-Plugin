#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// ---------------------------------------------------------------- Tema
struct Theme
{
    bool dark;
    juce::Colour bgTop, bgBottom, text, sub, purple, cyan, glassA, glassB;
};

inline Theme makeTheme (bool dark)
{
    using C = juce::Colour;
    if (dark) return { true, C (0xff090612), C (0xff150b2b), C (0xfff2ecff), C (0xff9b8fc0), C (0xff9b5cff), C (0xff35d7ff), C (0x24ffffff), C (0x08ffffff) };
    return { false, C (0xfff7f3ff), C (0xffe3d9fb), C (0xff2b1a52), C (0xff7d6fa8), C (0xff8a46ff), C (0xff0fb4ee), C (0xd9ffffff), C (0x73ffffff) };
}

inline juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

// ------------------------------------------------- Pieza base "liquid glass"
// Todo el aspecto de cristal sale de esta funcion: cambia aqui y cambia en todo el plugin.
namespace glass
{
    inline void panel (juce::Graphics& g, juce::Rectangle<float> r, float corner, const Theme& t,
                       juce::Colour glow = {}, float glowAmt = 0.f, juce::Colour tint = {})
    {
        using namespace juce;
        Path p;
        p.addRoundedRectangle (r, corner);

        if (glowAmt > 0.f)       DropShadow (glow.withAlpha (glowAmt), (int) (r.getHeight() * 0.4f), { 0, 0 }).drawForPath (g, p);
        else if (t.dark)         DropShadow (Colours::black.withAlpha (0.5f), 12, { 0, 4 }).drawForPath (g, p);
        else                     DropShadow (t.purple.withAlpha (0.16f), 14, { 0, 5 }).drawForPath (g, p);

        g.setGradientFill (ColourGradient (t.glassA, r.getX(), r.getY(), t.glassB, r.getX(), r.getBottom(), false));
        g.fillPath (p);

        if (! tint.isTransparent())
        {
            g.setGradientFill (ColourGradient (tint.withAlpha (0.78f), r.getX(), r.getY(), tint.withAlpha (0.30f), r.getX(), r.getBottom(), false));
            g.fillPath (p);
        }

        g.setGradientFill (ColourGradient (Colours::white.withAlpha (t.dark ? 0.15f : 0.55f), r.getX(), r.getY(),
                                           Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.5f, false));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f), corner);

        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.6f), r.getX(), r.getY(),
                                           Colours::white.withAlpha (0.08f), r.getX(), r.getBottom(), false));
        g.drawRoundedRectangle (r.reduced (0.7f), corner, 1.3f);
    }
}

// ------------------------------------------------------------ LookAndFeel
class ArchaicLNF : public juce::LookAndFeel_V4
{
public:
    ArchaicLNF() { setDark (true); }

    void setDark (bool d)
    {
        th = makeTheme (d);
        setColour (juce::PopupMenu::backgroundColourId, d ? juce::Colour (0xff1b0f33) : juce::Colour (0xfffaf7ff));
        setColour (juce::PopupMenu::textColourId, th.text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, th.purple.withAlpha (0.55f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::textColourId, th.text);
    }
    const Theme& theme() const { return th; }

    // ---- Knobs
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        using namespace juce;
        const float d = (float) jmin (w, h);
        auto r = Rectangle<float> (d, d).withCentre (Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre()).reduced (d * 0.04f);
        const auto c = r.getCentre();
        const float R = r.getWidth() * 0.5f, ringR = R * 0.9f, ringW = R * 0.09f;

        glass::panel (g, r.reduced (R * 0.22f), R, th);

        Path track;
        track.addCentredArc (c.x, c.y, ringR, ringR, 0.f, a0, a1, true);
        g.setColour (th.text.withAlpha (0.14f));
        g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

        const float ang = a0 + pos * (a1 - a0);
        auto arc = [&] (float from, float to, Colour col)
        {
            if (to - from < 0.001f) return;
            Path p;
            p.addCentredArc (c.x, c.y, ringR, ringR, 0.f, from, to, true);
            g.setColour (col.withAlpha (th.dark ? 0.30f : 0.22f));
            g.strokePath (p, PathStrokeType (ringW * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (col);
            g.strokePath (p, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
        };
        if (s.getComponentID() == "color")   // 0-100 morado (mezcla), 100-200 celeste (cola resonante)
        {
            const float mid = (a0 + a1) * 0.5f;
            arc (a0, jmin (ang, mid), th.purple);
            arc (mid, ang, th.cyan);
        }
        else arc (a0, ang, th.purple.interpolatedWith (th.cyan, pos * 0.8f));

        const Point<float> tip (c.x + std::sin (ang) * R * 0.62f, c.y - std::cos (ang) * R * 0.62f);
        g.setColour (th.cyan.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (R * 0.2f, R * 0.2f).withCentre (tip));
        g.setColour (th.dark ? Colours::white : th.purple.darker (0.3f));
        g.fillEllipse (Rectangle<float> (R * 0.1f, R * 0.1f).withCentre (tip));

        g.setColour (th.text);
        g.setFont (uiFont (R * 0.24f, true));
        g.drawText (s.getTextFromValue (s.getValue()), Rectangle<float> (R * 1.0f, R * 0.4f).withCentre (c), Justification::centred);
    }

    // ---- Botones de texto (teclas de la tonalidad)
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (2.f);
        const bool on = b.getToggleState();
        glass::panel (g, r, r.getHeight() * 0.32f, th, on ? th.purple : juce::Colour(), on ? 0.55f : 0.f, on ? th.purple : juce::Colour());
        if (hover && ! on) { g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (r, r.getHeight() * 0.32f); }
        if (down) { g.setColour (juce::Colours::black.withAlpha (0.12f)); g.fillRoundedRectangle (r, r.getHeight() * 0.32f); }
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setColour (b.getToggleState() ? juce::Colours::white : th.text);
        g.setFont (uiFont ((float) b.getHeight() * 0.38f, b.getToggleState()));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }

    // ---- Interruptores: tema, bypass, calidad
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (3.f);
        const bool on = b.getToggleState();
        const auto id = b.getComponentID();

        if (id == "theme")   // interruptor claro/oscuro: esfera a la derecha = oscuro
        {
            glass::panel (g, r, r.getHeight() * 0.5f, th);
            const float pad = r.getHeight() * 0.11f, dia = r.getHeight() - pad * 2.f;
            auto ball = Rectangle<float> (dia, dia).withY (r.getY() + pad).withX (on ? r.getRight() - pad - dia : r.getX() + pad);
            Path bp; bp.addEllipse (ball);
            DropShadow (th.purple.withAlpha (0.8f), (int) dia, { 0, 0 }).drawForPath (g, bp);
            g.setGradientFill (ColourGradient (th.purple.brighter (0.5f), ball.getX(), ball.getY(), th.purple.darker (0.2f), ball.getX(), ball.getBottom(), false));
            g.fillEllipse (ball);
            g.setColour (Colours::white.withAlpha (0.55f));
            g.drawEllipse (ball.reduced (0.7f), 1.2f);
            auto c = ball.getCentre(); const float u = dia * 0.5f;
            g.setColour (Colours::white);
            if (on)   // luna
            {
                g.fillEllipse (Rectangle<float> (u * 0.9f, u * 0.9f).withCentre (c));
                g.setColour (th.purple.interpolatedWith (th.purple.darker (0.2f), 0.5f));
                g.fillEllipse (Rectangle<float> (u * 0.8f, u * 0.8f).withCentre (c.translated (u * 0.28f, -u * 0.2f)));
            }
            else      // sol
            {
                g.fillEllipse (Rectangle<float> (u * 0.55f, u * 0.55f).withCentre (c));
                for (int i = 0; i < 8; ++i)
                {
                    const float a = (float) i * MathConstants<float>::pi / 4.f;
                    g.drawLine (c.x + std::cos (a) * u * 0.42f, c.y + std::sin (a) * u * 0.42f, c.x + std::cos (a) * u * 0.62f, c.y + std::sin (a) * u * 0.62f, 1.6f);
                }
            }
        }
        else if (id == "bypass")   // encendido = procesando; apagado (gris) = bypass
        {
            const bool active = ! on;
            const float s = jmin (r.getWidth(), r.getHeight());
            auto sq = Rectangle<float> (s, s).withCentre (r.getCentre());
            glass::panel (g, sq, s * 0.5f, th, th.cyan, active ? 0.35f : 0.f);
            const auto col = active ? th.cyan : th.sub.withAlpha (0.7f);
            const float rad = s * 0.22f; const auto c = sq.getCentre();
            Path p;
            p.addCentredArc (c.x, c.y, rad, rad, 0.f, 0.6f, MathConstants<float>::twoPi - 0.6f, true);
            p.startNewSubPath (c.x, c.y - rad * 1.15f);
            p.lineTo (c.x, c.y - rad * 0.1f);
            if (active) { g.setColour (col.withAlpha (0.3f)); g.strokePath (p, PathStrokeType (rad * 0.6f, PathStrokeType::curved, PathStrokeType::rounded)); }
            g.setColour (col);
            g.strokePath (p, PathStrokeType (rad * 0.24f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        else   // calidad: FAST | QUALITY
        {
            glass::panel (g, r, r.getHeight() * 0.5f, th);
            auto half = r.withWidth (r.getWidth() * 0.5f);
            auto sel = on ? half.withX (r.getCentreX()) : half;
            glass::panel (g, sel.reduced (2.f), sel.getHeight() * 0.5f, th, th.purple, 0.5f, th.purple);
            g.setFont (uiFont (r.getHeight() * 0.36f, true));
            g.setColour (on ? th.sub : Colours::white);   g.drawText ("FAST", half, Justification::centred);
            g.setColour (on ? Colours::white : th.sub);   g.drawText ("QUALITY", half.withX (r.getCentreX()), Justification::centred);
        }
        if (hover) { g.setColour (Colours::white.withAlpha (0.04f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }
    }

    // ---- Combo (escala)
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (2.f);
        glass::panel (g, r, r.getHeight() * 0.32f, th);
        juce::Path p;
        const float cx = r.getRight() - r.getHeight() * 0.55f, cy = r.getCentreY(), s = r.getHeight() * 0.13f;
        p.addTriangle (cx - s, cy - s * 0.6f, cx + s, cy - s * 0.6f, cx, cy + s * 0.8f);
        g.setColour (th.cyan);
        g.fillPath (p);
    }
    juce::Font getComboBoxFont (juce::ComboBox& c) override { return uiFont ((float) c.getHeight() * 0.4f); }
    void positionComboBoxText (juce::ComboBox& c, juce::Label& l) override
    {
        l.setBounds (c.getHeight() / 3, 1, c.getWidth() - c.getHeight() * 2, c.getHeight() - 2);
        l.setFont (getComboBoxFont (c));
    }

private:
    Theme th = makeTheme (true);
};
