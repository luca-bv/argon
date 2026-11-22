# Handoff: Argon browser UI

## Overview
Argon is a continuation of the Arc browser (which is being discontinued). It keeps Arc's full feature set — vertical tabs, spaces, pinned vs. temporary tabs, auto‑archive, favorites, command bar, Peek — with a quieter, more minimal chrome inspired by Zen. This bundle covers:

1. **Argon Prototype** — the two chosen layouts ("Quiet sidebar" and "Ticks") as a working prototype, switchable with ⌃⇧L.
2. **Argon Customize** — a drag‑and‑drop layout editor where nearly every part of the chrome can be rearranged and restyled.

## About the design files
The files in this bundle are **design references created in HTML** — prototypes showing intended look and behavior, not production code. Recreate them in the target environment (for a Chromium‑based browser: the native browser UI layer, e.g. Views/C++ on Chromium, or a WebUI/React side panel if that's the chosen architecture) using its established patterns. Do not ship the HTML.

Files are "Design Components" (`.dc.html`): a template with `{{ }}` holes plus a `class Component` logic block at the bottom. Read the logic class for behavior; the template for exact styles (all inline). Open them directly in a browser to interact (they load `support.js`).

## Fidelity
**High‑fidelity.** Colors, type, spacing, radii and interactions are final intent. Placeholders: page content (grey bars), favicons (letter tiles), favorites tiles.

---

## Screens / views

### 1. Window shell (both layouts)
- Window background ("frame"): `#f3f1ed` (Paper tone). Page sits on it as a white card.
- Traffic lights: 12px circles `#ff5f57 #febc2e #28c840`, gap 7px, at left 16 / top 16.
- Page card: background `#fff`, radius 10px, shadow `0 0 0 1px rgba(29,28,26,.06)`, inset from frame by 8px (frame gap) on all sides except the sidebar side.
- Page stand‑in: centered column max‑width 620–660px, padding 72–88px top / 40px sides. Host line 13px `#8a857c` with 8px accent dot; H1 42–44px / weight 650 / line‑height 1.05 / letter‑spacing −0.015em.

### 2. Quiet sidebar layout (Prototype "Quiet sidebar", Customize preset "Quiet")
- Sidebar width **224px**, transparent on frame. Padding: 46–52px top (clears traffic lights), 14px right, 18px bottom, 26–28px left (room for edge pill).
- Vertical order, gap ~14px, separated by **1px dividers** `rgba(29,28,26,.1)`:
  1. Space name — 12px / 600 / `#8a857c`
  2. Pinned tabs ("kept")
  3. Loose tabs (temporary) + "+ New" row + "Clear" button (only when loose tabs exist)
- **Tab row**: height 30px, radius 7px, padding 0 8px, gap 9px, 13px text.
  - Inactive: text `#5c5953`, weight 400, transparent.
  - Hover: bg `rgba(29,28,26,.05)`, text `#1d1c1a`.
  - Active: bg `rgba(29,28,26,.06–.07)`, text `#1d1c1a`, weight 600.
  - Favicon: 16×16, radius 5px, white 9.5px/700 letter on `oklch(0.55 0.12 H)` where H = hash of host (stand‑in for real favicons).
  - Title truncates with ellipsis.
  - Loose tabs only: right side shows **archive timer** (11px, `#8a857c`; amber `oklch(0.55 0.14 55)` when < 2h left). On hover it's replaced by a 22–24px **×** close button.
- **Edge pill (space switcher)**: on the window's left edge, vertically centered.
  - Collapsed: 6×56px pill `rgba(29,28,26,.14)`, contains one 4px dot per space; current space dot is 14px tall, accent colored.
  - Hover → expands into white pill (padding 5px, radius 999, shadow `0 0 0 1px rgba(29,28,26,.08), 0 10px 30px rgba(29,28,26,.16)`) with 34–36px circular icon buttons; current = accent bg + white icon. Tooltip "Name · ⌃N".

### 3. Ticks layout (Prototype "Ticks", Customize preset "Ticks")
- No sidebar in flow — the page goes full‑bleed (no frame gap, no radius in the prototype).
- Left edge: a column of 2px horizontal ticks, one per tab (gap 7px, an 8px extra gap between pinned and loose groups). Inactive 10px wide `rgba(29,28,26,.3)`; active 18px wide in space accent.
- Hovering the edge reveals a floating tab panel: 240px, padding 10px, radius 14px, bg `rgba(255,255,255,.98)`, shadow `0 0 0 1px rgba(29,28,26,.07), 0 18px 50px rgba(29,28,26,.16)`. Contents match the Quiet sidebar (space name, dividers, pinned, loose, + New) plus a footer row of space icons (30px) and Clear. Slide transition 220ms `cubic-bezier(.2,.8,.2,1)`.
- Mouse‑leave closes it.

### 4. Command bar (Prototype, ⌘K)
- Scrim `rgba(29,28,26,.22)`. Panel: width min(600px, 92%), 14vh from top, radius 16px, shadow `0 24px 70px rgba(0,0,0,.28)`.
- Input: 18px, padding 18px 20px, bottom border `#efede9`. Placeholder "Search tabs, type a URL, or run a command".
- Results: 40px rows, radius 9px, 8px colored dot (space accent) + title 14px + meta 12px `#8a857c` ("Switch to tab" / space name / shortcut). Selected row bg `rgba(29,28,26,.06)`.
- Footer hint bar `#faf9f7`: "↑↓ move · ↵ open · esc close", 11.5px.
- Result order: matching tabs across all spaces (max 6) → "Open <url>" / "Search the web for “q”" → commands (switch layout, clear loose tabs, go to space N).

### 5. Toast / undo
- Bottom‑center, `#1d1c1a` bg, `#f3f1ed` text 13px, radius 12px. Undo button `rgba(255,255,255,.14)`. Shown on close tab / clear loose; 6s with undo, 1.4s for info.

### 6. Customize page
- Left: live browser window (radius 12, shadow `0 0 0 1px rgba(0,0,0,.08), 0 20px 60px rgba(40,30,10,.18)`) on canvas `#e4e1db`. Status line above: mode title + hint.
- Right: 320px inspector, bg `#faf9f7`, sections with 11px/600 `#8a857c` headings:
  - **Mode**: Edit layout / Use (⌃⇧E).
  - **Start from** presets: Quiet, Ticks, Classic, Top bar.
  - **Modules tray**: draggable chips (grip icon + label). Becomes red‑tinted "Drop here to remove" while dragging a placed module.
  - **Sidebar**: Side (Left/Right), Behavior (Pinned / Auto‑hide / Off), Width 180–320.
  - **Tabs**: Icons (Tile/Dot/None), Density (Compact 26px/12.5px, Regular 30px/13px, Roomy 36px/14px), Archive after (12h/24h/7d/Never), Close × (On hover/Never).
  - **Spaces**: Switcher (Icons/Dots/Names), Edge pill (Left/Right/Off), per‑space color from 6 hues (250, 300, 155, 30, 75, 200 → `oklch(0.56 0.13 H)`).
  - **Window**: Tone (Paper/Cool/Tinted/Dark), Frame gap 0–16, Corners 0–20, Allo roundness 0–100.
  - Footer: "Saved automatically" + Reset.
- Segmented controls: track `#ece9e3` radius 8 padding 2; selected `#fff` + `0 1px 2px rgba(0,0,0,.12)`.

#### Modules (draggable units)
`spaceName, nav (back/forward/reload), address, favorites (4‑col grid), pinned, loose, newTab, clear, spaces (switcher), archive (count), divider, spacer`. Divider and spacer can be placed multiple times; others once.
- Two drop zones: **Sidebar** (vertical) and **Top bar** (horizontal, 40px, appears when non‑empty or in Edit mode). Modules adapt orientation (tabs become chips max 170px; divider becomes vertical 18px; favorites become a row of 30px tiles; address 240px).
- Edit mode: each module has a 1px dashed outline, grab cursor, black 16px remove badge top‑right; interactions inside modules are disabled. Drop indicator: 2px `#d4572a` line before/after the hovered module based on pointer midpoint.
- Presets (zone contents):
  - Quiet: side `[spaceName, divider, pinned, divider, loose, newTab]`, edge pill left.
  - Ticks: side `[…Quiet, spacer, spaces]`, behavior Auto‑hide, width 248.
  - Classic (Arc‑like): side `[nav, address, favorites, spaceName, pinned, divider, newTab, loose, spacer, archive, spaces]`, width 264, Dots switcher.
  - Top bar: side `[]`, top `[nav, pinned, divider, loose, newTab, spacer, address, spaces]`, sidebar Off.

---

## Interactions & behavior
- **Spaces**: ⌃1–3 switches; each space remembers its own active tab. Switching changes accent (and frame tint when Tone = Tinted).
- **Tabs**: click activates; ⌃↑/⌃↓ cycles within space; ⌥W closes active loose tab. Closing selects the next loose tab, else previous, else last pinned. Pinned tabs can't be closed from the row.
- **Auto‑archive**: loose tabs idle longer than the threshold (default 12h) move to Archive; archive count increments. Timer shows remaining time (`Nm` < 1h, `Nh` < 48h, else `Nd`).
- **Clear**: archives all loose tabs in the current space (undoable).
- **New tab**: opens command bar (Prototype) / inserts a "New tab" at top of loose (Customize).
- **Layout switch**: ⌃⇧L (Prototype). Avoid ⌘T/⌘W/⌘L/⌘\ conflicts during prototyping; in the real browser map New Tab to ⌘T and the command bar to ⌘T/⌘L as in Arc.
- **Customize persistence**: `{ zones, cfg }` saved to localStorage key `argon-customize-v1`; in the product, persist to the browser profile prefs.

## State
- `spaces[]: { name, hue, icon, kept: Tab[], loose: Tab[] }`, `Tab: { id, title, host, lastActiveAt }`
- `currentSpace`, `activeTabBySpace{}`, `archived[]`
- UI: `layout ('quiet'|'ticks')`, `commandOpen`, `query`, `selectedIndex`, `pillOpen`, `ticksOpen`, `toast{text, undo}`
- Customize: `zones: { side: Module[], top: Module[] }`, `Module: { id, type }`, `cfg: { side, behavior, width, density, icon, closeHover, archive, switcher, edgePill, pillSide, hues[], tone, gap, radius, rond }`

## Design tokens
- Ink `#1d1c1a` · secondary `#5c5953` · muted `#8a857c` · faint `#a19d95`
- Frame Paper `#f3f1ed` · Cool `#eceff3` · Tinted `oklch(0.93 0.035 H)` · Dark `#1c1b19` (ink `#f1efe9`, page `#272623`)
- Page `#fff` · skeleton bars `#f1efeb` · inspector `#faf9f7` · segmented track `#ece9e3` · canvas `#e4e1db`
- Lines `rgba(29,28,26,.1)` · hover `rgba(29,28,26,.04–.05)` · active `rgba(29,28,26,.06–.07)` · field `rgba(29,28,26,.05)`
- Space accents `oklch(0.56 0.13 H)` (dark mode L=0.68): Personal 250, Argon 300, School 155
- Warning (archive soon) `oklch(0.55 0.14 55)` · drop indicator `#d4572a`
- Radii: 5 (favicon) · 6–7 (rows, small buttons) · 9 (fields, results) · 10 (page) · 12 (window, toast) · 14 (floating panels) · 16 (command bar) · 999 (pills)
- Spacing: 4/6/8/10/14/22px; row heights 26/30/36
- Type: **Allo** (variable, open source — github.com/joshpuckett/allo) with `font-feature-settings: 'ss01','tnum'` (single‑storey a, tabular figures) and `font-variation-settings: 'ROND' 100` (fully rounded). Sizes 11 / 12 / 12.5 / 13 / 14 / 18 / 42–44px; weights 400 / 600 / 650.

## Assets
- Font: Allo variable WOFF2 (loaded from jsDelivr in the mocks — bundle it locally in the product).
- Icons: simple inline 16px stroke SVGs (1.5–1.6 stroke) for back/forward/reload, lock, archive, close, and space icons (house, code brackets, book). Replace with the product's icon set.
- Favicons and favorites are letter tiles — replace with real site favicons.

## Files
- `Argon Prototype.dc.html` — Quiet sidebar + Ticks, command bar, undo, shortcuts.
- `Argon Customize.dc.html` — drag‑and‑drop layout editor.
- `ArgonModule.dc.html` — renderer for each draggable module (used by Customize).
- `Argon Directions.dc.html` — exploration history (rounds 1–5) for context.
- `support.js` — runtime needed to open the `.dc.html` files in a browser.
