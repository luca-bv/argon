# Argon

An Arc-style browser for macOS, built as patches on Helium (ungoogled-chromium, Chromium 158). The UI target is
`design_handoff_argon_browser/`.

**Start with `docs/notes.md`, section "Where things stand"**: it holds the plan, what's built, what's been tested,
and the user's decisions. Update it whenever a step lands or the user decides something. It is the work plan.

## Code layout

- `src/`: Argon's own code as plain files, laid out like the Chromium tree (mostly
  `src/chrome/browser/ui/views/argon/`). CI turns it into `argon/src.patch` (`ci/src-patch.sh`).
  - Every file needs a final newline.
  - Binary files go in as base64 text and are decoded by a GN action. See the Allo font in `resources/`.
- `patches/argon/`: small hooks into upstream files, one per area, listed in `patches/series`.
  - Write each hook against the **post-Helium** copy of the file, because Helium patches many of them. Get it with
    `dev/replay.sh`, and pristine files with `dev/fetch-chromium.sh`.
  - Generate the hook with `diff -u --label a/<path> --label b/<path>`, then check it with `patch -p1 --fuzz=0`.
  - In `.grd`/`.grdp` files, never use the last line (`</grit-part>`) as context. A resumed build applies Argon's
    patches after Helium's name substitution, which rewrites those files without a final newline.
  - Changing Helium's own series (`ci/assemble.sh` drops) changes the resume manifest and forces a full ~17 h
    build. Restored Helium patches go in `patches/argon/helium/` instead.

## Working rhythm

Work the patch list from "Where things stand" in order, one patch at a time:

1. Read the upstream code the patch touches before writing it.
2. Write the Argon code in `src/` and the hooks in `patches/argon/`.
3. Have Fable review it (below), and apply its fixes. For non-trivial fixes, send them back to the same reviewer
   with SendMessage.
4. Commit and push the reviewed work.
5. Build it. Keep writing the next patch while the build runs.

Use one Fable reviewer per area: the toolbar, profiles, pins and so on. Spawn a fresh one to design anything that
needs research before code (profiles across windows and the Arc import were done that way). A reviewer can stop on
a server error (529); resume it with SendMessage.

## Builds

CI only. Nothing is built locally.

- Start one with `gh workflow run build.yml -R luca-bv/argon -f tag=0.22.2.1`. It resumes from the last run, so an
  Argon-only change takes about 35–50 min.
- Start a build only after the previous run finishes, so it resumes from that run.
- The dmg goes to the draft release `argon-0.22.2.1`.
- **Before every build, have Fable review the change.** Spawn an Agent with `model: "fable"` and give it the diff,
  the post-Helium copies, and what to check: compile, GN, runtime, and fit with Helium's code. The user asked
  for this; each CI round shows only the first compile error, so reading first saves rounds.
- **Watch** each build in the background: `gh run watch <id> -R luca-bv/argon --exit-status` with
  `run_in_background`. It finishes when the run ends and wakes the session, so there's no polling. Nothing watches
  while no session is open: a failure then waits for "check the build".
- **Auto-fix:** a failed build opens a "Build failed" issue (the `report-failure` job), and the run's build
  directory is kept so the rerun resumes.
  1. Read the errors with `gh run view <id> --log-failed | grep 'error:'`. The build stops at the first failing
     file, so there's usually one.
  2. Check the 158 API with `dev/fetch-chromium.sh`, fix it, commit with the cause, and push.
  3. Close the issue, naming the cause and the commit.
  4. Rerun. Only the changed files recompile.

  Stop and ask the user instead when a fix would change behaviour or drop a feature.

## Trying a build

Downloading and installing needs the user's go-ahead each time; they want to be present for it.

1. Download the dmg with `gh release download`.
2. Quit Helium.
3. Copy `Helium.app` to `/Applications` and run `xattr -dr com.apple.quarantine` on it.
4. Launch with the test profile:
   ```bash
   open -a /Applications/Helium.app --args --user-data-dir="$HOME/argon-test-profile"
   ```

## Working with the user

- Copy Arc's behaviour wherever profiles, spaces and pins are concerned.
- Ask when a choice is the user's: scope, what Argon should do, anything that downloads.
- Customize waits until the current patches are built and tested; Peek and Little Arc come after Customize.
