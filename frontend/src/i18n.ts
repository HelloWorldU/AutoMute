import { createI18n } from 'vue-i18n'

// 初始语言：C++ 注入的 __lang（持久化选择）优先，否则按系统语言猜。
function initialLocale(): 'zh' | 'en' {
  const saved = (window as unknown as { __lang?: string }).__lang
  if (saved === 'zh' || saved === 'en') return saved
  return navigator.language?.toLowerCase().startsWith('zh') ? 'zh' : 'en'
}

const messages = {
  zh: {
    agg: { muted: '🔇 静音中', pass: '🔊 放行', idle: '未运行' },
    win: { min: '最小化', max: '最大化 / 还原', close: '关闭', lang: '中 / EN' },
    card: {
      app: '目标 App',
      enroll: '圈人 · 边看边登记（说话时点）',
      list: '目标名单 · 开了开关才掐',
    },
    sel: { placeholder: '选一个在出声的进程', sounding: '出声中' },
    btn: {
      refresh: '刷新', start: '开始', stop: '停止', capture: '抓取当前说话人',
      cutAll: '全掐', passAll: '全放', clear: '清空',
      clearYes: '清空', clearNo: '算了',
    },
    enroll: {
      placeholder: '名字（可留空自动命名）',
      done: '已登记 #{idx}：点名字可改名、右侧开关掐他、✕ 删除',
      pickFirst: '请先选一个 App',
      needStart: '先点「开始」再抓取',
      targetWord: '目标',
    },
    list: {
      count: '{m}/{n} 掐',
      emptyRunning: '某人说话时点「抓取当前说话人」登记',
      emptyIdle: '还没登记目标',
      renameTip: '点击改名',
      meterTip: '相似度（竖线=0.5 触发阈值）',
      muted: '静音中', pass: '放行', delete: '删除',
      clearConfirm: '清空整个名单？已保存的声纹也会删除。',
    },
    route: {
      routed: '已自动路由到 VB-CABLE',
      cable_missing: '未装 VB-CABLE，请手动路由或安装',
      route_failed: '自动路由失败，请手动设置',
      unavailable: '本机不支持自动路由，请手动把该 App 设为 CABLE Input',
    },
    err: {
      model: '模型未加载', pid: '无效 PID', engine: '启动失败：{detail}',
      no_audio: '还没抓到声音，等目标出声再试', enroll: '抓取失败：{detail}',
      modelLoad: '模型加载失败：{detail}',
    },
    banner: {
      cable: '未检测到 VB-CABLE：装一下 vb-audio.com/Cable/ 才能自动隔离原声。',
      routeUnavailable: '本机不支持自动路由，开始后需手动在「音量合成器」把该 App 设为 CABLE Input。',
    },
  },
  en: {
    agg: { muted: '🔇 Muted', pass: '🔊 Passing', idle: 'Idle' },
    win: { min: 'Minimize', max: 'Maximize / Restore', close: 'Close', lang: 'EN / 中' },
    card: {
      app: 'Target app',
      enroll: 'Enroll · capture while watching (click as they speak)',
      list: 'Targets · switch on to cut',
    },
    sel: { placeholder: 'Pick a sounding process', sounding: 'sounding' },
    btn: {
      refresh: 'Refresh', start: 'Start', stop: 'Stop', capture: 'Capture speaker',
      cutAll: 'Cut all', passAll: 'Pass all', clear: 'Clear',
      clearYes: 'Clear', clearNo: 'Cancel',
    },
    enroll: {
      placeholder: 'Name (blank = auto)',
      done: 'Enrolled #{idx}: click the name to rename, switch to cut, ✕ to delete',
      pickFirst: 'Pick an app first',
      needStart: 'Click Start before capturing',
      targetWord: 'Target',
    },
    list: {
      count: '{m}/{n} cut',
      emptyRunning: 'Capture someone while they speak',
      emptyIdle: 'No targets yet',
      renameTip: 'Click to rename',
      meterTip: 'Similarity (line = 0.5 threshold)',
      muted: 'Muted', pass: 'Passing', delete: 'Delete',
      clearConfirm: 'Clear the whole list? Saved voiceprints are deleted too.',
    },
    route: {
      routed: 'Auto-routed to VB-CABLE',
      cable_missing: 'VB-CABLE not installed — route manually or install it',
      route_failed: 'Auto-route failed — set it manually',
      unavailable: 'Auto-route unsupported here — set this app to CABLE Input manually',
    },
    err: {
      model: 'Model not loaded', pid: 'Invalid PID', engine: 'Start failed: {detail}',
      no_audio: 'No audio yet — wait until they speak', enroll: 'Capture failed: {detail}',
      modelLoad: 'Model failed to load: {detail}',
    },
    banner: {
      cable: 'VB-CABLE not detected: install it from vb-audio.com/Cable/ to isolate the original sound.',
      routeUnavailable: 'Auto-route unsupported here; after Start, set this app to "CABLE Input" in Windows volume mixer.',
    },
  },
}

export const i18n = createI18n({
  legacy: false,
  locale: initialLocale(),
  fallbackLocale: 'en',
  messages,
})
