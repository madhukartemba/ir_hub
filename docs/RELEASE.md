# Releasing a new firmware version

`scripts/release.py` is the single command that ships an OTA-deliverable
firmware. It bumps the version, builds the binaries, drops a versioned
copy under `binaries/` (which is what the device actually downloads via
Cloudflare Pages), updates `ota/manifest.json`, and commits + pushes in
the right order so that no device can ever see a manifest pointing at a
binary that doesn't exist yet. GitHub Releases are optional and opt-in.

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

The script will refuse to run if any of `pio` or `git` is missing.
If you pass `--github-release`, it will also require `gh` and authenticated
GitHub CLI access.

You also need:

- A clean git working tree (only `platformio.ini` and `ota/manifest.json` are
  allowed to be dirty, since the script is about to overwrite them anyway).
- The branch you want to release from checked out — usually `main`.
- `OTA_MANIFEST_URL` set in `include/secrets.h` on the devices you've already
  flashed, pointing at the manifest in *this* repo. See `OTA_RELEASES.md`
  for the one-time device setup.

## Quick start

Cut a release with automatic patch bump (uses `platformio.ini`'s current
`custom_firmware_version`, increments patch by 1):

```bash
scripts/release.py --notes "Fix touch-button EMI during recording"
```

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
scripts/release.py 1.0.1 --github-release --auto-notes
```

Inspect what would happen, without doing anything:

```bash
scripts/release.py 1.0.1 --dry-run
```

## All flags

| Flag | Purpose |
| --- | --- |
| `version` (positional, optional) | New semver, e.g. `1.0.1` or `1.0.1-rc1` (no leading `v`). If omitted, script auto-bumps patch from `platformio.ini` (for example `1.0.13 -> 1.0.14`). |
| `--envs` | PlatformIO envs to build. Default: `ir_hub_version_3`. |
| `--notes` | Release notes as a string. Mutually exclusive with `--notes-file`. |
| `--notes-file PATH` | Read release notes from a markdown file. |
| `--auto-notes` | Let `gh` generate notes from commits since the last tag. |
| `--github-release` | Also create/update a GitHub Release with uploaded binary assets (optional). |
| `--draft` | Create the GitHub Release as a draft (publish manually). |
| `--repo OWNER/REPO` | Override the auto-detected GitHub slug. |
| `--no-push` | Commit locally but don't push (useful for reviewing the commit first). |
| `--no-commit` | Skip the git add/commit/push entirely (the manifest still gets rewritten on disk). |
| `--no-release` | Deprecated alias; keeps GitHub Release creation disabled. |
| `--dry-run` | Print every action it would take, then exit. |

## What it does, step by step

1. **Preflight checks**
   - `pio`, `gh`, `git` are on PATH.
   - `gh auth status` returns success.
   - Working tree is clean (excluding the two files about to be rewritten).
   - The `vX.Y.Z` tag doesn't exist yet locally.
2. **Resolves release version**:
   - Uses the provided positional `version` when passed, or
   - auto-bumps patch from `platformio.ini`'s `custom_firmware_version`
     when omitted.
   Then **bumps `custom_firmware_version`** in `platformio.ini` so the next
   build compiles in the new `FIRMWARE_VERSION` macro.
3. **Builds** each env in `--envs` via `pio run -e <env>`.
4. **Stages binaries** in two places:
   - `release/firmware_<variant>.bin` — gitignored, used only as the asset
     uploaded to the GitHub Release for human visibility.
   - `binaries/firmware_<variant>_v<version>.bin` — committed to git. This
     is the file the device actually downloads, via jsDelivr's CDN.
   The variant is derived from the env name (`ir_hub_version_3` → `v3`) so
   it matches the device's `OTA_HW_VARIANT`.
5. **(Optional) Creates the GitHub Release** (only when `--github-release`
   is set) with `gh release create vX.Y.Z firmware_*.bin --title vX.Y.Z`.
   If the tag already exists from a partial previous run, falls back to
   `gh release upload --clobber` so re-runs are idempotent.
6. **Updates `ota/manifest.json`** with the new version and per-variant URLs
   pointing at jsDelivr:

   ```json
   {
     "variants": {
       "v3": {
         "version": "1.0.1",
         "url": "https://cdn.jsdelivr.net/gh/<you>/<repo>@main/binaries/firmware_v3_v1.0.1.bin"
       }
     }
   }
   ```

   Repo slug auto-detected from `git remote origin`. Existing variants you
   didn't build are left untouched.
7. **Commits and pushes** `platformio.ini` + `ota/manifest.json` +
   `binaries/firmware_<variant>_v<version>.bin` with the message
   `chore(release): vX.Y.Z`.
8. **No manual CDN purge needed**. Cloudflare Pages deploys fresh content
   for the new commit; the binary path is versioned (`..._vX.Y.Z.bin`)
   so each release is a unique URL.

If `--github-release` is used, the release goes live in step 5 *before*
the manifest is published in step 7 — a device polling between those two
steps either sees the old manifest (and does nothing) or sees the new
manifest (and the binary URL already works). It can never see
"new manifest -> 404 binary".

## After running

Devices check the manifest 30 s after boot and then every 1 hour, so a
release rolls out within an hour organically. To trigger an immediate
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
  `git push`. Devices will pick up the manifest as soon as jsDelivr's edge
  has it (purge with
  `curl https://purge.jsdelivr.net/gh/<you>/<repo>@main/ota/manifest.json`
  if you don't want to wait the default ~12 h TTL).

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

**"cannot auto-bump ... pre-release/non-semver"** — Auto-bump only works for
stable semver in `platformio.ini` (`X.Y.Z`). If current value is something
like `1.0.1-rc1` (or non-semver), pass an explicit positional version.

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
