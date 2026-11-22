// The sidebar for one window, in the Quiet layout (design_handoff_argon_browser): the active space's kept (pinned)
// and loose tabs, the edge-pill space switcher, and the command bar. Space and pin changes go to background.js;
// focusing and closing tabs of this space happen here.
import * as S from './spaces.js';

const { id: windowId } = await chrome.windows.getCurrent();
const send = (cmd, args = {}) => chrome.runtime.sendMessage({ cmd, windowId, ...args });
const $ = id => document.getElementById(id);
const el = (tag, cls, text) => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text != null) e.textContent = text;
  return e;
};
const favicon = (url, tab) => (/^(https?|data):/.test(tab?.favIconUrl ?? '') ? tab.favIconUrl
  : `/_favicon/?pageUrl=${encodeURIComponent(url || '')}&size=32`);

let state, space, kept = [], loose = [], archiveAfter = S.ARCHIVE_OPTIONS[0];

async function render() {
  state = await S.load();
  space = await S.activeSpace(state, windowId);
  const { pinTabs } = await S.session();
  ({ archiveAfter = S.ARCHIVE_OPTIONS[0] } = await chrome.storage.local.get('archiveAfter'));
  ({ kept, loose } = S.splitTabs(space, await S.tabsOf(space, windowId), pinTabs));
  const now = Date.now();

  document.documentElement.dataset.color = space.color;
  $('space-name').textContent = space.name;
  $('kept').replaceChildren(...kept.map(({ pin, tab }) => row({ tab, url: pin.url, title: tab?.title || pin.title, pin })));
  $('loose').replaceChildren(...loose.map(tab => row({ tab, url: tab.url, title: tab.title || tab.url || 'New Tab', now })));
  $('clear').hidden = !loose.length;

  $('pill').querySelector('.dots').replaceChildren(...state.spaces.map(s => {
    const dot = el('span');
    dot.classList.toggle('on', s.id === space.id);
    return dot;
  }));
  const add = el('button', 'add', '+');
  add.title = 'New space';
  add.onclick = newSpace;
  $('pill').querySelector('nav').replaceChildren(...state.spaces.map((s, n) => {
    const b = el('button', s.id === space.id ? 'on' : '', [...s.name][0].toUpperCase());
    b.title = n < 9 ? `${s.name} · ⌃${n + 1}` : s.name;
    b.setAttribute('aria-label', s.name);
    b.onclick = () => send('switch', { spaceId: s.id });
    return b;
  }), add);
}

function row({ tab, url, title, pin, now }) {
  const r = $('row').content.firstElementChild.cloneNode(true);
  r.querySelector('img').src = favicon(url, tab);
  r.querySelector('.title').textContent = title;
  r.title = url || '';
  r.classList.toggle('loose', !pin);
  r.classList.toggle('active', !!tab?.active);
  r.classList.toggle('closed', !tab);
  r.classList.toggle('discarded', !!tab?.discarded);
  const ms = pin ? null : S.timeLeft(tab, archiveAfter, now);
  const timer = r.querySelector('.timer');
  timer.textContent = ms == null ? '' : S.left(ms);
  timer.classList.toggle('soon', ms != null && ms < 2 * S.HOUR);
  r.querySelector('.open').onclick = () => (pin ? send('openPin', { pinId: pin.id }) : chrome.tabs.update(tab.id, { active: true }));
  r.querySelector('.x').onclick = () => closeTabs([tab.id]);
  r.onauxclick = e => e.button === 1 && !pin && closeTabs([tab.id]);
  r.ondragstart = e => {
    dragged = { tabId: tab?.id, pinId: pin?.id };
    e.dataTransfer.effectAllowed = 'move';
    r.classList.add('dragging');
  };
  r.ondragend = () => { dragged = null; r.classList.remove('dragging'); };
  return r;
}

// Drag a loose tab onto the kept list to pin it; drag a kept tab down to unpin it.
let dragged = null;
function dropZone(zone, accepts, onDrop) {
  zone.ondragover = e => {
    if (!dragged || !accepts(dragged)) return;
    e.preventDefault();
    zone.classList.add('drop');
  };
  zone.ondragleave = e => zone.contains(e.relatedTarget) || zone.classList.remove('drop');
  zone.ondrop = e => {
    e.preventDefault();
    zone.classList.remove('drop');
    onDrop(dragged);
  };
}
dropZone($('kept'), d => !d.pinId, d => send('pin', { tabId: d.tabId }));
dropZone($('loose-zone'), d => d.pinId, d => send('unpin', { pinId: d.pinId }));

