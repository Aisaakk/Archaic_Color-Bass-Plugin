#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// ---------------------------------------------------------------- Tema
// Interfaz limpia (estilo claro, perillas oscuras). El modo oscuro es la alternativa.
struct Theme
{
    bool dark;
    juce::Colour bg, panelTop, panelBot, text, sub, knobTop, knob, track, well, pink, amber, violet, shadow, light;
};

inline Theme makeTheme (bool dark)
{
    using C = juce::Colour;
    if (dark) return { true,  C (0xff131315), C (0xff2b2b30), C (0xff232327), C (0xfff0f0f2), C (0xff9c9ca6), C (0xff2a2a2f), C (0xff0e0e10),
                              C (0xff3b3b42), C (0xff070709), C (0xffff5c98), C (0xffffa84a), C (0xff9d78ff), C (0x80000000), C (0x0cffffff) };
    return            { false, C (0xffe3e3e6), C (0xfff7f7f8), C (0xffebebee), C (0xff222226), C (0xff777780), C (0xff303036), C (0xff17171b),
                              C (0xffcdcdd3), C (0xff0b0b0e), C (0xffe8387f), C (0xfff0902a), C (0xff8b5cf6), C (0x30000000), C (0xf2ffffff) };
}

inline juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

// ------------------------------------------------- Piezas base (todo el aspecto sale de aqui)
namespace ui
{
    // panel elevado con sombra suave (solo se dibuja en el fondo guardado en cache)
    inline void panel (juce::Graphics& g, juce::Rectangle<float> r, float corner, const Theme& t)
    {
        using namespace juce;
        Path p;
        p.addRoundedRectangle (r, corner);
        DropShadow (t.shadow, 16, { 5, 7 }).drawForPath (g, p);
        DropShadow (t.light, 12, { -4, -4 }).drawForPath (g, p);
        g.setGradientFill (ColourGradient (t.panelTop, r.getX(), r.getY(), t.panelBot, r.getX(), r.getBottom(), false));
        g.fillPath (p);
        g.setColour (t.dark ? Colours::white.withAlpha (0.06f) : Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.f);
    }

