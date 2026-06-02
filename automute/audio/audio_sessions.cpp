#include "automute/audio/audio_sessions.h"
#include "automute/audio/com_ptr.h"

#include <windows.h>

#include <audiopolicy.h>
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

// 遍历默认输出设备的所有会话，对每个会话调用 fn(ctrl2, pid, state)。
template <typename Fn> void forEachSession(Fn fn) {
  bool didInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

  ComPtr<IMMDeviceEnumerator> en;
  if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                 CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                 reinterpret_cast<void**>(&en)))) {
    ComPtr<IMMDevice> dev;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev))) {
      ComPtr<IAudioSessionManager2> mgr;
      if (SUCCEEDED(dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                                  nullptr,
                                  reinterpret_cast<void**>(&mgr)))) {
        ComPtr<IAudioSessionEnumerator> sessions;
        if (SUCCEEDED(mgr->GetSessionEnumerator(&sessions))) {
          int count = 0;
          sessions->GetCount(&count);
          for (int i = 0; i < count; ++i) {
            ComPtr<IAudioSessionControl> ctrl;
            if (FAILED(sessions->GetSession(i, &ctrl)))
              continue;
            ComPtr<IAudioSessionControl2> ctrl2;
            if (FAILED(ctrl->QueryInterface(
                    __uuidof(IAudioSessionControl2),
                    reinterpret_cast<void**>(&ctrl2))))
              continue;
            DWORD pid = 0;
            ctrl2->GetProcessId(&pid);
            AudioSessionState state = AudioSessionStateInactive;
            ctrl2->GetState(&state);
            fn(ctrl2.get(), pid, state);
          }
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
  forEachSession([&](IAudioSessionControl2* ctrl2, DWORD pid,
                     AudioSessionState state) {
    (void)ctrl2;
    if (pid == 0)
      return; // 系统会话
    out.push_back(
        {(uint32_t)pid, processName(pid), state == AudioSessionStateActive});
  });
  return out;
}

void printAudioSessions() {
  printf("正在出声/有音频会话的进程（--proc 用 PID）:\n");
  for (auto& s : listAudioSessions())
    printf("  PID %-6u %-30s %s\n", s.pid, s.name.c_str(),
           s.active ? "[出声中]" : "[静默]");
}
