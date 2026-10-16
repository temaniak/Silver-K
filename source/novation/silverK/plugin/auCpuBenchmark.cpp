// Native-arm64 paced AU comparison host. No device, installation or UI required.
// Usage: bundle state-or-- report-prefix. Each process hosts exactly one AU.
#include <juce_audio_utils/juce_audio_utils.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/resource.h>
#include "dsp56kBase/threadtools.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
double cpuSeconds() {
    rusage u{};
    if (getrusage(RUSAGE_SELF, &u)) throw std::runtime_error("getrusage failed");
    return u.ru_utime.tv_sec + u.ru_stime.tv_sec
        + (u.ru_utime.tv_usec + u.ru_stime.tv_usec) * 1.e-6;
}
void check(OSStatus result, const char* where) {
    if (result) throw std::runtime_error(std::string(where) + ": " + std::to_string(result));
}
juce::String string(CFStringRef s) { return juce::String::fromCFString(s); }
OSType fourcc(CFStringRef s) {
    auto v = string(s).toStdString();
    if (v.size() != 4) throw std::runtime_error("Invalid component code");
    OSType result = 0;
    for (unsigned char c : v) result = (result << 8) | c;
    return result;
}
CFPropertyListRef state(AudioUnit unit) {
    CFPropertyListRef result = nullptr; UInt32 size = sizeof(result);
    check(AudioUnitGetProperty(unit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global,
                              0, &result, &size), "get state");
    return result;
}
}

