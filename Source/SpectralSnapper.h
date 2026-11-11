#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "Parameters.h"

// Ajusta el espectro (100 Hz - 6 kHz) a una escala: phase vocoder que mueve
// las frecuencias fuera de la tonalidad hacia la nota de la escala mas cercana.
class SpectralSnapper
{
public:
    static constexpr int numViz = 96, maxN = 4096, osamp = 4;
    static constexpr float loHz = 100.f, hiHz = 6000.f;

    struct Params { float color = 100.f; std::array<bool, 12> mask {}; };   // mask = notas permitidas (pitch classes absolutos)

    std::array<std::atomic<float>, numViz> vizDry, vizTuned;   // 0..1, lo lee la interfaz

    SpectralSnapper() : fftS (10), fftL (12)
    {
        winS.resize (1024);
        winL.resize (4096);
        for (int i = 0; i < 1024; ++i) winS[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / 1024.f);
        for (int i = 0; i < 4096; ++i) winL[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / 4096.f);
        for (auto& v : vizDry) v.store (0.f);
        for (auto& v : vizTuned) v.store (0.f);
        for (auto& c : ch) c.alloc();
        const size_t h = (size_t) maxN / 2 + 1;
        magn.assign (h, 0.f); phs.assign (h, 0.f); trueBin.assign (h, 0.f); outRe.assign (h, 0.f); outIm.assign (h, 0.f); outFreq.assign (h, 0.f); peaks.assign (h, 0);
        tmpDry.assign (numViz, 0.f); tmpTuned.assign (numViz, 0.f);
        setSize (4096);
    }

    void prepare (double sr) { sampleRate = sr; setSize (N); }

    void setSize (int n)
    {
        N = n;
        fft = (n == 1024) ? &fftS : &fftL;
        win = (n == 1024) ? winS.data() : winL.data();
        for (auto& c : ch) c.reset (N - N / osamp);
    }

    int getLatency() const { return N - N / osamp; }

    // procesa in-place: salida = dry retrasado*(1-mix) + wet*mix*gain
    void process (int c, float* x, int num, const Params& p, float mix, float gain)
    {
        auto& s = ch[(size_t) c];
        const int hop = N / osamp, lat = N - hop;
        for (int i = 0; i < num; ++i)
        {
            const float in = x[i];
            s.dryBuf[(size_t) s.dryPos] = in;
            const float dry = s.dryBuf[(size_t) ((s.dryPos - lat + maxN) % maxN)];
            s.dryPos = (s.dryPos + 1) % maxN;

            s.inFifo[(size_t) s.rover] = in;
            const float wet = s.outFifo[(size_t) (s.rover - lat)];
            ++s.rover;
            if (s.rover >= N) { s.rover = lat; processFrame (c, p); }
            x[i] = dry * (1.f - mix) + wet * mix * gain;
        }
    }

private:
    struct Chan
    {
        std::vector<float> inFifo, outFifo, accum, ws, lastPhase, prevPhase, heldMag, heldPhase, heldFreq, dryBuf;
        int rover = 0, dryPos = 0;
        void alloc()
        {
            const size_t h = (size_t) maxN / 2 + 1;
            inFifo.assign (maxN, 0.f); outFifo.assign (maxN, 0.f); accum.assign (maxN, 0.f); ws.assign (2 * maxN, 0.f);
            dryBuf.assign (maxN, 0.f);
            lastPhase.assign (h, 0.f); prevPhase.assign (h, 0.f); heldMag.assign (h, 0.f); heldPhase.assign (h, 0.f); heldFreq.assign (h, 0.f);
        }
        void reset (int lat)
        {
            for (auto* v : { &inFifo, &outFifo, &accum, &ws, &dryBuf, &lastPhase, &prevPhase, &heldMag, &heldPhase, &heldFreq })
                std::fill (v->begin(), v->end(), 0.f);
            rover = lat; dryPos = 0;
        }
    };

    static int vizCol (float hz)
    {
        return juce::jlimit (0, numViz - 1, (int) (std::log (hz / loHz) / std::log (hiHz / loHz) * (float) numViz));
    }

