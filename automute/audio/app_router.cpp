// MinGW 下需 INITGUID 才能实例化 PKEY_Device_FriendlyName（同 audio_sessions.cpp）。
#define INITGUID

#include "automute/audio/app_router.h"
#include "automute/audio/audio_sessions.h"
#include "automute/audio/com_ptr.h"

#include <windows.h>

#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <roapi.h>
#include <winstring.h>

#include <fstream>
#include <sstream>

namespace approuter {
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

std::wstring utf8ToWide(const std::string& s) {
  if (s.empty())
    return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  if (n <= 0)
    return L"";
  std::wstring w(n - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
  return w;
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

} // namespace

std::vector<Endpoint> listRenderEndpoints() {
  std::vector<Endpoint> out;
  bool didInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

  ComPtr<IMMDeviceEnumerator> en;
  if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                 CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                 reinterpret_cast<void**>(&en)))) {
    ComPtr<IMMDeviceCollection> col;
    if (SUCCEEDED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col))) {
      UINT count = 0;
      col->GetCount(&count);
      for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> dev;
        if (FAILED(col->Item(i, &dev)))
          continue;
        LPWSTR id = nullptr;
        if (FAILED(dev->GetId(&id)) || !id)
          continue;
        out.push_back({wideToUtf8(id), deviceFriendlyName(dev.get())});
        CoTaskMemFree(id);
      }
    }
  }
  if (didInit)
    CoUninitialize();
  return out;
}

bool findRenderEndpoint(const std::string& nameSubstr, Endpoint& out) {
  for (auto& ep : listRenderEndpoints()) {
    if (ep.name.find(nameSubstr) != std::string::npos) {
      out = ep;
      return true;
    }
  }
  return false;
}

bool cableInstalled(std::string& endpointId) {
  Endpoint ep;
  // VB-CABLE 的渲染端友好名形如 "CABLE Input (VB-Audio Virtual Cable)"。
  if (findRenderEndpoint("CABLE Input", ep)) {
    endpointId = ep.id;
    return true;
  }
  return false;
}

// ───────────────────────── M4.3b：未公开路由 ─────────────────────────

// 未公开 IAudioPolicyConfigFactory（EarTrumpet 权威定义，本机 19044 实测验通）。
// vtable：IUnknown(0-2)+IInspectable(3-5)+19 个 incomplete(6-24)+
//         Set(25)/Get(26)/Clear(27)。前 19 个只占槽位，不调。
// ⚠️【必须外部链接，不能放匿名命名空间】否则 GCC -O2 把它当"全可见多态类型"
//    去虚化(devirtualize)到自己生成的 vtable，而真实对象是外部 COM 对象、vtable
//    不是 GCC 那张 → 虚调用跳飞段错误（本机 -O2 实测崩，移出匿名后解决）。
#define APC_RES(n) virtual HRESULT STDMETHODCALLTYPE __res_##n() = 0;
struct IAudioPolicyConfigFactory : public IInspectable {
  APC_RES(0) APC_RES(1) APC_RES(2) APC_RES(3) APC_RES(4) APC_RES(5) APC_RES(6)
  APC_RES(7) APC_RES(8) APC_RES(9) APC_RES(10) APC_RES(11) APC_RES(12)
  APC_RES(13) APC_RES(14) APC_RES(15) APC_RES(16) APC_RES(17) APC_RES(18)
  virtual HRESULT STDMETHODCALLTYPE SetPersistedDefaultAudioEndpoint(
      DWORD pid, EDataFlow flow, ERole role, HSTRING devId) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetPersistedDefaultAudioEndpoint(
      DWORD pid, EDataFlow flow, ERole role, HSTRING* devId) = 0;
  virtual HRESULT STDMETHODCALLTYPE
  ClearAllPersistedApplicationDefaultEndpoints() = 0;
};
#undef APC_RES