// Closing the active tab selects the next loose tab, else the previous, else the last open kept tab, else a new tab
// (so Chromium never jumps into another space's collapsed group).
async function closeTabs(ids) {
  const gone = loose.filter(t => ids.includes(t.id));
  if (!gone.length) return;
  const active = gone.find(t => t.active);
  if (active) {
    const rest = loose.filter(t => !ids.includes(t.id));
    const next = rest[Math.min(loose.indexOf(active), rest.length - 1)] ?? kept.findLast(k => k.tab)?.tab;
    if (next) await chrome.tabs.update(next.id, { active: true });
    else await chrome.tabs.create({ windowId });
  }
  await chrome.tabs.remove(gone.map(t => t.id));
  toast(gone.length === 1 ? `Closed “${gone[0].title}”` : `${gone.length} loose tabs closed`, async () => {
    for (const _ of gone) await chrome.sessions.restore();
  });
}

function cycle(step) {
  const tabs = [...kept.map(k => k.tab).filter(Boolean), ...loose];
  if (!tabs.length) return;
  const i = tabs.findIndex(t => t.active);
  chrome.tabs.update(tabs[(i + step + tabs.length) % tabs.length].id, { active: true });
}

let toastTimer, undoFn;
function toast(text, undo) {
  clearTimeout(toastTimer);
  undoFn = undo;
  $('toast').querySelector('span').textContent = text;
  $('toast').querySelector('button').hidden = !undo;
  $('toast').hidden = false;
  toastTimer = setTimeout(() => { $('toast').hidden = true; undoFn = null; }, undo ? 6000 : 1400);
}
function undo() {
  const fn = undoFn;
  $('toast').hidden = true;
  undoFn = null;
  fn?.();
}
$('toast').querySelector('button').onclick = undo;

// Command bar. A prompt ({placeholder, value, action, submit}) turns it into a one-line text field, e.g. naming a space.
let results = [], sel = 0, prompt = null, all = [];

async function openCmd(p = null) {
  prompt = p;
  sel = 0;
  $('q').value = p?.value ?? '';
  $('q').placeholder = p?.placeholder ?? 'Search tabs, type a URL, or run a command';
  $('cmd').hidden = false;
  $('q').focus();
  $('q').select();
  const groups = new Map((await chrome.tabGroups.query({ windowId })).map(g => [g.id, g.title]));
  all = (await chrome.tabs.query({ windowId }))
    .map(tab => ({ tab, space: state.spaces.find(s => s.name === groups.get(tab.groupId)) }))
    .filter(x => x.space);
  renderResults();
}
const closeCmd = () => { $('cmd').hidden = true; prompt = null; };
const newSpace = () => openCmd({
  placeholder: 'Name the new space', action: v => `Create space “${v}”`, submit: name => send('addSpace', { name }),
});

function buildResults(raw) {
  const q = raw.trim();
  const { action, submit } = prompt ?? {};  // run() clears prompt before the result runs
  if (prompt) return q ? [{ title: action(q), meta: '↵', run: () => submit(q) }] : [];
  const match = s => !q || (s || '').toLowerCase().includes(q.toLowerCase());
  const tabs = all.filter(({ tab }) => match(tab.title) || match(tab.url)).slice(0, 6).map(({ tab, space: s }) => ({
    title: tab.title || tab.url, meta: s.id === space.id ? 'Switch to tab' : s.name, color: s.color,
    run: () => send('focusTab', { tabId: tab.id }),
  }));
  const url = S.toUrl(q);
  const web = !q ? [] : [url
    ? { title: `Open ${q}`, meta: 'New tab', run: () => chrome.tabs.create({ windowId, url }) }
    : { title: `Search the web for “${q}”`, meta: 'New tab', run: () => chrome.search.query({ text: q, disposition: 'NEW_TAB' }) }];
  // Commands the query starts ("new sp") rank above the web search; other matches go below it.
  const cmds = commands().filter(c => match(c.title));
  const lead = cmds.filter(c => q && c.title.toLowerCase().startsWith(q.toLowerCase()));
  const rest = cmds.filter(c => !lead.includes(c));
  return url ? [...web, ...tabs, ...lead, ...rest] : [...tabs, ...lead, ...web, ...rest];
}

