// C++（gui_main.cpp 的 w.bind）暴露到 window 上的绑定。返回值是 C++ 拼的 JSON，
// webview 已 JSON.parse → 这里就是解析后的对象。调用契约【与 C++ 保持一致，勿改名】。
export {}

export interface AppInfo {
  pid: number
  name: string
  active: boolean
  device: string
}

export interface TargetView {
  name: string
  sim: number
  muted: boolean
}

export interface Status {
  running: boolean
  muted: boolean
  similarity: number
  routed: boolean
  routeStatus: string // 码：'' | 'routed' | 'cable_missing' | 'route_failed' | 'unavailable'
  routeDetail: string // 失败时的原始诊断（未翻译）
  targets: TargetView[]
}

declare global {
  interface Window {
    listApps(): Promise<AppInfo[]>
    cableStatus(): Promise<{ available: boolean; installed: boolean }>
    // 后端只返回码 + 诊断 detail，前端本地化。
    startEngine(pid: number): Promise<{ ok: boolean; error?: string; detail?: string }>
    stopEngine(): Promise<{ ok: boolean }>
    capture(name: string): Promise<{ ok: boolean; idx?: number; error?: string; detail?: string }>
    setMuted(idx: number, on: boolean): Promise<{ ok: boolean }>
    renameTarget(idx: number, name: string): Promise<{ ok: boolean }>
    removeTarget(idx: number): Promise<{ ok: boolean }>
    clearTargets(): Promise<{ ok: boolean }>
    setLang(code: string): Promise<{ ok: boolean }>
    getStatus(): Promise<Status>
    // 窗口外壳（Uw：无边框 + 自绘标题栏）
    winMinimize?(): Promise<unknown>
    winToggleMaximize?(): Promise<unknown>
    winClose?(): Promise<unknown>
    winDrag?(): Promise<unknown>
    winResize?(ht: number): Promise<unknown>
    winIsMaximized?(): Promise<boolean>
    // 模型加载失败时 C++ 注入的全局
    __prepareErr?: string
  }
}