    void processFrame (int c, const Params& p)
    {
        auto& s = ch[(size_t) c];
        const int hop = N / osamp, half = N / 2;
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float expct = twoPi * (float) hop / (float) N;
        const float binHz = (float) sampleRate / (float) N;
        float* d = s.ws.data();

        for (int k = 0; k < N; ++k) d[k] = s.inFifo[(size_t) k] * win[k];
        std::fill (d + N, d + 2 * N, 0.f);
        fft->performRealOnlyForwardTransform (d, true);

        // pitch classes dentro de la escala y desplazamiento al mas cercano
        const auto& inScale = p.mask;
        int snap[12];
        for (int pc = 0; pc < 12; ++pc)
        {
            snap[pc] = 0;
            if (! inScale[pc])
                for (int dd = 1; dd <= 6; ++dd)
                {
                    if (inScale[(pc - dd + 12) % 12]) { snap[pc] = -dd; break; }
                    if (inScale[(pc + dd) % 12]) { snap[pc] = dd; break; }
                }
        }

        const bool viz = (c == 0);
        if (viz) { std::fill (tmpDry.begin(), tmpDry.end(), 0.f); std::fill (tmpTuned.begin(), tmpTuned.end(), 0.f); }

        // 1) analisis: magnitud, fase y frecuencia real (en bins) de cada bin
        for (int k = 0; k <= half; ++k)
        {
            const float re = d[2 * k], im = d[2 * k + 1];
            magn[(size_t) k] = std::sqrt (re * re + im * im);
            const float ph = std::atan2 (im, re);
            phs[(size_t) k] = ph;
            float t = ph - s.lastPhase[(size_t) k];
            s.lastPhase[(size_t) k] = ph;
            t = std::remainder (t - (float) k * expct, twoPi);
            trueBin[(size_t) k] = (float) k + t * (float) osamp / twoPi;
            outRe[(size_t) k] = 0.f; outIm[(size_t) k] = 0.f; outFreq[(size_t) k] = (float) k;
        }

        // 2) picos espectrales (cada pico + su zona de influencia se mueve como una unidad)
        const float thresh = 3e-4f * (float) N / 4.f;
        int nPeaks = 0;
        for (int k = 1; k < half; ++k)
        {
            const float m = magn[(size_t) k];
            if (m > thresh && m >= magn[(size_t) k - 1] && m > magn[(size_t) k + 1]
                && (k < 2 || m >= magn[(size_t) k - 2]) && (k + 2 > half || m > magn[(size_t) k + 2]))
                peaks[(size_t) nPeaks++] = k;
        }

        // 3) mover cada pico a la nota de la escala; la fase se propaga (o se conserva si no se mueve)
        for (int i = 0; i < nPeaks; ++i)
        {
            const int pk = peaks[(size_t) i];
            const int lo = (i == 0) ? 0 : (peaks[(size_t) i - 1] + pk) / 2 + 1;
            const int hi = (i == nPeaks - 1) ? half : (pk + peaks[(size_t) i + 1]) / 2;
            const float tb = trueBin[(size_t) pk], fHz = tb * binHz;

            float ratio = 1.f;
            int delta = 0, sh = 0;
            if (fHz >= loHz && fHz <= hiHz)
            {
                const int nn = (int) std::lround (69.f + 12.f * std::log2 (fHz / 440.f));
                sh = snap[((nn % 12) + 12) % 12];
                if (sh != 0) { ratio = std::exp2 ((float) sh / 12.f); delta = (int) std::lround ((float) pk * ratio) - pk; }
                if (viz)
                {
                    const int c0 = vizCol (fHz);
                    tmpDry[(size_t) c0] = std::max (tmpDry[(size_t) c0], magn[(size_t) pk]);
                    if (sh != 0)
                    {
                        const int c1 = vizCol (juce::jlimit (loHz, hiHz, fHz * ratio));
                        tmpTuned[(size_t) c1] = std::max (tmpTuned[(size_t) c1], magn[(size_t) pk]);
                    }
                }
            }
            const float freqNew = tb * ratio;
            const int tp = juce::jlimit (0, half, pk + delta);
            const float psi = (sh == 0) ? phs[(size_t) pk] : s.prevPhase[(size_t) tp] + freqNew * expct;
            for (int k = lo; k <= hi; ++k)
            {
                const int kt = k + delta;
                if (kt < 0 || kt > half) continue;
                const float ph = psi + (phs[(size_t) k] - phs[(size_t) pk]);
                outRe[(size_t) kt] += magn[(size_t) k] * std::cos (ph);
                outIm[(size_t) kt] += magn[(size_t) k] * std::sin (ph);
                outFreq[(size_t) kt] = freqNew;
            }
        }

        // 4) COLOR 100-200: las frecuencias en tonalidad se sostienen (cola resonante)
        const float amount = juce::jlimit (0.f, 1.f, (p.color - 100.f) / 100.f);
        const int kLo = (int) std::ceil (loHz / binHz), kHi = juce::jmin (half, (int) std::floor (hiHz / binHz));
        const float decay = amount > 0.f ? std::exp (-((float) hop / (float) sampleRate) / (0.03f + amount * 0.7f)) : 0.f;
        for (int k = 0; k <= half; ++k)
        {
            const size_t u = (size_t) k;
            const float m = std::sqrt (outRe[u] * outRe[u] + outIm[u] * outIm[u]);
            auto& hm = s.heldMag[u];
            const bool tail = amount > 0.f && k >= kLo && k <= kHi;
            if (tail) hm *= decay;
            if (! tail || m >= hm)
            {
                hm = m; s.heldPhase[u] = std::atan2 (outIm[u], outRe[u]); s.heldFreq[u] = outFreq[u];
            }
            else
            {
                s.heldPhase[u] = std::remainder (s.heldPhase[u] + s.heldFreq[u] * expct, twoPi);
                outRe[u] = hm * std::cos (s.heldPhase[u]);
                outIm[u] = hm * std::sin (s.heldPhase[u]);
            }
            s.prevPhase[u] = std::atan2 (outIm[u], outRe[u]);
            d[2 * k] = outRe[u];
            d[2 * k + 1] = outIm[u];
        }
        d[1] = 0.f; d[2 * half + 1] = 0.f;
        for (int k = 1; k < half; ++k) { d[2 * (N - k)] = d[2 * k]; d[2 * (N - k) + 1] = -d[2 * k + 1]; }
        fft->performRealOnlyInverseTransform (d);

        const float outScale = 1.f / ((float) osamp * 0.375f);   // Hann^2, solape 75 %
        for (int k = 0; k < N; ++k) s.accum[(size_t) k] += d[k] * win[k] * outScale;
        for (int k = 0; k < hop; ++k) s.outFifo[(size_t) k] = s.accum[(size_t) k];
        std::copy (s.accum.begin() + hop, s.accum.begin() + N, s.accum.begin());
        std::fill (s.accum.begin() + N - hop, s.accum.begin() + N, 0.f);
        for (int k = 0; k < N - hop; ++k) s.inFifo[(size_t) k] = s.inFifo[(size_t) (k + hop)];

        if (viz)
        {
            auto norm = [n = N] (float m) { return juce::jlimit (0.f, 1.f, (juce::Decibels::gainToDecibels (m * 4.f / (float) n, -90.f) + 80.f) / 80.f); };
            for (int i = 0; i < numViz; ++i)
            {
                vizDry[(size_t) i].store (norm (tmpDry[(size_t) i]));
                vizTuned[(size_t) i].store (norm (tmpTuned[(size_t) i]));
            }
        }
    }

    juce::dsp::FFT fftS, fftL;
    juce::dsp::FFT* fft = nullptr;
    std::vector<float> winS, winL;
    const float* win = nullptr;
    int N = 4096;
    double sampleRate = 44100.0;
    std::array<Chan, 2> ch;
    std::vector<float> magn, phs, trueBin, outRe, outIm, outFreq, tmpDry, tmpTuned;
    std::vector<int> peaks;
};
