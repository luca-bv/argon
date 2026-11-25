# Argon development notes

Decisions, patch checks and plans behind [the README](../README.md). Dated entries are snapshots.

## Scope

- An Arc continuation, eventually for other people, not just personal use.
- Its own interface, written natively in C++ (Chromium Views) as `patches/argon/ui/*`, from the design in
  `design_handoff_argon_browser/`. Customizable later.
- A robust command bar.
- Lighter than stock Helium where possible (tab-policy patches), with Helium's privacy changes plus more.
- Rented Macs kept to a minimum: everything that can be done on GitHub's free runners is.

## Where things stand (2026-11-25)

Plan, in the user's order: finish every patch except Customize (done), get a clean build and have the user test
it, then Customize (layout and settings in profile prefs, then the Customize page, the Ticks layout, custom space
colors), then Peek and Little Arc. Rebranding, signing and notarization, and update delivery come before anyone
else uses Argon.

- Last user-tested build: run 45532163822 (spaces, Allo in the sidebar, command bar, auto-archive), installed with
  the test profile. The user's reports from it were fixed after: saved-group chips in the bookmarks bar (now hidden
  with the toolbar), no rounding on the collapsed sidebar (its top corner is now rounded). Their "spaces aren't
  compartmentalized" meant separate logins, which became profile-per-space.
- Built but never run: run 45537464022 (no toolbar, navigation row, ⌃1–9, colors, undo toast), 45543347571
  (bookmarks bar, collapsed corner, cross-profile switcher), 45549245685 (profile menu and moves, restore session,
  color menu, command bar commands, F6 focus, per-space pins, tab row menu, Arc import, Show/Hide toolbar, Not
  secure, 10px corners). Expect bugs: all of it was reviewed by reading, none of it tried.
