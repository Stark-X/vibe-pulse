import { getCurrentWindow, LogicalSize } from '@tauri-apps/api/window';
import { invoke } from '@tauri-apps/api/core';
import { register as registerShortcut } from '@tauri-apps/plugin-global-shortcut';
import claudeCodeIcon from './assets/claude-code.png';
import codexIcon from './assets/codex.png';

const AGENT_ICON = {
  'Claude Code': { img: claudeCodeIcon },
  'Codex':       { img: codexIcon },
  'Gemini CLI':  { color: 'var(--violet)', glyph: 'G' },
  'Cursor':      { color: 'var(--green)',  glyph: '>' },
  'OpenCode':    { color: 'var(--coral)',  glyph: 'O' },
  'Droid':       { color: 'var(--sky)',    glyph: 'D' },
  'Kiro':        { color: 'var(--green)',  glyph: 'K' },
  'Qoder':       { color: 'var(--violet)', glyph: 'Q' },
  'Copilot':     { color: 'var(--peach)',  glyph: '+' },
  'Kimi Code':   { color: 'var(--violet)', glyph: 'M' },
  'CodeBuddy':   { color: 'var(--coral)',  glyph: 'B' },
};

// ============ App state ============
const st = {
  agents: [],       // AgentInfo[] from Tauri backend
  lastHash: '',
  theme: 'midnight',
  shape: 'round',
  settingsOpen: false,
  clock: '',
};

// ============ DOM helpers ============
function h(tag, attrs, ...children) {
  const el = document.createElement(tag);
  if (attrs) {
    for (const [k, v] of Object.entries(attrs)) {
      if (k === 'className') el.className = v;
      else if (k.startsWith('on')) el.addEventListener(k.slice(2).toLowerCase(), v);
      else if (k === 'style' && typeof v === 'object') Object.assign(el.style, v);
      else if (v === true) el.setAttribute(k, '');
      else if (v !== false && v != null) el.setAttribute(k, v);
    }
  }
  for (const child of children.flat()) {
    if (child == null) continue;
    el.append(typeof child === 'string' ? document.createTextNode(child) : child);
  }
  return el;
}

function buildIcon(tool) {
  const c = AGENT_ICON[tool];
  if (c?.img) {
    return h('img', { className: 'agent-icon-img', src: c.img, alt: tool });
  }
  const g = c || { color: 'var(--text-2)', glyph: '·' };
  return h('div', { className: 'agent-icon', style: {
    background: `color-mix(in oklch, ${g.color} 18%, var(--bg-3))`,
    border: `1px solid color-mix(in oklch, ${g.color} 45%, var(--border))`,
    color: g.color,
  } }, g.glyph);
}

// ============ Render ============
function render() {
  const root = document.getElementById('pulse-root');
  root.innerHTML = '';

  const agents = st.agents;
  const isEmpty = agents.length === 0;
  const running = agents.filter(a => a.status === 'running');

  const heartCls = running.length > 0 ? 'busy' : '';
  const headline = isEmpty
    ? 'No active agents'
    : running.length === 1
      ? running[0].name
      : `${agents.length} agents`;
  const sub = isEmpty ? 'Pulse is listening…' : `${running.length} running · ${st.clock}`;
  const metaChip = isEmpty
    ? null
    : h('span', { className: 'chip' }, `${agents.length} live`);

  const barWidth = isEmpty ? 280 : Math.min(420, 280 + agents.length * 20);

  const pulse = h('div', {
    className: 'pulse',
    'data-shape': st.shape === 'pill' ? 'pill' : st.shape === 'sharp' ? 'sharp' : 'default',
    style: { width: barWidth + 'px' },
  });

  const head = h('div', { className: 'pulse-head' },
    h('div', { className: `heart ${heartCls}` }),
    h('div', { style: { display: 'flex', flexDirection: 'column', minWidth: 0, gap: 1 } },
      h('div', { className: 'head-title' }, headline),
      h('div', { className: 'head-sub mono' }, sub),
    ),
    h('div', { className: 'head-meta' }, metaChip),
  );

  pulse.append(head);

  if (!isEmpty) {
    const body = h('div', { className: 'pulse-body' });
    body.append(...buildAgentList(agents));
    pulse.append(body);
  }

  root.append(pulse);
  resizeWindow();
}

function buildAgentList(agents) {
  const groups = [
    { label: 'Running', items: agents.filter(a => a.status === 'running') },
    { label: 'Exited',  items: agents.filter(a => a.status === 'exited') },
  ];
  const els = [];

  for (const g of groups) {
    if (!g.items.length) continue;
    els.push(h('div', { className: 'section-label' }, `${g.label} · ${g.items.length}`));
    els.push(h('div', { className: 'agent-list', style: { marginBottom: 8 } },
      ...g.items.map(a => buildAgentRow(a)),
    ));
  }

  els.push(h('div', { className: 'foot' },
    h('span', null, 'click row to jump'),
    h('span', { className: 'kbds' }, h('span', null, h('kbd', null, 'Super+P'))),
  ));
  return els;
}

function buildAgentRow(agent) {
  const indicatorCls = agent.status === 'running' ? 'working' : 'done';
  const canJump = !!agent.window_address;
  const title = agent.session_name ? `${agent.name} · ${agent.session_name}` : agent.name;
  const whereEl = agent.current_step
    ? h('div', { className: 'where', title: agent.current_step }, agent.current_step)
    : null;

  return h('button', {
    className: 'agent-row',
    title: canJump ? `Jump to ${agent.tool_type}` : 'No terminal window found',
    onClick: () => jumpToAgent(agent),
  },
    h('span', { className: `indicator ${indicatorCls}` }),
    h('div', { style: { minWidth: 0 } },
      h('div', { className: 'name' }, title),
      whereEl,
    ),
    buildIcon(agent.tool_type),
    canJump ? null : h('span', { className: 'age', style: { color: 'var(--text-3)' } }, '—'),
  );
}

