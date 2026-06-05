<script setup lang="ts">
// AutoMute GUI 前端（Vue 3 + Naive UI）。所有 window.xxx 是 C++ 绑定（webview.d.ts）。
// Uw：无边框 + 自绘标题栏（窗口控制走 winMinimize/winToggleMaximize/winClose/winDrag）。
import { computed, onMounted, onUnmounted, ref, nextTick } from 'vue'
import {
  NConfigProvider, NGlobalStyle, darkTheme, type GlobalThemeOverrides,
  NCard, NSelect, NButton, NInput, NProgress, NSwitch, NAlert,
  NText, NSpace, NEmpty, NTag, NInputGroup,
} from 'naive-ui'
import type { AppInfo, Status } from './webview'

const themeOverrides: GlobalThemeOverrides = {
  common: {
    primaryColor: '#5b8cff', primaryColorHover: '#74a0ff',
    primaryColorPressed: '#4a7bef', borderRadius: '8px',
    bodyColor: '#1a1b1e', cardColor: '#232428',
  },
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
          <span class="agg">
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
              <n-button quaternary @click="refreshApps">刷新</n-button>
            </div>
            <div class="row">
              <n-button :type="running ? 'error' : 'primary'" :loading="busy"
                        @click="toggleRun">{{ running ? '停止' : '开始' }}</n-button>
              <n-text depth="3" class="rmsg">{{ routeMsg }}</n-text>
            </div>
          </n-space>
        </n-card>

        <n-card title="圈人 · 边看边登记（说话时点）" size="small" class="card">
          <n-input-group>
            <n-input v-model:value="nameInput" placeholder="名字（可留空自动命名）"
                     @keyup.enter="capture" />
            <n-button type="primary" :disabled="!running" @click="capture">
              抓取当前说话人
            </n-button>
          </n-input-group>
          <n-text v-if="capMsg" depth="3" class="capmsg">{{ capMsg }}</n-text>
        </n-card>

        <n-card title="目标名单 · 开了开关才掐" size="small" class="card">
          <n-empty v-if="!targets.length"
                   :description="running ? '某人说话时点「抓取当前说话人」登记' : '还没登记目标'" />
          <div v-else class="tgts">
            <div v-for="(t, i) in targets" :key="i" class="tgt">
              <n-input v-if="editingIdx === i" v-model:value="editingName" size="tiny"
                       class="nm-edit" autofocus @blur="commitEdit"
                       @keyup.enter="commitEdit" @keyup.esc="editingIdx = -1" />
              <span v-else class="nm" title="点击改名" @click="startEdit(i, t.name)">{{ t.name }}</span>
              <n-progress class="bar" type="line" :percentage="simPct(t.sim)"
                          :status="t.muted ? 'error' : 'info'" :show-indicator="false"
                          :height="8" :border-radius="4" />
              <span class="sim">{{ t.sim.toFixed(2) }}</span>
              <n-tag :type="t.muted ? 'error' : 'default'" size="small" round>
                {{ t.muted ? '静音中' : '放行' }}
              </n-tag>
              <n-switch :value="t.muted" size="small"
                        @update:value="(v: boolean) => setMuted(i, v)" />
              <n-button quaternary circle size="tiny" class="del" title="删除"
                        @click="removeTarget(i)">✕</n-button>
            </div>
          </div>
        </n-card>
      </div>
    </div>
  </n-config-provider>
</template>

<style>
html, body, #app { height: 100%; }
body { margin: 0; background: #1a1b1e; color: #e3e3e6;
       font-family: 'Segoe UI', system-ui, sans-serif; }
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
.titlebar { height: 36px; flex: none; display: flex; align-items: center;
            justify-content: space-between; user-select: none;
            border-bottom: 1px solid #2c2d31; }
.tb-left { display: flex; align-items: baseline; gap: 10px; padding-left: 14px; }
.logo { font-size: 15px; font-weight: 700; letter-spacing: .3px; }
.agg { font-size: 12px; color: #9aa0a6; }
.tb-ctrls { display: flex; height: 100%; }
.wc { width: 44px; height: 100%; border: 0; background: transparent; color: #c8c9cd;
      font-family: 'Segoe Fluent Icons', 'Segoe MDL2 Assets'; font-size: 10px;
      cursor: pointer; display: flex; align-items: center; justify-content: center;
      transition: background .12s; }
.wc:hover { background: #2c2d31; }
.wc.close:hover { background: #c0392b; color: #fff; }

/* 内容区（可滚动） */
.body { flex: 1; overflow: auto; padding: 14px; display: flex;
        flex-direction: column; gap: 12px; min-width: 0; }
.banner { font-size: 13px; }
.card { min-width: 0; }
.row { display: flex; align-items: center; gap: 8px; }
.grow { flex: 1; min-width: 0; }
.rmsg, .capmsg { font-size: 13px; }
.capmsg { display: block; margin-top: 8px; }
.tgts { display: flex; flex-direction: column; }
.tgt { display: flex; align-items: center; gap: 10px; padding: 7px 0; }
.tgt + .tgt { border-top: 1px solid #2c2d31; }
.nm, .nm-edit { width: 84px; flex: none; }
.nm { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; cursor: pointer; }
.nm:hover { text-decoration: underline dotted #9aa0a6; }
.bar { flex: 1; min-width: 40px; }
.sim { width: 38px; text-align: right; font-size: 13px; color: #9aa0a6;
       font-variant-numeric: tabular-nums; }
.del { color: #9aa0a6; }
</style>
