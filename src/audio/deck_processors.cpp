#include <algorithm>
#include <cmath>
#include <numbers>

#include "audio/deck.h"
#include "audio/deck_processor.h"
#include "audio/eq.h"
#include "audio/filter.h"
#include "audio/metronome.h"
#include "core/strings.h"

namespace imdj {

namespace {

constexpr float HALF_PI = std::numbers::pi_v<float> / 2.0f;

class VstChainProcessor : public DeckProcessor {
public:
    const char* id() const override { return "plugins"; }
    const char* name() const override { return Tr("chain.plugins"); }
    bool hostsPlugins() const override { return true; }

    void process(const StereoBlock& block, const DeckContext& context) override
    {
        const VstTransportInfo transport{
            .bpm = context.grid.bpm * std::clamp(context.rate, -8.0, 8.0),
            .beat = context.grid.beatAt(context.startFrame),
            .positionSamples = static_cast<int64_t>(context.startFrame),
        };
        context.deck.vstChain.process(
            block.left.data(), block.right.data(), static_cast<int32_t>(block.frames()), transport
        );
    }
};

class EqProcessor : public DeckProcessor {
public:
    const char* id() const override { return "eq"; }
    const char* name() const override { return Tr("chain.eq"); }

    void process(const StereoBlock& block, const DeckContext& context) override
    {
        eq_.process(block, context.deck.eqGains(), context.sampleRate);
    }

    void reset() override { eq_.reset(); }

private:
    ThreeBandEq eq_;
};

class FilterProcessor : public DeckProcessor {
public:
    const char* id() const override { return "filter"; }
    const char* name() const override { return Tr("chain.filter"); }

    void process(const StereoBlock& block, const DeckContext& context) override
    {
        filter_.process(block, context.deck.filter.load(), context.sampleRate);
    }

    void reset() override { filter_.reset(); }

private:
    OneKnobFilter filter_;
};

class PanProcessor : public DeckProcessor {
public:
    const char* id() const override { return "pan"; }
    const char* name() const override { return Tr("chain.pan"); }

    void process(const StereoBlock& block, const DeckContext& context) override
    {
        const float pan = context.deck.pan.load();
        const GainStep left = left_.next(pan > 0.0f ? std::cos(pan * HALF_PI) : 1.0f);
        const GainStep right = right_.next(pan < 0.0f ? std::cos(-pan * HALF_PI) : 1.0f);
        if (left.unity() && right.unity()) {
            return;
        }

        block.scale(left, right);
    }

private:
    SmoothedGain left_;
    SmoothedGain right_;
};

class MetronomeProcessor : public DeckProcessor {
public:
    const char* id() const override { return "metronome"; }
    const char* name() const override { return Tr("chain.metronome"); }

    void process(const StereoBlock& block, const DeckContext& context) override
    {
        if (!context.deck.metronomeEnabled.load()) {
            return;
        }

        AddMetronomeClicks(
            block, context.grid, context.startFrame, context.rate, BEATS_PER_BAR, context.metronomeVolume
        );
    }
};

} // namespace

void BuildDefaultDeckChain(DeckChain& chain)
{
    chain.add(std::make_unique<VstChainProcessor>());
    chain.add(std::make_unique<EqProcessor>());
    chain.add(std::make_unique<FilterProcessor>());
    chain.add(std::make_unique<PanProcessor>());
    chain.add(std::make_unique<MetronomeProcessor>());
}

} // namespace imdj
