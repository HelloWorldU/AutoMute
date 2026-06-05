<script setup lang="ts">
// AutoMute GUI 前端（Vue 3 + Naive UI）。所有 window.xxx 是 C++ 绑定（webview.d.ts）。
// Uw：无边框 + 自绘标题栏（窗口控制走 winMinimize/winToggleMaximize/winClose/winDrag）。
import { computed, onMounted, onUnmounted, ref, nextTick } from 'vue'
import {
  NConfigProvider, NGlobalStyle, darkTheme, type GlobalThemeOverrides,
  NCard, NSelect, NButton, NInput, NSwitch, NAlert,
  NText, NSpace, NEmpty, NTag, NInputGroup,
} from 'naive-ui'
import type { AppInfo, Status } from './webview'

// —— 设计令牌（精致暗色 · 现代极简）——
const themeOverrides: GlobalThemeOverrides = {
  common: {
    bodyColor: '#161719',
    cardColor: '#1d1e22',
    popoverColor: '#202126',
    borderColor: 'rgba(255,255,255,0.08)',
    primaryColor: '#6b7bff', primaryColorHover: '#828fff',
    primaryColorPressed: '#5a69f0', primaryColorSuppl: '#6b7bff',
    infoColor: '#6b7bff', infoColorHover: '#828fff', infoColorPressed: '#5a69f0',
    errorColor: '#e5484d', errorColorHover: '#ec5f63', errorColorPressed: '#d83a40',
    textColorBase: '#e8e9ec', textColor1: '#e8e9ec',
    textColor2: '#b3b7be', textColor3: '#71767e',
    borderRadius: '9px', borderRadiusSmall: '6px',
    fontSize: '14px', fontWeightStrong: '600',
  },
  Card: {
    paddingSmall: '13px 15px 15px', titleFontSizeSmall: '12px',
    titleFontWeight: '600', titleTextColor: '#8b9099',
    borderColor: 'rgba(255,255,255,0.07)', color: '#1d1e22',
  },
  Button: { borderRadiusMedium: '8px', borderRadiusSmall: '7px', fontWeightStrong: '500' },
  Input: { borderRadius: '8px', color: '#141519', colorFocus: '#141519' },
  InternalSelection: { borderRadius: '8px', color: '#141519' },
  Progress: { railColor: 'rgba(255,255,255,0.06)' },
  Tag: { borderRadius: '6px' },
  Alert: { borderRadius: '9px' },
}

const apps = ref<AppInfo[]>([])
const selectedPid = ref<number | null>(null)
const status = ref<Status | null>(null)
const banner = ref('')
const capMsg = ref('')
const busy = ref(false)
const nameInput = ref('')

const running = computed(() => status.value?.running ?? false)
const targets = computed(() => status.value?.targets ?? [])
const routeMsg = computed(() => status.value?.routeMsg ?? '')

const appOptions = computed(() => {
  const byPid = new Map<number, AppInfo>()
  for (const a of apps.value) {
    const cur = byPid.get(a.pid)
    if (!cur || (a.active && !cur.active)) byPid.set(a.pid, a)
  }
  return [...byPid.values()].map((a) => ({
    label: `${a.name} · PID ${a.pid}${a.active ? ' · 出声中' : ''} → ${a.device}`,
    value: a.pid,
  }))
})

async function refreshApps() {
  apps.value = await window.listApps()
  if (selectedPid.value == null && appOptions.value.length)
    selectedPid.value = appOptions.value[0].value
}

async function toggleRun() {
  if (!running.value && !selectedPid.value) { banner.value = '请先选一个 App'; return }
  busy.value = true
  try {
    if (!running.value) await window.startEngine(selectedPid.value!)
    else await window.stopEngine()
  } finally {
    busy.value = false
    kickPoll()
  }
}

async function capture() {
  if (!running.value) { capMsg.value = '先点「开始」再抓取'; return }
  const r = await window.capture(nameInput.value.trim())
  capMsg.value = r.ok
    ? `已登记 #${r.idx}：点名字可改名、右侧开关掐他、✕ 删除`
    : '抓取失败：' + r.msg
  if (r.ok) nameInput.value = ''
}

async function setMuted(idx: number, on: boolean) { await window.setMuted(idx, on) }
async function removeTarget(idx: number) { await window.removeTarget(idx); kickPoll() }

const editingIdx = ref(-1)
const editingName = ref('')
async function startEdit(idx: number, name: string) {
  editingIdx.value = idx; editingName.value = name; await nextTick()
}
async function commitEdit() {
  const idx = editingIdx.value
  if (idx < 0) return
  const v = editingName.value.trim()
  editingIdx.value = -1
  if (v) await window.renameTarget(idx, v)
}

