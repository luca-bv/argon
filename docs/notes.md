# Argon development notes

Decisions, patch checks and plans behind [the README](../README.md). Dated entries are snapshots.

## Scope

- An Arc continuation, eventually for other people, not just personal use.
- Its own interface, written natively in C++ (Chromium Views) as `patches/argon/ui/*`, from the design in
  `design_handoff_argon_browser/`. Customizable later.
- A robust command bar.
- Lighter than stock Helium where possible (tab-policy patches), with Helium's privacy changes plus more.
- Rented Macs kept to a minimum: everything that can be done on GitHub's free runners is.

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
Candidates to bring back as Argon patches (each needs a re-apply check against upstream):
`remove-google-promotional-resources` (first-run intro, Gemini spark, sign-in promos), `remove-devtools-annoyances`,
`profile-picker-cleanup`, `profile-customization-cleanup`, `remove-dead-profile-actions`, `licenses-in-credits`,
`ublock-show-in-settings`, `helium-noise-page-info`. `helium/macos/open-shortcuts-settings-action` stays but is dead
code without the shortcuts page.

Status: CI pipeline, EC2 dev scripts, three weight patches and the first interface patches written; nothing
compiled yet. Spaces designed, not started.

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
run after a failure). A rerun looks for the newest failed run whose assembled tree differs only in
`patches/argon/` (`find-resume` compares a hash of everything else, stored in the `argon_state` artifact). If it
finds one, it downloads that run's build directory instead of preparing the source, and `ci/swap-patches.sh` reverts
the old Argon patches and applies the new ones. Files that come out unchanged keep their timestamps, so only the
fixed files recompile. Tick "fresh" to build from scratch. Artifacts expire after 90 days.

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
  files, edit, `he build && he run`, `quilt refresh`. Patch names must start with `argon/`.
- Seeing the UI: enable Screen Sharing on the instance and connect over an SSH tunnel (port 5900).
- Saving work: `he unmerge`, then `dev/pull-patches.sh` copies `patches/argon/` and the series back into this
  repo; commit from here. Stop before releasing the host and check nothing is left only on the instance.

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

- [ ] 1. Switchable spaces (300–450 lines): new `chrome/browser/ui/argon/` controller (every unpinned tab in a
      group, one group always focused, "Home" on first run) and a space switcher in the sidebar footer; flip
      `kTabGroupsFocusing`; in `tab_strip_model.cc` make new tabs join the focused group and make activating a
      tab in another group switch spaces instead of exiting focus (about 15 `ExitFocusMode` call sites to audit).
- [ ] 2. Pins (`argon_pin_url`, sorted to the top of the group with a divider)
- [ ] 3. Custom colors and gradients (pref keyed by saved-group GUID, painted in `vertical_tab_strip_region_view.cc`)
- [ ] 4. Profile per space: a profile-bound space lives in that profile's window; switching swaps windows at the
      same frame. Expect a visible swap.
- [ ] 5. Auto-archive: a timer in the controller closes unpinned, non-pin, inactive, silent tabs idle for N hours;
      they land in Recently Closed/History.
- [ ] Arc import that keeps spaces and pinned tabs (extend Helium's `add-arc-importer.patch`)
- [ ] Command bar, Peek, Little Arc

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

Native Views in `chrome/browser/ui/views/argon/`, one patch per piece, written against Chromium 158 without a
checkout (uncompiled until the first EC2 session).

- [ ] Sidebar (`argon/ui/sidebar.patch`, in `patches/series`): `ArgonSidebarView`, the Quiet sidebar's space
      name, kept tabs, divider, loose tabs with a close button on hover, "+ New" and "Clear". It replaces the tab
      list and new tab button inside upstream's `VerticalTabStripRegionView`, which keeps width, collapse, resize and
      the window controls. Until Spaces and pins: the space is the focused group ("Home" if none), kept tabs are
      Chromium-pinned tabs. Applies with `--fuzz=0` on the stripped Helium 0.22.2 series. On the Mac: `gn check`,
      check it in the collapsed state.
- [x] Vertical tabs on by default (`argon/ui/vertical-tabs-default.patch`: the `kVerticalTabsEnabled` pref default;
      stock Chromium starts horizontal, and the sidebar only exists in vertical mode). Only affects profiles that
      never set it.
- [ ] Paper palette and Allo (color mixer for frame/sidebar/page tones, bundled font)
- [ ] Edge-pill space switcher (after Spaces stage 1)
- [ ] Archive timers on loose tabs (with auto-archive, stage 5)
- [ ] Command bar, then Ticks layout and Customize

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
- [ ] No spare renderer process: not a feature flag on desktop in Chromium 158 (only Android has one), needs a
      patch in `content/browser/renderer_host/spare_render_process_host_manager*`

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