    // boton plano ligeramente elevado (barato de dibujar)
    inline void button (juce::Graphics& g, juce::Rectangle<float> r, float corner, const Theme& t, bool on)
    {
        using namespace juce;
        if (on)
        {
            g.setGradientFill (ColourGradient (t.knobTop, r.getX(), r.getY(), t.knob, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, corner);
            return;
        }
        g.setColour (t.shadow.withMultipliedAlpha (0.8f));
        g.fillRoundedRectangle (r.translated (0.f, 1.5f), corner);
        g.setGradientFill (ColourGradient (t.dark ? t.panelTop.brighter (0.15f) : Colours::white, r.getX(), r.getY(), t.panelBot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (t.track.withAlpha (t.dark ? 0.8f : 0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.f);
    }

    // pantalla oscura empotrada
    inline void well (juce::Graphics& g, juce::Rectangle<float> r, float corner, const Theme& t)
    {
        g.setColour (t.well);
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.f);
    }
}

// ------------------------------------------------------------ LookAndFeel
class ArchaicLNF : public juce::LookAndFeel_V4
{
public:
    float glow = 0.f;   // 0 = sin brillo; sube solo cuando entra sonido al plugin

    ArchaicLNF() { setDark (false); }

    void setDark (bool d)
    {
        th = makeTheme (d);
        setColour (juce::PopupMenu::backgroundColourId, d ? juce::Colour (0xff1d1d21) : juce::Colour (0xfff7f7f8));
        setColour (juce::PopupMenu::textColourId, th.text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, th.knob);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ComboBox::textColourId, th.text);
    }
    const Theme& theme() const { return th; }

    // ---- Perillas: disco oscuro + anillo de color
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        using namespace juce;
        const float d = (float) jmin (w, h);
        auto r = Rectangle<float> (d, d).withCentre (Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre());
        const auto c = r.getCentre();
        const float R = r.getWidth() * 0.5f, ringR = R * 0.9f, ringW = jmax (2.f, R * 0.075f), discR = R * 0.7f;

        Path track;
        track.addCentredArc (c.x, c.y, ringR, ringR, 0.f, a0, a1, true);
        g.setColour (th.track);
        g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

        const float ang = a0 + pos * (a1 - a0);
        auto arc = [&] (float from, float to, Colour col)
        {
            if (to - from < 0.001f) return;
            Path p;
            p.addCentredArc (c.x, c.y, ringR, ringR, 0.f, from, to, true);
            if (glow > 0.02f)
            {
                g.setColour (col.withAlpha (0.35f * glow));
                g.strokePath (p, PathStrokeType (ringW * 2.8f, PathStrokeType::curved, PathStrokeType::rounded));
            }
            g.setColour (col);
            g.strokePath (p, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
        };
        if (s.getComponentID() == "color")   // 0-100 mezcla (rosa), 100-200 cola resonante (naranja)
        {
            const float mid = (a0 + a1) * 0.5f;
            arc (a0, jmin (ang, mid), th.pink);
            arc (mid, ang, th.amber);
        }
        else arc (a0, ang, th.pink.interpolatedWith (th.amber, pos * 0.8f));

        g.setColour (Colours::black.withAlpha (th.dark ? 0.5f : 0.18f));
        g.fillEllipse (Rectangle<float> (discR * 2.f, discR * 2.f).withCentre (c.translated (0.f, R * 0.04f)));
        g.setGradientFill (ColourGradient (th.knobTop, c.x, c.y - discR, th.knob, c.x, c.y + discR, false));
        g.fillEllipse (Rectangle<float> (discR * 2.f, discR * 2.f).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawEllipse (Rectangle<float> (discR * 2.f, discR * 2.f).withCentre (c).reduced (0.5f), 1.f);

        const Point<float> tip (c.x + std::sin (ang) * discR * 0.78f, c.y - std::cos (ang) * discR * 0.78f);
        g.setColour (Colours::white.withAlpha (0.92f));
        g.fillEllipse (Rectangle<float> (discR * 0.14f, discR * 0.14f).withCentre (tip));

        g.setColour (Colours::white.withAlpha (0.92f));
        g.setFont (uiFont (discR * (s.getComponentID() == "color" ? 0.34f : 0.4f), true));
        g.drawText (s.getTextFromValue (s.getValue()), Rectangle<float> (discR * 1.7f, discR * 0.7f).withCentre (c.translated (0.f, discR * 0.12f)), Justification::centred);
    }

    // ---- Teclas de la tonalidad
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (3.f);
        ui::button (g, r, juce::jmin (8.f, r.getHeight() * 0.3f), th, b.getToggleState());
        if (hover && ! b.getToggleState()) { g.setColour (th.pink.withAlpha (0.08f)); g.fillRoundedRectangle (r, 8.f); }
        if (down) { g.setColour (juce::Colours::black.withAlpha (0.08f)); g.fillRoundedRectangle (r, 8.f); }
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        g.setColour (on ? juce::Colours::white : th.text);
        g.setFont (uiFont ((float) b.getHeight() * 0.36f, on));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
        if (on)   // punto de color (brilla solo con sonido)
        {
            const float dd = (float) b.getHeight() * 0.1f;
            auto dot = juce::Rectangle<float> (dd, dd).withCentre ({ (float) b.getWidth() * 0.5f, (float) b.getHeight() * 0.82f });
            if (glow > 0.02f) { g.setColour (th.pink.withAlpha (0.4f * glow)); g.fillEllipse (dot.expanded (dd * 0.8f)); }
            g.setColour (th.pink);
            g.fillEllipse (dot);
        }
    }

    // ---- Interruptores: tema, bypass, MIDI, calidad
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (3.f);
        const bool on = b.getToggleState();
        const auto id = b.getComponentID();

        if (id == "theme")   // esfera a la derecha = oscuro
        {
            g.setColour (th.track);
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            const float pad = r.getHeight() * 0.1f, dia = r.getHeight() - pad * 2.f;
            auto ball = Rectangle<float> (dia, dia).withY (r.getY() + pad).withX (on ? r.getRight() - pad - dia : r.getX() + pad);
            g.setGradientFill (ColourGradient (th.knobTop, ball.getX(), ball.getY(), th.knob, ball.getX(), ball.getBottom(), false));
            g.fillEllipse (ball);
            auto c = ball.getCentre(); const float u = dia * 0.5f;
            g.setColour (Colours::white);
            if (on)
            {
                g.fillEllipse (Rectangle<float> (u * 0.9f, u * 0.9f).withCentre (c));
                g.setColour (th.knobTop);
                g.fillEllipse (Rectangle<float> (u * 0.8f, u * 0.8f).withCentre (c.translated (u * 0.28f, -u * 0.2f)));
            }
            else
            {
                g.fillEllipse (Rectangle<float> (u * 0.55f, u * 0.55f).withCentre (c));
                for (int i = 0; i < 8; ++i)
                {
                    const float a = (float) i * MathConstants<float>::pi / 4.f;
                    g.drawLine (c.x + std::cos (a) * u * 0.42f, c.y + std::sin (a) * u * 0.42f, c.x + std::cos (a) * u * 0.62f, c.y + std::sin (a) * u * 0.62f, 1.5f);
                }
            }
        }
        else if (id == "bypass")   // encendido (color) = procesando; gris = bypass
        {
            const bool active = ! on;
            const float s = jmin (r.getWidth(), r.getHeight());
            auto sq = Rectangle<float> (s, s).withCentre (r.getCentre());
            ui::button (g, sq, s * 0.5f, th, false);
            const auto col = active ? th.pink : th.sub.withAlpha (0.8f);
            const float rad = s * 0.22f; const auto c = sq.getCentre();
            Path p;
            p.addCentredArc (c.x, c.y, rad, rad, 0.f, 0.6f, MathConstants<float>::twoPi - 0.6f, true);
            p.startNewSubPath (c.x, c.y - rad * 1.15f);
            p.lineTo (c.x, c.y - rad * 0.1f);
            if (active && glow > 0.02f) { g.setColour (col.withAlpha (0.35f * glow)); g.strokePath (p, PathStrokeType (rad * 0.7f, PathStrokeType::curved, PathStrokeType::rounded)); }
            g.setColour (col);
            g.strokePath (p, PathStrokeType (rad * 0.24f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        else if (id == "midi")
        {
            const bool held = (bool) b.getProperties()["held"];
            ui::button (g, r, r.getHeight() * 0.5f, th, on);
            g.setColour (on ? Colours::white : th.sub);
            g.setFont (uiFont (r.getHeight() * 0.4f, true));
            g.drawText ("MIDI", r.withTrimmedLeft (r.getHeight() * 0.3f), Justification::centred);
            auto dot = Rectangle<float> (r.getHeight() * 0.22f, r.getHeight() * 0.22f).withCentre ({ r.getX() + r.getHeight() * 0.42f, r.getCentreY() });
            const auto col = (on && held) ? th.pink : th.sub.withAlpha (0.6f);
            if (on && held && glow > 0.02f) { g.setColour (col.withAlpha (0.4f * glow)); g.fillEllipse (dot.expanded (r.getHeight() * 0.1f)); }
            g.setColour (col);
            g.fillEllipse (dot);
        }
        else   // calidad: FAST | QUALITY
        {
            g.setColour (th.track);
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            auto half = r.withWidth (r.getWidth() * 0.5f);
            auto sel = on ? half.withX (r.getCentreX()) : half;
            ui::button (g, sel.reduced (2.f), sel.getHeight() * 0.5f, th, true);
            g.setFont (uiFont (r.getHeight() * 0.3f, true));
            g.setColour (on ? th.sub : Colours::white);   g.drawText ("FAST", half, Justification::centred);
            g.setColour (on ? Colours::white : th.sub);   g.drawText ("QUALITY", half.withX (r.getCentreX()), Justification::centred);
        }
        if (hover) { g.setColour (Colours::black.withAlpha (0.03f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f); }
    }

    // ---- Combos
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (3.f);
        ui::button (g, r, juce::jmin (8.f, r.getHeight() * 0.3f), th, false);
        juce::Path p;
        const float cx = r.getRight() - r.getHeight() * 0.5f, cy = r.getCentreY(), s = r.getHeight() * 0.12f;
        p.addTriangle (cx - s, cy - s * 0.6f, cx + s, cy - s * 0.6f, cx, cy + s * 0.8f);
        g.setColour (th.sub);
        g.fillPath (p);
    }
    juce::Font getComboBoxFont (juce::ComboBox& c) override { return uiFont ((float) c.getHeight() * (c.getWidth() < c.getHeight() * 3 ? 0.31f : 0.38f)); }
    void positionComboBoxText (juce::ComboBox& c, juce::Label& l) override
    {
        l.setBounds (c.getHeight() / 3, 1, c.getWidth() - c.getHeight() / 3 - (int) (c.getHeight() * 0.8f), c.getHeight() - 2);
        l.setFont (getComboBoxFont (c));
    }

private:
    Theme th = makeTheme (false);
};
