// MinGW 下需在本 TU 定义 INITGUID，PKEY_Device_FriendlyName 才会真正实例化
//（否则只有 extern 声明，链接报 undefined reference）。必须在 Windows 头之前。
#define INITGUID

#include "automute/audio/audio_sessions.h"
#include "automute/audio/com_ptr.h"

#include <windows.h>

#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>

#include <cstdio>

namespace {

std::string wideToUtf8(const wchar_t* w) {
  if (!w)
    return "";
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 0)
    return "";
  std::string s(n - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  return s;
}

std::string processName(DWORD pid) {
  std::string name = "(pid " + std::to_string(pid) + ")";
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (h) {
    wchar_t path[MAX_PATH];
    DWORD sz = MAX_PATH;
    if (QueryFullProcessImageNameW(h, 0, path, &sz)) {
      std::wstring w(path);
      auto pos = w.find_last_of(L"\\/");
      name = wideToUtf8((pos == std::wstring::npos) ? w.c_str()
                                                    : w.c_str() + pos + 1);
    }
    CloseHandle(h);
  }
  return name;
}

std::string deviceFriendlyName(IMMDevice* dev) {
  std::string name = "(unknown)";
  ComPtr<IPropertyStore> props;
  if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
    PROPVARIANT pv;
    PropVariantInit(&pv);
    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &pv)) &&
        pv.vt == VT_LPWSTR)
      name = wideToUtf8(pv.pwszVal);
    PropVariantClear(&pv);
  }
  return name;
}

// 遍历【每个活动输出设备】的所有会话，对每个会话调用 fn(pid, state, 设备名)。
// 注意：路由到虚拟声卡的进程，其会话在那个声卡设备上，只看默认设备会漏掉。
template <typename Fn> void forEachSession(Fn fn) {
  bool didInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

  ComPtr<IMMDeviceEnumerator> en;
  if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                 CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                 reinterpret_cast<void**>(&en)))) {
    ComPtr<IMMDeviceCollection> col;
    if (SUCCEEDED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col))) {
      UINT devCount = 0;
      col->GetCount(&devCount);
      for (UINT d = 0; d < devCount; ++d) {
        ComPtr<IMMDevice> dev;
        if (FAILED(col->Item(d, &dev)))
          continue;
        std::string devName = deviceFriendlyName(dev.get());

        ComPtr<IAudioSessionManager2> mgr;
        if (FAILED(dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                                 nullptr, reinterpret_cast<void**>(&mgr))))
          continue;
        ComPtr<IAudioSessionEnumerator> sessions;
        if (FAILED(mgr->GetSessionEnumerator(&sessions)))
          continue;
        int count = 0;
        sessions->GetCount(&count);
        for (int i = 0; i < count; ++i) {
          ComPtr<IAudioSessionControl> ctrl;
          if (FAILED(sessions->GetSession(i, &ctrl)))
            continue;
          ComPtr<IAudioSessionControl2> ctrl2;
          if (FAILED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2),
                                          reinterpret_cast<void**>(&ctrl2))))
            continue;
          DWORD pid = 0;
          ctrl2->GetProcessId(&pid);
          AudioSessionState state = AudioSessionStateInactive;
          ctrl2->GetState(&state);
          fn(pid, state, devName);
        }
      }
    }
  }
  if (didInit)
    CoUninitialize();
}

} // namespace

std::vector<AudioSessionInfo> listAudioSessions() {
  std::vector<AudioSessionInfo> out;
  forEachSession([&](DWORD pid, AudioSessionState state,
                     const std::string& dev) {
    if (pid == 0)
      return; // 系统会话
    out.push_back({(uint32_t)pid, processName(pid),
                   state == AudioSessionStateActive, dev});
  });
  return out;
}

void printAudioSessions() {
  printf("有音频会话的进程（--proc 用 PID）:\n");
  for (auto& s : listAudioSessions())
    printf("  PID %-6u %-22s %-8s 输出→ %s\n", s.pid, s.name.c_str(),
           s.active ? "[出声中]" : "[静默]", s.device.c_str());
}
