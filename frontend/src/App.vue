<script setup lang="ts">
// AutoMute GUI 前端（Vue 3 + Naive UI + vue-i18n）。window.xxx 是 C++ 绑定（webview.d.ts）。
// 文案全走 i18n；后端只返回"码"，前端本地化。Uw：无边框 + 自绘标题栏。
import { computed, onMounted, onUnmounted, ref, nextTick } from 'vue'
import { useI18n } from 'vue-i18n'
import {
  NConfigProvider, NGlobalStyle, darkTheme, type GlobalThemeOverrides,
  zhCN, enUS, dateZhCN, dateEnUS,
  NCard, NSelect, NButton, NInput, NSwitch, NAlert,
  NText, NSpace, NEmpty, NTag, NInputGroup, NPopconfirm,
} from 'naive-ui'
import type { AppInfo, Status } from './webview'

const { t, locale } = useI18n()
const naiveLocale = computed(() => (locale.value === 'zh' ? zhCN : enUS))
const naiveDate = computed(() => (locale.value === 'zh' ? dateZhCN : dateEnUS))
function toggleLang() {
  locale.value = locale.value === 'zh' ? 'en' : 'zh'
  window.setLang?.(locale.value) // 持久化选择
}

const themeOverrides: GlobalThemeOverrides = {
  common: {
    bodyColor: '#161719', cardColor: '#1d1e22', popoverColor: '#202126',
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

// 持久态消息存"键+参数"，切语言时自动重译。
type Msg = { key: string; params?: Record<string, unknown> } | null
const apps = ref<AppInfo[]>([])
const selectedPid = ref<number | null>(null)
const status = ref<Status | null>(null)
const banner = ref<Msg>(null)
const capMsg = ref<Msg>(null)
const busy = ref(false)
const nameInput = ref('')

const running = computed(() => status.value?.running ?? false)
const targets = computed(() => status.value?.targets ?? [])
const bannerText = computed(() => (banner.value ? t(banner.value.key, banner.value.params ?? {}) : ''))
const capText = computed(() => (capMsg.value ? t(capMsg.value.key, capMsg.value.params ?? {}) : ''))
const routeLine = computed(() => {
  const s = status.value?.routeStatus
  if (!s) return ''
  const d = status.value?.routeDetail
  return t('route.' + s) + (d ? ` (${d})` : '')
})

const appOptions = computed(() => {
  const byPid = new Map<number, AppInfo>()
  for (const a of apps.value) {
    const cur = byPid.get(a.pid)
    if (!cur || (a.active && !cur.active)) byPid.set(a.pid, a)
  }
  return [...byPid.values()].map((a) => ({
    label: `${a.name} · PID ${a.pid}${a.active ? ' · ' + t('sel.sounding') : ''} → ${a.device}`,
    value: a.pid,
  }))
})

async function refreshApps() {
  apps.value = await window.listApps()
  if (selectedPid.value == null && appOptions.value.length)
    selectedPid.value = appOptions.value[0].value
}

async function toggleRun() {
  if (!running.value && !selectedPid.value) { banner.value = { key: 'enroll.pickFirst' }; return }
  busy.value = true
  try {
    if (!running.value) {
      const r = await window.startEngine(selectedPid.value!)
      if (!r.ok) banner.value = { key: 'err.' + (r.error ?? 'engine'), params: { detail: r.detail ?? '' } }
    } else {
      await window.stopEngine()
    }
  } finally {
    busy.value = false
    kickPoll()
  }
}

async function capture() {
  if (!running.value) { capMsg.value = { key: 'enroll.needStart' }; return }
  const name = nameInput.value.trim() || `${t('enroll.targetWord')} ${targets.value.length + 1}`
  const r = await window.capture(name)
  if (r.ok) {
    capMsg.value = { key: 'enroll.done', params: { idx: r.idx } }
    nameInput.value = ''
  } else {
    capMsg.value = r.error === 'no_audio'
      ? { key: 'err.no_audio' }
      : { key: 'err.enroll', params: { detail: r.detail ?? '' } }
  }
}

async function setMuted(idx: number, on: boolean) { await window.setMuted(idx, on) }
async function removeTarget(idx: number) { await window.removeTarget(idx); kickPoll() }

const mutedCount = computed(() => targets.value.filter((t) => t.muted).length)
async function muteAll(on: boolean) {
  for (let i = 0; i < targets.value.length; ++i) await window.setMuted(i, on)
  kickPoll()
}
async function clearAll() { await window.clearTargets?.(); kickPoll() }

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
  if (window.__prepareErr) banner.value = { key: 'err.modelLoad', params: { detail: window.__prepareErr } }
  try {
    const c = await window.cableStatus()
    if (!c.installed) banner.value = { key: 'banner.cable' }
    else if (!c.available) banner.value = { key: 'banner.routeUnavailable' }
  } catch { /* ignore */ }
  await refreshApps()
  refreshMax()
  poll()
}
onMounted(init)
onUnmounted(() => { if (timer) clearTimeout(timer) })

function simPct(s: number) { return Math.max(0, Math.min(1, s)) * 100 }

// Uw：自绘标题栏窗口控制（Segoe Fluent/MDL2 图标 PUA 码点）。
const ICON = {
  min: String.fromCharCode(0xe921), max: String.fromCharCode(0xe922),
  restore: String.fromCharCode(0xe923), close: String.fromCharCode(0xe8bb),
}
const isMax = ref(false)
async function refreshMax() { isMax.value = (await window.winIsMaximized?.()) ?? false }
function winMin() { window.winMinimize?.() }
async function winMax() { await window.winToggleMaximize?.(); refreshMax() }
function winClose() { window.winClose?.() }
function onTitleDown(e: MouseEvent) {
  if ((e.target as HTMLElement).closest('.wc')) return
  window.winDrag?.()
}
function onTitleDbl(e: MouseEvent) {
  if ((e.target as HTMLElement).closest('.wc')) return
  winMax()
}
const HT = { L: 10, R: 11, T: 12, TL: 13, TR: 14, B: 15, BL: 16, BR: 17 }
function rz(ht: number) { if (!isMax.value) window.winResize?.(ht) }
</script>

<template>
  <n-config-provider :theme="darkTheme" :theme-overrides="themeOverrides"
                     :locale="naiveLocale" :date-locale="naiveDate">
    <n-global-style />
    <div class="shell">
      <div class="rz rz-t" @mousedown="rz(HT.T)"></div>
      <div class="rz rz-b" @mousedown="rz(HT.B)"></div>
      <div class="rz rz-l" @mousedown="rz(HT.L)"></div>
      <div class="rz rz-r" @mousedown="rz(HT.R)"></div>
      <div class="rz rz-tl" @mousedown="rz(HT.TL)"></div>
      <div class="rz rz-tr" @mousedown="rz(HT.TR)"></div>
      <div class="rz rz-bl" @mousedown="rz(HT.BL)"></div>
      <div class="rz rz-br" @mousedown="rz(HT.BR)"></div>

      <div class="titlebar" @mousedown="onTitleDown" @dblclick="onTitleDbl">
        <div class="tb-left">
          <span class="logo">AutoMute</span>
          <span class="agg" :class="{ pulsing: running && status?.muted }">
            <template v-if="running">
              {{ status?.muted ? t('agg.muted') : t('agg.pass') }} ·
              {{ (status?.similarity ?? 0).toFixed(2) }}
            </template>
            <template v-else>{{ t('agg.idle') }}</template>
          </span>
        </div>
        <div class="tb-ctrls">
          <button class="wc langbtn" :title="t('win.lang')" @click="toggleLang">
            {{ locale === 'zh' ? '中/EN' : 'EN/中' }}
          </button>
          <button class="wc" :title="t('win.min')" @click="winMin">{{ ICON.min }}</button>
          <button class="wc" :title="t('win.max')" @click="winMax">
            {{ isMax ? ICON.restore : ICON.max }}
          </button>
          <button class="wc close" :title="t('win.close')" @click="winClose">{{ ICON.close }}</button>
        </div>
      </div>

      <div class="body">
        <n-alert v-if="banner" type="warning" :bordered="false" class="banner" closable
                 @close="banner = null">{{ bannerText }}</n-alert>

        <n-card :title="t('card.app')" size="small" class="card">
          <n-space vertical :size="10">
            <div class="row">
              <n-select v-model:value="selectedPid" :options="appOptions"
                        :placeholder="t('sel.placeholder')" class="grow" />
              <n-button quaternary @click="refreshApps">
                <template #icon>
                  <svg viewBox="0 0 24 24" width="15" height="15" fill="none"
                       stroke="currentColor" stroke-width="2" stroke-linecap="round"
                       stroke-linejoin="round"><path d="M21 12a9 9 0 1 1-2.64-6.36" /><path d="M21 3v6h-6" /></svg>
                </template>
                {{ t('btn.refresh') }}
              </n-button>
            </div>
            <div class="row">
              <n-button :type="running ? 'error' : 'primary'" :loading="busy" @click="toggleRun">
                <template #icon>
                  <svg v-if="running" viewBox="0 0 24 24" width="14" height="14"><rect x="6" y="6" width="12" height="12" rx="2" fill="currentColor" /></svg>
                  <svg v-else viewBox="0 0 24 24" width="14" height="14"><path d="M7 5v14l12-7z" fill="currentColor" /></svg>
                </template>
                {{ running ? t('btn.stop') : t('btn.start') }}
              </n-button>
              <n-text depth="3" class="rmsg">{{ routeLine }}</n-text>
            </div>
          </n-space>
        </n-card>

        <n-card :title="t('card.enroll')" size="small" class="card">
          <n-input-group>
            <n-input v-model:value="nameInput" :placeholder="t('enroll.placeholder')"
                     @keyup.enter="capture" />
            <n-button type="primary" :disabled="!running" @click="capture">
              <template #icon>
                <svg viewBox="0 0 24 24" width="15" height="15" fill="none"
                     stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="8" /><circle cx="12" cy="12" r="2.5" fill="currentColor" stroke="none" /></svg>
              </template>
              {{ t('btn.capture') }}
            </n-button>
          </n-input-group>
          <n-text v-if="capMsg" depth="3" class="capmsg">{{ capText }}</n-text>
        </n-card>

        <n-card :title="t('card.list')" size="small" class="card">
          <template #header-extra>
            <div class="bulk" v-if="targets.length">
              <span class="cnt">{{ t('list.count', { m: mutedCount, n: targets.length }) }}</span>
              <n-button text size="tiny" type="error" @click="muteAll(true)">{{ t('btn.cutAll') }}</n-button>
              <n-button text size="tiny" @click="muteAll(false)">{{ t('btn.passAll') }}</n-button>
              <n-popconfirm @positive-click="clearAll" :positive-text="t('btn.clearYes')" :negative-text="t('btn.clearNo')">
                <template #trigger>
                  <n-button text size="tiny" class="cnt">{{ t('btn.clear') }}</n-button>
                </template>
                {{ t('list.clearConfirm') }}
              </n-popconfirm>
            </div>
          </template>
          <n-empty v-if="!targets.length"
                   :description="running ? t('list.emptyRunning') : t('list.emptyIdle')" />
          <transition-group v-else name="tgt" tag="div" class="tgts">
            <div v-for="(tt, i) in targets" :key="i" class="tgt"
                 :class="{ speaking: tt.sim >= 0.5, cut: tt.sim >= 0.5 && tt.muted }">
              <n-input v-if="editingIdx === i" v-model:value="editingName" size="tiny"
                       class="nm-edit" autofocus @blur="commitEdit"
                       @keyup.enter="commitEdit" @keyup.esc="editingIdx = -1" />
              <span v-else class="nm" :title="t('list.renameTip')" @click="startEdit(i, tt.name)">{{ tt.name }}</span>
              <div class="meter" :title="t('list.meterTip')">
                <div class="meter-fill" :class="{ over: tt.sim >= 0.5 }"
                     :style="{ width: simPct(tt.sim) + '%' }"></div>
                <span class="meter-thresh"></span>
              </div>
              <span class="sim">{{ tt.sim.toFixed(2) }}</span>
              <n-tag :type="tt.muted ? 'error' : 'default'" size="small" round>
                {{ tt.muted ? t('list.muted') : t('list.pass') }}
              </n-tag>
              <n-switch :value="tt.muted" size="small"
                        @update:value="(v: boolean) => setMuted(i, v)" />
              <n-button quaternary circle size="tiny" class="del" :title="t('list.delete')"
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

.rz { position: fixed; z-index: 9999; }
.rz-t { top: 0; left: 6px; right: 6px; height: 5px; cursor: ns-resize; }
.rz-b { bottom: 0; left: 6px; right: 6px; height: 5px; cursor: ns-resize; }
.rz-l { left: 0; top: 6px; bottom: 6px; width: 5px; cursor: ew-resize; }
.rz-r { right: 0; top: 6px; bottom: 6px; width: 5px; cursor: ew-resize; }
.rz-tl { top: 0; left: 0; width: 8px; height: 8px; cursor: nwse-resize; }
.rz-tr { top: 0; right: 0; width: 8px; height: 8px; cursor: nesw-resize; }
.rz-bl { bottom: 0; left: 0; width: 8px; height: 8px; cursor: nesw-resize; }
.rz-br { bottom: 0; right: 0; width: 8px; height: 8px; cursor: nwse-resize; }

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
.wc.langbtn { width: auto; padding: 0 11px; font-family: inherit; font-size: 11px;
      font-weight: 600; letter-spacing: .3px; }

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
       border-radius: 7px; position: relative; transition: background .12s; }
