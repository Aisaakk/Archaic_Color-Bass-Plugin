#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// "Liquid": dispersion. Una cadena de 24 filtros pasa-todo cuyo coeficiente se modula lentamente:
// retrasa cada frecuencia de forma distinta (efecto de "gota"/agua) sin cambiar el nivel.
class Liquid
{
public:
    static constexpr int stages = 24, subBlock = 16;
    static constexpr double twoPi = 6.283185307179586;

    void prepare (double sr) { sampleRate = sr; reset(); }

    void reset()
    {
        for (auto& c : chan) c.z.fill (0.0);
        for (int i = 0; i < stages; ++i) phase[(size_t) i] = 0.37 * i;
        wetMix = 0.f;
    }

    // amount 0..1 (cantidad de dispersion), flow 0..1 (velocidad/profundidad del movimiento)
    void process (float* const* data, int numCh, int n, float amount, float flow)
    {
        numCh = std::min (numCh, 2);
        const float targetMix = std::min (1.f, amount * 10.f);
        if (targetMix <= 0.f && wetMix <= 0.0005f) { wetMix = 0.f; return; }

        const double base = 0.2 + 0.72 * (double) amount;
        const double depth = (double) amount * (0.04 + 0.2 * (double) flow);
        const double rate = 0.12 + 1.4 * (double) flow;   // Hz

        for (int start = 0; start < n; start += subBlock)
        {
            const int len = std::min (subBlock, n - start);
            std::array<std::array<double, stages>, 2> a {};
            for (int s = 0; s < stages; ++s)
            {
                phase[(size_t) s] = std::fmod (phase[(size_t) s] + twoPi * rate * (1.0 + 0.11 * s) * len / sampleRate, twoPi);
                for (int c = 0; c < numCh; ++c)
                    a[(size_t) c][(size_t) s] = std::clamp (base + depth * std::sin (phase[(size_t) s] + c * 1.9), -0.97, 0.97);
            }
            for (int c = 0; c < numCh; ++c)
            {
                float* x = data[c] + start;
                auto& z = chan[(size_t) c].z;
                float wm = wetMix;
                for (int i = 0; i < len; ++i)
                {
                    wm += (targetMix - wm) * 0.002f;
                    const double in = (double) x[i];
                    double y = in;
                    for (int s = 0; s < stages; ++s)
                    {
                        const double out = a[(size_t) c][(size_t) s] * y + z[(size_t) s];
                        z[(size_t) s] = y - a[(size_t) c][(size_t) s] * out;
                        y = out;
                    }
                    x[i] = (float) (in * (1.0 - (double) wm) + y * (double) wm);
                }
                if (c == numCh - 1) wetMix = wm;
            }
        }
    }

private:
    struct Chan { std::array<double, stages> z {}; };
    std::array<Chan, 2> chan;
    std::array<double, stages> phase {};
    double sampleRate = 44100.0;
    float wetMix = 0.f;
};
