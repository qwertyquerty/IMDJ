#include <chrono>

#include "vst/vst_host.h"

namespace imdj {

void VstChain::add(std::unique_ptr<VstPluginInstance> plugin)
{
    std::lock_guard lock(mutex_);
    plugins_.push_back(std::move(plugin));
}

void VstChain::retire(std::unique_ptr<VstPluginInstance> plugin)
{
    plugin->closeEditor();

    std::promise<void> done;
    Teardown teardown;
    teardown.finished = done.get_future();
    teardown.thread = std::thread([plugin = std::move(plugin), done = std::move(done)]() mutable {
        plugin.reset();
        done.set_value();
    });

    std::lock_guard lock(mutex_);
    teardowns_.push_back(std::move(teardown));
}

void VstChain::removeAt(size_t index)
{
    std::unique_ptr<VstPluginInstance> removed;
    {
        std::lock_guard lock(mutex_);
        if (index >= plugins_.size()) {
            return;
        }

        removed = std::move(plugins_[index]);
        plugins_.erase(plugins_.begin() + static_cast<std::ptrdiff_t>(index));
    }

    retire(std::move(removed));
}

void VstChain::moveUp(size_t index)
{
    std::lock_guard lock(mutex_);
    if (index > 0 && index < plugins_.size()) {
        std::swap(plugins_[index - 1], plugins_[index]);
    }
}

void VstChain::moveDown(size_t index)
{
    std::lock_guard lock(mutex_);
    if (index + 1 < plugins_.size()) {
        std::swap(plugins_[index], plugins_[index + 1]);
    }
}

void VstChain::clear()
{
    std::vector<std::unique_ptr<VstPluginInstance>> removed;
    {
        std::lock_guard lock(mutex_);
        removed.swap(plugins_);
    }

    for (std::unique_ptr<VstPluginInstance>& plugin : removed) {
        retire(std::move(plugin));
    }

    std::vector<Teardown> teardowns;
    {
        std::lock_guard lock(mutex_);
        teardowns.swap(teardowns_);
    }

    for (Teardown& teardown : teardowns) {
        if (teardown.finished.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
            teardown.thread.join();
        }
        else {
            teardown.thread.detach();
        }
    }
}

void VstChain::process(float* left, float* right, int32_t numFrames, const VstTransportInfo& transport)
{
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock) {
        return;
    }

    for (std::unique_ptr<VstPluginInstance>& plugin : plugins_) {
        plugin->process(left, right, numFrames, transport);
    }
}

void VstChain::pumpEditors()
{
    std::lock_guard lock(mutex_);
    for (std::unique_ptr<VstPluginInstance>& plugin : plugins_) {
        plugin->pumpEditor();
    }

    std::erase_if(teardowns_, [](Teardown& teardown) {
        if (teardown.finished.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }

        teardown.thread.join();
        return true;
    });
}

} // namespace imdj
