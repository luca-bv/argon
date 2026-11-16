# Argon

A personal fork of [Helium](https://github.com/imputnet/helium) (ungoogled-chromium based, GPL-3.0) that rebuilds
automatically on every Helium release, adding Arc's workflow and a stricter tab policy for lower memory use.
Argon sits next to helium in the periodic table (and starts with "Ar", for Arc).

Status: plan only. No patches or builds yet.

## How it builds

1. A scheduled GitHub Actions job watches `imputnet/helium-macos` for a new release tag.
2. It checks out that tag, appends `patches/series` from this repo to Helium's series, and applies it.
3. If everything applies, it runs Helium's own macOS build workflow unchanged (10 chained jobs on `macos-latest`;
   free for public repos). arm64 only, to halve build time.
4. If a patch fails to apply, it opens an issue instead of releasing.

Personal use only, so: ad-hoc signed, no notarization, no Sparkle update server; builds are downloaded from
Releases. Helium's name and icons are non-free and can't be reused, so the app is rebranded.

Developing a patch needs a Mac with 32 GB+ RAM and 200 GB+ free disk (rented while working on a feature);
CI is only for release builds.

## Arc features

Helium already has: vertical tabs, compact toolbar, minimal address bar, split view, zen mode, and an Arc importer
(pinned tabs become bookmark folders).

To add:

- [ ] Spaces: separate sidebars, each with its own pinned tabs, color and optionally profile
- [ ] Pinned / unpinned sidebar sections, unpinned tabs archived after a while
- [ ] Arc import that keeps spaces and pinned tabs (from `StorableSidebar.json`)
- [ ] Command bar, Peek, Little Arc (later, maybe)

Risk: Spaces edits the same tab-strip code as Helium's `helium/ui/layout/vertical.patch` (1,711 lines), so
Helium sidebar changes will break it and need manual rebases.

## Weight

Benchmark (LeanBrowser's `scripts/bench.sh`, 10 sites, M1 8 GB, 2026-11-15): Helium 0.21.3.1 used 1,950–2,247 MB
and 16–22 s of idle CPU per minute; LeanBrowser (WebKit, aggressive tab budget) used 426–490 MB. The gap is tab
policy, not the engine.

Helium already: Memory Saver on, infinite tab freezing, lazy session restore.

Try first on stock Helium (no patch): Memory Saver at Maximum, "Preload pages" off, Battery Saver always on,
an h264ify-style extension to avoid AV1 software decoding. Benchmark with a 10-minute idle.

Patches, biggest saving first:

- [ ] Live-tab budget: keep only the 2 / 5 / 10 (by RAM) most recent background tabs loaded, discard the rest at once
- [ ] Discard on battery, Low Power Mode, memory pressure and high thermal state
- [ ] Throttle background tabs immediately instead of after 5 minutes (check the feature param exists)
- [ ] No spare renderer process (check it's a feature flag)

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
