#pragma once
#include <array>
#include <cmath>
#include <algorithm>

// Banco de resonadores afinados (inspirado en el concepto de Bismuth):
// hasta 8 notas x 8 armonicos (segun la forma de onda), filtros SVF (TPT) en doble precision.
//   Spacing: estira/comprime la serie armonica (inarmonico)
//   Drift:   desafinacion lenta + decorrelacion estereo
//   Shift:   cambiador de frecuencia (Hz) sobre la salida resonada
class ResonatorBank
{
public:
    static constexpr int maxVoices = 8, partials = 8, subBlock = 16;
    static constexpr double pi = 3.14159265358979323846;

    struct Params
    {
        int nVoices = 0;
        std::array<float, maxVoices> freq {};
        float decay = 0.5f;     // 0..1
        float spacing = 0.f;    // -1..1
        float drift = 0.f;      // 0..1
        float shiftHz = 0.f;
        int wave = 3;           // 0 seno, 1 triangulo, 2 cuadrada, 3 sierra, 4 pulso
    };

    // amplitud del armonico n (1..partials) segun la forma de onda
    static float waveAmp (int wave, int n)
    {
        switch (wave)
        {
            case 0: return n == 1 ? 1.f : 0.f;
            case 1: return (n % 2) ? 1.f / (float) (n * n) : 0.f;
            case 2: return (n % 2) ? 1.f / (float) n : 0.f;
            case 3: return 1.f / (float) n;
            default: { const float a = std::abs (std::sin ((float) pi * (float) n * 0.25f)); return a < 1e-4f ? 0.f : a / (float) n; }
        }
    }

    void prepare (double sr) { sampleRate = sr; reset(); }

    void reset()
    {
        for (auto& c : chan)
        {
            for (auto& v : c.ic1) v.fill (0.0);
            for (auto& v : c.ic2) v.fill (0.0);
            c.hil.reset(); c.eIn = c.eOut = c.agc = c.target = 0.0;
        }
        voiceGain.fill (0.f); curFreq.fill (0.f);
        for (int v = 0; v < maxVoices; ++v) { phase1[(size_t) v] = 0.7 * v; phase2[(size_t) v] = 1.9 * v + 0.3; }
        shiftPhase = 0.0;
    }