.tgt:hover { background: rgba(255,255,255,0.03); }
.tgt.speaking { background: rgba(107,123,255,0.09); }
.tgt.speaking.cut { background: rgba(229,72,77,0.10); }
.tgt.speaking::before { content: ''; position: absolute; left: 0; top: 6px; bottom: 6px;
       width: 3px; border-radius: 2px; background: #6b7bff; }
.tgt.speaking.cut::before { background: #e5484d; }
.bulk { display: flex; align-items: center; gap: 6px; }
.cnt { font-size: 12px; color: #8b9099; font-variant-numeric: tabular-nums; }
.nm, .nm-edit { width: 82px; flex: none; }
.nm { overflow: hidden; text-overflow: ellipsis; white-space: nowrap; cursor: pointer;
      font-size: 13.5px; }
.nm:hover { color: #6b7bff; }
.sim { width: 38px; text-align: right; font-size: 12.5px; color: #8b9099;
       font-variant-numeric: tabular-nums; }
.del { color: #71767e; }
.del:hover { color: #e5484d; }

.meter { flex: 1; min-width: 40px; height: 8px; position: relative;
         background: rgba(255,255,255,0.06); border-radius: 5px; overflow: hidden; }
.meter-fill { height: 100%; border-radius: 5px;
              background: linear-gradient(90deg, #5a69f0, #8b9bff);
              transition: width .15s ease, background .2s, box-shadow .2s; }
.meter-fill.over { background: linear-gradient(90deg, #e5484d, #ff7a7e);
                   box-shadow: 0 0 10px rgba(229,72,77,.55); }
.meter-thresh { position: absolute; top: -1px; bottom: -1px; left: 50%;
                width: 2px; background: rgba(255,255,255,0.28); border-radius: 1px; }

.tgt-enter-active, .tgt-leave-active { transition: all .22s ease; }
.tgt-enter-from, .tgt-leave-to { opacity: 0; transform: translateY(-6px); }

.agg.pulsing { color: #ff8488; animation: pulse 1.3s ease-in-out infinite; }
@keyframes pulse { 0%,100% { opacity: 1; } 50% { opacity: .45; } }
</style>