namespace {

// 21H2+/Win11 用 ab3d4648，旧版 2a59116d（方法序相同）。先试新、回退旧。
const GUID IID_APC_21H2 = {
    0xab3d4648, 0xe242, 0x459f, {0xb0, 0x2f, 0x54, 0x1c, 0x70, 0x30, 0x63, 0x24}};
const GUID IID_APC_Downlevel = {
    0x2a59116d, 0x6c4f, 0x45e0, {0xa7, 0x4f, 0x70, 0x7e, 0x3f, 0xef, 0x92, 0x58}};

// 路由的两个 role（媒体播放用这俩；通信 role 留给语音不动）。
const ERole kRoles[] = {eConsole, eMultimedia};

// 激活工厂（调用方负责 RoInitialize 与 Release）。失败返回 nullptr。
IAudioPolicyConfigFactory* activateFactory() {
  HSTRING cls = nullptr;
  const wchar_t* name = L"Windows.Media.Internal.AudioPolicyConfig";
  if (FAILED(WindowsCreateString(name, (UINT32)wcslen(name), &cls)))
    return nullptr;
  IAudioPolicyConfigFactory* fac = nullptr;
  HRESULT hr = RoGetActivationFactory(cls, IID_APC_21H2,
                                      reinterpret_cast<void**>(&fac));
  if (FAILED(hr) || !fac)
    hr = RoGetActivationFactory(cls, IID_APC_Downlevel,
                                reinterpret_cast<void**>(&fac));
  WindowsDeleteString(cls);
  return SUCCEEDED(hr) ? fac : nullptr;
}

// 把端点 id（{0.0.0...}.{...} 形式，ASCII）打包成路由 API 要的设备路径。
std::wstring packRender(const std::string& endpointId) {
  return L"\\\\?\\SWD#MMDEVAPI#" + utf8ToWide(endpointId) +
         L"#{e6327cad-dcec-4949-ae8a-991e976a79d2}";
}

std::string exeNameOf(DWORD pid) {
  std::string name;
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (h) {
    wchar_t path[MAX_PATH];
    DWORD sz = MAX_PATH;
    if (QueryFullProcessImageNameW(h, 0, path, &sz)) {
      std::wstring w(path);
      auto pos = w.find_last_of(L"\\/");
      name = wideToUtf8(pos == std::wstring::npos ? w.c_str()
                                                  : w.c_str() + pos + 1);
    }
    CloseHandle(h);
  }
  return name;
}

std::string journalPath() {
  const char* base = getenv("LOCALAPPDATA");
  std::string dir = base ? base : ".";
  dir += "\\AutoMute";
  CreateDirectoryA(dir.c_str(), nullptr); // 已存在则无害失败
  return dir + "\\route_journal.txt";
}

// journal：路由【前】落盘，记 pid/exe + 两 role 原值；还原后删。崩溃后启动据此兜底。
struct Journal {
  DWORD pid = 0;
  std::string exe;
  std::string origConsole, origMultimedia; // UTF-8，空=原本无覆盖
  bool valid = false;
};

void writeJournal(const Journal& j) {
  std::ofstream f(journalPath(), std::ios::trunc);
  if (!f)
    return;
  f << "pid=" << j.pid << "\n";
  f << "exe=" << j.exe << "\n";
  f << "console=" << j.origConsole << "\n";
  f << "multimedia=" << j.origMultimedia << "\n";
}

Journal readJournal() {
  Journal j;
  std::ifstream f(journalPath());
  if (!f)
    return j;
  std::string line;
  while (std::getline(f, line)) {
    auto eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    std::string k = line.substr(0, eq), v = line.substr(eq + 1);
    if (k == "pid")
      j.pid = (DWORD)std::stoul(v.empty() ? "0" : v);
    else if (k == "exe")
      j.exe = v;
    else if (k == "console")
      j.origConsole = v;
    else if (k == "multimedia")
      j.origMultimedia = v;
  }
  j.valid = (j.pid != 0);
  return j;
}

void deleteJournal() { DeleteFileA(journalPath().c_str()); }

// 读某进程某 role 的持久端点（UTF-8；无覆盖/失败返回空）。
std::string getPersisted(IAudioPolicyConfigFactory* fac, DWORD pid, ERole role) {
  HSTRING h = nullptr;
  if (FAILED(fac->GetPersistedDefaultAudioEndpoint(pid, eRender, role, &h)))
    return ""; // E_INVALIDARG = 无覆盖，正常
  UINT32 l = 0;
  const wchar_t* b = WindowsGetStringRawBuffer(h, &l);
  std::string s = (l && b) ? wideToUtf8(std::wstring(b, l).c_str()) : "";
  WindowsDeleteString(h);
  return s;
}

// 设某进程某 role 的持久端点（dev 为 UTF-8 打包路径；空=清除还原成默认）。
HRESULT setPersisted(IAudioPolicyConfigFactory* fac, DWORD pid, ERole role,
                     const std::string& dev) {
  HSTRING h = nullptr;
  if (!dev.empty()) {
    std::wstring w = utf8ToWide(dev);
    WindowsCreateString(w.c_str(), (UINT32)w.size(), &h);
  }
  HRESULT hr = fac->SetPersistedDefaultAudioEndpoint(pid, eRender, role, h);
  if (h)
    WindowsDeleteString(h);
  return hr;
}

// 在当前音频会话里按 exe 名找一个活的 pid（崩溃后 App 换了 pid 时用）。
DWORD findPidByExe(const std::string& exe) {
  if (exe.empty())
    return 0;
  for (auto& s : listAudioSessions())
    if (s.name == exe)
      return s.pid;
  return 0;
}

} // namespace

