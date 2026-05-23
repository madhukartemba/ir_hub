#!/usr/bin/env python3
"""Cut a new IR Hub OTA release.

Workflow per invocation:
  1. Bump `custom_firmware_version` in platformio.ini.
  2. Build each requested PlatformIO env.
  3. Stage binaries as `release/firmware_<variant>.bin`.
  4. Create the GitHub Release and upload the binaries.
  5. Update `ota/manifest.json` to point at the new release URLs.
  6. Commit `platformio.ini` + `ota/manifest.json` and push.

Order matters: we create the Release *before* publishing the manifest so a
device polling between the two steps never sees a manifest pointing at a
not-yet-uploaded binary.

Examples:
  scripts/release.py 1.0.1
  scripts/release.py 1.0.1 --envs ir_hub_version_3
  scripts/release.py 1.0.1 --envs ir_hub_version_0 ir_hub_version_1 ir_hub_version_3
  scripts/release.py 1.0.1 --notes "Fix touch-button EMI during recording"
  scripts/release.py 1.0.1 --dry-run

Prerequisites:
  - PlatformIO CLI (`pio`) on PATH.
  - GitHub CLI (`gh`) on PATH, authenticated (`gh auth status`).
  - Clean working tree on the branch you intend to release from.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Iterable, List, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
PLATFORMIO_INI = REPO_ROOT / "platformio.ini"
MANIFEST_PATH = REPO_ROOT / "ota" / "manifest.json"
# Where we stage binaries for the GitHub Release upload (gitignored).
RELEASE_DIR = REPO_ROOT / "release"
# Where we commit binaries that the device pulls via jsDelivr. Versioned
# filenames so jsDelivr's CDN sees each one as a brand-new URL (no cache
# coherency issues even across the global edge network).
BINARIES_DIR = REPO_ROOT / "binaries"

DEFAULT_ENVS = ["ir_hub_version_3"]


# ---------------------------------------------------------------------------
# Shell helpers
# ---------------------------------------------------------------------------

def run(cmd: List[str], *, capture: bool = False, check: bool = True,
        cwd: Optional[Path] = None) -> subprocess.CompletedProcess:
    print(f"$ {' '.join(cmd)}")
    return subprocess.run(
        cmd,
        check=check,
        cwd=str(cwd or REPO_ROOT),
        text=True,
        capture_output=capture,
    )


def run_quiet(cmd: List[str], cwd: Optional[Path] = None) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd,
        check=False,
        cwd=str(cwd or REPO_ROOT),
        text=True,
        capture_output=True,
    )


# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------

def check_tools() -> None:
    missing = [tool for tool in ("pio", "gh", "git") if shutil.which(tool) is None]
    if missing:
        sys.exit(f"ERROR: required tools not found on PATH: {', '.join(missing)}")

    # `gh auth status` exits non-zero for cosmetic warnings (missing scopes,
    # stale GHES host, etc.) even when github.com auth is fine. Probe with an
    # actual API call instead — if `gh api user` returns a login, we're good.
    probe = run_quiet(["gh", "api", "user", "--jq", ".login"])
    if probe.returncode != 0 or not probe.stdout.strip():
        sys.stderr.write("ERROR: gh cannot make authenticated API calls.\n")
        sys.stderr.write("Run `gh auth login` and confirm `gh api user` works.\n")
        if probe.stderr.strip():
            sys.stderr.write(f"gh said: {probe.stderr.strip()}\n")
        sys.exit(1)


def detect_repo_slug() -> str:
    res = run_quiet(["git", "remote", "get-url", "origin"])
    if res.returncode != 0:
        sys.exit("ERROR: cannot read git remote 'origin'. Add one or use --repo.")
    url = res.stdout.strip()
    match = re.search(r"github\.com[:/]([^/]+/[^/.]+?)(?:\.git)?/?$", url)
    if not match:
        sys.exit(f"ERROR: could not parse GitHub slug from origin URL: {url}")
    return match.group(1)


def assert_clean_worktree() -> None:
    res = run(["git", "status", "--porcelain"], capture=True)
    blocking = []
    for line in res.stdout.splitlines():
        if not line.strip():
            continue
        path = line[3:].strip()
        # platformio.ini and ota/manifest.json are about to be re-written; allow
        # them to be dirty so re-running the script after a partial failure works.
        if path in ("platformio.ini", "ota/manifest.json"):
            continue
        blocking.append(line)
    if blocking:
        print("ERROR: working tree has uncommitted changes:", file=sys.stderr)
        for line in blocking:
            print("  " + line, file=sys.stderr)
        sys.exit(1)


def assert_tag_unused(version: str) -> None:
    res = run_quiet(["git", "rev-parse", "--verify", f"v{version}"])
    if res.returncode == 0:
        sys.exit(f"ERROR: git tag v{version} already exists locally. "
                 f"Pick a new version or delete the tag with `git tag -d v{version}`.")


# ---------------------------------------------------------------------------
# Build pipeline
# ---------------------------------------------------------------------------

def env_to_variant(env_name: str) -> str:
    """Derive OTA_HW_VARIANT from the PlatformIO env name (`ir_hub_version_3` -> `v3`)."""
    match = re.search(r"version_(\w+)", env_name)
    if not match:
        sys.exit(f"ERROR: cannot derive hardware variant from env '{env_name}'")
    return f"v{match.group(1)}"


def update_platformio_version(version: str, dry_run: bool) -> None:
    text = PLATFORMIO_INI.read_text()
    pattern = re.compile(r"^(custom_firmware_version\s*=\s*).+$", re.M)
    if not pattern.search(text):
        sys.exit("ERROR: `custom_firmware_version` not found in platformio.ini")
    new_text = pattern.sub(rf"\g<1>{version}", text)
    if dry_run:
        print(f"[dry-run] platformio.ini custom_firmware_version -> {version}")
        return
    PLATFORMIO_INI.write_text(new_text)
    print(f"Updated platformio.ini custom_firmware_version -> {version}")


def build_env(env_name: str, dry_run: bool) -> Path:
    bin_path = REPO_ROOT / ".pio" / "build" / env_name / "firmware.bin"
    if dry_run:
        print(f"[dry-run] pio run -e {env_name}")
        return bin_path
    run(["pio", "run", "-e", env_name])
    if not bin_path.exists():
        sys.exit(f"ERROR: expected firmware not found at {bin_path}")
    return bin_path


def stage_binary(env_name: str, source: Path, dry_run: bool) -> Path:
    """Copy build artifact to the gitignored upload-staging dir for `gh release`."""
    variant = env_to_variant(env_name)
    dst = RELEASE_DIR / f"firmware_{variant}.bin"
    if dry_run:
        print(f"[dry-run] cp {source} -> {dst}")
        return dst
    RELEASE_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dst)
    size_kb = dst.stat().st_size / 1024
    print(f"Staged {dst.relative_to(REPO_ROOT)} ({size_kb:.1f} KB)")
    return dst


def commit_binary_for_cdn(env_name: str, version: str, source: Path,
                          dry_run: bool) -> Path:
    """Copy build artifact into the git-tracked `binaries/` dir so jsDelivr
    can serve it via its MFLN-friendly TLS edge (which is what the device
    actually downloads). Versioned filename so the CDN treats each release
    as a unique URL with no cache invalidation needed."""
    variant = env_to_variant(env_name)
    dst = BINARIES_DIR / f"firmware_{variant}_v{version}.bin"
    if dry_run:
        print(f"[dry-run] cp {source} -> {dst}")
        return dst
    BINARIES_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dst)
    size_kb = dst.stat().st_size / 1024
    print(f"Committed {dst.relative_to(REPO_ROOT)} ({size_kb:.1f} KB) for jsDelivr")
    return dst


def update_manifest(version: str, envs: Iterable[str], repo_slug: str,
                    dry_run: bool) -> None:
    if MANIFEST_PATH.exists():
        manifest = json.loads(MANIFEST_PATH.read_text())
    else:
        manifest = {}
    manifest.setdefault("variants", {})

    for env_name in envs:
        variant = env_to_variant(env_name)
        # jsDelivr URL pointing at the binary we committed under binaries/.
        # The device's TLS budget can't handle a full-size record from
        # objects.githubusercontent.com (where GitHub Release downloads
        # land), so we deliberately route via jsDelivr's MFLN-friendly edge.
        url = (
            f"https://cdn.jsdelivr.net/gh/{repo_slug}@main/"
            f"binaries/firmware_{variant}_v{version}.bin"
        )
        manifest["variants"][variant] = {"version": version, "url": url}

    serialized = json.dumps(manifest, indent=2) + "\n"
    if dry_run:
        print(f"[dry-run] {MANIFEST_PATH} would become:")
        for line in serialized.splitlines():
            print(f"  {line}")
        return

    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(serialized)
    print(f"Updated {MANIFEST_PATH.relative_to(REPO_ROOT)}")


# ---------------------------------------------------------------------------
# GitHub release + git plumbing
# ---------------------------------------------------------------------------

def create_release(version: str, notes: Optional[str], notes_file: Optional[Path],
                   assets: List[Path], generate_notes: bool, draft: bool,
                   dry_run: bool) -> None:
    tag = f"v{version}"
    cmd = ["gh", "release", "create", tag, *[str(a) for a in assets],
           "--title", tag]
    if notes_file is not None:
        cmd += ["--notes-file", str(notes_file)]
    elif notes is not None:
        cmd += ["--notes", notes]
    elif generate_notes:
        cmd += ["--generate-notes"]
    else:
        cmd += ["--notes", f"Release {tag}"]
    if draft:
        cmd += ["--draft"]

    if dry_run:
        print(f"[dry-run] {' '.join(cmd)}")
        return

    res = subprocess.run(cmd, cwd=str(REPO_ROOT))
    if res.returncode != 0:
        # Tag may already exist on GitHub from a previous partial run. Upload
        # the assets with --clobber so re-running is idempotent.
        print("WARN: `gh release create` failed; attempting `gh release upload --clobber`")
        upload_cmd = ["gh", "release", "upload", tag, "--clobber",
                      *[str(a) for a in assets]]
        run(upload_cmd)


def purge_jsdelivr(repo_slug: str, dry_run: bool) -> None:
    """Ask jsDelivr to drop its cached copy of the manifest so devices see the
    new version immediately instead of waiting up to ~12 h for the TTL."""
    url = (f"https://purge.jsdelivr.net/gh/{repo_slug}@main/ota/manifest.json")
    if dry_run:
        print(f"[dry-run] GET {url}")
        return
    print(f"$ purge jsDelivr cache for ota/manifest.json")
    try:
        with urllib.request.urlopen(url, timeout=15) as resp:
            body = resp.read().decode("utf-8", errors="replace")
            data = {}
            try:
                data = json.loads(body)
            except json.JSONDecodeError:
                pass
            status = data.get("status", "unknown")
            print(f"  jsDelivr purge status: {status}")
    except (urllib.error.URLError, OSError) as exc:
        # Non-fatal: devices will pick up the new manifest after the natural
        # TTL expires. Print so the operator knows but don't abort the release.
        print(f"  WARN: jsDelivr purge failed: {exc}")


def commit_and_push(version: str, do_push: bool, dry_run: bool) -> None:
    if dry_run:
        print(f"[dry-run] git add platformio.ini ota/manifest.json binaries/")
        print(f"[dry-run] git commit -m 'chore(release): v{version}'")
        if do_push:
            print(f"[dry-run] git push")
        return

    run(["git", "add", "platformio.ini", "ota/manifest.json", "binaries/"])
    diff = run_quiet(["git", "diff", "--cached", "--quiet"])
    if diff.returncode == 0:
        print("Nothing to commit (platformio.ini + manifest + binaries already up to date).")
    else:
        run(["git", "commit", "-m", f"chore(release): v{version}"])

    if do_push:
        run(["git", "push"])


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("version", help="Semantic version, e.g. 1.0.1 (no leading 'v')")
    parser.add_argument("--envs", nargs="+", default=DEFAULT_ENVS,
                        help=f"PlatformIO envs to build (default: {DEFAULT_ENVS})")
    parser.add_argument("--notes", default=None,
                        help="Release notes (markdown). Mutually exclusive with --notes-file.")
    parser.add_argument("--notes-file", type=Path, default=None,
                        help="Read release notes from a file.")
    parser.add_argument("--auto-notes", action="store_true",
                        help="Let gh auto-generate release notes from commits since the last tag.")
    parser.add_argument("--draft", action="store_true",
                        help="Create the GitHub Release as a draft.")
    parser.add_argument("--repo", default=None,
                        help="Override the GitHub repo slug (default: parse `git remote origin`).")
    parser.add_argument("--no-push", action="store_true",
                        help="Commit but don't push (useful for reviewing locally first).")
    parser.add_argument("--no-commit", action="store_true",
                        help="Skip git add/commit/push entirely.")
    parser.add_argument("--no-release", action="store_true",
                        help="Skip `gh release create` (build + manifest only).")
    parser.add_argument("--dry-run", action="store_true",
                        help="Print every action without executing it.")
    args = parser.parse_args()

    if not re.match(r"^\d+\.\d+\.\d+(-[\w.]+)?$", args.version):
        sys.exit(f"ERROR: version must be semver (X.Y.Z or X.Y.Z-suffix), got '{args.version}'")

    if args.notes and args.notes_file:
        sys.exit("ERROR: pass --notes or --notes-file, not both.")

    # Always validate tooling, even in dry-run, so the dry-run actually proves
    # a real run would proceed past preflight.
    check_tools()

    repo_slug = args.repo or detect_repo_slug()
    print(f"=== Releasing v{args.version} for {', '.join(args.envs)} to {repo_slug} ===")

    if not args.dry_run:
        assert_clean_worktree()
        if not args.no_release:
            assert_tag_unused(args.version)

    update_platformio_version(args.version, args.dry_run)

    binaries: List[Path] = []
    for env_name in args.envs:
        src = build_env(env_name, args.dry_run)
        binaries.append(stage_binary(env_name, src, args.dry_run))
        # Also drop a versioned copy under binaries/ so jsDelivr can serve
        # it. This is the URL the device actually downloads from — the
        # gh-release asset is just for human visibility.
        commit_binary_for_cdn(env_name, args.version, src, args.dry_run)

    # Create the Release first so the manifest URL is live before any device
    # has a chance to read the bumped manifest.
    if not args.no_release:
        create_release(
            args.version, args.notes, args.notes_file, binaries,
            generate_notes=args.auto_notes, draft=args.draft, dry_run=args.dry_run,
        )

    update_manifest(args.version, args.envs, repo_slug, args.dry_run)

    if not args.no_commit:
        commit_and_push(args.version, do_push=not args.no_push, dry_run=args.dry_run)

    # Purge jsDelivr's CDN cache for the manifest path so devices see the bump
    # without waiting on the default ~12 h TTL.
    if not args.no_release:
        purge_jsdelivr(repo_slug, args.dry_run)

    print()
    if args.dry_run:
        print("Dry run complete. Re-run without --dry-run to actually release.")
    else:
        print(f"Done. Devices will pick up v{args.version} on their next 6h poll.")
        print(f"For an immediate push, publish an empty MQTT message to:")
        print(f"  ir_hub/<mac>/ota/check")


if __name__ == "__main__":
    main()