function commands() {
  const current = kept.find(k => k.tab?.active);
  const activeLoose = loose.find(t => t.active);
  const i = state.spaces.indexOf(space);
  const heir = state.spaces[i > 0 ? i - 1 : 1];
  const nextArchive = S.ARCHIVE_OPTIONS[(S.ARCHIVE_OPTIONS.indexOf(archiveAfter) + 1) % S.ARCHIVE_OPTIONS.length];
  return [
    { title: 'New tab', run: () => chrome.tabs.create({ windowId }) },
    activeLoose && { title: 'Pin this tab', meta: `Keep in ${space.name}`, run: () => send('pin', { tabId: activeLoose.id }) },
    current && { title: 'Unpin this tab', run: () => send('unpin', { pinId: current.pin.id }) },
    current && current.tab.url !== current.pin.url && {
      title: 'Back to pinned page', run: () => chrome.tabs.update(current.tab.id, { url: current.pin.url }),
    },
    loose.length && { title: 'Clear loose tabs', meta: space.name, run: () => closeTabs(loose.map(t => t.id)) },
    ...state.spaces.map((s, n) => s !== space && {
      title: `Go to ${s.name}`, meta: n < 9 ? `⌃${n + 1}` : '', color: s.color, run: () => send('switch', { spaceId: s.id }),
    }),
    { title: 'New space…', run: newSpace },
    { title: `Rename ${space.name}…`, run: renameSpace },
    { title: `Change color of ${space.name}`, run: () => send('recolor', { spaceId: space.id }) },
    heir && { title: `Delete ${space.name}`, meta: `Tabs move to ${heir.name}`, run: () => send('removeSpace', { spaceId: space.id }) },
    {
      title: `Auto-archive loose tabs: ${S.archiveLabel(archiveAfter)}`, meta: `Change to ${S.archiveLabel(nextArchive)}`,
      run: async () => {
        await chrome.storage.local.set({ archiveAfter: nextArchive });
        toast(nextArchive ? `Loose tabs close after ${S.archiveLabel(nextArchive)} unused` : 'Auto-archive off');
      },
    },
  ].filter(Boolean);
}
const renameSpace = () => openCmd({
  value: space.name, placeholder: 'Space name', action: v => `Rename to “${v}”`,
  submit: name => send('renameSpace', { spaceId: space.id, name }),
});

function renderResults() {
  results = buildResults($('q').value);
  sel = Math.min(sel, Math.max(0, results.length - 1));
  $('results').replaceChildren(...results.map((r, n) => {
    const d = el('div', 'result');
    d.setAttribute('role', 'option');
    const dot = el('i');
    if (r.color) dot.dataset.color = r.color;
    d.append(dot, el('span', 'title', r.title), el('span', 'meta', r.meta || ''));
    d.onmouseenter = () => { sel = n; mark(); };
    d.onclick = () => run(r);
    return d;
  }));
  mark();
}
function mark() {
  [...$('results').children].forEach((d, n) => {
    d.classList.toggle('sel', n === sel);
    d.setAttribute('aria-selected', n === sel);
    if (n === sel) d.scrollIntoView({ block: 'nearest' });
  });
}
function run(r) {
  closeCmd();
  r.run();
}

$('q').oninput = () => { sel = 0; renderResults(); };
$('cmd').onmousedown = e => e.target === $('cmd') && closeCmd();
$('new').onclick = () => openCmd();
$('clear').onclick = () => closeTabs(loose.map(t => t.id));
$('space-name').ondblclick = renameSpace;

addEventListener('keydown', e => {
  const mod = e.metaKey || e.ctrlKey;
  if (mod && e.code === 'KeyK') {
    e.preventDefault();
    return $('cmd').hidden ? openCmd() : closeCmd();
  }
  if (!$('cmd').hidden) {
    if ((e.key === 'ArrowDown' || e.key === 'ArrowUp') && results.length) {
      e.preventDefault();
      sel = (sel + (e.key === 'ArrowDown' ? 1 : -1) + results.length) % results.length;
      mark();
    } else if (e.key === 'Enter') {
      e.preventDefault();
      if (results[sel]) run(results[sel]);
    } else if (e.key === 'Escape') closeCmd();
    return;
  }
  if (mod && e.code === 'KeyZ' && undoFn) {
    e.preventDefault();
    undo();
  } else if (e.ctrlKey && !e.metaKey && /^Digit[1-9]$/.test(e.code)) {
    e.preventDefault();
    const s = state.spaces[+e.code.slice(5) - 1];
    if (s) send('switch', { spaceId: s.id });
  } else if (e.altKey && e.code === 'KeyW') {
    e.preventDefault();
    const active = loose.find(t => t.active);
    if (active) closeTabs([active.id]);
  } else if (e.ctrlKey && (e.key === 'ArrowDown' || e.key === 'ArrowUp')) {
    e.preventDefault();
    cycle(e.key === 'ArrowDown' ? 1 : -1);
  }
});

let pending;
const refresh = () => { clearTimeout(pending); pending = setTimeout(render, 30); };
for (const ev of ['onCreated', 'onRemoved', 'onUpdated', 'onActivated', 'onMoved', 'onAttached', 'onDetached', 'onReplaced']) {
  chrome.tabs[ev].addListener(refresh);
}
for (const ev of ['onCreated', 'onRemoved', 'onUpdated']) chrome.tabGroups[ev].addListener(refresh);
setInterval(refresh, 60e3);  // archive timers

// The command-bar shortcut (background.js) opens this panel and asks for the bar through storage, since a panel
// that was closed misses messages sent while it loads.
const wantsCmd = v => v?.windowId === windowId && Date.now() - v.at < 3000;
chrome.storage.onChanged.addListener((changes, area) => {
  if (area === 'session' && wantsCmd(changes.cmdBar?.newValue)) openCmd();
  refresh();
});

await render();
const { cmdBar } = await chrome.storage.session.get('cmdBar');
if (wantsCmd(cmdBar)) openCmd();
