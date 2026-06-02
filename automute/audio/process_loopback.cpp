#include "automute/audio/process_loopback.h"
#include "automute/audio/com_ptr.h"

#include <windows.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>

#include <vector>

// ---- 手动补全 MinGW 缺失的进程 loopback API（源自 audioclientactivationparams.h）----
// 用 VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK 这个宏判断官方头是否已提供。
#ifndef VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK
typedef enum {
  AUDIOCLIENT_ACTIVATION_TYPE_DEFAULT = 0,
  AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK = 1
} AUDIOCLIENT_ACTIVATION_TYPE;

typedef enum {
  PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE = 0,
  PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE = 1
} PROCESS_LOOPBACK_MODE;

typedef struct {
  DWORD TargetProcessId;
  PROCESS_LOOPBACK_MODE ProcessLoopbackMode;
} AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS;

typedef struct {
  AUDIOCLIENT_ACTIVATION_TYPE ActivationType;
  union {
    AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS ProcessLoopbackParams;
  };
} AUDIOCLIENT_ACTIVATION_PARAMS;

#define VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK L"VAD\\Process_Loopback"
#endif

namespace {

// ActivateAudioInterfaceAsync 是异步的：完成时回调这个处理器。我们用事件同步等待。
class CompletionHandler : public IActivateAudioInterfaceCompletionHandler,
                          public IAgileObject {
public:
  HANDLE done = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  HRESULT activateResult = E_FAIL;
  IAudioClient* client = nullptr;

  STDMETHODIMP ActivateCompleted(
      IActivateAudioInterfaceAsyncOperation* op) override {
    IUnknown* unk = nullptr;
    op->GetActivateResult(&activateResult, &unk);
    if (unk) {
      unk->QueryInterface(__uuidof(IAudioClient),
                          reinterpret_cast<void**>(&client));
      unk->Release();
    }
    SetEvent(done);
    return S_OK;
  }

  // 最小 IUnknown（栈对象，引用计数不真正释放）。
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (riid == __uuidof(IUnknown) ||
        riid == __uuidof(IActivateAudioInterfaceCompletionHandler)) {
      *ppv = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
      return S_OK;
    }
    if (riid == __uuidof(IAgileObject)) {
      *ppv = static_cast<IAgileObject*>(this);
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  STDMETHODIMP_(ULONG) AddRef() override { return 1; }
  STDMETHODIMP_(ULONG) Release() override { return 1; }
};

} // namespace

struct ProcessLoopbackCapture::Impl {
  ComPtr<IAudioClient> client;
  ComPtr<IAudioCaptureClient> capture;
  HANDLE bufferEvent = nullptr;
  bool coInitialized = false;

  ~Impl() {
    capture.reset();
    client.reset();
    if (bufferEvent)
      CloseHandle(bufferEvent);
    if (coInitialized)
      CoUninitialize();
  }
};

ProcessLoopbackCapture::~ProcessLoopbackCapture() { delete impl_; }

void ProcessLoopbackCapture::fail(const std::string& where, long hr) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s failed (hr=0x%08lX)", where.c_str(),
           static_cast<unsigned long>(hr));
  error_ = buf;
}

bool ProcessLoopbackCapture::initialize(uint32_t pid) {
  impl_ = new Impl();

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    fail("CoInitializeEx", hr);
    return false;
  }
  impl_->coInitialized = true;

  // 把"抓哪个进程"打包成 PROPVARIANT(BLOB)，交给异步激活。
  AUDIOCLIENT_ACTIVATION_PARAMS params = {};
  params.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
  params.ProcessLoopbackParams.TargetProcessId = (DWORD)pid;
  params.ProcessLoopbackParams.ProcessLoopbackMode =
      PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

  PROPVARIANT pv = {};
  pv.vt = VT_BLOB;
  pv.blob.cbSize = sizeof(params);
  pv.blob.pBlobData = reinterpret_cast<BYTE*>(&params);

  CompletionHandler handler;
  IActivateAudioInterfaceAsyncOperation* op = nullptr;
  hr = ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
                                   __uuidof(IAudioClient), &pv, &handler, &op);
  if (FAILED(hr)) {
    fail("ActivateAudioInterfaceAsync", hr);
    return false;
  }
  WaitForSingleObject(handler.done, INFINITE);
  if (op)
    op->Release();
  if (FAILED(handler.activateResult) || !handler.client) {
    fail("GetActivateResult", handler.activateResult);
    return false;
  }
  impl_->client.attach(handler.client); // 接管所有权（已 AddRef）

  // 进程 loopback 没有 mix format，自己指定：48kHz / 2ch / float（与设备 loopback 一致）。
  WAVEFORMATEX wf = {};
  wf.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
  wf.nChannels = 2;
  wf.nSamplesPerSec = 48000;
  wf.wBitsPerSample = 32;
  wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
  wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
  wf.cbSize = 0;

  REFERENCE_TIME dur = 2000000; // 200ms
  hr = impl_->client->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, dur, 0,
      &wf, nullptr);
  if (FAILED(hr)) {
    fail("IAudioClient::Initialize", hr);
    return false;
  }
  sampleRate_ = wf.nSamplesPerSec;
  channels_ = wf.nChannels;

  impl_->bufferEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (FAILED(impl_->client->SetEventHandle(impl_->bufferEvent))) {
    fail("SetEventHandle", -1);
    return false;
  }
  hr = impl_->client->GetService(__uuidof(IAudioCaptureClient),
                                 reinterpret_cast<void**>(&impl_->capture));
  if (FAILED(hr)) {
    fail("GetService(IAudioCaptureClient)", hr);
    return false;
  }
  return true;
}

void ProcessLoopbackCapture::run(const DataCallback& cb,
                                 std::atomic<bool>& keepRunning) {
  HRESULT hr = impl_->client->Start();
  if (FAILED(hr)) {
    fail("IAudioClient::Start", hr);
    return;
  }
  std::vector<float> scratch;

  while (keepRunning.load(std::memory_order_relaxed)) {
    if (WaitForSingleObject(impl_->bufferEvent, 200) != WAIT_OBJECT_0)
      continue;

    UINT32 packet = 0;
    while (SUCCEEDED(impl_->capture->GetNextPacketSize(&packet)) &&
           packet > 0) {
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      if (FAILED(impl_->capture->GetBuffer(&data, &frames, &flags, nullptr,
                                           nullptr)))
        break;
      const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
      if (silent) {
        scratch.assign((size_t)frames * channels_, 0.0f);
        cb(scratch.data(), frames, channels_);
      } else {
        cb(reinterpret_cast<const float*>(data), frames, channels_);
      }
      impl_->capture->ReleaseBuffer(frames);
    }
  }
  impl_->client->Stop();
}
