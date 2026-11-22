// Owns every change to spaces and groups, one at a time, so the panel and tab events can't race into duplicate
// groups. The panel reads storage directly and sends its changes here.
import * as S from './spaces.js';

chrome.sidePanel.setPanelBehavior({ openPanelOnActionClick: true });

let queue = Promise.resolve();
function serial(fn) {
  const run = queue.then(fn);
  queue = run.catch(e => console.error(e));
  return run;
}

async function addToSpace(tabIds, space, windowId) {
  const group = await S.groupOf(space, windowId);
  if (group) return chrome.tabs.group({ tabIds, groupId: group.id });
  const groupId = await chrome.tabs.group({ tabIds, createProperties: { windowId } });
  await chrome.tabGroups.update(groupId, { title: space.name, color: space.color });
}

async function setSession(key, map) {
  await chrome.storage.session.set({ [key]: map });
}

// Shows the space, on tabId if given, else on its last-used tab.
async function switchTo(state, space, windowId, tabId) {
  const { active } = await S.session();
  active[windowId] = space.id;
  await setSession('active', active);
  let tabs = await S.tabsOf(space, windowId);
  if (!tabs.length) {
    tabs = [await chrome.tabs.create({ windowId })];
    await addToSpace([tabs[0].id], space, windowId);
  }
  const target = tabs.find(t => t.id === tabId) ?? tabs.find(t => t.active) ?? tabs.reduce((a, b) => ((b.lastAccessed ?? 0) > (a.lastAccessed ?? 0) ? b : a));
  await chrome.tabs.update(target.id, { active: true });  // first, so collapsing the old group doesn't pick a tab
  const mine = (await chrome.tabs.get(target.id)).groupId;
  for (const g of await chrome.tabGroups.query({ windowId })) {
    const isSpace = state.spaces.some(s => s.name === g.title);
    if (isSpace && g.collapsed !== (g.id !== mine)) await chrome.tabGroups.update(g.id, { collapsed: g.id !== mine });
  }
}

// Pinning a tab ties it to the pin; opening a pin focuses that tab, or a tab of the space already at the pin's
// address, or opens one.
async function openPin(space, pinId, windowId) {
  const pin = space.pins.find(p => p.id === pinId);
  if (!pin) return;
  const { pinTabs } = await S.session();
  let tab = pinTabs[pinId] && (await chrome.tabs.get(pinTabs[pinId]).catch(() => null));
  tab ??= (await S.tabsOf(space, windowId)).find(t => t.url === pin.url && !Object.values(pinTabs).includes(t.id));
  if (!tab) {
    tab = await chrome.tabs.create({ windowId, url: pin.url });
    await addToSpace([tab.id], space, windowId);
  }
  pinTabs[pinId] = tab.id;
  await setSession('pinTabs', pinTabs);
  await chrome.tabs.update(tab.id, { active: true });
  await chrome.windows.update(tab.windowId, { focused: true });
}

