// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

// The Customize page: a preview window of sample tabs on the left, the
// inspector on the right (design_handoff_argon_browser/README.md §6; the
// logic is "Argon Customize.dc.html"'s Component, ported). Saves
// {zones, cfg} to Local State (argon.customize) and the tone to the profile
// (argon.tone, prefs::kBrowserColorScheme) through argon_customize_ui.cc; the
// real sidebar redraws from those. No innerHTML: Trusted Types are on, and
// stored strings reach the DOM.

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';

// Mirrors argon_prefs.cc (DefaultSide(), Settings); keep them in step.
const DEFAULT_SIDE = [
  'nav', 'spaceName', 'divider', 'favorites', 'divider', 'pinned', 'divider',
  'loose', 'newTab', 'divider', 'spaces',
];
const DEFAULT_CFG = {
  side: 'Left', behavior: 'Pinned', rond: 50, density: 'Regular',
  icon: 'Tile', closeHover: true, switcher: 'Icons', edgePill: true,
  pillSide: 'Left', archive: '12h', gap: 8, radius: 10, hues: [250, 300, 155],
};
// Upstream's (kVerticalTabsUncollapsedWidth). cfg.width stays unset until
// chosen, so windows keep their own width.
const DEFAULT_WIDTH = 240;

const LABELS = {
  spaceName: 'Space name', nav: 'Back · Forward · Reload', address: 'Address',
  favorites: 'Favorites', pinned: 'Pinned tabs', loose: 'Loose tabs',
  newTab: 'New tab', clear: 'Clear loose', spaces: 'Space switcher',
  archive: 'Archive', divider: 'Divider', spacer: 'Spacer',
};
const REPEATABLE = new Set(['divider', 'spacer']);
const CHOICES = {
  side: ['Left', 'Right'], behavior: ['Pinned', 'Auto-hide', 'Off'],
  density: ['Compact', 'Regular', 'Roomy'], icon: ['Tile', 'Dot', 'None'],
  switcher: ['Icons', 'Dots', 'Names'], pillSide: ['Left', 'Right'],
  archive: ['12h', '24h', '7d', 'Never'],
};
const RANGES = {rond: [0, 100], gap: [0, 16], radius: [0, 20]};
const HUES = [250, 300, 155, 30, 75, 200];
const DENSITY = {
  Compact: {h: '26px', fs: '12.5px', gap: '2px'},
  Regular: {h: '30px', fs: '13px', gap: '6px'},
  Roomy: {h: '36px', fs: '14px', gap: '10px'},
};
const TONE_PREF = ['paper', 'cool', 'tinted'];

// Sample tabs; age is hours since last use.
const AGE = {p3: 2, p4: 9, a3: 0.2, a4: 5, a5: 11.4, s2: 1, s3: 7};
const T = (id, title, host) => ({id, title, host, age: AGE[id] || 0});
const SPACES = [
  {
    name: 'Personal',
    icon: 'M2.5 7.5 8 3l5.5 4.5V13a.5.5 0 0 1-.5.5H3a.5.5 0 0 1-.5-.5z',
    kept: [T('p1', 'Inbox', 'mail.icloud.com'),
           T('p2', 'Calendar', 'calendar.icloud.com')],
    loose: [T('p3', 'Hacker News', 'news.ycombinator.com'),
            T('p4', 'Watch later', 'youtube.com')],
  },
  {
    name: 'Argon',
    icon: 'm5.5 4.5-3.5 3.5 3.5 3.5M10.5 4.5 14 8l-3.5 3.5',
    kept: [T('a1', 'imputnet/helium', 'github.com'),
           T('a2', 'Chromium Code Search', 'source.chromium.org')],
    loose: [T('a3', 'Amazon EC2 Mac instances', 'docs.aws.amazon.com'),
            T('a4', 'memory_saver_mode_policy.cc', 'source.chromium.org'),
            T('a5', 'Building Helium for macOS', 'github.com')],
  },
  {
    name: 'School',
    icon: 'M2 3.5h4a2 2 0 0 1 2 2v8a1.5 1.5 0 0 0-1.5-1.5H2zM14 3.5h-4a2 2 0 0 0-2 2v8a1.5 1.5 0 0 1 1.5-1.5H14z',
    kept: [T('s1', 'Course dashboard', 'canvas.instructure.com')],
    loose: [T('s2', 'Essay draft', 'docs.google.com'),
            T('s3', 'Derivative of arctan(x)', 'wolframalpha.com')],
  },
];
const FAVS = [
  ['Mail', '#2f5fb3'], ['Calendar', '#b3402f'], ['GitHub', '#24292f'],
  ['Music', '#b0305a'], ['Notes', '#8a6a12'], ['Maps', '#2c7a4b'],
  ['Drive', '#1a6b8a'], ['Claude', '#9a4a2c'],
];

const S = {
  editing: true, cur: 1, reveal: false, archived: 14,
  active: {0: 'p3', 1: 'a3', 2: 's2'},
  spaces: structuredClone(SPACES),
  zones: {side: [], top: []}, cfg: {...DEFAULT_CFG},
  appearance: {tone: 'paper', scheme: 0, editable: true},
  // Dragging: what, from where, and the drop point.
  drag: null, dragId: null, dragFrom: null, drop: null,
  // A push that came in mid-drag, applied when it ends.
  pending: null,
};
let n = 0;
const newId = type => `${type}-${++n}`;
const darkMedia = matchMedia('(prefers-color-scheme: dark)');
const app = document.getElementById('app');
// Elements render() hands to paintDrag() and renderStage().
let refs = {};

