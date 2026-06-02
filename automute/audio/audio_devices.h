#pragma once
//
// 输出(render)设备枚举：给用户列设备、按序号选设备。
// 这是"抓取源 ≠ 回放目标"产品化路线的地基（配虚拟声卡即可消除反馈）。
//
#include <string>
#include <vector>

struct AudioDeviceInfo {
  int index;
  std::string name;
};

// 列出所有“活动”的输出设备（含友好名）。内部自管 COM 初始化。
std::vector<AudioDeviceInfo> listRenderDevices();

// 打印设备列表（供 --list 用）。
void printRenderDevices();
