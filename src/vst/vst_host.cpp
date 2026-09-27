#include "vst/vst_host.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <future>
#include <mutex>
#include <utility>
#include <vector>

#include "audio/audio_constants.h"
#include "core/base64.h"
#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/utility/stringconvert.h"
#include "vst/vst_host_internal.h"
#include "core/strings.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace imdj {

namespace {

IPtr<HostApplication>& HostContext()
{
    static IPtr<HostApplication> context = owned(new HostApplication());
    return context;
}

class ComponentHandler final : public U::Implements<U::Directly<IComponentHandler>> {
public:
    tresult PLUGIN_API beginEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API restartComponent(int32) override { return kResultOk; }

    tresult PLUGIN_API performEdit(ParamID id, ParamValue value) override
    {
        std::lock_guard lock(mutex_);
        pending_.emplace_back(id, value);

        return kResultOk;
    }

    void drainInto(ParameterChanges& changes)
    {
        std::lock_guard lock(mutex_);
        for (const auto& [id, value] : pending_) {
            int32 index = 0;
            if (IParamValueQueue* queue = changes.addParameterData(id, index)) {
                queue->addPoint(0, value, index);
            }
        }

        pending_.clear();
    }

private:
    std::mutex mutex_;
    std::vector<std::pair<ParamID, ParamValue>> pending_;
};

class PlugFrame final : public U::Implements<U::Directly<IPlugFrame>> {
public:
    explicit PlugFrame(EditorWindow& window) : window_(&window) {}

    void detach() { window_ = nullptr; }

    tresult PLUGIN_API resizeView(IPlugView* view, ViewRect* size) override
    {
        if (!view || !size || !window_) {
            return kResultFalse;
        }

        view->checkSizeConstraint(size);
        if (size->getWidth() > 0 && size->getHeight() > 0) {
            window_->resize(size->getWidth(), size->getHeight());
        }

        view->onSize(size);
        return kResultTrue;
    }

private:
    EditorWindow* window_;
};

} // namespace

void PluginThread::run(const std::function<void()>& task)
{
    std::promise<void> done;
    std::future<void> finished = done.get_future();
    post([&] {
        task();
        done.set_value();
    });
    finished.wait();
}

struct VstPluginInstance::Impl {
    PluginThread thread;

    VST3::Hosting::Module::Ptr module;
    IPtr<IComponent> component;
    IPtr<IAudioProcessor> processor;
    IPtr<IEditController> controller;
    IPtr<IConnectionPoint> componentConnection;
    IPtr<IConnectionPoint> controllerConnection;
    IPtr<ComponentHandler> handler;
    bool ownsController = false;
    bool active = false;

    ParameterChanges inputChanges;
    ParameterChanges outputChanges;
    EventList inputEvents;
    EventList outputEvents;
    double sampleRate = 0.0;
    int32 maxBlockFrames = 0;
    int64 processedSamples = 0;
    std::vector<float> outLeft;
    std::vector<float> outRight;

    std::unique_ptr<EditorWindow> window;
    IPtr<IPlugView> view;
    IPtr<PlugFrame> frame;
    std::atomic<bool> editorOpen{false};

    ~Impl()
    {
        thread.run([this] {
            closeEditor();
            release();
        });
    }

    Status load(VstPluginDescriptor& descriptor, double rate, int32 blockFrames)
    {
        std::string error;
        module = VST3::Hosting::Module::create(descriptor.modulePath, error);
        if (!module) {
            return Status::Fail(TrFormat("error.vst_module", error));
        }

        const VST3::Hosting::PluginFactory& factory = module->getFactory();
        const auto classes = factory.classInfos();
        const auto audioClass = std::ranges::find_if(classes, [](const VST3::Hosting::ClassInfo& info) {
            return info.category() == kVstAudioEffectClass;
        });
        if (audioClass == classes.end()) {
            return Status::Fail(TrFormat("error.vst_no_effect"));
        }

        descriptor.label = audioClass->name();
        component = factory.createInstance<IComponent>(audioClass->ID());
        if (!component || component->initialize(HostContext()) != kResultOk) {
            return Status::Fail(TrFormat("error.vst_component"));
        }

        processor = U::cast<IAudioProcessor>(component);
        if (!processor) {
            return Status::Fail(TrFormat("error.vst_not_effect"));
        }

        connectController(factory);

        if (component->getBusCount(kAudio, kInput) == 0 || component->getBusCount(kAudio, kOutput) == 0) {
            return Status::Fail(TrFormat("error.vst_no_stereo"));
        }

        SpeakerArrangement stereo = SpeakerArr::kStereo;
        processor->setBusArrangements(&stereo, 1, &stereo, 1);

        ProcessSetup setup{kRealtime, kSample32, blockFrames, rate};
        if (processor->setupProcessing(setup) != kResultOk) {
            return Status::Fail(TrFormat("error.vst_setup"));
        }

        component->activateBus(kAudio, kInput, 0, true);
        component->activateBus(kAudio, kOutput, 0, true);
        if (component->setActive(true) != kResultOk) {
            return Status::Fail(TrFormat("error.vst_activate"));
        }

        active = true;
        processor->setProcessing(true);

        sampleRate = rate;
        maxBlockFrames = blockFrames;
        outLeft.assign(static_cast<size_t>(blockFrames), 0.0f);
        outRight.assign(static_cast<size_t>(blockFrames), 0.0f);

        return Status::Ok();
    }