let timer: ReturnType<typeof setTimeout> | null = null
async function poll() {
  try { status.value = await window.getStatus() } catch { /* ignore */ }
  timer = setTimeout(poll, running.value ? 250 : 2000)
}
function kickPoll() { if (timer) clearTimeout(timer); poll() }

async function init() {
  if (window.__prepareErr) banner.value = '模型加载失败：' + window.__prepareErr
  try {
    const c = await window.cableStatus()
    if (!c.installed) banner.value = '未检测到 VB-CABLE：装一下 vb-audio.com/Cable/ 才能自动隔离原声。'
    else if (!c.available) banner.value = '本机不支持自动路由，开始后需手动在「音量合成器」把该 App 设为 CABLE Input。'
  } catch { /* ignore */ }
  await refreshApps()
  refreshMax()
  poll()
}
onMounted(init)
onUnmounted(() => { if (timer) clearTimeout(timer) })

function simPct(s: number) { return Math.max(0, Math.min(1, s)) * 100 }

// —— Uw：自绘标题栏窗口控制（Segoe Fluent/MDL2 图标 PUA 码点，用 fromCharCode 避免源码嵌字）——
const ICON = {
  min: String.fromCharCode(0xe921),
  max: String.fromCharCode(0xe922),
  restore: String.fromCharCode(0xe923),
  close: String.fromCharCode(0xe8bb),
}
const isMax = ref(false)
async function refreshMax() { isMax.value = (await window.winIsMaximized?.()) ?? false }
function winMin() { window.winMinimize?.() }
async function winMax() { await window.winToggleMaximize?.(); refreshMax() }
function winClose() { window.winClose?.() }
function onTitleDown(e: MouseEvent) {
  if ((e.target as HTMLElement).closest('.wc')) return // 点控制按钮不拖
  window.winDrag?.()
}
function onTitleDbl(e: MouseEvent) {
  if ((e.target as HTMLElement).closest('.wc')) return
  winMax()
}
// 边缘缩放手柄：WebView2 盖住整窗，靠 HTML 手柄发起缩放（HT 方向码）。
const HT = { L: 10, R: 11, T: 12, TL: 13, TR: 14, B: 15, BL: 16, BR: 17 }
function rz(ht: number) { if (!isMax.value) window.winResize?.(ht) }
</script>

