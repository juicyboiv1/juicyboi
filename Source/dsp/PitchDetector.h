#pragma once

#include <complex>
#include <vector>

namespace jb::dsp
{
/** Monophonic pitch detector for the Rev3.2 tuner (docs/tuner.md).

    YIN (de Cheveigne & Kawahara 2002) on an FFT-computed difference function:
    the cumulative-mean-normalised difference picks the period (absolute
    threshold + parabolic interpolation), then the estimate is refined on the
    normalised cross-correlation at the largest multiple of the period that
    fits the lag range (k periods -> k x finer resolution, amplitude-decay
    invariant), which gives sub-cent accuracy from drop tunings (~25 Hz) up to
    the top frets (~2 kHz).

    Not real-time safe by design (it is run off the audio thread); all
    buffers are allocated in prepare(), detect() itself does not allocate. */
class PitchDetector
{
public:
    struct Result
    {
        bool valid = false;
        double hz = 0.0;
        double clarity = 0.0;   ///< 1 - CMNDF at the chosen period (1 = perfectly periodic)
        double rmsDb = -200.0;
    };

    /** analysisRate: sample rate of the frames; window: integration window W;
        maxLag: largest lag (frames hold window + maxLag samples). */
    void prepare (double analysisRate, int window = 2048, int maxLag = 4096,
                  double minHz = 25.0, double maxHz = 2100.0);

    int frameLength() const noexcept { return w + lagMax; }
    double getRate() const noexcept { return rate; }

    /** frame: frameLength() samples, oldest first. */
    Result detect (const float* frame) noexcept;

    double silenceDb = -70.0;   ///< frames quieter than this (RMS dBFS) are not analysed
    double yinThreshold = 0.15;

private:
    void fft (std::vector<std::complex<double>>& a, bool inverse) noexcept;
    double refine (double tau0) const noexcept;

    double rate = 48000.0, fMin = 25.0, fMax = 2100.0;
    int w = 2048, lagMax = 4096, n = 8192, tauMin = 2, tauFirstMax = 1920;
    std::vector<std::complex<double>> fa, fb;
    std::vector<double> x, corr, e2, rho, cmnd;
    std::vector<int> bitrev;
    std::vector<std::complex<double>> twiddle;
    double e1 = 0.0;
};
} // namespace jb::dsp