// --- DOM -------------------------------------------------------------------

function setProps(el, props) {
  for (const [k, v] of Object.entries(props || {})) {
    if (v === undefined || v === null || v === false) {
      continue;
    }
    if (k.startsWith('on')) {
      el.addEventListener(k.slice(2), v);
    } else if (k === 'style') {
      el.style.cssText = v;
    } else {
      el.setAttribute(k, v === true ? '' : String(v));
    }
  }
}

function append(el, kids) {
  for (const kid of kids.flat(Infinity)) {
    if (kid === null || kid === undefined || kid === false || kid === '') {
      continue;
    }
    el.append(kid instanceof Node ? kid : String(kid));
  }
}

function h(tag, props, ...kids) {
  const el = document.createElement(tag);
  setProps(el, props);
  append(el, kids);
  return el;
}

function s(tag, props, ...kids) {
  const el = document.createElementNS('http://www.w3.org/2000/svg', tag);
  setProps(el, props);
  append(el, kids);
  return el;
}

function icon(paths, size, width = 1.6) {
  return s('svg', {
    width: size, height: size, viewBox: '0 0 16 16', fill: 'none',
    stroke: 'currentColor', 'stroke-width': width, 'stroke-linejoin': 'round',
    'stroke-linecap': 'round',
  }, [paths].flat().map(d => s('path', {d})));
}

const cross = (size, width) => icon('m3 3 10 10M13 3 3 13', size, width);

// --- State -----------------------------------------------------------------

const isObject = v => !!v && typeof v === 'object' && !Array.isArray(v);

// JSON with sorted keys: Local State's dicts come back sorted.
function canon(v) {
  return JSON.stringify(v, (k, x) => isObject(x) ?
      Object.fromEntries(Object.entries(x).sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0)) :
      x);
}

// A stored cfg, checked: Local State is a file anyone can edit. Keys this
// page doesn't know are kept.
function cleanCfg(stored) {
  const cfg = {...stored};
  for (const [k, options] of Object.entries(CHOICES)) {
    if (!options.includes(cfg[k])) {
      cfg[k] = DEFAULT_CFG[k];
    }
  }
  const clamp = (v, [lo, hi]) => Math.min(hi, Math.max(lo, Math.round(v)));
  for (const [k, range] of Object.entries(RANGES)) {
    cfg[k] = Number.isFinite(cfg[k]) ? clamp(cfg[k], range) : DEFAULT_CFG[k];
  }
  if (Number.isFinite(cfg.width)) {
    cfg.width = clamp(cfg.width, [180, 320]);
  } else {
    delete cfg.width;
  }
  for (const k of ['closeHover', 'edgePill']) {
    if (typeof cfg[k] !== 'boolean') {
      cfg[k] = DEFAULT_CFG[k];
    }
  }
  const hues = Array.isArray(cfg.hues) ? cfg.hues : [];
  cfg.hues = DEFAULT_CFG.hues.map((hue, i) => HUES.includes(hues[i]) ? hues[i] : hue);
  delete cfg.tone;  // The handoff keeps it here; Argon's is per profile.
  return cfg;
}

function zone(types, seen) {
  const items = [];
  for (const type of types) {
    if (!Object.hasOwn(LABELS, type) || (!REPEATABLE.has(type) && seen.has(type))) {
      continue;
    }
    seen.add(type);
    items.push({id: newId(type), type});
  }
  return items;
}

function load(stored) {
  stored = isObject(stored) ? stored : {};
  const zones = isObject(stored.zones) ? stored.zones : {};
  const seen = new Set();
  S.zones = {
    side: zone(Array.isArray(zones.side) ? zones.side : DEFAULT_SIDE, seen),
    top: zone(Array.isArray(zones.top) ? zones.top : [], seen),
  };
  S.cfg = cleanCfg(isObject(stored.cfg) ? stored.cfg : {});
}

const toStored = () => ({
  zones: {
    side: S.zones.side.map(x => x.type),
    top: S.zones.top.map(x => x.type),
  },
  cfg: S.cfg,
});

// What this page wrote lately, so its own writes coming back don't reload it.
const sent = [];
let saveTimer = 0;

function save() {
  const value = toStored();
  sent.push(canon(value));
  if (sent.length > 8) {
    sent.shift();
  }
  chrome.send('setCustomize', [value]);
}

function scheduleSave() {
  clearTimeout(saveTimer);
  saveTimer = setTimeout(save, 100);
}

// Pushes from any window, this page's own saves included; those (and an
// unchanged appearance) redraw nothing, so a slider isn't rebuilt mid-drag.
function onState(state) {
  const appearance = {
    tone: TONE_PREF.includes(state.tone) ? state.tone : 'paper',
    scheme: [0, 1, 2].includes(state.scheme) ? state.scheme : 0,
    editable: !!state.appearanceEditable,
  };
  const external = !sent.includes(canon(state.customize));
  const dirty = external || canon(appearance) !== canon(S.appearance);
  S.appearance = appearance;
  if (external) {
    sent.length = 0;  // Older saves of ours no longer say what's current.
    if (S.drag) {
      S.pending = state.customize;
    } else {
      load(state.customize);
    }
  }
  if (dirty && !S.drag) {
    render();
  }
}

