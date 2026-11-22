// node --test extension/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import * as S from './spaces.js';

test('spaces and pins', () => {
  const state = { spaces: [] };
  const home = S.addSpace(state, ' Home ');
  assert.equal(home.name, 'Home');
  assert.equal(S.addSpace(state, 'Home'), null, 'names are unique (tab groups are matched by title)');
  assert.equal(S.addSpace(state, '  '), null);
  const work = S.addSpace(state, 'Work');
  assert.notEqual(work.color, home.color);

  assert.equal(S.renameSpace(state, work.id, 'Home'), false);
  assert.ok(S.renameSpace(state, work.id, 'Job'));

  assert.equal(S.neighbour(state, home.id, 1), work);
  assert.equal(S.neighbour(state, home.id, -1), work, 'wraps around');

  const pin = S.addPin(work, 'https://a.example/', 'A');
  assert.equal(S.addPin(work, 'https://a.example/', 'A again'), null);
  S.addPin(home, 'https://a.example/', 'A');
  S.addPin(work, 'https://b.example/', '');
  assert.equal(work.pins[1].title, 'https://b.example/');

  assert.equal(S.removeSpace(state, work.id), home, 'pins go to the space before');
  assert.deepEqual(home.pins.map(p => p.url), ['https://a.example/', 'https://b.example/'], 'without duplicates');
  assert.equal(S.removeSpace(state, home.id), null, 'the last space stays');

  S.removePin(home, home.pins[0].id);
  assert.equal(home.pins.length, 1);
  assert.ok(pin.id);
});

test('kept and loose tabs', () => {
  const space = { pins: [{ id: 'p1', url: 'https://a/' }, { id: 'p2', url: 'https://b/' }, { id: 'p3', url: 'https://c/' }] };
  const tabs = [{ id: 1, url: 'https://x/' }, { id: 2, url: 'https://b/' }, { id: 3, url: 'https://b/' }, { id: 4, url: 'https://a/' }];
  const { kept, loose } = S.splitTabs(space, tabs, { p1: 1 });
  assert.deepEqual(kept.map(k => k.tab?.id), [1, 2, undefined], 'tied tab wins, then first tab at the pin URL');
  assert.deepEqual(loose.map(t => t.id), [3, 4], 'a duplicate stays loose');
});

test('archive timer', () => {
  const h = S.HOUR, now = 100 * h;
  assert.equal(S.timeLeft({ lastAccessed: now - h }, 12 * h, now), 11 * h);
  assert.equal(S.timeLeft({ lastAccessed: now - 20 * h }, 12 * h, now), 0);
  assert.equal(S.timeLeft({ lastAccessed: now, active: true }, 12 * h, now), null);
  assert.equal(S.timeLeft({ lastAccessed: now, audible: true }, 12 * h, now), null);
  assert.equal(S.timeLeft({ lastAccessed: now }, 0, now), null, 'never');
  assert.deepEqual([S.left(90e3), S.left(3 * h + 1), S.left(50 * h)], ['2m', '3h', '2d']);
});

test('command bar URLs', () => {
  assert.equal(S.toUrl('github.com/imputnet'), 'https://github.com/imputnet');
  assert.equal(S.toUrl('localhost:3000'), 'http://localhost:3000');
  assert.equal(S.toUrl('chrome://extensions'), 'chrome://extensions');
  assert.equal(S.toUrl('derivative of arctan'), null);
  assert.equal(S.toUrl('hello'), null);
});
