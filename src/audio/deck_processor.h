#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

#include "audio/audio_buffer.h"
#include "audio/beat_grid.h"

namespace imdj {

struct Deck;

struct DeckContext {
    Deck& deck;
    BeatGrid grid;
    double startFrame = 0.0;
    double rate = 1.0;
    double sampleRate = 48000.0;
    float metronomeVolume = 0.0f;
};

class DeckProcessor {
public:
    virtual ~DeckProcessor() = default;

    virtual const char* id() const = 0;
    virtual const char* name() const = 0;
    virtual void process(const StereoBlock& block, const DeckContext& context) = 0;
    virtual void reset() {}
    virtual bool hostsPlugins() const { return false; }

    bool enabled() const { return enabled_.load(); }
    void setEnabled(bool value) { enabled_.store(value); }

private:
    std::atomic<bool> enabled_{true};
};

class DeckChain {
public:
    static constexpr size_t MAX_PROCESSORS = 16;

    void add(std::unique_ptr<DeckProcessor> processor)
    {
        assert(processors_.size() < MAX_PROCESSORS);
        processors_.push_back(std::move(processor));
        resetOrder();
    }

    void resetOrder()
    {
        uint64_t order = 0;
        for (size_t slot = 0; slot < processors_.size(); ++slot) {
            order |= static_cast<uint64_t>(slot) << (slot * 4);
        }

        order_.store(order);
    }

    void process(const StereoBlock& block, const DeckContext& context)
    {
        const uint64_t order = order_.load();
        for (size_t position = 0; position < processors_.size(); ++position) {
            DeckProcessor& processor = *processors_[SlotAt(order, position)];
            if (processor.enabled()) {
                processor.process(block, context);
            }
        }
    }

    void reset()
    {
        for (std::unique_ptr<DeckProcessor>& processor : processors_) {
            processor->reset();
        }
    }

    size_t size() const { return processors_.size(); }
    DeckProcessor& at(size_t position) { return *processors_[SlotAt(order_.load(), position)]; }
    DeckProcessor& inSlot(size_t slot) { return *processors_[slot]; }

    void setOrder(const std::vector<size_t>& slots)
    {
        uint64_t order = 0;
        for (size_t position = 0; position < slots.size() && position < processors_.size(); ++position) {
            order |= static_cast<uint64_t>(slots[position]) << (position * 4);
        }

        order_.store(order);
    }

    void moveUp(size_t position)
    {
        if (position > 0 && position < processors_.size()) {
            swapPositions(position - 1, position);
        }
    }

    void moveDown(size_t position)
    {
        if (position + 1 < processors_.size()) {
            swapPositions(position, position + 1);
        }
    }

private:
    static size_t SlotAt(uint64_t order, size_t position) { return (order >> (position * 4)) & 0xF; }

    void swapPositions(size_t a, size_t b)
    {
        const uint64_t order = order_.load();
        const uint64_t slotA = SlotAt(order, a);
        const uint64_t slotB = SlotAt(order, b);
        const uint64_t cleared = order & ~((uint64_t{0xF} << (a * 4)) | (uint64_t{0xF} << (b * 4)));
        order_.store(cleared | (slotB << (a * 4)) | (slotA << (b * 4)));
    }

    std::atomic<uint64_t> order_{0};
    std::vector<std::unique_ptr<DeckProcessor>> processors_;
};

void BuildDefaultDeckChain(DeckChain& chain);

} // namespace imdj