// ============ Jump ============
async function jumpToAgent(agent) {
  if (!agent.window_address) {
    toast('No terminal window found for this agent');
    return;
  }
  try {
    await invoke('focus_window', { address: agent.window_address });
  } catch (e) {
    toast(`Jump failed: ${e}`);
  }
}

// ============ Polling ============
async function startPolling() {
  const poll = async () => {
    try {
      const agents = await invoke('get_agents');
      const hash = JSON.stringify(agents);
      if (hash !== st.lastHash) {
        st.lastHash = hash;
        st.agents = agents;
        render();
      }
    } catch (_) {}
  };

  await poll();
  setInterval(poll, 2000);
}

// ============ Global shortcut ============
async function registerShortcuts() {
  try {
    const win = getCurrentWindow();
    await registerShortcut('Super+P', async () => {
      const visible = await win.isVisible();
      if (visible) {
        await win.hide();
      } else {
        await win.show();
        await win.setFocus();
      }
    });
  } catch (_) {}
}

// ============ Toast ============
function toast(msg) {
  const root = document.getElementById('toast-root');
  root.innerHTML = '';
  root.append(h('div', { className: 'toast' }, msg));
  setTimeout(() => { root.innerHTML = ''; }, 2400);
}

// ============ Window resize ============
async function resizeWindow() {
  try {
    await new Promise(r => requestAnimationFrame(r));
    const pulse = document.querySelector('.pulse');
    if (!pulse) return;
    const bodyPad = 8; // matches body { padding: 8px }
    // Width from card's inline style (not body.scrollWidth which equals viewport width).
    // Height from body.scrollHeight (body is height:auto so it's accurate).
    const w  = Math.ceil(pulse.getBoundingClientRect().width) + bodyPad * 2;
    const ht = document.body.scrollHeight;
    const win = getCurrentWindow();
    const size = new LogicalSize(w, ht);
    // Set min+max+size so Hyprland (Wayland) is forced to accept the exact size.
    await win.setMinSize(size);
    await win.setMaxSize(size);
    await win.setSize(size);
  } catch (_) {}
}

// ============ Settings panel ============
function buildSettings() {
  const root = document.getElementById('settings-root');
  if (!root) return;
  if (!st.settingsOpen) { root.innerHTML = ''; return; }

  root.innerHTML = '';
  const panel = h('div', { className: 'settings-panel' });

  panel.append(h('div', { className: 'settings-hd' },
    h('b', null, 'Pulse · Settings'),
    h('button', { className: 'settings-x', onClick: () => { st.settingsOpen = false; buildSettings(); } }, '✕'),
  ));

  const body = h('div', { className: 'settings-body' });

  body.append(h('div', { className: 'settings-sect' }, 'Theme'));
  body.append(buildSeg(['midnight', 'aurora', 'carbon'], ['Midnight', 'Aurora', 'Carbon'], st.theme, (v) => {
    st.theme = v;
    document.documentElement.setAttribute('data-theme', v);
  }));

  body.append(h('div', { className: 'settings-sect' }, 'Form factor'));
  body.append(h('div', { className: 'settings-lbl' }, 'Shape'));
  body.append(buildSeg(['round', 'sharp', 'pill'], ['Round', 'Sharp', 'Pill'], st.shape, (v) => {
    st.shape = v;
    render();
  }));

  body.append(h('div', { className: 'settings-sect' }, 'About'));
  body.append(h('div', { className: 'settings-about' },
    'Floating overlay for AI coding agents. Ubuntu 24.04 + Hyprland. Super+P to toggle.',
  ));

  panel.append(body);
  root.append(panel);
}

function buildSeg(values, labels, current, onChange) {
  const seg = h('div', { className: 'settings-seg' });
  values.forEach((v, i) => {
    const btn = h('button', { className: v === current ? 'on' : '' }, labels[i]);
    btn.addEventListener('click', () => {
      onChange(v);
      buildSettings();
      render();
    });
    seg.append(btn);
  });
  return seg;
}

// ============ Clock ============
function updateClock() {
  const d = new Date();
  const hh = d.getHours().toString().padStart(2, '0');
  const mm = d.getMinutes().toString().padStart(2, '0');
  const d3 = d.toLocaleDateString('en', { weekday: 'short' });
  const day = d3[0].toUpperCase() + d3.slice(1).toLowerCase() + '.';
  st.clock = `${hh}:${mm} · ${day}`;
  render();
}

// ============ Init ============
async function init() {
  const settingsRoot = document.createElement('div');
  settingsRoot.id = 'settings-root';
  document.body.append(settingsRoot);

  document.documentElement.setAttribute('data-theme', st.theme);
  updateClock();
  render();
  buildSettings();
  setInterval(updateClock, 10000);

  await startPolling();
  await registerShortcuts();

  document.addEventListener('keydown', (e) => {
    if (e.code === 'Space' && !e.target.closest('input,select,textarea,button')) {
      e.preventDefault();
      st.settingsOpen = !st.settingsOpen;
      buildSettings();
    }
  });
}

document.addEventListener('DOMContentLoaded', init);