function changed() {
  render();
  scheduleSave();
}

function setCfg(update) {
  S.cfg = {...S.cfg, ...update};
  changed();
}

function preset(name) {
  const P = {
    // Argon's own sidebar (the user's choice over the handoff's shorter list).
    Quiet: [DEFAULT_SIDE, [], {edgePill: true, pillSide: 'Left', width: DEFAULT_WIDTH}],
    Ticks: [['spaceName', 'divider', 'pinned', 'divider', 'loose', 'newTab', 'spacer', 'spaces'],
            [], {behavior: 'Auto-hide', width: 248}],
    Classic: [['nav', 'address', 'favorites', 'spaceName', 'pinned', 'divider', 'newTab',
               'loose', 'spacer', 'archive', 'spaces'],
              [], {width: 264, switcher: 'Dots'}],
    'Top bar': [[], ['nav', 'pinned', 'divider', 'loose', 'newTab', 'spacer', 'address', 'spaces'],
                {behavior: 'Off', switcher: 'Dots'}],
  }[name];
  const seen = new Set();
  S.zones = {side: zone(P[0], seen), top: zone(P[1], seen)};
  // Colors and roundness carry over, as in the handoff.
  S.cfg = cleanCfg({
    ...DEFAULT_CFG, edgePill: false, hues: S.cfg.hues, rond: S.cfg.rond, ...P[2],
  });
}

// The swatch for the profile's prefs; one this page didn't write (system +
// cool) shows as its tone, as ApplyTone draws it in light mode.
function swatch() {
  const {scheme, tone} = S.appearance;
  if (scheme === 2) {
    return 'Dark';
  }
  if (tone !== 'paper') {
    return tone[0].toUpperCase() + tone.slice(1);
  }
  return scheme === 0 ? 'Auto' : 'Paper';
}

// What the window draws now.
function effectiveTone() {
  const {scheme, tone} = S.appearance;
  if (scheme === 2 || (scheme === 0 && darkMedia.matches)) {
    return 'Dark';
  }
  return tone[0].toUpperCase() + tone.slice(1);
}

function setAppearance(name) {
  const a = S.appearance;
  if (!a.editable) {
    return;
  }
  const [scheme, tone] = {
    Auto: [0, 'paper'], Paper: [1, 'paper'], Cool: [1, 'cool'],
    Tinted: [1, 'tinted'], Dark: [2, a.tone],
  }[name];
  S.appearance = {...a, scheme, tone};
  render();
  chrome.send('setAppearance', [scheme, tone]);
}

function tokens(tone, hue) {
  const L = {
    bg: '#f3f1ed', ink: '#1d1c1a', dim: '#5c5953', muted: '#8a857c',
    line: 'rgba(29,28,26,.1)', active: 'rgba(29,28,26,.07)',
    hover: 'rgba(29,28,26,.04)', field: 'rgba(29,28,26,.05)', page: '#fff',
    bar: '#f1efeb', card: '#fbfaf8', edit: 'rgba(29,28,26,.25)',
    pageSh: '0 0 0 1px rgba(29,28,26,.06)',
  };
  if (tone === 'Cool') {
    return {...L, bg: '#eceff3', card: '#f8f9fb'};
  }
  if (tone === 'Tinted') {
    return {
      ...L, bg: `oklch(0.93 0.035 ${hue})`, card: `oklch(0.975 0.012 ${hue})`,
      field: 'rgba(29,28,26,.06)',
    };
  }
  if (tone === 'Dark') {
    return {
      bg: '#1c1b19', ink: '#f1efe9', dim: '#c4bfb5', muted: '#8f8a80',
      line: 'rgba(255,255,255,.1)', active: 'rgba(255,255,255,.09)',
      hover: 'rgba(255,255,255,.05)', field: 'rgba(255,255,255,.07)',
      page: '#272623', bar: '#33312d', card: '#262522',
      edit: 'rgba(255,255,255,.28)', pageSh: '0 0 0 1px rgba(255,255,255,.06)',
    };
  }
  return L;
}

// --- Preview actions -------------------------------------------------------

const space = () => S.spaces[S.cur];

function activate(id) {
  S.active = {...S.active, [S.cur]: id};
  renderStage();
}

function closeTab(id) {
  const sp = space();
  const index = sp.loose.findIndex(t => t.id === id);
  const loose = sp.loose.filter(t => t.id !== id);
  if (S.active[S.cur] === id) {
    const next = loose[index] || loose[index - 1] || sp.kept[sp.kept.length - 1];
    S.active = {...S.active, [S.cur]: next ? next.id : null};
  }
  sp.loose = loose;
  S.archived++;
  renderStage();
}

function clearLoose() {
  const sp = space();
  if (sp.loose.some(t => t.id === S.active[S.cur])) {
    S.active = {...S.active, [S.cur]: sp.kept[0] ? sp.kept[0].id : null};
  }
  S.archived += sp.loose.length;
  sp.loose = [];
  renderStage();
}