struct AppRouter::Impl {
  bool roInited = false;
  IAudioPolicyConfigFactory* fac = nullptr;
  bool routed = false;
  DWORD pid = 0;
  std::string origConsole, origMultimedia; // 路由前的原值（UTF-8）
};

AppRouter::AppRouter() : impl_(new Impl()) {
  impl_->roInited = SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED));
  impl_->fac = activateFactory();
}

AppRouter::~AppRouter() {
  restore();
  if (impl_->fac)
    impl_->fac->Release();
  if (impl_->roInited)
    RoUninitialize();
  delete impl_;
}

bool AppRouter::available() const { return impl_->fac != nullptr; }
bool AppRouter::routed() const { return impl_->routed; }

bool AppRouter::route(uint32_t pid, const std::string& endpointId,
                      std::string& err) {
  if (!impl_->fac) {
    err = "未公开路由接口不可用（本 Windows 版本不支持），请手动设置";
    return false;
  }
  if (impl_->routed) {
    err = "已有路由在生效，请先 restore";
    return false;
  }
  // 1) 捕获原值（两 role）。
  impl_->origConsole = getPersisted(impl_->fac, pid, eConsole);
  impl_->origMultimedia = getPersisted(impl_->fac, pid, eMultimedia);

  // 2) 路由【前】先落 journal（崩溃也能据此还原）。
  Journal j;
  j.pid = pid;
  j.exe = exeNameOf(pid);
  j.origConsole = impl_->origConsole;
  j.origMultimedia = impl_->origMultimedia;
  writeJournal(j);

  // 3) 设两 role 到目标端点。
  std::string packed = wideToUtf8(packRender(endpointId).c_str());
  HRESULT hr = S_OK;
  for (ERole role : kRoles) {
    HRESULT r = setPersisted(impl_->fac, pid, role, packed);
    if (FAILED(r))
      hr = r;
  }
  if (FAILED(hr)) {
    // 回滚已设的，删 journal。
    for (ERole role : kRoles)
      setPersisted(impl_->fac, pid, role,
                   role == eConsole ? impl_->origConsole
                                    : impl_->origMultimedia);
    deleteJournal();
    char buf[64];
    snprintf(buf, sizeof buf, "0x%08lx", (unsigned long)hr);
    err = std::string("路由失败 hr=") + buf +
          "（进程无音频会话？）。请手动设置";
    return false;
  }
  impl_->routed = true;
  impl_->pid = pid;
  return true;
}

void AppRouter::restore() {
  if (!impl_->routed || !impl_->fac)
    return;
  for (ERole role : kRoles)
    setPersisted(impl_->fac, impl_->pid, role,
                 role == eConsole ? impl_->origConsole : impl_->origMultimedia);
  deleteJournal();
  impl_->routed = false;
}

bool AppRouter::clearAllRoutes(std::string& err) {
  if (!impl_->fac) {
    err = "未公开路由接口不可用";
    return false;
  }
  HRESULT hr = impl_->fac->ClearAllPersistedApplicationDefaultEndpoints();
  if (FAILED(hr)) {
    char buf[64];
    snprintf(buf, sizeof buf, "0x%08lx", (unsigned long)hr);
    err = std::string("ClearAll 失败 hr=") + buf;
    return false;
  }
  return true;
}

void AppRouter::recoverStaleRoutes() {
  Journal j = readJournal();
  if (!j.valid)
    return; // 上次正常退出，无残留
  bool ro = SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED));
  IAudioPolicyConfigFactory* fac = activateFactory();
  if (fac) {
    // 记录的 pid 可能已失效（App 重启换了 pid）→ 按 exe 名找当前活 pid。
    DWORD pid = j.pid;
    if (exeNameOf(pid).empty() || exeNameOf(pid) != j.exe) {
      DWORD alt = findPidByExe(j.exe);
      if (alt)
        pid = alt;
    }
    setPersisted(fac, pid, eConsole, j.origConsole);
    setPersisted(fac, pid, eMultimedia, j.origMultimedia);
    fac->Release();
  }
  deleteJournal();
  if (ro)
    RoUninitialize();
}

} // namespace approuter
