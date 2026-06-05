<script setup lang="ts">
// U0：最小验证页——证明 Vue 3 + Naive UI 跑进 webview，且能调通 C++ 绑定。
// 真正的功能界面在 U1 复刻。
import { ref } from 'vue'
import {
  NConfigProvider,
  NButton,
  NList,
  NListItem,
  NText,
  darkTheme,
} from 'naive-ui'
import type { AppInfo } from './webview'

const apps = ref<AppInfo[]>([])
const msg = ref('点按钮调 C++ 的 listApps()，验证 JS↔C++ 桥接')
const loading = ref(false)

async function load() {
  loading.value = true
  try {
    apps.value = await window.listApps()
    msg.value = `✅ 桥接通：拿到 ${apps.value.length} 个在出声的进程`
  } catch (e) {
    msg.value = '❌ 调用失败：' + e
  } finally {
    loading.value = false
  }
}
</script>

<template>
  <n-config-provider :theme="darkTheme">
    <div class="wrap">
      <h1>AutoMute <span class="tag">Vue 3 + Naive UI · U0</span></h1>
      <n-text depth="3">{{ msg }}</n-text>
      <div style="margin: 16px 0">
        <n-button type="primary" :loading="loading" @click="load">
          调 listApps()
        </n-button>
      </div>
      <n-list bordered v-if="apps.length">
        <n-list-item v-for="a in apps" :key="a.pid">
          {{ a.name }} · PID {{ a.pid }}
          <n-text depth="3"> → {{ a.device }}</n-text>
        </n-list-item>
      </n-list>
    </div>
  </n-config-provider>
</template>

<style>
body {
  margin: 0;
  background: #1e1f22;
  color: #e3e3e6;
  font-family: 'Segoe UI', system-ui, sans-serif;
}
.wrap {
  padding: 24px;
}
h1 {
  font-size: 20px;
  margin: 0 0 12px;
}
.tag {
  font-size: 12px;
  color: #9aa0a6;
  font-weight: 400;
}
</style>