function newTab() {
  const t = {id: 'n' + Date.now(), title: 'New tab', host: 'argon://new', age: 0};
  space().loose = [t, ...space().loose];
  S.active = {...S.active, [S.cur]: t.id};
  renderStage();
}

function pickSpace(i) {
  S.cur = i;
  render();  // The Tinted swatch follows the space.
}

// --- Drag and drop ---------------------------------------------------------
// The DOM isn't rebuilt mid-drag (removing the dragged node ends the drag):
// paintDrag() only moves the indicator, dims the source and lights the tray.

function paintDrag() {
  const d = S.drop;
  for (const z of ['side', 'top']) {
    (refs.lines[z] || []).forEach((el, i) => {
      el.style.display = d && d.zone === z && d.index === i ? 'block' : 'none';
    });
    const end = refs.end[z];
    if (end) {
      const length = S.zones[z].length;
      end.style.display =
          d && d.zone === z && d.index === length && length > 0 ? 'block' : 'none';
    }
  }
  refs.items.forEach((el, id) => {
    el.style.opacity = S.dragId === id ? '.35' : '1';
  });
  if (refs.tray) {
    refs.tray.classList.toggle('hot', !!S.dragFrom && S.dragFrom !== 'tray');
  }
}

const dragStart = src => e => {
  e.dataTransfer.effectAllowed = 'move';
  e.dataTransfer.setData('text/plain', src.type);
  S.drag = src;
  setTimeout(() => {
    S.dragId = src.id || null;
    S.dragFrom = src.from;
    paintDrag();
  }, 0);
};

function setDrop(z, index) {
  const d = S.drop;
  if (!d || d.zone !== z || d.index !== index) {
    S.drop = {zone: z, index};
    paintDrag();
  }
}

const itemOver = (z, i, horizontal) => e => {
  if (!S.drag) {
    return;
  }
  e.preventDefault();
  e.stopPropagation();
  const r = e.currentTarget.getBoundingClientRect();
  const before = horizontal ? e.clientX < r.left + r.width / 2 :
                              e.clientY < r.top + r.height / 2;
  setDrop(z, before ? i : i + 1);
};

const zoneOver = z => e => {
  if (!S.drag) {
    return;
  }
  e.preventDefault();
  if (!S.drop || S.drop.zone !== z) {
    setDrop(z, S.zones[z].length);
  }
};

// Ends a drag; true if one was going.
function finishDrag() {
  const dragging = !!S.drag || !!S.dragFrom;
  S.drag = S.dragId = S.dragFrom = S.drop = null;
  if (S.pending !== null) {
    load(S.pending);
    S.pending = null;
  }
  return dragging;
}

const zoneDrop = z => e => {
  e.preventDefault();
  const src = S.drag;
  const d = S.drop;
  if (!src) {
    return;
  }
  const zones = {side: [...S.zones.side], top: [...S.zones.top]};
  let index = d && d.zone === z ? d.index : zones[z].length;
  let item;
  if (src.from === 'tray') {
    item = {id: newId(src.type), type: src.type};
  } else {
    const from = zones[src.from];
    const k = from.findIndex(x => x.id === src.id);
    if (k < 0) {
      return;
    }
    item = from.splice(k, 1)[0];
    if (src.from === z && k < index) {
      index--;
    }
  }
  zones[z].splice(index, 0, item);
  finishDrag();
  S.zones = zones;
  changed();
};

function endDrag() {
  if (finishDrag()) {
    render();
  }
}

function removeItem(z, id) {
  S.zones = {...S.zones, [z]: S.zones[z].filter(x => x.id !== id)};
  changed();
}

function addItem(type) {
  S.zones = {...S.zones, side: [...S.zones.side, {id: newId(type), type}]};
  changed();
}

// --- Preview ---------------------------------------------------------------