const actions = {
  async switch(state, { windowId, spaceId }) {
    const space = state.spaces.find(s => s.id === spaceId);
    if (space) await switchTo(state, space, windowId);
  },
  // A tab from the command bar, maybe in another space.
  async focusTab(state, { windowId, tabId }) {
    const tab = await chrome.tabs.get(tabId);
    const group = tab.groupId === -1 ? null : await chrome.tabGroups.get(tab.groupId);
    const space = state.spaces.find(s => s.name === group?.title);
    if (space) await switchTo(state, space, windowId, tabId);
    else await chrome.tabs.update(tabId, { active: true });
  },
  async step(state, { windowId, step }) {
    const current = await S.activeSpace(state, windowId);
    await switchTo(state, S.neighbour(state, current.id, step), windowId);
  },
  async addSpace(state, { windowId, name }) {
    const space = S.addSpace(state, name);
    if (!space) return;
    await S.save(state);
    await switchTo(state, space, windowId);
  },
  async renameSpace(state, { spaceId, name }) {
    const space = state.spaces.find(s => s.id === spaceId);
    const old = space?.name;
    if (!space || !S.renameSpace(state, spaceId, name)) return;
    await S.save(state);
    for (const g of await chrome.tabGroups.query({ title: old })) {
      if (g.title === old) await chrome.tabGroups.update(g.id, { title: space.name });
    }
  },
  async recolor(state, { spaceId }) {
    const space = state.spaces.find(s => s.id === spaceId);
    if (!space) return;
    S.nextColor(space);
    await S.save(state);
    for (const g of await chrome.tabGroups.query({})) {
      if (g.title === space.name) await chrome.tabGroups.update(g.id, { color: space.color });
    }
  },
  // The space's tabs and pins move to the space before it; nothing is closed.
  async removeSpace(state, { windowId, spaceId }) {
    const gone = state.spaces.find(s => s.id === spaceId);
    const heir = gone && S.removeSpace(state, spaceId);
    if (!heir) return;
    await S.save(state);
    for (const g of await chrome.tabGroups.query({})) {
      if (g.title !== gone.name) continue;
      const tabs = await chrome.tabs.query({ groupId: g.id });
      await addToSpace(tabs.map(t => t.id), heir, g.windowId);
    }
    await switchTo(state, heir, windowId);
  },
  async pin(state, { windowId, tabId }) {
    const tab = await chrome.tabs.get(tabId);
    const space = await S.activeSpace(state, windowId);
    const pin = S.addPin(space, tab.url, tab.title);
    if (!pin) return;
    await S.save(state);
    const { pinTabs } = await S.session();
    pinTabs[pin.id] = tabId;
    await setSession('pinTabs', pinTabs);
  },
  async unpin(state, { windowId, pinId }) {
    S.removePin(await S.activeSpace(state, windowId), pinId);
    await S.save(state);
  },
  async openPin(state, { windowId, pinId }) {
    await openPin(await S.activeSpace(state, windowId), pinId, windowId);
  },
};

chrome.runtime.onMessage.addListener((msg, _sender, reply) => {
  serial(async () => actions[msg.cmd]?.(await S.load(), msg)).then(() => reply(true), () => reply(false));
  return true;
});

chrome.commands.onCommand.addListener(async (command, tab) => {
  if (command === 'command-bar') {
    if (!tab) return;
    // open() needs the shortcut's user gesture, so it goes first, before any await.
    chrome.sidePanel.open({ windowId: tab.windowId });
    return chrome.storage.session.set({ cmdBar: { windowId: tab.windowId, at: Date.now() } });
  }
  const win = await chrome.windows.getLastFocused({ windowTypes: ['normal'] });
  serial(async () => actions.step(await S.load(), { windowId: win.id, step: command === 'next-space' ? 1 : -1 }));
});

// New tabs join the window's active space, unless Chromium already put them in a group (links opened from a
// grouped tab) or they're Chromium-pinned.
chrome.tabs.onCreated.addListener(created => serial(async () => {
  const tab = await chrome.tabs.get(created.id).catch(() => null);
  if (!tab || tab.pinned || tab.groupId !== -1) return;
  const win = await chrome.windows.get(tab.windowId);
  if (win.type !== 'normal') return;
  const state = await S.load();
  await addToSpace([tab.id], await S.activeSpace(state, tab.windowId), tab.windowId);
}));

// First install: the tabs already open become the first space.
chrome.runtime.onInstalled.addListener(({ reason }) => reason === 'install' && serial(async () => {
  const state = await S.load();
  await S.save(state);
  for (const win of await chrome.windows.getAll({ windowTypes: ['normal'], populate: true })) {
    const loose = win.tabs.filter(t => !t.pinned && t.groupId === -1).map(t => t.id);
    if (loose.length) await addToSpace(loose, state.spaces[0], win.id);
  }
}));

// Auto-archive: loose tabs unused for longer than the setting are closed (they stay in History and Recently Closed).
chrome.alarms.create('archive', { periodInMinutes: 5 });
chrome.alarms.onAlarm.addListener(({ name }) => name === 'archive' && serial(async () => {
  const { archiveAfter = S.ARCHIVE_OPTIONS[0] } = await chrome.storage.local.get('archiveAfter');
  if (!archiveAfter) return;
  const state = await S.load();
  const { pinTabs } = await S.session();
  const now = Date.now(), stale = [];
  for (const win of await chrome.windows.getAll({ windowTypes: ['normal'] })) {
    for (const space of state.spaces) {
      const { loose } = S.splitTabs(space, await S.tabsOf(space, win.id), pinTabs);
      stale.push(...loose.filter(t => S.timeLeft(t, archiveAfter, now) === 0).map(t => t.id));
    }
  }
  if (stale.length) await chrome.tabs.remove(stale);
}));