    void connectController(const VST3::Hosting::PluginFactory& factory)
    {
        TUID controllerId{};
        if (component->getControllerClassId(controllerId) == kResultTrue) {
            controller = factory.createInstance<IEditController>(VST3::UID::fromTUID(controllerId));
        }

        ownsController = controller != nullptr;
        if (!ownsController) {
            controller = U::cast<IEditController>(component);
        }
        else {
            controller->initialize(HostContext());
            componentConnection = U::cast<IConnectionPoint>(component);
            controllerConnection = U::cast<IConnectionPoint>(controller);
            if (componentConnection && controllerConnection) {
                componentConnection->connect(controllerConnection);
                controllerConnection->connect(componentConnection);
            }

            MemoryStream stream;
            if (component->getState(&stream) == kResultOk) {
                stream.seek(0, IBStream::kIBSeekSet, nullptr);
                controller->setComponentState(&stream);
            }
        }

        if (controller) {
            handler = owned(new ComponentHandler());
            controller->setComponentHandler(handler);
        }
    }

    void release()
    {
        if (active) {
            processor->setProcessing(false);
            component->setActive(false);
            active = false;
        }

        if (componentConnection && controllerConnection) {
            componentConnection->disconnect(controllerConnection);
            controllerConnection->disconnect(componentConnection);
        }

        if (controller) {
            controller->setComponentHandler(nullptr);
            if (ownsController) {
                controller->terminate();
            }
        }

        if (component) {
            component->terminate();
        }

        componentConnection = nullptr;
        controllerConnection = nullptr;
        handler = nullptr;
        processor = nullptr;
        controller = nullptr;
        component = nullptr;

        module = nullptr;
    }

    const char* openEditor(const std::string& title)
    {
        view = owned(controller->createView(ViewType::kEditor));
        if (!view) {
            return "vst.no_editor";
        }

        ViewRect rect{0, 0, 400, 300};
        view->getSize(&rect);
        window = std::make_unique<EditorWindow>(std::max(64, rect.getWidth()), std::max(64, rect.getHeight()), title);
        if (!window->valid()) {
            view = nullptr;
            window.reset();

            return "vst.editor_window";
        }

        frame = owned(new PlugFrame(*window));
        view->setFrame(frame);

        bool attached = false;
        const bool crashed = !RunGuarded([&] {
            attached = window->parent() && view->isPlatformTypeSupported(EDITOR_PLATFORM_TYPE) == kResultTrue &&
                       view->attached(window->parent(), EDITOR_PLATFORM_TYPE) == kResultOk;
        });
        if (attached) {
            editorOpen = true;
            return nullptr;
        }

        if (!crashed) {
            RunGuarded([&] { view->setFrame(nullptr); });
        }

        view = nullptr;
        frame->detach();
        frame = nullptr;
        window.reset();

        return crashed ? "vst.editor_crashed" : "vst.editor_declined";
    }

    void closeEditor()
    {
        if (view) {
            RunGuarded([&] {
                view->setFrame(nullptr);
                view->removed();
            });
            view = nullptr;
        }

        if (frame) {
            frame->detach();
            frame = nullptr;
        }

        window.reset();
        editorOpen = false;
    }
};

VstPluginInstance::VstPluginInstance(VstPluginDescriptor descriptor) : descriptor_(std::move(descriptor)) {}

VstPluginInstance::~VstPluginInstance() = default;

Status VstPluginInstance::initialize(double sampleRate, int32_t maxBlockFrames)
{
    auto impl = std::make_unique<Impl>();
    Status status;
    impl->thread.run([&] { status = impl->load(descriptor_, sampleRate, maxBlockFrames); });
    if (status) {
        impl_ = std::move(impl);
    }

    return status;
}