- Installed for the user (2026-11-25): run 45549245685, with the test profile; waiting on their reports.
- Run 45564285997 (edge pill, eight Helium cleanups, Allo as the default UI font) failed in `swap-patches.sh`:
  `helium-noise-page-info`'s `page_info_strings.grdp` hunk used `</grit-part>` as context, which Helium's name
  substitution leaves without a final newline in a resumed tree (issue #5). The hunk now inserts before the
  enterprise strings; rerun after that.
- Customize started (2026-11-25, the user lifted "wait until tested"), stage 1 first. Decided: layout and
  behaviour settings are global (Local State), colors (tone) are per profile. Assumed, not confirmed: each profile's
  tone defaults to Auto (Paper or Dark, following macOS).
- Customize stage 1 written, reviewed by Fable, not built: `argon_prefs.*` (Local State dict `argon.customize` =
  the handoff's `{zones, cfg}`, defaults in code; profile string `argon.tone`). Wired: side module order, density,
  close ×, tab icons (Tile/Dot/None), archive after, edge pill on/off, frame gap, corners, tone Paper/Cool (Dark and
  Auto are Chromium's per-profile `kBrowserColorScheme`). Stored only: top zone, side, behavior, switcher, pill side,
  roundness, Tinted (drawn as Paper). Cool rides on `ColorProviderKey::app_controller` (null for normal windows),
  set in `ThemeService::GetColorProviderKey`. Hooks: new `argon/core/prefs.patch`, more in `colors.patch` and
  `page-card.patch`. Width stays upstream's (240, `kVerticalTabsUncollapsedWidth`); 224 would only need that pref.
- Still to do alongside: go through what the user sees in the installed build and fix it. Proposed, not yet
  decided: the handoff's exact dark tokens in `argon_palette.cc` (`Argon Customize.dc.html`, `tone()`).

## Base (patch check, 2026-11-20)

Applied in order with Helium's own flags (`patch -p1 --ignore-whitespace --fuzz=0`) to Helium 0.22.1.1
(Chromium 158.0.8127.36): the core series, helium-macos's series, then Argon's. Text only; nothing compiled.
Done without a Chromium checkout: the 2 GB source archive was streamed and only the 1,796 files the patches touch
were kept (~50 MB), plus Helium's separate uBlock and Sparkle downloads.

| Patch list | Applies |
|---|---|
| Full Helium + Argon's 3 memory patches | 388 / 388 |
| Without all 107 `helium/ui/*` patches | 278 / 281 |
| Curated: without the 59 that draw the window frame, toolbar, tab strip, address bar, app menu and theme | 323 / 327 |

**Interface base: stock Chromium** (2026-11-21, replacing the curated plan). `ci/assemble.sh` drops every
`helium/ui/*` patch, plus the six that need them or stop applying without them: `helium/core/custom-keyboard-shortcuts`
and `helium/settings/custom-keyboard-shortcuts-page` (built on Helium's layout commands), the
`helium/macos/{helium,disable-chromium}-immersive-fullscreen` pair (Chromium's own fullscreen comes back),
`helium/macos/native-frame-materials` (needs a file a UI patch creates) and `helium/macos/move-macos-settings` (no
longer applies). It also drops `helium/settings/sync-setup-contract` (adds an unused mojom to the shortcuts page's
mojom target) and trims `helium/core/force-paste-action`'s change to `browser_shortcut_metadata.cc` (a file only
the shortcuts patch creates; force paste itself stays). On Helium 0.22.2 / helium-macos 0.22.2.1 that keeps 279 of
394 (90 ungoogled base, 169 core and settings, 20 macOS). Argon's patches then target upstream Chromium code, which
moves less than Helium's UI, and the sidebar applies as is.

Replay check (2026-11-22): the assembled series applied in order with Helium's flags to the 1,208 files it touches
from the Chromium 158.0.8127.36 tarball: everything applies except the three patches on uBlock's and Sparkle's
separate downloads (not fetched; no dropped patch touches them). The first CI run had failed at force-paste-action,
which a symbol scan and a replay of only the macOS patches' files both missed: always replay the whole series.
Not compiled yet.

What goes with it, besides Helium's look: on-screen controls for features that stay underneath (pins sidebar, bangs
and translate UI, uBlock entry in Settings, noise indicator in page info, crash dialog), and cleanups in the UI block.
Brought back as Argon patches, unchanged copies in `patches/argon/helium/` (2026-11-25; Argon patches so resuming
still works; all eight dry-run cleanly on the assembled tree and touch no file Argon's patches do):
`remove-google-promotional-resources` (first-run intro, Gemini spark, sign-in promos), `remove-devtools-annoyances`,
`profile-picker-cleanup`, `profile-customization-cleanup`, `remove-dead-profile-actions`, `licenses-in-credits`,
`ublock-show-in-settings`, `helium-noise-page-info`. `helium/macos/open-shortcuts-settings-action` stays but is dead
code without the shortcuts page.

Status: see "Where things stand" above.

## How it builds

`.github/workflows/build.yml`, every 6 hours or by hand with a tag:

1. Finds the latest `imputnet/helium-macos` release. If branch `build/<tag>` exists, stops.
2. `ci/assemble.sh` clones that tag, applies `ci/helium-macos.patch` (skip signing keychain and Sparkle deltas
   when there's no certificate), drops Helium's UI patches (see Base; it fails if one of the named patches was
   renamed), appends `patches/series` to Helium's, and drops Helium's workflow files. The result is pushed as orphan
   branch `build/<tag>`, so every build is a browsable tree. Helium's core (`helium-chromium`) is committed as plain
   files, not a submodule, so its edited series survives the push (~12 MB).
3. Runs Helium's chained macOS build from that branch (10 jobs on `macos-latest`, free for public repos), arm64 only.
   `build-phase.yml` is Helium's, copied with one change: it checks out `build/<tag>`.
4. Uploads the dmg to a **draft** release `argon-<tag>` (drafts are only visible to collaborators, so the still
   Helium-branded app isn't published).
5. Any failure (a patch that no longer applies, a compile error) opens an issue.

Rebuild a tag: Actions > Build Argon > Run workflow, with the tag.

Resuming: a failed build still uploads its build directory (`ci/helium-macos.patch` makes do-build's upload steps
run after a failure). A rerun looks for the newest finished run (failed or stopped early) whose assembled tree differs only in
`patches/argon/` (`find-resume` compares a hash of everything else, stored in the `argon_state` artifact). If it
finds one, it downloads that run's build directory instead of preparing the source, and `ci/swap-patches.sh` reverts
the old Argon patches and applies the new ones. Files that come out unchanged keep their timestamps, so only the
fixed files recompile. Tick "fresh" to build from scratch. Artifacts expire after 90 days.
The phases after build_01 need `!failure() && !cancelled()` in their `if`: without it GitHub skips a job when any
job upstream of it was skipped (find-resume on a fresh run, retrieve-resources on a resumed one).

Phase handoff (2026-11-23): GitHub's free macOS runner (3 cores) needs ~17 h for a full build, so it takes 4 phases;
upstream Helium builds on Depot runners in one ~4 h job and never resumes. Helium's build archive was plain ustar,
which rounds file times to whole seconds, and siso then found every object older than its deps log
(`siso_explain`: "stale deps in depslog: output mtime older than log") and rebuilt everything in each phase.
`ci/helium-macos.patch` makes it `tar --format=pax`. `siso_explain` in `out/Default` says why each step ran.

For now ad-hoc signed, no notarization, no Sparkle update server; builds are downloaded from draft Releases.
Before anyone else uses it: Developer ID signing and notarization, Sparkle updates, rebranding (Helium's name and
icons are non-free), and its own or permitted use of Helium's services (updates, extension proxy, filter lists).

Developing a patch needs a Mac with 32 GB+ RAM and 200 GB+ free disk (rented while working on a feature);
CI is only for release builds.

## Developing on a rented EC2 Mac

- Instance: `mac-m4pro.metal` (48 GB, about $2/h in us-east-1, billed per Dedicated Host with a 24-hour minimum;
  `mac-m4.metal` has 24 GB, too little for linking). Give it a 300 GB EBS root volume; `dev/setup.sh` prints the
  APFS resize commands if the disk wasn't grown.
- Install Xcode 26 (full app, needs your Apple ID) and open it once.
- `git clone` this repo, then inside `tmux`: `dev/setup.sh` (installs brew deps, assembles `~/argon-tree` from the
  latest Helium tag plus Argon's patches, `he setup`, first `he build`; hours).
- Loop, in `~/argon-tree` after `source dev.sh`: `cd build/src && quilt new argon/ui/<name>.patch`, `quilt add`
  files, edit, `he build && he run`, `quilt refresh`. Patch names must start with `argon/`. New files of Argon's own
  go in `argon/src.patch` instead (`quilt pop` to it, `quilt add`, `quilt refresh`, `quilt push -a`).
- Seeing the UI: enable Screen Sharing on the instance and connect over an SSH tunnel (port 5900).
- Saving work: `he unmerge`, then `dev/pull-patches.sh` copies `patches/argon/` and the series back into this
  repo and unpacks `argon/src.patch` into `src/`; commit from here. Stop before releasing the host and check nothing is left only on the instance.

## Arc features

UI target: `design_handoff_argon_browser/` (open the `.dc.html` files in a browser).
Kept from Helium: an Arc importer (pinned tabs become bookmark folders). Helium's vertical tabs restyle, compact
toolbar, minimal address bar, split view and zen mode are UI patches, so they're gone; Chromium's own vertical tabs
remain.

### Spaces: built on Chromium's Tab Groups Focusing

Chromium 157/158 already ships most of Spaces behind `features::kTabGroupsFocusing` (off by default, Helium leaves
it off): `TabStripModel::EnterFocusMode(group)`, a vertical layout that hides every unpinned tab outside the
focused group, a window re-tint from the group color, session restore of groups and the focused group, and
next/previous-group commands. So:

- a space is a tab group, the active space is the focused group, and focus never exits;
- Favorites are Chromium pinned tabs (shared by all spaces, shown as the icon grid);
- an Arc pin is a tab in the space's group with tab extra data `argon_pin_url` (persists through session restore);
  closing it resets it to the pinned URL instead of closing it.

Rejected: one `TabStripModel` per space (breaks one-model-per-window assumptions everywhere) and a new per-tab
space property (re-implements filtering and persistence that focus mode already has).

Stages, one patch each:

- [x] 1. Switchable spaces: `ArgonSpaces` (`src/chrome/browser/ui/views/argon/argon_spaces.*`, owned by the
      sidebar) keeps every unpinned tab in a group and one group focused ("Home" on first run). Rather than
      patching the ~15 `ExitFocusMode` call sites, it observes focus changes and, a task later, re-focuses the
      active tab's group (or the last space), and moves ungrouped tabs into it. Hook `argon/ui/spaces.patch`
      turns on `kTabGroupsFocusing` and drops the "focus exited" toast. Switcher: a row of first-letter
      buttons and "+" at the bottom of the sidebar (`ArgonModule::kSpaces`), hidden while collapsed, and ⌃1–9
      (sidebar accelerators). Not yet: renaming from Argon's UI (the group editor works), spaces in horizontal-tabs mode.
- [ ] 2. Pins: Helium's `helium/core/pins/*` already gives Chromium-pinned tabs Arc's pin behavior (pinned URL,
      hibernate instead of close, synced across windows, other-origin links open outside), and the sidebar shows them
      in every space. Decided (2026-11-25): add Arc's per-space pins too (above the space's loose tabs, never
      archived, reset to their URL on close); global pins stay as Favorites. The Arc import maps Arc's pins to them.
      - [x] Step 1 (`argon_pins.*`, hooks `argon/ui/space-pins.patch`): the pinned URL is WebContents user data,
        saved as tab extra data `argon_pin_url` (session restore, session rebuilds and reopened windows are
        hooked next to Helium's own pin hooks); copied across discards (`kReplaced`). A user close (⌘W, ×) takes
        a pin back to its URL, or hands activation to the space's last used tab. Pins come first in their group
        (`Enforce`), get their own sidebar section (`kSpacePins`), never archive, are skipped by Clear, and move
        with their space between profiles. ⌘D (taken from Bookmark This Tab) and the command bar pin/unpin.
      - [x] Step 2 (`argon_tab_menu.*`): right-click a tab row: pin in this space / unpin, go back to the pinned
        page, pin this page instead, add to or remove from Favorites, close.
      - [x] Step 3 (`argon_arc_import.*`, command bar "Import from Arc"): see "Arc import" below.
- [ ] 3. Custom colors and gradients: with Customize (its settings hold per-space hues; decided 2026-11-25).
      Done so far: a space's accent is its group's color (set in the group editor); the current space's button is
      filled with it, the others drawn in it.
- [x] 4. Profile per space (Arc's model: spaces share the default profile; any space can get its own from its
      button's menu). A profile-bound space lives in that profile's window. `ArgonWindowSet`
      (`argon_window_set.*`): the first normal window of each profile, stacked at one frame with one shown; the
      sidebar lists all their spaces and switching swaps windows (show the target, then hide the old). Nothing
      stored. Decided (2026-11-25): restore the last session by default; moving a space reopens its tabs in the
      other profile with an undo toast; a new profile copies Helium's services consent.
      - [x] The space name at the top of the sidebar adds the profile ("Work · School") once other profiles' windows
        are in the set.
      - [x] Step 1: cross-window switcher and ⌃1–9, the activation rule that keeps one member shown, closing the
        hidden ones with the shown one, no swap out of fullscreen.
      - [x] Steps 2–3 (`argon_space_menu.*`): right-click a space: its profile (checked) and the others, "New
        profile for this space" (named after it, skips Helium's setup, copies the services consent). Picking one
        reopens the space's tabs in that profile's window (`profiles::SwitchToProfile`), closes the originals,
        and shows a toast whose Undo moves it back. Refused while a page would ask "Leave site?". The menu's
        Color submenu sets the space's accent.
      - [x] Step 4: "continue where you left off" by default (`argon/core/restore-session.patch`); hidden windows
        are open at quit, so session restore brings every profile's window back and the activation rule stacks
        them. Sidebar width and collapse follow the window on screen. Not done: reopening profiles whose window
        was closed before quitting.
- [x] 5. Auto-archive: `ArgonSpaces` checks every minute and closes unpinned, inactive, silent tabs unused for 12 h
      (`kArchiveAfter`, from `WebContents::GetLastActiveTime`); they land in Recently Closed/History. Not yet: the
      12 h / 24 h / 7 d / never setting, an Archive list.
- [x] Arc import that keeps spaces and pinned tabs, in the browser process (command bar "Import from Arc"; reads
      `~/Library/Application Support/Arc/StorableSidebar.json`, format as in Helium's `add-arc-importer.patch`).
      Decided (2026-11-25), copying Arc throughout profiles and spaces: each Arc profile maps to an Argon profile
      of the same name (created or reused) holding its spaces and Favorites; pinned folders are flattened into the
      space's pins; closing a pin works as in Arc (back to its URL, then away); ⌘D pins/unpins in the space.
      Built: read and parsed on the thread pool; Arc profile names from Arc's `User Data/Local State`; tabs added
      unloaded (Helium's `chrome::AddDeferredTab`); space colors mapped to the nearest group color; spaces whose name
      exists are skipped. Not imported: logins, history, folder names, Little Arc/Peek state.
- [ ] Peek and Little Arc (after Customize, decided 2026-11-25)

First thing on the EC2 Mac, before any code: run the first Argon build (stock Chromium interface) with
`--enable-features=TabGroupsFocusing,NonGroupFocus,TabGroupRibbon` and vertical tabs, and check that focusing a group
hides the others in the vertical tab strip, the window re-tints, and focus survives a relaunch. Then: which actions
still exit focus (Cmd+T, link in new tab, drag, closing the group's last tab, extension `tabs.create`); is the saved-group GUID stable across restarts; do
spaces come back after closing the last window; does focus freezing fight the live-tab budget.

`extension/` is a stopgap for stock Helium until stage 1 builds, then gets deleted; it is not the interface.
Its side panel mimics the Quiet sidebar (kept and loose tabs, archive timers, Clear with undo, edge-pill switcher,
⌘K command bar) over spaces as tab groups collapsed per space. Load it from `chrome://extensions` → Developer
mode → Load unpacked; `node --test extension/` checks its logic. Behaviors to carry over: unique space names,
deleting a space hands its tabs and pins to the previous one, pins reset to their URL, tabs open at first run
become "Home", loose tabs close after 12 h unused (setting: 12 h / 24 h / 7 d / never), Clear keeps one tab so the
window stays open.

## Interface

Native Views, aimed at the Customize design: a layout is zones of modules plus settings, and each zone draws its
modules in order. Argon's own code is plain files in `src/` (laid out like the Chromium tree); `ci/assemble.sh`
turns them into `argon/src.patch` (`ci/src-patch.sh`), applied before Argon's other patches. Those other patches
are only hooks into Chromium's files, kept small and one per area, since they're what breaks on a Chromium update.
Compiled only by CI; Argon has never been built locally.

- [x] Sidebar zone (`src/chrome/browser/ui/views/argon/`): `ArgonSidebarView` draws `kQuietSidebar`
      (`argon_layout.h`: space name, divider, kept tabs, divider, loose tabs, "+ New" with "Clear"); collapsed, rows
      show favicons only. Colors are `ArgonPalette` tokens derived from the frame color. Hook:
      `argon/ui/sidebar.patch` puts it in place of the tab list inside upstream's `VerticalTabStripRegionView`,
      which keeps width, collapse, resize and the window controls. Until Spaces and pins: the space is the focused
      group ("Home" if none), kept tabs are Chromium-pinned tabs.
- [x] Keyboard: F6 (pane focus) starts on the sidebar's active row (`argon/ui/sidebar-focus.patch`), and rows
      show a rounded focus ring.
- [x] Tab strip defaults (`argon/ui/tab-strip-defaults.patch`): vertical tabs on (the sidebar only exists in
      vertical mode), and the tab-search and tab-groups buttons unpinned from the sidebar header (they can be
      pinned back from their context menus). Only affects profiles that never set them.
- [x] Paper and Dark tones (`src/chrome/browser/ui/color/argon_color_mixer.cc`, hook `argon/ui/colors.patch`):
      frame `#f3f1ed` / `#1c1b19`, toolbar in the page color, so toolbar and page read as one card. Skipped under a
      custom theme or high contrast.
- [x] Page card (`argon/ui/page-card.patch`): with vertical tabs, the tabbed layout insets everything right of the
      sidebar once (8px from the frame, none on the sidebar side), so toolbar, infobars, page and side panel form one
      card; Chromium's shadow overlay draws its frame-colored corners, 10px as in the handoff. The collapsed sidebar
      rounds its own top corner. No 1px ring (the shadow box's shadow only shows with a side panel).
      Horizontal tabs with a side panel open would show frame-colored corners on the toolbar color.
- [x] Allo font in Argon's own views (`argon_font.*`): `resources/AlloVariable.ttf.b64` (base64, since `src/` reaches
      the build as a text patch) is decoded by a GN action into the framework's Resources and registered with
      CoreText on first use. Family "Allo Variable". Not applied: `ss01`/`tnum` and ROND 100 (FontList can't set
      features or axes; ROND stays 50). Also Chromium's own UI font (decided 2026-11-25): `UseArgonFontAsDefault()` at
      Mac startup (`argon/ui/allo-default-font.patch`) sets `gfx::FontList`'s default to "Allo Variable, 13px"; views
      that build a `gfx::Font` directly, and native Mac menus, keep the system font.
- [ ] Customize (design: `design_handoff_argon_browser/README.md` §6, `Argon Customize.dc.html`, `ArgonModule.dc.html`).
      Three stages, in dependency order, each its own build:
      1. Layout and settings in profile prefs: the zones (`side`, `top`: lists of modules) and the handoff's `cfg`
         (side, behavior pinned/auto-hide/off, width 180–320, density, tab icons, close ×, archive 12 h/24 h/7 d/never,
         switcher, edge pill side, per-space hues, tone Paper/Cool/Tinted/Dark, frame gap 0–16, corners 0–20, Allo
         roundness). Replace the constants with prefs and redraw on change: `kQuietSidebar` (`argon_layout.h`), row
         sizes, `kArchiveAfter`, the page card's gap and radius, the color mixer's tone, the edge pill.
      2. The Customize page: WebUI editing those prefs, so the native UI updates live. Left a preview, right the 320px
         inspector: Mode (Edit layout / Use, ⌃⇧E), presets Quiet/Ticks/Classic/Top bar, modules tray, and the
         Sidebar, Tabs, Spaces and Window sections, saved automatically, with Reset.
      3. What Customize adds: the Top bar zone with horizontal module variants, the remaining modules (address,
         Favorites grid, archive count, spacer), the Ticks layout (tick marks on the edge, tab panel on hover), custom
         space colors (the six handoff hues, the Tinted tone per space).
- [x] Edge pill (`argon_edge_pill.*`): on the sidebar's leading edge, vertically centered; a 6×56 pill with a dot per
      space (the current one 14px tall, in its accent), expanding on hover into a round button per space. It's the
      switcher while collapsed. No drop shadow yet (a hairline instead).
- [x] No toolbar with vertical tabs (`argon/ui/no-toolbar.patch`: `BrowserView::IsToolbarVisible()` false, the
      path fullscreen-without-toolbar already uses): the sidebar's first module (`kNavigation`) has back, forward,
      reload and the site (head-elided), which opens ⌘L. `--disable-features=ArgonHideToolbar` brings the toolbar
      back. The bookmarks bar is hidden the same way (it listed every space, as saved tab groups). The sidebar's address says "Not secure" (warning color) for plain HTTP and certificate errors. Known gaps: no extensions button (popups only via shortcuts), no app menu (the Mac menu bar covers
      most of it), no downloads button (⌘⇧J), no lock/not-secure indicator; bubbles that anchored to toolbar
      buttons (downloads, passwords, translate, page actions) open from the page card's top-left corner, and page
      info and permission bubbles 40px in from the window's top-left.
- [x] Undo toast (`argon_toast.*`): closing a tab from the sidebar or Clear shows 6 s with Undo (reopens the
      closed tabs from the recently closed list).
- [x] Archive timers on loose tabs: `Nm`/`Nh`/`Nd` left, amber under 2 h, replaced by × on hover; rows refresh in
      place every minute.
- [x] Command bar (`argon_command_bar.*`, hook `argon/ui/command-bar.patch`): ⌘T (`IDC_NEW_TAB`) opens it to start a
      new tab, ⌘L (`IDC_FOCUS_LOCATION`) on the current URL to replace it; falls back to Chromium's behavior outside
      normal windows. A `BubbleDialogDelegate` 14% from the window top: tabs in every space (substring match, max 6),
      open/search through the omnibox classifier, "Go to <space>". Argon's own new-tab needs call `chrome::NewTab`.
      Commands: "New space", "rename <name>", "Go to <space>" across profile windows, "Clear loose tabs", "Show/Hide
      toolbar" (per window, `ArgonToolbarHidden`: brings back extension buttons, downloads, the app menu and the
      site's details), "Downloads", "Extensions", pin/unpin, "Import from Arc". Tabs are ranked by last use.
- [ ] Ticks layout: Customize stage 3

Testing a build: the app is still named Helium and uses Helium's profile folder, so if Helium is installed, run it
with its own profile: `<path>/Helium.app/Contents/MacOS/Helium --user-data-dir="$HOME/argon-test-profile"`.

## Weight

Benchmark (LeanBrowser's `scripts/bench.sh`, 10 sites, M1 8 GB, 2026-11-15): Helium 0.21.3.1 used 1,950–2,247 MB
and 16–22 s of idle CPU per minute; LeanBrowser (WebKit, aggressive tab budget) used 426–490 MB. The gap is tab
policy, not the engine.

Helium already: Memory Saver on, infinite tab freezing, lazy session restore.

Try first on stock Helium (no patch): Memory Saver at Maximum, "Preload pages" off, Battery Saver always on,
an h264ify-style extension to avoid AV1 software decoding. Benchmark with a 10-minute idle.

Patches, biggest saving first:

- [x] Live-tab budget: keep only the 2 / 5 / 10 (by RAM) most recent background tabs loaded, discard the rest at once
- [x] Halve that budget on battery, Low Power Mode, memory pressure (warning or worse) and serious thermal state
      (both in `argon/core/live-tab-budget.patch`, hooked into `MemorySaverModePolicy`; only tabs Chromium would
      discard anyway count, so pinned, audible, form-input and notification-permission tabs stay loaded on top;
      a power or thermal change applies at the next tab switch; macOS-only code, uncompiled)
- [x] Memory Saver defaults to Maximum (`argon/core/memory-saver-maximum.patch`)
- [x] Intensive background throttling after 10 s instead of 5 min (`argon/core/quick-intensive-throttling.patch`)
- [x] No spare renderer process (`argon/core/no-spare-renderer.patch`): `kSpareRendererForSitePerProcess` off, so
      `RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes()` is false and the manager drops spares instead of
      keeping one warm. Direct `WarmupSpare()` callers (navigation predictors) can still make one briefly.

Not worth it: removing features (saves download size, not RAM), V8 lite mode (slow JS), weakening site isolation
(security).

Expected: around 1 GB in the same test (a guess); LeanBrowser's ~430 MB is out of reach because Chromium's
browser, GPU, network and uBlock processes stay resident.

## Fingerprinting

Helium Noise is on by default: per-site, per-session noise on canvas, audio and `hardwareConcurrency`, with
per-site exceptions. Off by default but in `chrome://flags`: `#fingerprinting-client-rects-noise`,
`#fingerprinting-canvas-measuretext-noise`, `#spoof-webgl-info`, `#remove-client-hints`. Not covered: fonts,
screen size, time zone, language. New noise sources can follow `helium/core/noise/hardware-concurrency.patch`
(170 lines) as a template.