<template>
  <n-config-provider :theme="darkTheme" :theme-overrides="themeOverrides">
    <n-global-style />
    <div class="shell">
      <!-- 边缘缩放手柄（WebView2 盖住整窗→父窗口边缘 hit-test 失效，用 HTML 手柄）-->
      <div class="rz rz-t" @mousedown="rz(HT.T)"></div>
      <div class="rz rz-b" @mousedown="rz(HT.B)"></div>
      <div class="rz rz-l" @mousedown="rz(HT.L)"></div>
      <div class="rz rz-r" @mousedown="rz(HT.R)"></div>
      <div class="rz rz-tl" @mousedown="rz(HT.TL)"></div>
      <div class="rz rz-tr" @mousedown="rz(HT.TR)"></div>
      <div class="rz rz-bl" @mousedown="rz(HT.BL)"></div>
      <div class="rz rz-br" @mousedown="rz(HT.BR)"></div>

      <!-- 自绘标题栏 -->
      <div class="titlebar" @mousedown="onTitleDown" @dblclick="onTitleDbl">
        <div class="tb-left">
          <span class="logo">AutoMute</span>
          <span class="agg" :class="{ pulsing: running && status?.muted }">
            <template v-if="running">
              {{ status?.muted ? '🔇 静音中' : '🔊 放行' }} ·
              {{ (status?.similarity ?? 0).toFixed(2) }}
            </template>
            <template v-else>未运行</template>
          </span>
        </div>
        <div class="tb-ctrls">
          <button class="wc" title="最小化" @click="winMin">{{ ICON.min }}</button>
          <button class="wc" title="最大化/还原" @click="winMax">
            {{ isMax ? ICON.restore : ICON.max }}
          </button>
          <button class="wc close" title="关闭" @click="winClose">{{ ICON.close }}</button>
        </div>
      </div>

      <!-- 内容区 -->
      <div class="body">
        <n-alert v-if="banner" type="warning" :bordered="false" class="banner" closable
                 @close="banner = ''">{{ banner }}</n-alert>

        <n-card title="目标 App" size="small" class="card">
          <n-space vertical :size="10">
            <div class="row">
              <n-select v-model:value="selectedPid" :options="appOptions"
                        placeholder="选一个在出声的进程" class="grow" />
              <n-button quaternary @click="refreshApps">
                <template #icon>
                  <svg viewBox="0 0 24 24" width="15" height="15" fill="none"
                       stroke="currentColor" stroke-width="2" stroke-linecap="round"
                       stroke-linejoin="round"><path d="M21 12a9 9 0 1 1-2.64-6.36" /><path d="M21 3v6h-6" /></svg>
                </template>
                刷新
              </n-button>
            </div>
            <div class="row">
              <n-button :type="running ? 'error' : 'primary'" :loading="busy"
                        @click="toggleRun">
                <template #icon>
                  <svg v-if="running" viewBox="0 0 24 24" width="14" height="14"><rect x="6" y="6" width="12" height="12" rx="2" fill="currentColor" /></svg>
                  <svg v-else viewBox="0 0 24 24" width="14" height="14"><path d="M7 5v14l12-7z" fill="currentColor" /></svg>
                </template>
                {{ running ? '停止' : '开始' }}
              </n-button>
              <n-text depth="3" class="rmsg">{{ routeMsg }}</n-text>
            </div>
          </n-space>
        </n-card>

        <n-card title="圈人 · 边看边登记（说话时点）" size="small" class="card">
          <n-input-group>
            <n-input v-model:value="nameInput" placeholder="名字（可留空自动命名）"
                     @keyup.enter="capture" />
            <n-button type="primary" :disabled="!running" @click="capture">
              <template #icon>
                <svg viewBox="0 0 24 24" width="15" height="15" fill="none"
                     stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="8" /><circle cx="12" cy="12" r="2.5" fill="currentColor" stroke="none" /></svg>
              </template>
              抓取当前说话人
            </n-button>
          </n-input-group>
          <n-text v-if="capMsg" depth="3" class="capmsg">{{ capMsg }}</n-text>
        </n-card>

        <n-card title="目标名单 · 开了开关才掐" size="small" class="card">
          <n-empty v-if="!targets.length"
                   :description="running ? '某人说话时点「抓取当前说话人」登记' : '还没登记目标'" />
          <transition-group v-else name="tgt" tag="div" class="tgts">
            <div v-for="(t, i) in targets" :key="i" class="tgt">
              <n-input v-if="editingIdx === i" v-model:value="editingName" size="tiny"
                       class="nm-edit" autofocus @blur="commitEdit"
                       @keyup.enter="commitEdit" @keyup.esc="editingIdx = -1" />
              <span v-else class="nm" title="点击改名" @click="startEdit(i, t.name)">{{ t.name }}</span>
              <!-- 自定义仪表：渐变填充 + 50% 阈值刻度 + 超阈发光 -->
              <div class="meter" title="相似度（竖线=0.5 触发阈值）">
                <div class="meter-fill" :class="{ over: t.sim >= 0.5 }"
                     :style="{ width: simPct(t.sim) + '%' }"></div>
                <span class="meter-thresh"></span>
              </div>
              <span class="sim">{{ t.sim.toFixed(2) }}</span>
              <n-tag :type="t.muted ? 'error' : 'default'" size="small" round>
                {{ t.muted ? '静音中' : '放行' }}
              </n-tag>
              <n-switch :value="t.muted" size="small"
                        @update:value="(v: boolean) => setMuted(i, v)" />
              <n-button quaternary circle size="tiny" class="del" title="删除"
                        @click="removeTarget(i)">✕</n-button>
            </div>
          </transition-group>
        </n-card>
      </div>
    </div>
  </n-config-provider>
</template>

<style>
html, body, #app { height: 100%; }
body { margin: 0; background: #161719; color: #e8e9ec;
       font-family: 'Segoe UI', system-ui, sans-serif;
       -webkit-font-smoothing: antialiased; }
*::-webkit-scrollbar { width: 9px; height: 9px; }
*::-webkit-scrollbar-thumb { background: rgba(255,255,255,0.1); border-radius: 5px;
       border: 2px solid transparent; background-clip: content-box; }
*::-webkit-scrollbar-thumb:hover { background: rgba(255,255,255,0.18); background-clip: content-box; }
.shell { height: 100vh; display: flex; flex-direction: column; overflow: hidden;
         position: relative; }