void VstPluginInstance::process(float* left, float* right, int32_t numFrames, const VstTransportInfo& transport)
{
    if (!impl_ || bypassed_) {
        return;
    }

    Impl& p = *impl_;
    const bool tempoKnown = transport.bpm > 0.0;
    ProcessContext context{};
    context.sampleRate = p.sampleRate;
    if (tempoKnown) {
        context.state = ProcessContext::kPlaying | ProcessContext::kTempoValid |
                        ProcessContext::kProjectTimeMusicValid | ProcessContext::kBarPositionValid |
                        ProcessContext::kTimeSigValid;
        context.tempo = transport.bpm;
        context.timeSigNumerator = BEATS_PER_BAR;
        context.timeSigDenominator = 4;
    }

    for (int32_t offset = 0; offset < numFrames; offset += p.maxBlockFrames) {
        const int32_t chunk = std::min(numFrames - offset, p.maxBlockFrames);
        if (tempoKnown) {
            context.projectTimeMusic = transport.beat + offset * transport.bpm / 60.0 / p.sampleRate;
            context.barPositionMusic = std::floor(context.projectTimeMusic / BEATS_PER_BAR) * BEATS_PER_BAR;
            context.projectTimeSamples = transport.positionSamples + offset;
        }
        else {
            context.projectTimeSamples = p.processedSamples;
            p.processedSamples += chunk;
        }

        if (p.handler) {
            p.handler->drainInto(p.inputChanges);
        }

        float* inputs[2] = {left + offset, right + offset};
        float* outputs[2] = {p.outLeft.data(), p.outRight.data()};
        AudioBusBuffers inBus;
        inBus.numChannels = 2;
        inBus.channelBuffers32 = inputs;
        AudioBusBuffers outBus;
        outBus.numChannels = 2;
        outBus.channelBuffers32 = outputs;

        ProcessData data;
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample32;
        data.numSamples = chunk;
        data.numInputs = 1;
        data.numOutputs = 1;
        data.inputs = &inBus;
        data.outputs = &outBus;
        data.inputParameterChanges = &p.inputChanges;
        data.outputParameterChanges = &p.outputChanges;
        data.inputEvents = &p.inputEvents;
        data.outputEvents = &p.outputEvents;
        data.processContext = &context;
        p.processor->process(data);

        p.inputChanges.clearQueue();
        p.outputChanges.clearQueue();
        std::copy_n(p.outLeft.data(), chunk, left + offset);
        std::copy_n(p.outRight.data(), chunk, right + offset);
    }
}

bool VstPluginInstance::isEditorOpen() const { return impl_ && impl_->editorOpen; }

void VstPluginInstance::toggleEditor()
{
    if (!impl_ || !impl_->controller) {
        return;
    }

    if (isEditorOpen()) {
        closeEditor();
        return;
    }

    const std::string title = descriptor_.label.empty() ? "Plugin" : descriptor_.label;
    const char* error = nullptr;
    impl_->thread.run([&] { error = impl_->openEditor(title); });
    lastEditorError_ = error ? Tr(error) : "";
}

void VstPluginInstance::closeEditor()
{
    if (isEditorOpen()) {
        impl_->thread.post([impl = impl_.get()] { impl->closeEditor(); });
    }
}

void VstPluginInstance::pumpEditor()
{
    if (isEditorOpen()) {
        impl_->thread.post([impl = impl_.get()] {
            if (impl->window && impl->window->closeRequested()) {
                impl->closeEditor();
            }
        });
    }
}

std::vector<VstParamInfo> VstPluginInstance::listParameters() const
{
    std::vector<VstParamInfo> params;
    if (!impl_ || !impl_->controller) {
        return params;
    }

    impl_->thread.run([&] {
        IEditController& controller = *impl_->controller;
        for (int32 i = 0; i < controller.getParameterCount(); ++i) {
            ParameterInfo info{};
            const bool listed = controller.getParameterInfo(i, info) == kResultOk &&
                                (info.flags & ParameterInfo::kCanAutomate) &&
                                !(info.flags & (ParameterInfo::kIsBypass | ParameterInfo::kIsHidden));
            if (listed) {
                params.push_back({static_cast<int32_t>(info.id), StringConvert::convert(info.title)});
            }
        }
    });

    return params;
}

void VstPluginInstance::setParameterNormalized(int32_t id, float value)
{
    if (!impl_ || !impl_->handler) {
        return;
    }

    impl_->handler->performEdit(static_cast<ParamID>(id), value);
    impl_->thread.post([controller = impl_->controller, id, value] {
        controller->setParamNormalized(static_cast<ParamID>(id), value);
    });
}

std::string VstPluginInstance::getStateBase64() const
{
    std::string state;
    if (!impl_) {
        return state;
    }

    impl_->thread.run([&] {
        MemoryStream stream;
        if (impl_->component->getState(&stream) == kResultOk) {
            state = Base64Encode(
                std::span(reinterpret_cast<const uint8_t*>(stream.getData()), static_cast<size_t>(stream.getSize()))
            );
        }
    });

    return state;
}

Status VstPluginInstance::setStateFromBase64(const std::string& base64)
{
    if (!impl_) {
        return Status::Fail(TrFormat("error.vst_not_loaded"));
    }

    std::vector<uint8_t> bytes = Base64Decode(base64);
    if (bytes.empty()) {
        return Status::Fail(TrFormat("error.vst_state_empty"));
    }

    bool restored = false;
    impl_->thread.run([&] {
        MemoryStream stream(bytes.data(), static_cast<TSize>(bytes.size()));
        restored = impl_->component->setState(&stream) == kResultOk;
        if (restored && impl_->controller) {
            stream.seek(0, IBStream::kIBSeekSet, nullptr);
            impl_->controller->setComponentState(&stream);
        }
    });

    return restored ? Status::Ok() : Status::Fail(TrFormat("error.vst_state_rejected"));
}

} // namespace imdj
