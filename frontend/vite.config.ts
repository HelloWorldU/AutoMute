import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { viteSingleFile } from 'vite-plugin-singlefile'

// 构建成【单个自包含 index.html】（CSS/JS 全内联）→ 经 scripts/embed-frontend.mjs
// 塞进 automute/gui_html.cpp 的 kIndexHtml，webview set_html 显示，C++ 侧不变。
export default defineConfig({
  plugins: [vue(), viteSingleFile()],
  build: {
    target: 'esnext',
    cssCodeSplit: false,
    assetsInlineLimit: 100000000,
    chunkSizeWarningLimit: 100000,
  },
})
