#include "Dsp.h"
#include "Editor.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace EqTune {
static const FUID processorUid(0x9DE36417, 0xD28644D2, 0xBCA0C355, 0x47F431CE);
static const FUID controllerUid(0xDFE3DA20, 0xAF974F52, 0x8FA2C913, 0x13827B04);

bool readValues(IBStream* stream, Values& values)
{
    if (!stream) return false;
    IBStreamer reader(stream, kLittleEndian);
    int32 version = 0;
    Values loaded = defaults;
    if (!reader.readInt32(version) || version < 1 || version > 3) return false;
    const auto count = version == 1 ? 8 : (version == 2 ? 10 : parameterCount);
    for (int i = 0; i < count; ++i)
        if (!reader.readDouble(loaded[i]) || !validValue(loaded[i])) return false;
    values = loaded;
    return true;
}

class Processor final : public AudioEffect {
public:
    Processor() { setControllerClass(controllerUid); }
    static FUnknown* create(void*) { return static_cast<IAudioProcessor*>(new Processor); }
    tresult PLUGIN_API initialize(FUnknown* context) override
    {
        const auto result = AudioEffect::initialize(context);
        if (result != kResultOk) return result;
        addAudioInput(STR16("Game audio"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Processed audio"), SpeakerArr::kStereo);
        return kResultOk;
    }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIn,
                                         SpeakerArrangement* outputs, int32 numOut) override
    {
        if (numIn != 1 || numOut != 1 || inputs[0] != outputs[0] ||
            (inputs[0] != SpeakerArr::kMono && inputs[0] != SpeakerArr::kStereo))
            return kResultFalse;
        return AudioEffect::setBusArrangements(inputs, numIn, outputs, numOut);
    }
    tresult PLUGIN_API canProcessSampleSize(int32 size) override
    {
        return size == kSample32 || size == kSample64 ? kResultTrue : kResultFalse;
    }
    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) override
    {
        if (!std::isfinite(setup.sampleRate) || setup.sampleRate <= 0)
            return kInvalidArgument;
        return AudioEffect::setupProcessing(setup);
    }
    tresult PLUGIN_API setActive(TBool active) override
    {
        if (active) engine.prepare(processSetup.sampleRate, values);
        return AudioEffect::setActive(active);
    }
    tresult PLUGIN_API process(ProcessData& data) override
    {
        if (data.inputParameterChanges) {
            for (int32 i = 0; i < data.inputParameterChanges->getParameterCount(); ++i) {
                auto* queue = data.inputParameterChanges->getParameterData(i);
                if (!queue || queue->getParameterId() >= parameterCount) continue;
                int32 offset = 0;
                ParamValue value = 0;
                if (queue->getPoint(queue->getPointCount() - 1, offset, value) == kResultTrue && validValue(value))
                    values[queue->getParameterId()] = value;
            }
        }
        if (data.numInputs == 0 || data.numOutputs == 0 || data.numSamples == 0) return kResultOk;
        const auto channels = data.inputs[0].numChannels;
        if (channels == 0 && data.outputs[0].numChannels == 0) return kResultOk;
        if (channels < 1 || channels > 2 || data.outputs[0].numChannels != channels) return kInvalidArgument;
        if (data.symbolicSampleSize == kSample32)
            engine.process(data.inputs[0].channelBuffers32, data.outputs[0].channelBuffers32,
                           channels, data.numSamples, values);
        else if (data.symbolicSampleSize == kSample64)
            engine.process(data.inputs[0].channelBuffers64, data.outputs[0].channelBuffers64,
                           channels, data.numSamples, values);
        else return kInvalidArgument;
        data.outputs[0].silenceFlags = 0;
        for (int channel = 0; channel < channels; ++channel) {
            bool silent = true;
            for (int frame = 0; frame < data.numSamples && silent; ++frame)
                silent = data.symbolicSampleSize == kSample32
                    ? data.outputs[0].channelBuffers32[channel][frame] == 0
                    : data.outputs[0].channelBuffers64[channel][frame] == 0;
            if (silent) data.outputs[0].silenceFlags |= uint64(1) << channel;
        }
        return kResultOk;
    }
    tresult PLUGIN_API getState(IBStream* stream) override
    {
        if (!stream) return kInvalidArgument;
        IBStreamer writer(stream, kLittleEndian);
        if (!writer.writeInt32(3)) return kResultFalse;
        for (const auto value : values)
            if (!writer.writeDouble(value)) return kResultFalse;
        return kResultOk;
    }
    tresult PLUGIN_API setState(IBStream* stream) override
    {
        return readValues(stream, values) ? kResultOk : kResultFalse;
    }
private:
    Values values = defaults;
    Engine engine;
};

class Controller final : public EditController {
public:
    static FUnknown* create(void*) { return static_cast<IEditController*>(new Controller); }
    tresult PLUGIN_API initialize(FUnknown* context) override
    {
        const auto result = EditController::initialize(context);
        if (result != kResultOk) return result;
        const TChar* names[] = { STR16("Low - 120 Hz"), STR16("Low-mid - 350 Hz"),
            STR16("Mid - 900 Hz"), STR16("Presence - 2.8 kHz"), STR16("Clarity - 5 kHz") };
        for (int i = 0; i < 5; ++i)
            parameters.addParameter(new RangeParameter(names[i], i, STR16("dB"), -6, 6, gainDb(defaults[i])));
        parameters.addParameter(new RangeParameter(STR16("Preamp"), preampId, STR16("dB"), -18, 0, -8));
        parameters.addParameter(new RangeParameter(STR16("Intensity"), intensityId, STR16("%"), 0, 100, 100));
        parameters.addParameter(STR16("Bypass EQ"), nullptr, 1, 0,
            ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, bypassId);
        parameters.addParameter(STR16("Loudness EQ"), nullptr, 1, 0,
            ParameterInfo::kCanAutomate, loudnessEnabledId);
        parameters.addParameter(new RangeParameter(STR16("Loudness intensity"), loudnessIntensityId,
            STR16("%"), 0, 100, 50));
        parameters.addParameter(STR16("Strong loudness"), nullptr, 1, 0,
            ParameterInfo::kCanAutomate, loudnessStrongId);
        return kResultOk;
    }
    tresult PLUGIN_API setComponentState(IBStream* stream) override
    {
        Values loaded {};
        if (!readValues(stream, loaded)) return kResultFalse;
        for (int i = 0; i < parameterCount; ++i) setParamNormalized(i, loaded[i]);
        return kResultOk;
    }
    IPlugView* PLUGIN_API createView(FIDString name) override
    {
        return name && std::strcmp(name, ViewType::kEditor) == 0 ? new Editor(this) : nullptr;
    }
};
}

bool InitModule() { return true; }
bool DeinitModule() { return true; }

BEGIN_FACTORY_DEF("EqTune", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(EqTune::processorUid), PClassInfo::kManyInstances,
    kVstAudioEffectClass, "EqTune", Vst::kDistributable, "Fx|EQ", "0.2.8",
    kVstVersionString, EqTune::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(EqTune::controllerUid), PClassInfo::kManyInstances,
    kVstComponentControllerClass, "EqTune Controller", 0, "", "0.2.8",
    kVstVersionString, EqTune::Controller::create)
END_FACTORY
