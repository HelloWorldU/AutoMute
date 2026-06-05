<script setup lang="ts">
// U1：用 Vue 3 + Naive UI 复刻完整功能界面（对齐原 vanilla 版的全部行为）。
// 所有 window.xxx 是 C++ 绑定（见 webview.d.ts），契约不变。
import { computed, onMounted, onUnmounted, ref, nextTick } from 'vue'
import {
  NConfigProvider, NGlobalStyle, darkTheme, type GlobalThemeOverrides,
  NCard, NSelect, NButton, NInput, NProgress, NSwitch, NAlert,
  NText, NSpace, NEmpty, NTag, NInputGroup,
} from 'naive-ui'
import type { AppInfo, Status } from './webview'

// —— 现代极简的主色/圆角（U2 再深化设计令牌）——
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
const starting = ref(false)

const running = computed(() => status.value?.running ?? false)
const targets = computed(() => status.value?.targets ?? [])
const routeMsg = computed(() => status.value?.routeMsg ?? '')

// 下拉选项：按 PID 去重（路由后 Windows 留过期会话残骸），优先"出声中"。
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
  if (!running.value) {
    if (!selectedPid.value) { banner.value = '请先选一个 App'; return }
    starting.value = true
    try { await window.start(selectedPid.value) } finally { starting.value = false }
  } else {
    await window.stop()
  }
  kickPoll()
}

async function capture() {
  if (!running.value) { capMsg.value = '先点「开始」再抓取'; return }
  const name = nameInput.value.trim()
  const r = await window.capture(name)
  capMsg.value = r.ok
    ? `已登记 #${r.idx}：点名字可改名、右侧开关掐他、✕ 删除`
    : '抓取失败：' + r.msg
  if (r.ok) nameInput.value = ''
}
const nameInput = ref('')

async function setMuted(idx: number, on: boolean) { await window.setMuted(idx, on) }
async function removeTarget(idx: number) { await window.removeTarget(idx); kickPoll() }

// —— 改名：点名字进编辑态，回车/失焦提交（编辑中不被轮询覆盖）——
const editingIdx = ref(-1)
const editingName = ref('')
async function startEdit(idx: number, name: string) {
  editingIdx.value = idx; editingName.value = name
  await nextTick()
}
async function commitEdit() {
  const idx = editingIdx.value
  if (idx < 0) return
  const v = editingName.value.trim()
  editingIdx.value = -1
  if (v) await window.renameTarget(idx, v)
}

// —— 自适应自调度轮询（运行 250ms / 闲置 2s；完成驱动闭环，防 CPU 雪崩）——
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
  poll()
}
onMounted(init)
onUnmounted(() => { if (timer) clearTimeout(timer) })

function simPct(s: number) { return Math.max(0, Math.min(1, s)) * 100 }
</script>

<template>
  <n-config-provider :theme="darkTheme" :theme-overrides="themeOverrides">
    <n-global-style />
    <div class="app">
      <header class="hd">
        <span class="logo">AutoMute</span>
        <n-text depth="3" class="agg">
          <template v-if="running">
            {{ status?.muted ? '🔇 静音中' : '🔊 放行' }} · 最高相似度
            {{ (status?.similarity ?? 0).toFixed(2) }}
          </template>
          <template v-else>未运行</template>
        </n-text>
      </header>

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
            <n-button :type="running ? 'error' : 'primary'" :loading="starting"
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
  </n-config-provider>
</template>

<style>
html, body, #app { height: 100%; }
body { margin: 0; background: #1a1b1e; color: #e3e3e6;
       font-family: 'Segoe UI', system-ui, sans-serif; }
.app { padding: 16px; display: flex; flex-direction: column; gap: 12px;
       min-width: 0; }
.hd { display: flex; align-items: baseline; gap: 12px; }
.logo { font-size: 19px; font-weight: 700; letter-spacing: .3px; }
.agg { font-size: 13px; }
.banner { font-size: 13px; }
.card { min-width: 0; }
.row { display: flex; align-items: center; gap: 8px; }
.grow { flex: 1; min-width: 0; }          /* min-width:0 → 防长选项撑破横向 */
.rmsg, .capmsg { font-size: 13px; }
.capmsg { display: block; margin-top: 8px; }
.tgts { display: flex; flex-direction: column; }
.tgt { display: flex; align-items: center; gap: 10px; padding: 7px 0; }
.tgt + .tgt { border-top: 1px solid #2c2d31; }
.nm, .nm-edit { width: 84px; flex: none; }
.nm { overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
      cursor: pointer; }
.nm:hover { text-decoration: underline dotted #9aa0a6; }
.bar { flex: 1; min-width: 40px; }
.sim { width: 38px; text-align: right; font-size: 13px; color: #9aa0a6;
       font-variant-numeric: tabular-nums; }
.del { color: #9aa0a6; }
</style>
