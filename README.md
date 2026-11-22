# Argon

A browser for people who miss Arc: spaces, pinned and temporary tabs that archive themselves, a quiet sidebar and a
command bar. It's built on [Helium](https://github.com/imputnet/helium), an ungoogled-chromium browser, so it keeps
Helium's privacy work, and it adds a stricter tab policy so it uses less memory.

The name comes from the periodic table: argon sits next to helium, and its symbol, Ar, is the start of Arc.

> **Status: early, not usable yet.** No build has been compiled, and there are no downloads. macOS on Apple silicon
> only, for now.

## What it will do

- **Spaces.** Each space is its own set of tabs. You switch with an edge pill or the keyboard, and a space can be
  tied to a profile.
- **Kept and loose tabs.** Kept tabs stay. Loose tabs close after 12 hours unused (or 24 hours, 7 days, never), and
  "Clear" closes them all at once, with undo.
- **A quiet sidebar.** One column with the space name, kept tabs, a divider and loose tabs, and nothing else until
  you hover. Designs are in [`design_handoff_argon_browser/`](design_handoff_argon_browser/README.md): open the
  `.dc.html` files in a browser to click through them.
- **A command bar** (⌘K) for tabs, spaces, history, commands and search.
- **Lower memory use.** Only the few most recently used background tabs stay loaded. The rest are discarded at once,
  and fewer stay loaded on battery or under memory pressure.
- **Helium underneath.** No Google services, bundled uBlock Origin, fingerprinting noise, and Helium's privacy
  defaults.

## How it works

Argon isn't a full copy of Chromium. It's a set of patches on top of Helium's:

1. [`ci/assemble.sh`](ci/assemble.sh) takes a Helium macOS release and removes Helium's interface patches. What's
   left is Chromium's stock interface with all of Helium's features underneath.
2. It adds Argon's patches from [`patches/`](patches/series): the interface (native C++ Chromium Views in
   `chrome/browser/ui/views/argon/`) and the memory changes.
3. [GitHub Actions](.github/workflows/build.yml) checks for a new Helium release every 6 hours, assembles the tree
   on a `build/<tag>` branch, and builds it on GitHub's macOS runners. Any failure opens an issue, such as a patch
   that no longer applies.

So Argon follows Helium releases automatically. When a Helium update breaks a patch, that patch gets fixed.

## Repository layout

| Path | What it is |
|---|---|
| `patches/argon/` | Argon's patches, applied after Helium's ([`patches/series`](patches/series) sets the order) |
| `ci/` | Assembles the build tree from a Helium release |
| `.github/workflows/` | The automatic build |
| `dev/` | Sets up a development Mac and copies patches back into this repo |
| `design_handoff_argon_browser/` | The interface design: HTML prototypes and the tokens and specs |
| `extension/` | A temporary side-panel extension that mimics the sidebar on stock Helium, until the native one builds |
| `docs/notes.md` | Development notes: decisions, patch checks and the plan for each feature |

## Building

Chromium is big. Building it needs a Mac with 32 GB+ of RAM and about 200 GB of free disk, and it takes hours.
On such a Mac:

```bash
git clone https://github.com/luca-bv/argon && cd argon
dev/setup.sh
```

This installs the build tools, assembles `~/argon-tree` from the latest Helium release plus Argon's patches, and
builds it once. After that, work happens with Helium's `he` tool and `quilt`. See
[docs/notes.md](docs/notes.md#developing-on-a-rented-ec2-mac) for the full loop.

Builds are still named Helium and use Helium's profile folder. If you have Helium installed, run a test build with
its own profile:

```bash
Helium.app/Contents/MacOS/Helium --user-data-dir="$HOME/argon-test-profile"
```

## Trying the stopgap extension

`extension/` runs on Helium or any Chromium browser. Open `chrome://extensions`, turn on Developer mode, click "Load
unpacked" and choose the `extension` folder. `node --test extension/` runs its tests.

## Roadmap

- [ ] First compiled build
- [ ] Native sidebar (written, not compiled)
- [ ] Spaces, then pins, colors, profile per space and auto-archive
- [ ] Command bar, Peek, Little Arc
- [ ] Arc import that keeps spaces and pinned tabs
- [ ] Own name and icons, Developer ID signing, notarization, updates

The plan for each item is in [docs/notes.md](docs/notes.md).

## Credits

Argon is built on [Helium](https://github.com/imputnet/helium) and
[helium-macos](https://github.com/imputnet/helium-macos) by imput, which are built on
[ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium) and
[Chromium](https://www.chromium.org/). Helium is GPL-3.0, and Chromium is under a BSD-style license. The interface
font is [Allo](https://github.com/joshpuckett/allo) (SIL Open Font License, in `extension/fonts/`).

Argon isn't affiliated with The Browser Company (Arc) or with imput (Helium).

## License

Argon is licensed under the [GNU General Public License v3.0](LICENSE), like Helium. Code Argon's patches change in
Chromium keeps Chromium's own license.
