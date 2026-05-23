# Releasing a new firmware version

`scripts/release.py` is the single command that ships an OTA-deliverable
firmware. It bumps the version, builds the binaries, uploads them to a GitHub
Release, updates `ota/manifest.json`, and pushes — in the right order so that
no device can ever see a manifest pointing at a binary that doesn't exist
yet.

For the underlying OTA mechanism (manifest schema, security model, what the
device does on the wire), see [`OTA_RELEASES.md`](OTA_RELEASES.md). This
file is the day-to-day operator's guide.

## Prerequisites

One-time setup on your machine:

```bash
pio --version            # PlatformIO CLI
gh --version             # GitHub CLI
gh auth login            # authenticate gh against github.com
```

The script will refuse to run if any of `pio`, `gh`, or `git` is missing, or
if `gh auth status` reports unauthenticated.

You also need:

- A clean git working tree (only `platformio.ini` and `ota/manifest.json` are
  allowed to be dirty, since the script is about to overwrite them anyway).
- The branch you want to release from checked out — usually `main`.
- `OTA_MANIFEST_URL` set in `include/secrets.h` on the devices you've already
  flashed, pointing at the manifest in *this* repo. See `OTA_RELEASES.md`
  for the one-time device setup.

## Quick start

Cut a v3-only release with manual notes:

```bash
scripts/release.py 1.0.1 --notes "Fix touch-button EMI during recording"
```

Cut a release for every PCB variant in one go:

```bash
scripts/release.py 1.0.1 \
    --envs ir_hub_version_0 ir_hub_version_1 ir_hub_version_3 \
    --notes "Bring all variants to parity"
```

Let GitHub generate notes from your commits since the last tag:

```bash
scripts/release.py 1.0.1 --auto-notes
```

Inspect what would happen, without doing anything:

```bash
scripts/release.py 1.0.1 --dry-run
```

## All flags

| Flag | Purpose |
| --- | --- |
| `version` (positional) | New semver, e.g. `1.0.1` or `1.0.1-rc1`. No leading `v`. |
| `--envs` | PlatformIO envs to build. Default: `ir_hub_version_3`. |
| `--notes` | Release notes as a string. Mutually exclusive with `--notes-file`. |
| `--notes-file PATH` | Read release notes from a markdown file. |
| `--auto-notes` | Let `gh` generate notes from commits since the last tag. |
| `--draft` | Create the GitHub Release as a draft (publish manually). |
| `--repo OWNER/REPO` | Override the auto-detected GitHub slug. |
| `--no-push` | Commit locally but don't push (useful for reviewing the commit first). |
| `--no-commit` | Skip the git add/commit/push entirely (the manifest still gets rewritten on disk). |
| `--no-release` | Skip `gh release create` (build + stage + manifest only). |
| `--dry-run` | Print every action it would take, then exit. |

## What it does, step by step

1. **Preflight checks**
   - `pio`, `gh`, `git` are on PATH.
   - `gh auth status` returns success.
   - Working tree is clean (excluding the two files about to be rewritten).
   - The `vX.Y.Z` tag doesn't exist yet locally.
2. **Bumps `custom_firmware_version`** in `platformio.ini` so the next build
   compiles in the new `FIRMWARE_VERSION` macro.
3. **Builds** each env in `--envs` via `pio run -e <env>`.
4. **Stages binaries** as `release/firmware_<variant>.bin` (e.g.
   `release/firmware_v3.bin`). The variant is derived from the env name
   (`ir_hub_version_3` → `v3`) so it matches the device's `OTA_HW_VARIANT`.
   The `release/` directory is gitignored.
5. **Creates the GitHub Release** with `gh release create vX.Y.Z firmware_*.bin
   --title vX.Y.Z`. If the tag already exists from a partial previous run,
   falls back to `gh release upload --clobber` so re-runs are idempotent.
6. **Updates `ota/manifest.json`** with the new version and per-variant URLs:

   ```json
   {
     "variants": {
       "v3": {
         "version": "1.0.1",
         "url": "https://github.com/<you>/<repo>/releases/download/v1.0.1/firmware_v3.bin"
       }
     }
   }
   ```

   Repo slug auto-detected from `git remote origin`. Existing variants you
   didn't build are left untouched.
7. **Commits and pushes** `platformio.ini` + `ota/manifest.json` with the
   message `chore(release): vX.Y.Z`.

The release goes live in step 5 *before* the manifest is published in
step 7 — a device polling between those two steps either sees the old
manifest (and does nothing) or sees the new manifest (and the binary URL
already works). It can never see "new manifest → 404 binary".

## After running

Devices check the manifest 30 s after boot and then every 6 hours, so a
release rolls out within 6 hours organically. To trigger an immediate
pickup, publish an empty MQTT message to the device's OTA topic:

```bash
mosquitto_pub -h <broker> -u <user> -P <pass> \
    -t "ir_hub/<mac>/ota/check" -n
```

The MAC is the lowercase, no-separator hex shown in the device's MQTT
discovery topics (and printed in the boot log).

## Recovering from a partial release

The script is designed to be re-run safely. Common scenarios:

- **`pio run` failed mid-build.** Fix the code, commit, re-run the script.
  The version bump already in `platformio.ini` is fine — the script will
  detect it and continue.
- **Build succeeded, `gh release create` failed (e.g. network).** Re-run the
  script with the same version. It detects the existing tag and uses
  `gh release upload --clobber` to push the binaries to the already-created
  release.
- **Release uploaded but `git push` failed.** Push manually:
  `git push`. Devices will pick up the manifest as soon as it's reachable on
  `raw.githubusercontent.com`.

If you need to abandon a release entirely:

```bash
gh release delete vX.Y.Z --yes        # remove the GitHub release
git tag -d vX.Y.Z                     # remove local tag
git push --delete origin vX.Y.Z       # remove remote tag (only if pushed)
git revert HEAD                       # back out the manifest bump
```

## Troubleshooting

**"working tree has uncommitted changes"** — Stash or commit your local edits
first. The script intentionally won't proceed with random work-in-progress
files staged for a release.

**"git tag vX.Y.Z already exists locally"** — You've already run a release
for this version. Either pick a new version (recommended) or delete the
old tag (`git tag -d vX.Y.Z`) if you really want to overwrite.

**`gh auth status` failed** — Run `gh auth login` and follow the prompts.

**Device never picks up the new release** — Check:

1. `OTA_MANIFEST_URL` is set in the device's `secrets.h` and the device was
   flashed *with* that URL baked in.
2. The device has Wi-Fi.
3. The manifest URL returns 200 in your browser.
4. `FIRMWARE_VERSION` on the device's home screen is older than the
   manifest's version (the device only updates when the manifest version is
   strictly newer).
5. The device's free heap is above ~18 KB at check time (look for
   `[OTA-HTTP] Skipping check, low heap` in the serial log).