/* 边缘缩放手柄：贴窗口四边四角，透明，盖在最上层 */
.rz { position: fixed; z-index: 9999; }
.rz-t { top: 0; left: 6px; right: 6px; height: 5px; cursor: ns-resize; }
.rz-b { bottom: 0; left: 6px; right: 6px; height: 5px; cursor: ns-resize; }
.rz-l { left: 0; top: 6px; bottom: 6px; width: 5px; cursor: ew-resize; }
.rz-r { right: 0; top: 6px; bottom: 6px; width: 5px; cursor: ew-resize; }
.rz-tl { top: 0; left: 0; width: 8px; height: 8px; cursor: nwse-resize; }
.rz-tr { top: 0; right: 0; width: 8px; height: 8px; cursor: nesw-resize; }
.rz-bl { bottom: 0; left: 0; width: 8px; height: 8px; cursor: nesw-resize; }
.rz-br { bottom: 0; right: 0; width: 8px; height: 8px; cursor: nwse-resize; }

/* 自绘标题栏 */
.titlebar { height: 38px; flex: none; display: flex; align-items: center;
            justify-content: space-between; user-select: none;
            background: #131416; border-bottom: 1px solid rgba(255,255,255,0.06); }
.tb-left { display: flex; align-items: center; gap: 9px; padding-left: 14px; }
.logo { font-size: 14px; font-weight: 650; letter-spacing: .2px;
        display: flex; align-items: center; gap: 8px; }
.logo::before { content: ''; width: 8px; height: 8px; border-radius: 50%;
        background: linear-gradient(135deg, #6b7bff, #9b6bff);
        box-shadow: 0 0 8px rgba(107,123,255,.6); }
.agg { font-size: 12px; color: #8b9099; font-variant-numeric: tabular-nums; }
.tb-ctrls { display: flex; height: 100%; }
.wc { width: 46px; height: 100%; border: 0; background: transparent; color: #b3b7be;
      font-family: 'Segoe Fluent Icons', 'Segoe MDL2 Assets'; font-size: 10px;
      cursor: pointer; display: flex; align-items: center; justify-content: center;
      transition: background .12s, color .12s; }
.wc:hover { background: rgba(255,255,255,0.07); color: #e8e9ec; }
.wc.close:hover { background: #e5484d; color: #fff; }

/* 内容区（可滚动） */
.body { flex: 1; overflow: auto; padding: 14px; display: flex;
        flex-direction: column; gap: 11px; min-width: 0; }
.banner { font-size: 13px; }
.card { min-width: 0; }
.row { display: flex; align-items: center; gap: 8px; }
.grow { flex: 1; min-width: 0; }
.rmsg, .capmsg { font-size: 12.5px; color: #8b9099; }
.capmsg { display: block; margin-top: 9px; }
.tgts { display: flex; flex-direction: column; margin: -3px -6px; }
.tgt { display: flex; align-items: center; gap: 10px; padding: 8px 6px;
       border-radius: 7px; transition: background .12s; }
.tgt:hover { background: rgba(255,255,255,0.03); }
.nm, .nm-edit { width: 82px; flex: none; }
.nm { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; cursor: pointer;
      font-size: 13.5px; }
.nm:hover { color: #6b7bff; }
.sim { width: 38px; text-align: right; font-size: 12.5px; color: #8b9099;
       font-variant-numeric: tabular-nums; }
.del { color: #71767e; }
.del:hover { color: #e5484d; }

/* 自定义相似度仪表：渐变填充 + 阈值刻度 + 超阈发光 */
.meter { flex: 1; min-width: 40px; height: 8px; position: relative;
         background: rgba(255,255,255,0.06); border-radius: 5px; overflow: hidden; }
.meter-fill { height: 100%; border-radius: 5px;
              background: linear-gradient(90deg, #5a69f0, #8b9bff);
              transition: width .15s ease, background .2s, box-shadow .2s; }
.meter-fill.over { background: linear-gradient(90deg, #e5484d, #ff7a7e);
                   box-shadow: 0 0 10px rgba(229,72,77,.55); }
.meter-thresh { position: absolute; top: -1px; bottom: -1px; left: 50%;
                width: 2px; background: rgba(255,255,255,0.28); border-radius: 1px; }

/* 目标行进出动效 */
.tgt-enter-active, .tgt-leave-active { transition: all .22s ease; }
.tgt-enter-from, .tgt-leave-to { opacity: 0; transform: translateY(-6px); }

/* 静音中脉冲 */
.agg.pulsing { color: #ff8488; animation: pulse 1.3s ease-in-out infinite; }
@keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: .45; } }
</style>