function stage() {
  const c = S.cfg;
  const ed = S.editing;
  const tone = effectiveTone();
  const dark = tone === 'Dark';
  const tk = tokens(tone, c.hues[S.cur]);
  const sp = space();
  const act = S.active[S.cur];
  const accentOf = hue => `oklch(${dark ? 0.68 : 0.56} 0.13 ${hue})`;
  const accent = accentOf(c.hues[S.cur]);
  const D = DENSITY[c.density];
  const limit = {'12h': 12, '24h': 24, '7d': 168}[c.archive];
  const page = [...sp.kept, ...sp.loose].find(t => t.id === act);

  const timer = t => {
    if (!limit) {
      return null;
    }
    const r = Math.max(limit - t.age, 0);
    return h('span', {
      class: 'time',
      title: `Archives in ${r < 1 ? Math.round(r * 60) + ' min' : r.toFixed(r < 10 ? 1 : 0) + ' h'}`,
      style: `color:${r < 2 ? (dark ? 'oklch(0.75 0.13 65)' : 'oklch(0.55 0.14 55)') : tk.muted}`,
    }, r < 1 ? Math.max(1, Math.round(r * 60)) + 'm' :
       r < 48 ? Math.round(r) + 'h' : Math.round(r / 24) + 'd');
  };

  const row = (t, loose) => {
    const hostHue = [...t.host].reduce((a, ch) => (a * 31 + ch.charCodeAt(0)) % 360, 7);
    const iconBg = `background:oklch(0.55 0.12 ${hostHue})`;
    const closable = loose && c.closeHover;
    return h('div', {
      class: `row${t.id === act ? ' active' : ''}${closable ? ' closable' : ''}`,
    },
      h('button', {class: 'row-main', onclick: () => activate(t.id)},
        c.icon === 'Tile' &&
            h('span', {class: 'tile', style: iconBg},
              t.host.replace(/^www\./, '')[0].toUpperCase()),
        c.icon === 'Dot' && h('span', {class: 'dot', style: iconBg}),
        h('span', {class: 'title'}, t.title)),
      loose && timer(t),
      closable && h('button', {
        class: 'close', 'aria-label': 'Close tab', onclick: () => closeTab(t.id),
      }, icon('m4 4 8 8M12 4l-8 8', 10, 2)));
  };

  const switcherButton = (x, i, size) => {
    const on = i === S.cur;
    const a = accentOf(c.hues[i]);
    const sw = c.switcher;
    return h('button', {
      title: `${x.name} · ⌃${i + 1}`, 'aria-label': x.name,
      onclick: () => pickSpace(i),
      style: `padding:${sw === 'Names' ? '0 10px' : '0'};` +
          `background:${on && sw !== 'Dots' ? a : 'transparent'};color:${on ? '#fff' : tk.dim}`,
    },
      sw === 'Icons' && icon(x.icon, size),
      sw === 'Dots' && h('span', {
        style: `width:${on ? 10 : 7}px;height:${on ? 10 : 7}px;border-radius:50%;` +
            `background:${on ? a : tk.muted}`,
      }),
      sw === 'Names' && h('span', {}, x.name));
  };

  const module = type => {
    switch (type) {
      case 'spaceName':
        return h('div', {class: 'space-name'}, sp.name);
      case 'divider':
        return h('div', {class: 'divider'});
      case 'spacer':
        return ed && h('div', {class: 'spacer-label'}, 'Spacer');
      case 'pinned':
      case 'loose': {
        const tabs = type === 'pinned' ? sp.kept : sp.loose;
        return h('div', {class: 'tabs'},
          tabs.map(t => row(t, type === 'loose')),
          !tabs.length && h('div', {class: 'nothing'}, 'Nothing here'));
      }
      case 'favorites':
        return h('div', {class: 'favs'}, FAVS.map(([name, color]) =>
          h('button', {class: 'fav', title: name, 'aria-label': name},
            h('span', {style: `background:${color}`}, name[0]))));
      case 'nav':
        return h('div', {class: 'nav'},
          h('button', {'aria-label': 'Back'}, icon('M10 3 5 8l5 5', 15)),
          h('button', {'aria-label': 'Forward'}, icon('m6 3 5 5-5 5', 15)),
          h('button', {'aria-label': 'Reload'},
            icon(['M13 8a5 5 0 1 1-1.5-3.6', 'M13 2.5V5h-2.5'], 15)));
      case 'address':
        return h('button', {class: 'address'},
          s('svg', {
            width: 13, height: 13, viewBox: '0 0 16 16', fill: 'none',
            stroke: 'currentColor', 'stroke-width': 1.6,
          },
            s('rect', {x: 3.5, y: 7, width: 9, height: 6.5, rx: 1.5}),
            s('path', {d: 'M5.5 7V5a2.5 2.5 0 0 1 5 0v2'})),
          h('span', {class: 'title'}, page ? page.host : 'Search or type a URL'));
      case 'newTab':
        return h('button', {class: 'new-tab', onclick: newTab}, '+ New tab');
      case 'clear':
        return h('button', {class: 'clear', onclick: clearLoose}, 'Clear loose');
      case 'archive':
        return h('button', {
          class: 'archive', 'aria-label': 'Archive',
          title: limit ? `Loose tabs archive after ${c.archive} idle` : 'Auto-archive off',
        },
          s('svg', {
            width: 14, height: 14, viewBox: '0 0 16 16', fill: 'none',
            stroke: 'currentColor', 'stroke-width': 1.5,
          },
            s('rect', {x: 1.5, y: 2.5, width: 13, height: 3.5, rx: 1}),
            s('path', {d: 'M2.5 6v6.5a1 1 0 0 0 1 1h9a1 1 0 0 0 1-1V6M6.5 9h3'})),
          String(S.archived));
      case 'spaces':
        return h('div', {class: 'switcher'},
          S.spaces.map((x, i) => switcherButton(x, i, 14)));
    }
    return null;
  };

  const item = (x, z, i) => {
    const horizontal = z === 'top';
    const tabs = x.type === 'pinned' || x.type === 'loose';
    const spacer = x.type === 'spacer';
    const line = h('span', {class: 'drop-line'});
    refs.lines[z].push(line);
    const el = h('div', {
      class: 'item', draggable: ed ? 'true' : 'false',
      style: `flex:${spacer ? '1 1 0' : horizontal && tabs ? '0 1 auto' : '0 0 auto'};` +
          `min-height:${spacer && ed && !horizontal ? '28px' : '0'};` +
          `min-width:${spacer && ed && horizontal ? '56px' : '0'};` +
          `outline:${ed ? '1px dashed var(--edit)' : 'none'};cursor:${ed ? 'grab' : 'auto'}`,
      ondragstart: dragStart({from: z, id: x.id, type: x.type}),
      ondragover: itemOver(z, i, horizontal),
      ondragend: endDrag,
    },
      line,
      h('div', {class: 'item-body'}, h('div', {class: 'module'}, module(x.type))),
      ed && h('button', {
        class: 'remove', 'aria-label': 'Remove module', onclick: () => removeItem(z, x.id),
      }, cross(7, 3)));
    refs.items.set(x.id, el);
    return el;
  };

  const zoneEnd = z => {
    refs.end[z] = h('span', {class: 'drop-end'});
    return refs.end[z];
  };

  const left = c.side === 'Left';
  const autoUse = c.behavior === 'Auto-hide' && !ed;
  const off = c.behavior === 'Off' && !ed;
  const inFlow = !off && !autoUse;
  const pillLeft = c.pillSide === 'Left';
  const pillOnSide = c.edgePill && pillLeft === left;
  const pillNudge = autoUse && pillLeft === left ? '24px' : '0';
  const sidePad = pillOnSide && inFlow ? '26px' : '10px';
  const topVisible = ed || S.zones.top.length > 0;
  const g = c.gap + 'px';
  const leftFree = !(inFlow && left);

  const sidebar = h('aside', {
    class: 'sidebar', 'aria-label': 'Sidebar',
    onmouseleave: () => {
      if (autoUse && S.reveal) {
        S.reveal = false;
        renderStage();
      }
    },
    style: [
      `display:${off ? 'none' : 'flex'}`, `position:${autoUse ? 'absolute' : 'relative'}`,
      `top:${autoUse ? '8px' : 'auto'}`, `bottom:${autoUse ? '8px' : 'auto'}`,
      `left:${autoUse && left ? '8px' : 'auto'}`, `right:${autoUse && !left ? '8px' : 'auto'}`,
      `z-index:${autoUse ? 6 : 1}`, `width:${c.width ?? DEFAULT_WIDTH}px`,
      `background:${autoUse ? tk.card : 'transparent'}`,
      `border-radius:${autoUse ? '14px' : '0'}`,
      `box-shadow:${autoUse ? `0 0 0 1px ${tk.line},0 18px 50px rgba(0,0,0,.18)` : 'none'}`,
      `transform:translateX(${autoUse && !S.reveal ? (left ? '-110%' : '110%') : '0'})`,
      `padding:${left && inFlow ? '46px' : '14px'} ${left ? '10px' : sidePad} 14px ${left ? sidePad : '10px'}`,
    ].join(';'),
  },
    ed && c.behavior !== 'Pinned' && h('div', {class: 'note'},
      c.behavior === 'Off' ? 'Hidden when using' : 'Auto-hides when using'),
    h('div', {
      class: 'zone-side', style: `gap:${D.gap}`,
      ondragover: zoneOver('side'), ondrop: zoneDrop('side'),
    },
      S.zones.side.map((x, i) => item(x, 'side', i)),
      zoneEnd('side'),
      ed && !S.zones.side.length && h('div', {class: 'empty-zone'}, 'Drop modules here')));

  const ticks = autoUse && !S.reveal && h('div', {
    class: 'ticks',
    style: `left:${left ? '0' : 'auto'};right:${left ? 'auto' : '0'}`,
    onmouseenter: () => {
      S.reveal = true;
      renderStage();
    },
  }, h('div', {style: `align-items:${left ? 'flex-start' : 'flex-end'}`},
    [...sp.kept.map(t => [t, '0']), ...sp.loose.map((t, i) => [t, i === 0 ? '8px' : '0'])]
        .map(([t, gap]) => h('span', {
          style: `width:${t.id === act ? 16 : 9}px;background:${t.id === act ? accent : tk.edit};` +
              `margin-top:${gap}`,
        }))));

  const pill = c.edgePill && h('div', {
    class: 'pill',
    style: `left:${pillLeft ? pillNudge : 'auto'};right:${pillLeft ? 'auto' : pillNudge}`,
  },
    h('div', {class: 'pill-rest'}, S.spaces.map((x, i) => h('span', {
      style: `height:${i === S.cur ? 14 : 4}px;background:${i === S.cur ? accentOf(c.hues[i]) : tk.muted}`,
    }))),
    h('nav', {class: 'pill-open', 'aria-label': 'Spaces'}, S.spaces.map((x, i) => h('button', {
      title: `${x.name} · ⌃${i + 1}`, 'aria-label': x.name, onclick: () => pickSpace(i),
      style: `background:${i === S.cur ? accentOf(c.hues[i]) : 'transparent'};` +
          `color:${i === S.cur ? '#fff' : tk.dim}`,
    }, icon(x.icon, 16)))));

  const column = h('div', {
    class: 'column',
    style: `gap:${g};padding:${topVisible ? '4px' : leftFree ? '40px' : g} ` +
        `${inFlow && !left ? '0' : g} ${g} ${inFlow && left ? '0' : g}`,
  },
    topVisible && h('div', {
      class: 'zone-top', style: `padding:0 4px 0 ${leftFree ? '72px' : '0'}`,
      ondragover: zoneOver('top'), ondrop: zoneDrop('top'),
    },
      S.zones.top.map((x, i) => item(x, 'top', i)),
      zoneEnd('top'),
      ed && !S.zones.top.length &&
          h('div', {class: 'empty-zone'}, 'Drop modules here to add a top bar')),
    h('main', {
      class: 'page', style: `border-radius:${c.radius}px;box-shadow:${tk.pageSh}`,
    }, h('article', {},
      h('div', {class: 'host'}, h('span', {style: `background:${accent}`}),
        page ? page.host : sp.name),
      h('h1', {}, page ? page.title : 'No tab open'),
      h('div', {class: 'bars'}, [['100%', 0], ['92%', 0], ['78%', 0], ['85%', 12], ['60%', 0]]
          .map(([width, top]) => h('div', {style: `width:${width};margin-top:${top}px`}))),
      h('div', {class: 'stand-in'}, 'Stand-in for the web page'))));

  const vars = Object.entries(tk).filter(([k]) => k !== 'pageSh')
      .map(([k, v]) => `--${k}:${v}`).join(';');
  const el = h('div', {class: 'stage'},
    h('div', {class: 'status'},
      h('b', {}, ed ? 'Editing layout' : 'Using Argon'),
      h('span', {}, ed ? 'Drag modules to reorder, between sidebar and top bar, or back to the tray to remove.' :
          c.behavior === 'Auto-hide' ? 'Hover the edge ticks to open the sidebar.' :
          'Click tabs and spaces.')),
    h('div', {
      class: `win${ed ? ' editing' : ''}`,
      style: `flex-direction:${left ? 'row' : 'row-reverse'};${vars};` +
          `--row-h:${D.h};--fs:${D.fs}`,
    },
      h('div', {class: 'lights', 'aria-hidden': 'true'},
        ['#ff5f57', '#febc2e', '#28c840'].map(color => h('i', {style: `background:${color}`}))),
      sidebar, ticks, pill, column));
  refs.stage = el;
  return el;
}

