// AutoMute GUI 前端（M4.4）—— 纯 HTML/CSS/JS 内嵌为 C++ 字符串。
// 与 gui_main.cpp 的 bind 对接：window.listApps/start/stop/capture/setMuted/getStatus。
// v1 不上构建步骤/框架，图轻快；以后要换 Vue 等可平移。
const char* kIndexHtml = R"HTML(<!DOCTYPE html>
<html lang="zh"><head><meta charset="utf-8">
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { font-family: "Segoe UI", system-ui, sans-serif; margin: 0;
         background: #1e1f22; color: #e3e3e6; padding: 18px; }
  h1 { font-size: 20px; margin: 0 0 14px; }
  section { background: #2a2b2f; border-radius: 10px; padding: 14px;
            margin-bottom: 12px; }
  label.cap { display:block; font-size:12px; color:#9aa0a6; margin-bottom:8px;
              text-transform: uppercase; letter-spacing:.5px; }
  .row { display: flex; gap: 8px; align-items: center; margin-bottom: 8px; }
  .row:last-child { margin-bottom: 0; }
  select, input { flex: 1; background:#1e1f22; color:#e3e3e6; border:1px solid #3a3b40;
                  border-radius:6px; padding:8px; font-size:14px; }
  button { background:#3b6ef0; color:#fff; border:0; border-radius:6px;
           padding:8px 14px; font-size:14px; cursor:pointer; white-space:nowrap; }
  button:hover { background:#5681f3; }
  button.ghost { background:#3a3b40; }
  button.ghost:hover { background:#4a4b52; }
  button.danger { background:#c0392b; }
  .muted { color:#9aa0a6; font-size:13px; }
  #banner { background:#4a3a1a; border:1px solid #7a5a1a; color:#f0d28a;
            padding:10px 12px; border-radius:8px; margin-bottom:12px; font-size:13px; }
  .hidden { display:none; }
  .tgt { display:flex; align-items:center; gap:10px; padding:8px 0;
         border-top:1px solid #3a3b40; }
  .tgt:first-child { border-top:0; }
  .tgt .nm { width:78px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;
             cursor:pointer; }
  .tgt .nm:hover { text-decoration:underline dotted #9aa0a6; }
  .tgt .nm input { width:76px; background:#1e1f22; color:#e3e3e6;
                   border:1px solid #3b6ef0; border-radius:4px; padding:1px 4px; font-size:13px; }
  .tgt .x { background:transparent; color:#9aa0a6; padding:6px 8px; }
  .tgt .x:hover { background:#3a3b40; color:#e3e3e6; }
  .bar { flex:1; height:10px; background:#1e1f22; border-radius:5px; overflow:hidden; }
  .bar > i { display:block; height:100%; background:#3b6ef0; width:0%; transition:width .15s; }
  .pill { font-size:12px; padding:3px 8px; border-radius:12px; background:#3a3b40; }
  .pill.on { background:#c0392b; color:#fff; }
  #agg { font-size:14px; font-weight:600; }
</style></head>
<body>
  <h1>AutoMute <span id="agg" class="muted"></span></h1>
  <div id="banner" class="hidden"></div>

  <section>
    <label class="cap">目标 App</label>
    <div class="row">
      <select id="apps"></select>
      <button class="ghost" onclick="refreshApps()">刷新</button>
    </div>
    <div class="row">
      <button id="runBtn" onclick="toggleRun()">开始</button>
      <span id="routeMsg" class="muted"></span>
    </div>
  </section>

  <section>
    <label class="cap">圈人 · 边看边登记（说话时点）</label>
    <div class="row">
      <input id="name" placeholder="名字（可留空自动命名）">
      <button onclick="onCapture()">抓取当前说话人</button>
    </div>
    <div id="capMsg" class="muted"></div>
  </section>

  <section>
    <label class="cap">目标名单（开了开关才掐）</label>
    <div id="targets" class="muted">还没登记目标。开始后，在某人说话时点「抓取」。</div>
  </section>

<script>
let running = false;
let targetEls = []; // 缓存每行的元素引用，避免每次轮询重建 DOM（重建会吃掉点击）

function show(id, t){ document.getElementById(id).textContent = t; }

async function refreshApps(){
  const apps = await window.listApps();
  // 按 PID 去重：同一进程可能在多个设备留有会话（含路由后的过期残骸），
  // 优先保留"出声中"的那条，避免下拉里同一 App 出现两次。
  const byPid = new Map();
  for(const a of apps){
    const cur = byPid.get(a.pid);
    if(!cur || (a.active && !cur.active)) byPid.set(a.pid, a);
  }
  const uniq = [...byPid.values()];
  const sel = document.getElementById('apps');
  const prev = sel.value;
  sel.innerHTML = '';
  for(const a of uniq){
    const o = document.createElement('option');
    o.value = a.pid;
    o.textContent = `${a.name} · PID ${a.pid} ${a.active?'· 出声中':''} → ${a.device}`;
    sel.appendChild(o);
  }
  if(!uniq.length){
    const o = document.createElement('option');
    o.textContent = '（没有在出声的进程，先让目标 App 放声音再刷新）';
    sel.appendChild(o);
  }
  if(prev) sel.value = prev; // 刷新后尽量保持原选择
}

async function toggleRun(){
  const btn = document.getElementById('runBtn');
  if(!running){
    const pid = parseInt(document.getElementById('apps').value || '0');
    if(!pid){ show('routeMsg','请先选一个 App'); return; }
    btn.disabled = true;
    const r = await window.start(pid);
    btn.disabled = false;
    show('routeMsg', r.msg || '');
    if(r.ok){ running = true; btn.textContent = '停止'; btn.classList.add('danger'); }
  } else {
    await window.stop();
    running = false; btn.textContent = '开始'; btn.classList.remove('danger');
    show('routeMsg','');
  }
}

async function onCapture(){
  if(!running){ show('capMsg','先点「开始」再抓取'); return; }
  const name = document.getElementById('name').value.trim();
  const r = await window.capture(name);
  show('capMsg', r.ok ? `已登记 #${r.idx}：点它的名字可改名、✕ 可删、开关掐他` : ('抓取失败：'+r.msg));
  if(r.ok) document.getElementById('name').value='';
}

async function toggleMute(idx, on){ await window.setMuted(idx, on); }

// 点名字 → 原地变输入框，回车/失焦提交改名，Esc 取消。
function startRename(idx, nmEl){
  if(nmEl.dataset.editing === '1') return;
  nmEl.dataset.editing = '1';
  const old = nmEl.textContent;
  const inp = document.createElement('input');
  inp.value = old;
  nmEl.textContent = '';
  nmEl.appendChild(inp);
  inp.focus(); inp.select();
  let done = false;
  const finish = async (commit)=>{
    if(done) return; done = true;
    const v = inp.value.trim();
    nmEl.dataset.editing = '0';
    nmEl.textContent = (commit && v) ? v : old;
    if(commit && v && v !== old) await window.renameTarget(idx, v);
  };
  inp.onkeydown = (ev)=>{
    if(ev.key === 'Enter') finish(true);
    else if(ev.key === 'Escape') finish(false);
  };
  inp.onblur = ()=> finish(true);
}

function renderTargets(s){
  const box = document.getElementById('targets');
  if(!s.targets.length){
    targetEls = [];
    box.className='muted';
    box.textContent = running ? '开始了，在某人说话时点「抓取当前说话人」登记。'
                              : '还没登记目标。';
    return;
  }
  // 只有目标【数量变化】时才重建结构；否则原地更新，按钮元素稳定不被销毁。
  if(targetEls.length !== s.targets.length){
    box.className='';
    box.innerHTML='';
    targetEls = s.targets.map((t,i)=>{
      const row = document.createElement('div'); row.className='tgt';
      row.innerHTML = `<span class="nm" title="点击改名"></span>`
        + `<div class="bar"><i></i></div>`
        + `<span class="sim muted" style="width:42px;text-align:right"></span>`
        + `<span class="pill"></span>`;
      const nm = row.querySelector('.nm');
      nm.onclick = ()=> startRename(i, nm);
      const btn = document.createElement('button');
      // 点击时按按钮自己缓存的当前状态取反，不依赖闭包里的旧值。
      btn.onclick = ()=> toggleMute(i, btn.dataset.muted !== '1');
      const del = document.createElement('button');
      del.className='x'; del.textContent='✕'; del.title='删除目标';
      del.onclick = ()=> window.removeTarget(i);
      row.appendChild(btn); row.appendChild(del);
      box.appendChild(row);
      return { nm, bar: row.querySelector('.bar > i'),
               sim: row.querySelector('.sim'), pill: row.querySelector('.pill'), btn };
    });
  }
  // 每次轮询只改动态字段（条宽/数值/开关），不动元素本身。
  s.targets.forEach((t,i)=>{
    const e = targetEls[i];
    // 正在改名时别覆盖输入框（否则每 250ms 把你打的字冲掉）。
    if(e.nm.dataset.editing !== '1') e.nm.textContent = t.name;
    e.bar.style.width = (Math.max(0, Math.min(1, t.sim))*100) + '%';
    e.sim.textContent = t.sim.toFixed(2);
    e.pill.textContent = t.muted ? '静音中' : '放行';
    e.pill.className = 'pill' + (t.muted ? ' on' : '');
    e.btn.textContent = t.muted ? '关掉' : '掐他';
    e.btn.className = t.muted ? 'ghost' : '';
    e.btn.dataset.muted = t.muted ? '1' : '0';
  });
}

async function poll(){
  try{
    const s = await window.getStatus();
    running = s.running;
    const rb = document.getElementById('runBtn');
    rb.textContent = running ? '停止' : '开始';
    rb.classList.toggle('danger', running);
    document.getElementById('agg').textContent =
      running ? `${s.muted?'🔇 静音中':'🔊 放行'} · 最高相似度 ${s.similarity.toFixed(2)}` : '未运行';
    if(s.routeMsg) show('routeMsg', s.routeMsg);
    renderTargets(s);
  }catch(e){}
}

async function init(){
  if(window.__prepareErr){
    const b=document.getElementById('banner'); b.className='';
    b.textContent='模型加载失败：'+window.__prepareErr;
  }
  try{
    const c = await window.cableStatus();
    if(!c.installed || !c.available){
      const b=document.getElementById('banner'); b.className='';
      b.textContent = !c.installed
        ? '未检测到 VB-CABLE：装一下 vb-audio.com/Cable/ 才能自动隔离原声。'
        : '本机不支持自动路由，开始后需手动在「音量合成器」把该 App 设为 CABLE Input。';
    }
  }catch(e){}
  await refreshApps();
  setInterval(poll, 250);
}
init();
</script>
</body></html>)HTML";
