#pragma once

// Thin wrapper around juce::dsp::Oversampling so the nonlinear stages
// (OD + preamp cascade) run at a higher internal rate to keep aliasing from
// the waveshapers down. FIR equiripple half-band filters + integer latency
// so the dry/bypass path can be delay-compensated exactly.
//
// TODO: expose the factor (2x/4x/8x) as a "quality" setting; consider IIR
// polyphase for a low-latency live mode.

#include <juce_dsp/juce_dsp.h>

namespace jb::dsp
{
class OversampledNonlinear
{
public:
    static constexpr size_t factorLog2 = 2; // 2^2 = 4x

    void prepare (int numChannels, int maxBlockSize)
    {
        os = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) numChannels, factorLog2,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true,   // max quality
            true);  // integer latency
        os->initProcessing ((size_t) maxBlockSize);
        os->reset();
    }

    void reset() { if (os) os->reset(); }

    int getFactor() const noexcept { return 1 << factorLog2; }
    int getLatencySamples() const { return os ? (int) os->getLatencyInSamples() : 0; }

    /** Upsamples, calls fn(oversampledBlock), downsamples back in place. */
    template <typename Fn>
    void process (juce::dsp::AudioBlock<float>& block, Fn&& fn)
    {
        auto up = os->processSamplesUp (block);
        fn (up);
        os->processSamplesDown (block);
    }

    /** Two same-latency output paths from one nonlinear callback. The second
        upsampled block is scratch (overwritten by fn); its independent downsampler
        keeps both model outputs warm without touching the legacy filter state. */
    template <typename Fn>
    void processParallel (juce::dsp::AudioBlock<float>& block,
                          juce::dsp::AudioBlock<float>& otherBlock,
                          OversampledNonlinear& other, Fn&& fn)
    {
        auto up = os->processSamplesUp (block);
        auto otherUp = other.os->processSamplesUp (otherBlock);
        fn (up, otherUp);
        os->processSamplesDown (block);
        other.os->processSamplesDown (otherBlock);
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
};
} // namespace jb::dsp