// --- Inspector -------------------------------------------------------------

function seg(current, options, pick, key, cls = 'seg') {
  return h('div', {class: cls}, options.map(o => h('button', {
    class: o === current ? 'on' : '', 'data-k': `${key}-${o}`, onclick: () => pick(o),
  }, o)));
}

const field = (label, control) =>
    h('div', {class: 'field'}, h('span', {}, label), control);

function range(label, key, min, max, step, value) {
  const out = h('output', {}, String(value));
  return h('label', {class: 'field range'}, h('span', {}, label),
    h('input', {
      type: 'range', min, max, step, value, 'data-k': `range-${key}`,
      // Only the preview redraws, so the slider isn't rebuilt under the mouse.
      oninput: e => {
        const v = Number(e.target.value);
        out.textContent = String(v);
        S.cfg = {...S.cfg, [key]: v};
        renderStage();
        scheduleSave();
      },
    }), out);
}

function inspector() {
  const c = S.cfg;
  const placed = new Set([...S.zones.side, ...S.zones.top].map(x => x.type));
  const tray = Object.keys(LABELS).filter(t => REPEATABLE.has(t) || !placed.has(t));
  const tone = swatch();
  const editable = S.appearance.editable;
  const tones = [
    ['Auto', 'linear-gradient(135deg,#f3f1ed 72%,#1c1b19 72%)', '#1d1c1a'],
    ['Paper', '#f3f1ed', '#1d1c1a'], ['Cool', '#eceff3', '#1d1c1a'],
    ['Tinted', `oklch(0.93 0.035 ${c.hues[S.cur]})`, '#1d1c1a'],
    ['Dark', '#1c1b19', '#f1efe9'],
  ];

  refs.tray = h('div', {
    class: 'tray',
    ondragover: e => {
      if (S.drag && S.drag.from !== 'tray') {
        e.preventDefault();
        if (S.drop) {
          S.drop = null;
          paintDrag();
        }
      }
    },
    ondrop: e => {
      e.preventDefault();
      const src = S.drag;
      // Before finishDrag(), whose pending load would renumber the ids.
      if (src && src.from !== 'tray') {
        S.zones = {...S.zones, [src.from]: S.zones[src.from].filter(x => x.id !== src.id)};
        scheduleSave();
      }
      finishDrag();
      render();
    },
  },
    h('div', {class: 'tray-hot'}, 'Drop here to remove'),
    tray.map(t => h('button', {
      class: 'chip', draggable: 'true', 'data-k': `tray-${t}`,
      ondragstart: dragStart({from: 'tray', type: t}), ondragend: endDrag,
      onclick: () => addItem(t),
    },
      s('svg', {width: 8, height: 12, viewBox: '0 0 8 12', fill: '#a19d95'},
        [[2, 2], [6, 2], [2, 6], [6, 6], [2, 10], [6, 10]]
            .map(([cx, cy]) => s('circle', {cx, cy, r: 1}))),
      LABELS[t])),
    !tray.length && h('span', {class: 'tray-empty'}, 'Every module is placed'));

  return h('aside', {class: 'inspector', 'aria-label': 'Customize'},
    h('div', {class: 'section'},
      h('div', {class: 'title-row'}, h('span', {}, 'Customize'), h('span', {}, '⌃⇧E edit / use')),
      seg(S.editing ? 'Edit layout' : 'Use', ['Edit layout', 'Use'], o => {
        S.editing = o === 'Edit layout';
        S.reveal = false;
        render();
      }, 'mode', 'seg mode')),
    h('div', {class: 'section tight'},
      h('div', {class: 'heading'}, h('span', {}, 'Start from')),
      h('div', {class: 'presets'}, ['Quiet', 'Ticks', 'Classic', 'Top bar'].map(name =>
        h('button', {
          'data-k': `preset-${name}`,
          onclick: () => {
            preset(name);
            changed();
          },
        }, name)))),
    h('div', {class: 'section tight'},
      h('div', {class: 'heading'}, h('span', {}, 'Modules'), h('span', {}, 'drag in · click to add')),
      refs.tray),
    h('div', {class: 'section'},
      h('div', {class: 'heading'}, h('span', {}, 'Sidebar')),
      field('Side', seg(c.side, CHOICES.side, o => setCfg({side: o}), 'side')),
      field('Behavior', seg(c.behavior, CHOICES.behavior, o => setCfg({behavior: o}), 'behavior')),
      range('Width', 'width', 180, 320, 4, c.width ?? DEFAULT_WIDTH)),
    h('div', {class: 'section'},
      h('div', {class: 'heading'}, h('span', {}, 'Tabs')),
      field('Icons', seg(c.icon, CHOICES.icon, o => setCfg({icon: o}), 'icon')),
      field('Density', seg(c.density, CHOICES.density, o => setCfg({density: o}), 'density')),
      field('Archive after', seg(c.archive, CHOICES.archive, o => setCfg({archive: o}), 'archive')),
      field('Close ×', seg(c.closeHover ? 'On hover' : 'Never', ['On hover', 'Never'],
                           o => setCfg({closeHover: o === 'On hover'}), 'close'))),
    h('div', {class: 'section'},
      h('div', {class: 'heading'}, h('span', {}, 'Spaces')),
      field('Switcher', seg(c.switcher, CHOICES.switcher, o => setCfg({switcher: o}), 'switcher')),
      field('Edge pill', seg(c.edgePill ? c.pillSide : 'Off', ['Left', 'Right', 'Off'],
                             o => setCfg(o === 'Off' ? {edgePill: false} : {edgePill: true, pillSide: o}),
                             'pill')),
      S.spaces.map((x, i) => field(x.name, h('div', {class: 'hues'}, HUES.map(hue => h('button', {
        class: c.hues[i] === hue ? 'ring' : '', 'aria-label': `${x.name} color`,
        'data-k': `hue-${i}-${hue}`, style: `background:oklch(0.56 0.13 ${hue})`,
        onclick: () => setCfg({hues: c.hues.map((v, j) => j === i ? hue : v)}),
      })))))),
    h('div', {class: 'section'},
      h('div', {class: 'heading'}, h('span', {}, 'Window')),
      field('Tone', h('div', {class: 'tones'}, tones.map(([name, bg, fg]) => h('button', {
        class: tone === name ? 'ring' : '', title: name, disabled: !editable,
        'data-k': `tone-${name}`, style: `background:${bg};color:${fg}`,
        onclick: () => setAppearance(name),
      }, name)))),
      field('', h('span', {class: 'hint'},
                  editable ? 'Tone is this profile’s own.' : 'Set the tone from a normal window.')),
      range('Frame gap', 'gap', 0, 16, 1, c.gap),
      range('Corners', 'radius', 0, 20, 1, c.radius),
      range('Roundness', 'rond', 0, 100, 5, c.rond)),
    h('div', {class: 'footer'},
      h('span', {}, 'Saved automatically'),
      h('button', {
        'data-k': 'reset',
        onclick: () => {
          preset('Quiet');
          S.editing = true;
          changed();
          setAppearance('Auto');
        },
      }, 'Reset')));
}

