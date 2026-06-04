// MinGW 下需 INITGUID 才能实例化 PKEY_Device_FriendlyName（同 audio_sessions.cpp）。
#define INITGUID

#include "automute/audio/app_router.h"
#include "automute/audio/com_ptr.h"

#include <windows.h>

#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>

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

} // namespace approuter