    // procesa in-place; data[c] = canal c; salida = dry*(1-mix) + resonado*mix
    void process (float* const* data, int numCh, int n, const Params& p, float mix)
    {
        if (mix <= 0.0001f && ! anyRing) { return; }
        numCh = std::min (numCh, 2);
        const double sr = sampleRate;
        const float Q = 8.f * std::pow (100.f, p.decay);
        const float k = 1.f / Q;
        const double aEnv = 1.0 - std::exp (-1.0 / (0.08 * sr)), aGain = 1.0 - std::exp (-1.0 / (0.15 * sr));
        const float expo = 1.f + 0.6f * p.spacing;

        for (int start = 0; start < n; start += subBlock)
        {
            const int len = std::min (subBlock, n - start);

            for (int v = 0; v < maxVoices; ++v)
            {
                const bool on = v < p.nVoices;
                const float target = on ? 1.f / std::sqrt ((float) std::max (1, p.nVoices)) : 0.f;
                voiceGain[(size_t) v] += (target - voiceGain[(size_t) v]) * 0.25f;
                if (on)
                {
                    if (curFreq[(size_t) v] < 1.f || voiceGain[(size_t) v] < 0.001f) curFreq[(size_t) v] = p.freq[(size_t) v];
                    else curFreq[(size_t) v] += (p.freq[(size_t) v] - curFreq[(size_t) v]) * 0.25f;
                }
                phase1[(size_t) v] += 2.0 * pi * (0.05 + 0.031 * v) * len / sr;
                phase2[(size_t) v] += 2.0 * pi * (0.13 + 0.047 * v) * len / sr;

                for (int c = 0; c < numCh; ++c)
                {
                    const double lfo = 0.6 * std::sin (phase1[(size_t) v] + c * 2.2) + 0.4 * std::sin (phase2[(size_t) v] + c * 4.1);
                    const double cents = (double) p.drift * 35.0 * lfo;
                    const double detune = std::pow (2.0, cents / 1200.0);
                    for (int n_ = 0; n_ < partials; ++n_)
                    {
                        const double fRaw = (double) curFreq[(size_t) v] * std::pow ((double) (n_ + 1), (double) expo) * detune;
                        const double f = std::min (fRaw, 0.45 * sr);
                        const double g = std::tan (pi * f / sr);
                        auto& co = chan[(size_t) c].co[(size_t) (v * partials + n_)];
                        co.a1 = 1.0 / (1.0 + g * (g + (double) k));
                        co.a2 = g * co.a1;
                        co.a3 = g * co.a2;
                        const float wa = waveAmp (p.wave, n_ + 1);
                        co.ampT = (fRaw >= 0.45 * sr || wa == 0.f) ? 0.0 : (double) (voiceGain[(size_t) v] * wa);
                        co.amp += (co.ampT - co.amp) * 0.35;
                        if (co.ampT == 0.0 && co.amp < 1e-6) co.amp = 0.0;
                    }
                }
            }

            double energy = 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chan[(size_t) c];
                float* x = data[c] + start;
                for (int i = 0; i < len; ++i)
                {
                    const double in = (double) x[i];
                    double wet = 0.0;
                    for (int f = 0; f < maxVoices * partials; ++f)
                    {
                        if (voiceGain[(size_t) (f / partials)] < 1e-5f && f / partials >= p.nVoices) continue;
                        const auto& co = ch.co[(size_t) f];
                        if (co.amp == 0.0) continue;
                        double& ic1 = ch.ic1[(size_t) (f / partials)][(size_t) (f % partials)];
                        double& ic2 = ch.ic2[(size_t) (f / partials)][(size_t) (f % partials)];
                        if (co.amp < 1e-7 && co.ampT < 1e-7) { ic1 = ic2 = 0.0; continue; }
                        const double v3 = in - ic2;
                        const double v1 = co.a1 * ic1 + co.a2 * v3;
                        const double v2 = ic2 + co.a2 * ic1 + co.a3 * v3;
                        ic1 = 2.0 * v1 - ic1;
                        ic2 = 2.0 * v2 - ic2;
                        wet += co.amp * (double) k * v1;   // BP de pico unitario
                    }
                    // Nivel automatico: el resonado sigue la sonoridad de la entrada (cualquier decaimiento);
                    // con entrada en silencio se congela la ganancia para que la cola resuene completa.
                    ch.eIn += (in * in - ch.eIn) * aEnv;
                    ch.eOut += (wet * wet - ch.eOut) * aEnv;
                    if (ch.eIn > 1e-7) ch.target = std::min (2000.0, 0.7 * std::sqrt (ch.eIn / (ch.eOut + 1e-20)));
                    ch.agc += (ch.target - ch.agc) * aGain;
                    wet *= ch.agc;
                    if (std::abs (p.shiftHz) > 0.01f)
                    {
                        double q;
                        const double ii = ch.hil.process (wet, q);
                        const double ph = shiftPhase + 2.0 * pi * (double) p.shiftHz * (double) i / sr;
                        wet = ii * std::cos (ph) + q * std::sin (ph);
                    }
                    wet = softClip (wet);
                    energy += wet * wet;
                    x[i] = (float) ((double) in * (1.0 - (double) mix) + wet * (double) mix);
                }
            }
            shiftPhase = std::fmod (shiftPhase + 2.0 * pi * (double) p.shiftHz * (double) len / sr, 2.0 * pi);
            anyRing = energy > 1e-12;
        }
    }

private:
    static double softClip (double x)
    {
        const double a = std::abs (x);
        if (a <= 0.8) return x;
        const double y = 0.8 + 0.2 * std::tanh ((a - 0.8) / 0.2);
        return x < 0 ? -y : y;
    }

    struct Coef { double a1 = 0, a2 = 0, a3 = 0, amp = 0, ampT = 0; };

    // Hilbert IIR de 2 ramas (Niemitalo)
    struct Hilbert
    {
        struct AP { double a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
            double tick (double x) { const double y = a2 * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
        std::array<AP, 4> pa, pb;
        double dly = 0;
        Hilbert()
        {
            const double ca[4] = { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
            const double cb[4] = { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
            for (int i = 0; i < 4; ++i) { pa[(size_t) i].a2 = ca[i] * ca[i]; pb[(size_t) i].a2 = cb[i] * cb[i]; }
        }
        void reset() { for (auto& s : pa) s = AP { s.a2 }; for (auto& s : pb) s = AP { s.a2 }; dly = 0; }
        double process (double x, double& q)
        {
            double a = x, b = x;
            for (auto& s : pa) a = s.tick (a);
            for (auto& s : pb) b = s.tick (b);
            q = b;                // rama sin retraso
            const double out = dly; dly = a;   // rama A con 1 muestra de retraso (verificado: 90 grados +-0.7)
            return out;
        }
    };

    struct Chan
    {
        std::array<Coef, maxVoices * partials> co {};
        std::array<std::array<double, partials>, maxVoices> ic1 {}, ic2 {};
        Hilbert hil;
        double eIn = 0, eOut = 0, agc = 0, target = 0;
    };

    std::array<Chan, 2> chan;
    std::array<float, maxVoices> voiceGain {}, curFreq {};
    std::array<double, maxVoices> phase1 {}, phase2 {};
    double sampleRate = 44100.0, shiftPhase = 0.0;
    bool anyRing = false;
};