// --- Rendering -------------------------------------------------------------

function freshRefs() {
  refs = {...refs, lines: {side: [], top: []}, end: {}, items: new Map()};
}

function render() {
  const focused = document.activeElement && document.activeElement.dataset ?
      document.activeElement.dataset.k : undefined;
  freshRefs();
  refs.root = h('div', {class: 'root'}, stage(), inspector());
  refs.root.style.fontVariationSettings = `'ROND' ${S.cfg.rond}`;
  app.replaceChildren(refs.root);
  paintDrag();
  if (focused) {
    const again = app.querySelector(`[data-k="${CSS.escape(focused)}"]`);
    if (again) {
      again.focus();
    }
  }
}

function renderStage() {
  if (!refs.stage) {
    return;
  }
  const old = refs.stage;
  freshRefs();
  old.replaceWith(stage());
  refs.root.style.fontVariationSettings = `'ROND' ${S.cfg.rond}`;
  paintDrag();
}

window.addEventListener('keydown', e => {
  if (e.ctrlKey && e.shiftKey && e.code === 'KeyE') {
    e.preventDefault();
    S.editing = !S.editing;
    S.reveal = false;
    render();
  }
  // No ⌃1–3 for the sample spaces: ⌃1–9 stay Argon's own space shortcuts.
});
darkMedia.addEventListener('change', () => render());

addWebUiListener('argon-state-changed', onState);
sendWithPromise('getState').then(onState);
