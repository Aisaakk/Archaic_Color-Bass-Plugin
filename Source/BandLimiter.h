#pragma once
#include <array>
#include <cmath>
#include <algorithm>

// Rango de frecuencias afectado (split). Se aplica a la DIFERENCIA entre la señal procesada y la
// original (con la misma latencia): fuera del rango queda la original intacta; con el rango completo
// no hay ningun filtro. Filtros Linkwitz-Riley de 4o orden (dos Butterworth en cascada).
class BandLimiter
{
public:
    static constexpr double pi = 3.14159265358979323846;

    void prepare (double sr) { sampleRate = sr; for (auto& c : ch) c = {}; }

    void setRange (float lowHz, float highHz)
    {
        hpOn = lowHz > 20.5f;
        lpOn = highHz < 19990.f;
        highHz = std::max (highHz, lowHz * 1.05f);
        setup (hpCo, lowHz); setup (lpCo, highHz);
    }
    bool isActive() const { return hpOn || lpOn; }

    // x = señal procesada, dry = original alineada; devuelve en x: dry + banda(x - dry)
    void apply (int c, float* x, const float* dry, int n)
    {
        auto& s = ch[(size_t) c];
        for (int i = 0; i < n; ++i)
        {
            double d = (double) x[i] - (double) dry[i];
            if (hpOn) { d = tick (s.hp[0], hpCo, d, true); d = tick (s.hp[1], hpCo, d, true); }
            if (lpOn) { d = tick (s.lp[0], lpCo, d, false); d = tick (s.lp[1], lpCo, d, false); }
            x[i] = (float) ((double) dry[i] + d);
        }
    }

private:
    struct Co { double a1 = 0, a2 = 0, a3 = 0; };
    struct St { double ic1 = 0, ic2 = 0; };
    struct Chan { std::array<St, 2> hp {}, lp {}; };

    void setup (Co& co, float f) const
    {
        const double k = std::sqrt (2.0);
        const double g = std::tan (pi * std::min ((double) f, 0.45 * sampleRate) / sampleRate);
        co.a1 = 1.0 / (1.0 + g * (g + k)); co.a2 = g * co.a1; co.a3 = g * co.a2;
    }
    static double tick (St& s, const Co& co, double x, bool high)
    {
        const double k = std::sqrt (2.0);
        const double v3 = x - s.ic2;
        const double v1 = co.a1 * s.ic1 + co.a2 * v3;
        const double v2 = s.ic2 + co.a2 * s.ic1 + co.a3 * v3;
        s.ic1 = 2.0 * v1 - s.ic1; s.ic2 = 2.0 * v2 - s.ic2;
        return high ? x - k * v1 - v2 : v2;
    }

    std::array<Chan, 2> ch;
    Co hpCo, lpCo;
    bool hpOn = false, lpOn = false;
    double sampleRate = 44100.0;
};
