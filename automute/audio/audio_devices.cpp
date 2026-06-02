// MinGW 下需在本 TU 定义 INITGUID，PKEY_Device_FriendlyName 等属性键才会真正实例化
//（否则只有 extern 声明，链接报 undefined reference）。必须在 Windows 头之前。
#define INITGUID

#include "automute/audio/audio_devices.h"
#include "automute/audio/com_ptr.h"

#include <windows.h>

#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>

#include <cstdio>

static std::string wideToUtf8(const wchar_t* w) {
  if (!w)
    return "";
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 0)
    return "";
  std::string s(n - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  return s;
}

std::vector<AudioDeviceInfo> listRenderDevices() {
  std::vector<AudioDeviceInfo> out;
  bool didInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

  ComPtr<IMMDeviceEnumerator> en;
  if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                 CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                 reinterpret_cast<void**>(&en)))) {
    ComPtr<IMMDeviceCollection> col;
    if (SUCCEEDED(
            en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col))) {
      UINT count = 0;
      col->GetCount(&count);
      for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> dev;
        if (FAILED(col->Item(i, &dev)))
          continue;
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
        out.push_back({(int)i, name});
      }
    }
  }

  if (didInit)
    CoUninitialize();
  return out;
}

void printRenderDevices() {
  printf("输出设备列表（--out 用序号；--in 用对应的 loopback 源序号）:\n");
  for (auto& d : listRenderDevices())
    printf("  [%d] %s\n", d.index, d.name.c_str());
}
