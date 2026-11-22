// A space is a tab group, one per space in each window, matched by its title. Only the active space's group is
// expanded, and the side panel lists only its tabs. A pin is a saved {url, title} in a space, tied to at most one tab.
//
// chrome.storage.local  {spaces: [{id, name, color, pins: [{id, url, title}]}]}
// chrome.storage.session {active: {windowId: spaceId}, pinTabs: {pinId: tabId}}  (cleared when the browser quits)

export const COLORS = ['blue', 'red', 'yellow', 'green', 'pink', 'purple', 'cyan', 'orange', 'grey'];

// Pure helpers on the stored state (checked by spaces.test.js).

const nameTaken = (state, name, exceptId) => state.spaces.some(s => s.name === name && s.id !== exceptId);

export function addSpace(state, name) {
  name = name.trim();
  if (!name || nameTaken(state, name)) return null;
  const space = { id: crypto.randomUUID(), name, color: COLORS[state.spaces.length % COLORS.length], pins: [] };
  state.spaces.push(space);
  return space;
}

export function renameSpace(state, id, name) {
  name = name.trim();
  const space = state.spaces.find(s => s.id === id);
  if (!space || !name || nameTaken(state, name, id)) return false;
  space.name = name;
  return true;
}

// Removing a space hands its pins to the space that takes its place, so nothing is lost. Returns that space.
export function removeSpace(state, id) {
  const i = state.spaces.findIndex(s => s.id === id);
  if (i < 0 || state.spaces.length < 2) return null;
  const [gone] = state.spaces.splice(i, 1);
  const heir = state.spaces[Math.max(0, i - 1)];
  for (const pin of gone.pins) if (!heir.pins.some(p => p.url === pin.url)) heir.pins.push(pin);
  return heir;
}

export function nextColor(space) {
  space.color = COLORS[(COLORS.indexOf(space.color) + 1) % COLORS.length];
}

export function addPin(space, url, title) {
  if (!url || space.pins.some(p => p.url === url)) return null;
  const pin = { id: crypto.randomUUID(), url, title: title || url };
  space.pins.push(pin);
  return pin;
}

export function removePin(space, pinId) {
  space.pins = space.pins.filter(p => p.id !== pinId);
}

export function neighbour(state, id, step) {
  const n = state.spaces.length;
  const i = state.spaces.findIndex(s => s.id === id);
  return state.spaces[(((i + step) % n) + n) % n];
}

// Storage and tab-group access.

export async function load() {
  const { spaces = [] } = await chrome.storage.local.get('spaces');
  const state = { spaces };
  if (!spaces.length) addSpace(state, 'Home');
  return state;
}

export const save = state => chrome.storage.local.set({ spaces: state.spaces });

export async function session() {
  const { active = {}, pinTabs = {} } = await chrome.storage.session.get(['active', 'pinTabs']);
  return { active, pinTabs };
}

export async function activeSpace(state, windowId) {
  const { active } = await session();
  const chosen = state.spaces.find(s => s.id === active[windowId]);
  if (chosen) return chosen;
  // After a restart: whichever space's group Chromium restored expanded.
  const open = await chrome.tabGroups.query({ windowId, collapsed: false });
  return state.spaces.find(s => open.some(g => g.title === s.name)) ?? state.spaces[0];
}

export async function groupOf(space, windowId) {
  const groups = await chrome.tabGroups.query({ windowId });
  return groups.find(g => g.title === space.name);
}

export async function tabsOf(space, windowId) {
  const group = await groupOf(space, windowId);
  return group ? chrome.tabs.query({ groupId: group.id }) : [];
}

// A pin's tab is the one it's tied to this session, else an untied tab of the space at the pin's address (so pins
// survive a restart, when storage.session is empty). Everything else in the space is loose.
export function splitTabs(space, tabs, pinTabs) {
  const free = new Set(tabs);
  const kept = space.pins.map(pin => {
    let tab = tabs.find(t => t.id === pinTabs[pin.id]);
    tab ??= [...free].find(t => t.url === pin.url);
    if (tab) free.delete(tab);
    return { pin, tab };
  });
  return { kept, loose: tabs.filter(t => free.has(t)) };
}

export const HOUR = 3600e3;
export const ARCHIVE_OPTIONS = [12 * HOUR, 24 * HOUR, 7 * 24 * HOUR, 0];  // 0: never
export const archiveLabel = ms => (ms ? left(ms) : 'Never');

// Time until a loose tab is archived, or null if it never is (active, playing audio, or archiving off).
export function timeLeft(tab, after, now) {
  if (!after || tab.active || tab.audible || !tab.lastAccessed) return null;
  return Math.max(0, after - (now - tab.lastAccessed));
}

export function left(ms) {
  if (ms < HOUR) return `${Math.max(1, Math.ceil(ms / 60e3))}m`;
  if (ms < 48 * HOUR) return `${Math.floor(ms / HOUR)}h`;
  return `${Math.floor(ms / (24 * HOUR))}d`;
}

// What the command bar opens for a query: a URL if it looks like one, else null (search the web).
export function toUrl(q) {
  q = q.trim();
  if (/\s/.test(q)) return null;
  if (/^(https?|chrome|about|file):/i.test(q)) return q;
  if (/^(localhost|[\w-]+(\.[\w-]+)*\.[a-z]{2,}|\d+(\.\d+){3})(:\d+)?([/?#].*)?$/i.test(q)) return `${/^(localhost|\d)/i.test(q) ? 'http' : 'https'}://${q}`;
  return null;
}
