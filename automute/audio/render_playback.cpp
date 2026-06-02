#include "automute/audio/render_playback.h"
#include "automute/audio/com_ptr.h"

#include <windows.h>

#include <audioclient.h>
#include <mmdeviceapi.h>

#include <vector>

struct RenderPlayback::Impl {
  ComPtr<IMMDeviceEnumerator> enumerator;
  ComPtr<IMMDevice> device;
  ComPtr<IAudioClient> client;
  ComPtr<IAudioRenderClient> render;
  WAVEFORMATEX* mixFormat = nullptr;
  HANDLE bufferEvent = nullptr; // 引擎“要数据了”时触发它
  UINT32 bufferFrames = 0;      // 设备缓冲总帧数
  bool coInitialized = false;

  ~Impl() {
    if (mixFormat)
      CoTaskMemFree(mixFormat);
    if (bufferEvent)
      CloseHandle(bufferEvent);
    render.reset();
    client.reset();
    device.reset();
    enumerator.reset();
    if (coInitialized)
      CoUninitialize();
  }
};

RenderPlayback::~RenderPlayback() { delete impl_; }

void RenderPlayback::fail(const std::string& where, long hr) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s failed (hr=0x%08lX)", where.c_str(),
           static_cast<unsigned long>(hr));
  error_ = buf;
}

bool RenderPlayback::initialize(int deviceIndex) {
  impl_ = new Impl();

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    fail("CoInitializeEx", hr);
    return false;
  }
  impl_->coInitialized = true;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator),
                        reinterpret_cast<void**>(&impl_->enumerator));
  if (FAILED(hr)) {
    fail("CoCreateInstance", hr);
    return false;
  }

  if (deviceIndex < 0) {
    hr = impl_->enumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                    &impl_->device);
    if (FAILED(hr)) {
      fail("GetDefaultAudioEndpoint", hr);
      return false;
    }
  } else {
    ComPtr<IMMDeviceCollection> col;
    hr = impl_->enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                               &col);
    if (FAILED(hr)) {
      fail("EnumAudioEndpoints", hr);
      return false;
    }
    hr = col->Item((UINT)deviceIndex, &impl_->device);
    if (FAILED(hr)) {
      fail("Collection::Item(设备序号越界?)", hr);
      return false;
    }
  }

  hr = impl_->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(&impl_->client));
  if (FAILED(hr)) {
    fail("Activate", hr);
    return false;
  }

  hr = impl_->client->GetMixFormat(&impl_->mixFormat);
  if (FAILED(hr)) {
    fail("GetMixFormat", hr);
    return false;
  }
  sampleRate_ = impl_->mixFormat->nSamplesPerSec;
  channels_ = impl_->mixFormat->nChannels;

  // 事件驱动模式：传 EVENTCALLBACK 标志，buffer/periodicity 给 0 =
  // 用引擎默认周期。
  hr = impl_->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0,
                                 impl_->mixFormat, nullptr);
  if (FAILED(hr)) {
    fail("IAudioClient::Initialize", hr);
    return false;
  }

  // 创建事件并注册：引擎需要数据时会 SetEvent 它，渲染线程靠它醒来。
  impl_->bufferEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!impl_->bufferEvent) {
    fail("CreateEvent", GetLastError());
    return false;
  }
  hr = impl_->client->SetEventHandle(impl_->bufferEvent);
  if (FAILED(hr)) {
    fail("SetEventHandle", hr);
    return false;
  }

  hr = impl_->client->GetBufferSize(&impl_->bufferFrames);
  if (FAILED(hr)) {
    fail("GetBufferSize", hr);
    return false;
  }

  hr = impl_->client->GetService(__uuidof(IAudioRenderClient),
                                 reinterpret_cast<void**>(&impl_->render));
  if (FAILED(hr)) {
    fail("GetService", hr);
    return false;
  }

  return true;
}

void RenderPlayback::run(SpscRingBuffer<float>& src,
                         std::atomic<bool>& keepRunning,
                         std::atomic<bool>* muted) {
  const uint32_t ch = channels_;

  // 先把整个设备缓冲填一遍静音，确保启动平滑。
  BYTE* data = nullptr;
  HRESULT hr = impl_->render->GetBuffer(impl_->bufferFrames, &data);
  if (SUCCEEDED(hr)) {
    impl_->render->ReleaseBuffer(impl_->bufferFrames,
                                 AUDCLNT_BUFFERFLAGS_SILENT);
  }

  hr = impl_->client->Start();
  if (FAILED(hr)) {
    fail("IAudioClient::Start", hr);
    return;
  }

  std::vector<float> chunk;

  while (keepRunning.load(std::memory_order_relaxed)) {
    // 等引擎唤醒（最多等 200ms，以便能响应退出）。
    DWORD wait = WaitForSingleObject(impl_->bufferEvent, 200);
    if (wait != WAIT_OBJECT_0)
      continue;

    UINT32 padding = 0; // 设备缓冲里还没播完的帧数
    if (FAILED(impl_->client->GetCurrentPadding(&padding)))
      break;

    const UINT32 framesToWrite = impl_->bufferFrames - padding;
    if (framesToWrite == 0)
      continue;

    if (FAILED(impl_->render->GetBuffer(framesToWrite, &data)))
      break;

    // 从环形缓冲取 framesToWrite*ch 个样本；不够就用 0 补（underrun → 静音）。
    const size_t need = static_cast<size_t>(framesToWrite) * ch;
    chunk.resize(need);
    const size_t got = src.read(chunk.data(), need);
    for (size_t i = got; i < need; ++i)
      chunk[i] = 0.0f;

    // 自动掐声：判定为目标在说话时，本块输出静音。
    if (muted && muted->load(std::memory_order_relaxed))
      std::memset(chunk.data(), 0, need * sizeof(float));

    std::memcpy(data, chunk.data(), need * sizeof(float));
    impl_->render->ReleaseBuffer(framesToWrite, 0);
  }

  impl_->client->Stop();
}
