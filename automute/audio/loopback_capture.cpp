#include "automute/audio/loopback_capture.h"
#include "automute/audio/com_ptr.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#include <vector>

// COM 对象集中放在 Impl 里，头文件不暴露 WASAPI 类型。
struct LoopbackCapture::Impl {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice>           device;
    ComPtr<IAudioClient>        client;
    ComPtr<IAudioCaptureClient> capture;
    WAVEFORMATEX*               mixFormat = nullptr; // 由 GetMixFormat 用 CoTaskMemAlloc 分配
    bool coInitialized = false;

    ~Impl() {
        if (mixFormat) CoTaskMemFree(mixFormat);
        // ComPtr 自动 Release；顺序：先 capture/client 再 device/enumerator。
        capture.reset();
        client.reset();
        device.reset();
        enumerator.reset();
        if (coInitialized) CoUninitialize();
    }
};

LoopbackCapture::~LoopbackCapture() {
    delete impl_;
}

void LoopbackCapture::fail(const std::string& where, long hr) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s failed (hr=0x%08lX)", where.c_str(), static_cast<unsigned long>(hr));
    error_ = buf;
}

bool LoopbackCapture::initialize() {
    impl_ = new Impl();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { fail("CoInitializeEx", hr); return false; }
    impl_->coInitialized = true;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&impl_->enumerator));
    if (FAILED(hr)) { fail("CoCreateInstance(MMDeviceEnumerator)", hr); return false; }

    // loopback 抓的是“输出端”(eRender) 正在播的声音，eConsole = 默认播放设备。
    hr = impl_->enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &impl_->device);
    if (FAILED(hr)) { fail("GetDefaultAudioEndpoint", hr); return false; }

    hr = impl_->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void**>(&impl_->client));
    if (FAILED(hr)) { fail("Activate(IAudioClient)", hr); return false; }

    hr = impl_->client->GetMixFormat(&impl_->mixFormat);
    if (FAILED(hr)) { fail("GetMixFormat", hr); return false; }

    WAVEFORMATEX* wf = impl_->mixFormat;
    sampleRate_    = wf->nSamplesPerSec;
    channels_      = wf->nChannels;
    bitsPerSample_ = wf->wBitsPerSample;

    // 判断采样格式：共享模式 mix format 几乎总是 32-bit float。
    if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        isFloat_ = true;
    } else if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wf);
        isFloat_ = (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }

    // 关键：AUDCLNT_STREAMFLAGS_LOOPBACK 让“渲染设备”反向变成可抓取的来源。
    REFERENCE_TIME bufferDuration = 10'000'000 / 50; // 200ms 缓冲（单位 100ns）
    hr = impl_->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                   AUDCLNT_STREAMFLAGS_LOOPBACK,
                                   bufferDuration, 0, wf, nullptr);
    if (FAILED(hr)) { fail("IAudioClient::Initialize", hr); return false; }

    hr = impl_->client->GetService(__uuidof(IAudioCaptureClient),
                                   reinterpret_cast<void**>(&impl_->capture));
    if (FAILED(hr)) { fail("GetService(IAudioCaptureClient)", hr); return false; }

    return true;
}

void LoopbackCapture::run(const DataCallback& cb, std::atomic<bool>& keepRunning) {
    HRESULT hr = impl_->client->Start();
    if (FAILED(hr)) { fail("IAudioClient::Start", hr); return; }

    std::vector<float> scratch; // 非 float 格式时用于转换

    while (keepRunning.load(std::memory_order_relaxed)) {
        UINT32 packetFrames = 0;
        hr = impl_->capture->GetNextPacketSize(&packetFrames);
        if (FAILED(hr)) { fail("GetNextPacketSize", hr); break; }

        if (packetFrames == 0) {
            // 没有新数据，睡半个缓冲周期再轮询（loopback 不适合事件驱动）。
            Sleep(5);
            continue;
        }

        BYTE*  data = nullptr;
        UINT32 frames = 0;
        DWORD  flags = 0;
        hr = impl_->capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
        if (FAILED(hr)) { fail("GetBuffer", hr); break; }

        const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

        if (isFloat_ && !silent) {
            cb(reinterpret_cast<const float*>(data), frames, channels_);
        } else if (silent) {
            scratch.assign(static_cast<size_t>(frames) * channels_, 0.0f);
            cb(scratch.data(), frames, channels_);
        } else {
            // 退路：把 16-bit PCM 转成 float（共享模式下基本用不到）。
            scratch.resize(static_cast<size_t>(frames) * channels_);
            if (bitsPerSample_ == 16) {
                const int16_t* pcm = reinterpret_cast<const int16_t*>(data);
                for (size_t i = 0; i < scratch.size(); ++i)
                    scratch[i] = pcm[i] / 32768.0f;
            }
            cb(scratch.data(), frames, channels_);
        }

        hr = impl_->capture->ReleaseBuffer(frames);
        if (FAILED(hr)) { fail("ReleaseBuffer", hr); break; }
    }

    impl_->client->Stop();
}