int main(int argc, char** argv) try {
    if (argc != 4) return 2;
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File bundlePath(argv[1]), prefix(argv[3]);
    auto url = CFURLCreateFromFileSystemRepresentation(nullptr,
        reinterpret_cast<const UInt8*>(bundlePath.getFullPathName().toRawUTF8()),
        bundlePath.getFullPathName().getNumBytesAsUTF8(), true);
    auto bundle = CFBundleCreate(nullptr, url); CFRelease(url);
    if (!bundle || !CFBundleLoadExecutable(bundle)) throw std::runtime_error("load bundle");
    auto components = static_cast<CFArrayRef>(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("AudioComponents")));
    if (!components || CFArrayGetCount(components) < 1) throw std::runtime_error("component metadata");
    auto info = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(components, 0));
    auto value = [&](CFStringRef key) { return static_cast<CFStringRef>(CFDictionaryGetValue(info, key)); };
    auto factory = reinterpret_cast<AudioComponentFactoryFunction>(CFBundleGetFunctionPointerForName(bundle, value(CFSTR("factoryFunction"))));
    if (!factory) throw std::runtime_error("factory");
    AudioComponentDescription desc{fourcc(value(CFSTR("type"))), fourcc(value(CFSTR("subtype"))), fourcc(value(CFSTR("manufacturer"))), 0, 0};
    auto component = AudioComponentRegister(&desc, CFSTR("Isolated CPU comparison"), 0x100, factory);
    AudioUnit unit = nullptr;
    check(component ? AudioComponentInstanceNew(component, &unit) : -1, "create AU");
    struct Cleanup { AudioUnit unit; ~Cleanup() { AudioUnitUninitialize(unit); AudioComponentInstanceDispose(unit); } } cleanup{unit};
    if (juce::String(argv[2]) != "-") {
        juce::MemoryBlock bytes;
        if (!juce::File(argv[2]).loadFileAsData(bytes)) throw std::runtime_error("state file");
        auto previous = state(unit);
        auto dictionary = CFDictionaryCreateMutableCopy(nullptr, 0, static_cast<CFDictionaryRef>(previous)); CFRelease(previous);
        auto data = CFDataCreate(nullptr, static_cast<const UInt8*>(bytes.getData()), bytes.getSize());
        CFDictionarySetValue(dictionary, CFSTR("jucePluginState"), data); CFRelease(data);
        auto result = AudioUnitSetProperty(unit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &dictionary, sizeof(dictionary));
        CFRelease(dictionary); check(result, "restore state");
    }
    constexpr unsigned block = 256, rate = 48000;
    AudioStreamBasicDescription format{};
    format.mSampleRate = rate; format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    format.mBytesPerPacket = format.mBytesPerFrame = 4; format.mFramesPerPacket = 1;
    format.mChannelsPerFrame = 2; format.mBitsPerChannel = 32;
    UInt32 maximum = block;
    check(AudioUnitSetProperty(unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maximum, sizeof(maximum)), "max block");
    check(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, sizeof(format)), "format");
    check(AudioUnitInitialize(unit), "initialize");
    double latency = 0; UInt32 size = sizeof(latency);
    check(AudioUnitGetProperty(unit, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &latency, &size), "latency");
    juce::var report(new juce::DynamicObject()); auto* object = report.getDynamicObject();
    object->setProperty("bundle", bundlePath.getFullPathName());
    object->setProperty("version", string(static_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleShortVersionString")))));
    object->setProperty("host_rate", int(rate)); object->setProperty("block", int(block)); object->setProperty("latency_seconds", latency);
    juce::var scenario(new juce::DynamicObject());
    const auto scenarioPath = juce::SystemStats::getEnvironmentVariable("AU_BENCH_SCENARIO", {});
    if (scenarioPath.isNotEmpty()) {
        scenario = juce::JSON::parse(juce::File(scenarioPath));
        if (!scenario.isObject()) throw std::runtime_error("scenario JSON object required");
    }
    const int parts = scenario.hasProperty("parts") ? int(scenario["parts"]) : 1;
    if (parts != 1 && parts != 4) throw std::runtime_error("one or four parts required");
    juce::Array<juce::var> phases{0, 1, 8, 16};
    if (auto* values = scenario["phases"].getArray()) phases = *values;
    if (phases.isEmpty()) throw std::runtime_error("empty phases");
    for (const auto& v : phases) if (int(v) < 0 || int(v) > 16) throw std::runtime_error("0..16 held notes required");
    object->setProperty("scenario", scenario);
    UInt32 paramBytes = 0; Boolean writable = false;
    if (!AudioUnitGetPropertyInfo(unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, &paramBytes, &writable)) {
        std::vector<AudioUnitParameterID> ids(paramBytes / sizeof(AudioUnitParameterID));
        check(AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, ids.data(), &paramBytes), "parameter list");
        juce::Array<juce::var> params;
        for (auto id : ids) {
            AudioUnitParameterInfo p{}; size = sizeof(p);
            if (AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, id, &p, &size)) continue;
            AudioUnitParameterValue v = 0; AudioUnitGetParameter(unit, id, kAudioUnitScope_Global, 0, &v);
            juce::var row(new juce::DynamicObject()); auto* d = row.getDynamicObject();
            d->setProperty("id", int(id)); d->setProperty("name", p.flags & kAudioUnitParameterFlag_HasCFNameString ? string(p.cfNameString) : juce::String(p.name));
            d->setProperty("value", v); d->setProperty("min", p.minValue); d->setProperty("max", p.maxValue);
            if ((p.flags & kAudioUnitParameterFlag_CFNameRelease) && p.cfNameString) CFRelease(p.cfNameString);
            params.add(row);
        }
        object->setProperty("parameters", params);
    }
    if (!juce::File(prefix.getFullPathName() + ".metadata.json").replaceWithText(juce::JSON::toString(report))) return 3;
    auto saveState = [&](const char* suffix) {
        auto s = state(unit); auto data = CFPropertyListCreateData(nullptr, s, kCFPropertyListBinaryFormat_v1_0, 0, nullptr); CFRelease(s);
        if (!data) throw std::runtime_error("serialize state");
        const bool ok = juce::File(prefix.getFullPathName() + suffix).replaceWithData(CFDataGetBytePtr(data), size_t(CFDataGetLength(data)));
        CFRelease(data); if (!ok) throw std::runtime_error("write state");
    };
    auto underruns = [&]() -> juce::int64 {
        auto s = state(unit);
        auto raw = static_cast<CFDataRef>(CFDictionaryGetValue(static_cast<CFDictionaryRef>(s), CFSTR("jucePluginState")));
        auto xml = raw ? juce::AudioProcessor::getXmlFromBinary(CFDataGetBytePtr(raw), int(CFDataGetLength(raw))) : nullptr;
        const auto count = xml ? xml->getStringAttribute("underruns", "-1").getLargeIntValue() : -1;
        CFRelease(s); return count;
    };
    std::atomic<bool> done{false}; std::exception_ptr error; juce::Array<juce::var> rows;
    std::thread audio([&] { try {
        if (!dsp56k::ThreadTools::setCurrentThreadPriority(dsp56k::ThreadPriority::Highest)
            || !dsp56k::ThreadTools::setCurrentThreadRealtimeParameters(rate, block)) throw std::runtime_error("audio priority");
        std::array<std::array<float, block>, 2> output{};
        uint64_t frame = 0;
        auto render = [&](int blocks) {
            double peak = 0, maxMs = 0, settledPeak = 0, settledSquares = 0;
            uint64_t settledSamples = 0; int late = 0; bool finite = true;
            uint64_t hash = 14695981039346656037ull;
            const auto start = std::chrono::steady_clock::now(); const auto cpu = cpuSeconds();
            for (int n = 0; n < blocks; ++n) {
                struct Buffers { UInt32 count; AudioBuffer b[2]; } b{2, {{1, block * 4, output[0].data()}, {1, block * 4, output[1].data()}}};
                AudioTimeStamp t{}; t.mSampleTime = double(frame); t.mFlags = kAudioTimeStampSampleTimeValid;
                AudioUnitRenderActionFlags flags = 0; const auto call = std::chrono::steady_clock::now();
                check(AudioUnitRender(unit, &flags, &t, 0, block, reinterpret_cast<AudioBufferList*>(&b)), "render");
                const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - call).count();
                maxMs = std::max(maxMs, ms); late += ms > block * 1000.0 / rate;
                for (auto& c : output) for (float sample : c) {
                    finite &= std::isfinite(sample); peak = std::max(peak, double(std::abs(sample)));
                    if (n >= blocks / 4) { settledPeak = std::max(settledPeak, double(std::abs(sample))); settledSquares += double(sample) * sample; ++settledSamples; }
                    uint32_t bits; std::memcpy(&bits, &sample, sizeof(bits)); hash = (hash ^ bits) * 1099511628211ull;
                }
                frame += block;
                std::this_thread::sleep_until(start + std::chrono::nanoseconds(int64_t((n + 1) * block * 1000000000.0 / rate)));
            }
            juce::var row(new juce::DynamicObject()); auto* d = row.getDynamicObject();
            d->setProperty("process_cpu_seconds", cpuSeconds() - cpu);
            d->setProperty("wall_seconds", std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
            d->setProperty("audio_seconds", blocks * block / double(rate)); d->setProperty("peak", peak);
            d->setProperty("finite", finite); d->setProperty("over_budget_callbacks", late); d->setProperty("max_callback_ms", maxMs);
            d->setProperty("audio_hash", juce::String(hash));
            d->setProperty("settled_peak", settledPeak);
            d->setProperty("settled_rms", settledSamples ? std::sqrt(settledSquares / settledSamples) : 0.0);
            if (!finite) throw std::runtime_error("non-finite audio");
            return row;
        };
        render(750); // Four seconds of paced boot/queue settling.
        if (auto* messages = scenario["setup"].getArray()) for (const auto& message : *messages) {
            if (message.hasProperty("sysex_file")) {
                juce::MemoryBlock bytes;
                if (!juce::File(message["sysex_file"].toString()).loadFileAsData(bytes)) throw std::runtime_error("setup SysEx file");
                check(MusicDeviceSysEx(unit, static_cast<const UInt8*>(bytes.getData()), UInt32(bytes.getSize())), "setup SysEx");
            } else {
                auto* bytes = message["midi"].getArray();
                if (!bytes || bytes->size() != 3) throw std::runtime_error("setup MIDI triple required");
                check(MusicDeviceMIDIEvent(unit, UInt32(int((*bytes)[0])), UInt32(int((*bytes)[1])), UInt32(int((*bytes)[2])), 0), "setup MIDI");
            }
        }
        if (scenario.hasProperty("setup")) render(750);
        if (bool(scenario["probe_parts"])) {
            juce::Array<juce::var> probes;
            for (int part = 0; part < parts; ++part) {
                check(MusicDeviceMIDIEvent(unit, 0x90 | part, 48 + part * 5, 100, 0), "probe note on");
                auto probe = render(188); probe.getDynamicObject()->setProperty("channel", part + 1); probes.add(probe);
                object->setProperty("part_probes", probes);
                std::cout << "part_probe=" << juce::JSON::toString(probe, true) << std::endl;
                check(MusicDeviceMIDIEvent(unit, 0x80 | part, 48 + part * 5, 0, 0), "probe note off");
                render(375);
                if (double(probe["settled_peak"]) < 1.e-4) throw std::runtime_error("silent part probe");
            }
            object->setProperty("part_probes", probes);
        }
        const auto parameterFile = juce::SystemStats::getEnvironmentVariable("AU_BENCH_PARAMETERS", {});
        if (parameterFile.isNotEmpty()) {
            auto settings = juce::JSON::parse(juce::File(parameterFile));
            if (!settings.isObject()) throw std::runtime_error("parameter JSON object required");
            for (const auto& setting : settings.getDynamicObject()->getProperties()) {
                int found = 0;
                for (const auto& p : *report["parameters"].getArray()) if (p["name"].toString() == setting.name.toString()) {
                    const float v = float(setting.value);
                    if (v < float(p["min"]) || v > float(p["max"])) throw std::runtime_error("parameter out of range");
                    check(AudioUnitSetParameter(unit, AudioUnitParameterID(int(p["id"])), kAudioUnitScope_Global, 0, v, 0), "set parameter");
                    ++found;
                }
                if (found != 1) throw std::runtime_error("parameter missing or ambiguous: " + setting.name.toString().toStdString());
            }
            object->setProperty("parameter_overrides", settings);
        }
        const int seconds = juce::SystemStats::getEnvironmentVariable("AU_BENCH_SECONDS", "12").getIntValue();
        if (seconds < 4 || seconds > 60) throw std::runtime_error("duration 4..60 required");
        for (const auto& phase : phases) {
            const int voices = int(phase);
            for (int i = 0; i < voices; ++i) check(MusicDeviceMIDIEvent(unit, 0x90 | (i % parts), 48 + i, 100, 0), "note on");
            render(750); // Warm up each active voice path outside measurement.
            const auto beforeUnderruns = underruns();
            std::cout << "measuring voices=" << voices << std::endl;
            auto row = render(seconds * rate / block);
            const auto afterUnderruns = underruns();
            row.getDynamicObject()->setProperty("voices_requested", voices);
            row.getDynamicObject()->setProperty("missing_frames", beforeUnderruns >= 0 && afterUnderruns >= 0 ? afterUnderruns - beforeUnderruns : -1);
            rows.add(row);
            std::cout << juce::JSON::toString(row, true) << std::endl;
            for (int i = 0; i < voices; ++i) check(MusicDeviceMIDIEvent(unit, 0x80 | (i % parts), 48 + i, 0, 0), "note off");
            auto tail = render(375);
            row.getDynamicObject()->setProperty("tail_settled_rms", tail["settled_rms"]);
            row.getDynamicObject()->setProperty("tail_settled_peak", tail["settled_peak"]);
            row.getDynamicObject()->setProperty("tail_audio_hash", tail["audio_hash"]);
            if (voices && bool(scenario["require_fx_tail"]) && double(tail["settled_rms"]) < 1.e-5) throw std::runtime_error("missing effect tail");
            for (int part = 0; part < parts; ++part) check(MusicDeviceMIDIEvent(unit, 0xb0 | part, 120, 0, 0), "all sound off");
        }
    } catch (...) { error = std::current_exception(); } done = true; });
    while (!done.load()) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    audio.join(); if (error) {
        object->setProperty("measurements", rows); saveState(".failed.plist");
        juce::File(prefix.getFullPathName() + ".failed.json").replaceWithText(juce::JSON::toString(report));
        std::rethrow_exception(error);
    }
    for (auto& p : *report["parameters"].getArray()) {
        AudioUnitParameterValue v = 0;
        check(AudioUnitGetParameter(unit, AudioUnitParameterID(int(p["id"])), kAudioUnitScope_Global, 0, &v), "effective parameter");
        p.getDynamicObject()->setProperty("effective_value", v);
    }
    object->setProperty("measurements", rows); saveState(".plist");
    if (!juce::File(prefix.getFullPathName() + ".json").replaceWithText(juce::JSON::toString(report))) return 3;
    CFRelease(bundle);
    for (const auto& r : rows) if (int(r["voices_requested"]) && double(r["peak"]) < 1.e-4) return 4;
    for (const auto& r : rows) if (int(r["missing_frames"]) > 0 || int(r["over_budget_callbacks"]) > 0) return 5;
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << std::endl; return 1; }
