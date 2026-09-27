#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/result.h"

namespace imdj {

struct VstPluginDescriptor {
    std::string modulePath;
    std::string label;
};

std::vector<VstPluginDescriptor> ScanVst3Plugins(const std::string& extraDir);

struct VstTransportInfo {
    double bpm = 0.0;
    double beat = 0.0;
    int64_t positionSamples = 0;
};

struct VstParamInfo {
    int32_t id = 0;
    std::string title;
};

class VstPluginInstance {
public:
    explicit VstPluginInstance(VstPluginDescriptor descriptor);
    ~VstPluginInstance();
    VstPluginInstance(const VstPluginInstance&) = delete;
    VstPluginInstance& operator=(const VstPluginInstance&) = delete;

    Status initialize(double sampleRate, int32_t maxBlockFrames);

    const VstPluginDescriptor& descriptor() const { return descriptor_; }

    bool bypassed() const { return bypassed_; }
    void setBypassed(bool bypassed) { bypassed_ = bypassed; }

    void process(float* left, float* right, int32_t numFrames, const VstTransportInfo& transport);

    bool isEditorOpen() const;
    void toggleEditor();
    void closeEditor();
    void pumpEditor();
    const std::string& lastEditorError() const { return lastEditorError_; }

    std::vector<VstParamInfo> listParameters() const;
    void setParameterNormalized(int32_t id, float value);

    std::string getStateBase64() const;
    Status setStateFromBase64(const std::string& base64);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    VstPluginDescriptor descriptor_;
    std::string lastEditorError_;
    std::atomic<bool> bypassed_{false};
};

class VstChain {
public:
    void add(std::unique_ptr<VstPluginInstance> plugin);
    void removeAt(size_t index);
    void moveUp(size_t index);
    void moveDown(size_t index);
    void clear();

    size_t size() const { return plugins_.size(); }
    VstPluginInstance& at(size_t index) { return *plugins_[index]; }
    const VstPluginInstance& at(size_t index) const { return *plugins_[index]; }

    void process(float* left, float* right, int32_t numFrames, const VstTransportInfo& transport);
    void pumpEditors();

    template <typename Fn>
    void withPlugin(size_t index, Fn fn)
    {
        std::shared_lock lock(mutex_);
        if (index < plugins_.size()) {
            fn(*plugins_[index]);
        }
    }

private:
    struct Teardown {
        std::thread thread;
        std::future<void> finished;
    };

    void retire(std::unique_ptr<VstPluginInstance> plugin);

    std::vector<std::unique_ptr<VstPluginInstance>> plugins_;
    std::vector<Teardown> teardowns_;
    mutable std::shared_mutex mutex_;
};

} // namespace imdj
