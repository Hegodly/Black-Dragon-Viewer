#!/usr/bin/env python3
"""CI-gated, PR-validated promotion into the independent KAMI Windows viewer."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

DESTINATION_BRANCH = "feature/kami-live-viewer-integration"
WINDOWS_WORKFLOW_ID = 405470198
PROMOTION_BATCH_LIMIT = 3
SOURCE_CI_EVENTS = frozenset({"push", "workflow_dispatch"})
VIEWER_ARTIFACT = "KAMI-Native-Viewer-Candidate"
REQUIRED_FEATURES = (
    "ui", "dx12_world", "terrain", "mesh_residency", "sl_login",
    "sl_transport", "avatar", "inventory", "messaging", "read_receipts",
    "camera", "graphics", "creator_studio", "photo_studio", "smart_sort",
    "accessibility", "hotfixes",
)
ALLOWED_SOURCE_BRANCHES = frozenset({
    "feature/kami-original-core",
    "feature/kami-original-viewer-reintegration",
    "feature/kami-r72-black-dragon-geometry-port",
    "feature/kami-r88-dx12-renderer-migration",
    "feature/kami-r89-dx12-renderer-phase2",
    "feature/kami-original-creator-photo-studios",
    "feature/kami-native-feature-restoration",
    "feature/kami-native-camera-system",
    "feature/kami-live-simulator-udp-and-login-ui",
    "feature/kami-dx12-only-live-region-bridge",
    "feature/kami-sl-xmlrpc-login-and-sim-handshake",
    "feature/kami-live-sl-connectivity-foundations",
    "feature/kami-avatar-body-physics-dx12",
    "feature/kami-avatar-locomotion-dx12",
    "feature/kami-native-messenger-offline-preview",
})
# These R72 source commits have already been faithfully ported and tested on
# the destination under different destination commit IDs.
ALREADY_PORTED_SOURCE_PREFIXES = frozenset({
    "cfb1071f96", "7883f98a99", "ea69a77922", "103245bcec",
    "1b34e2ad10", "648581a529", "1929304e0b", "cfbe844b08",
    "111f689833", "dbb7d295ee", "7b71916866",
})
IMPLEMENTATION_PREFIXES = ("src/", "include/", "resources/", "assets/")
TEST_PREFIX = "tests/"
ROOT_IMPLEMENTATION_FILES = frozenset({"CMakeLists.txt"})
SHA_RE = re.compile(r"^[0-9a-f]{40}$")


def latest_workflow_run(
    runs: list[dict[str, Any]]
) -> dict[str, Any] | None:
    if not runs:
        return None
    return max(
        runs,
        key=lambda item: (
            str(item.get("created_at", "")),
            int(item.get("run_number", 0)),
        ),
    )


def latest_successful_workflow_run(
    runs: list[dict[str, Any]]
) -> dict[str, Any] | None:
    latest = latest_workflow_run(runs)
    if (
        latest is not None
        and latest.get("status") == "completed"
        and latest.get("conclusion") == "success"
    ):
        return latest
    return None


def has_successful_windows_job(jobs: list[dict[str, Any]]) -> bool:
    return any(
        job.get("name") == "windows"
        and job.get("status") == "completed"
        and job.get("conclusion") == "success"
        for job in jobs
    )


def windows_job_succeeded(run: dict[str, Any]) -> bool:
    run_id = run.get("id")
    if not run_id:
        return False
    jobs = api_request(f"actions/runs/{run_id}/jobs?per_page=100").get("jobs", [])
    return has_successful_windows_job(jobs)


def parse_patch_equivalent_commits(output: str) -> set[str]:
    """Return source SHAs that git-cherry marks as already represented upstream."""
    equivalent: set[str] = set()
    for line in output.splitlines():
        fields = line.strip().split()
        if len(fields) == 2 and fields[0] == "-" and SHA_RE.fullmatch(fields[1]):
            equivalent.add(fields[1])
    return equivalent


def manifest_errors(manifest: dict[str, Any], live_acceptance: str = "") -> list[str]:
    errors: list[str] = []
    if manifest.get("application") != "KAMIViewer.exe":
        errors.append("The manifest must target the independent KAMIViewer.exe.")
    if not isinstance(manifest.get("release_allowed"), bool):
        errors.append("release_allowed must be an explicit JSON boolean.")
    features = manifest.get("features")
    if not isinstance(features, dict):
        return errors + ["The feature manifest has no features object."]
    missing = [name for name in REQUIRED_FEATURES if name not in features]
    if missing:
        errors.append("Missing required feature gates: " + ", ".join(missing))
    for name, entry in features.items():
        if not isinstance(entry, dict):
            errors.append(f"Feature {name} must be an object.")
            continue
        if entry.get("status") not in {"unverified", "partial", "verified"}:
            errors.append(f"Feature {name} has an invalid status.")
        if not isinstance(entry.get("acceptance"), str) or not entry["acceptance"].strip():
            errors.append(f"Feature {name} has no acceptance criteria.")
        dependencies = entry.get("dependencies")
        if not isinstance(dependencies, list):
            errors.append(f"Feature {name} must declare a dependencies array.")
        elif any(not isinstance(dep, str) or not dep.strip() for dep in dependencies):
            errors.append(f"Feature {name} contains an empty dependency.")
        else:
            for dep in dependencies:
                if dep not in features:
                    errors.append(f"Feature {name} depends on unknown feature {dep}.")

    state: dict[str, int] = {}
    stack: list[str] = []

    def visit(name: str) -> None:
        if state.get(name) == 2:
            return
        if state.get(name) == 1:
            errors.append("Feature dependency cycle: " + " -> ".join(stack + [name]))
            return
        state[name] = 1
        stack.append(name)
        entry = features.get(name, {})
        dependencies = entry.get("dependencies", []) if isinstance(entry, dict) else []
        if not isinstance(dependencies, list):
            dependencies = []
        for dep in dependencies:
            if isinstance(dep, str) and dep in features:
                visit(dep)
        stack.pop()
        state[name] = 2

    for name in features:
        visit(name)

    if manifest.get("release_allowed") is True:
        blockers = manifest.get("release_blockers")
        if not isinstance(blockers, list) or blockers:
            errors.append("Release cannot be enabled while blockers remain.")
        if any(
            not isinstance(entry, dict) or entry.get("status") != "verified"
            for entry in features.values()
        ):
            errors.append("Release requires every feature to be verified.")
        if "LIVE_SL_ACCEPTANCE: PASS" not in live_acceptance:
            errors.append("Release requires authenticated live SL acceptance evidence.")
    return errors


def promotion_decision(
    source_branch: str, changed_files: list[str], parent_count: int
) -> tuple[bool, str]:
    if source_branch == DESTINATION_BRANCH:
        return False, "The destination branch is not a promotion source."
    if source_branch not in ALLOWED_SOURCE_BRANCHES:
        return False, "The source branch is not on the reviewed integration allowlist."
    if parent_count != 1:
        return False, "Merge commits are not auto-promoted; select a single implementation commit."
    if not changed_files:
        return False, "The source commit has no inspectable changed files."
    allowed = all(
        path.startswith(IMPLEMENTATION_PREFIXES)
        or path.startswith(TEST_PREFIX)
        or path in ROOT_IMPLEMENTATION_FILES
        for path in changed_files
    )
    if not allowed:
        return False, "The commit mixes implementation with workflow, documentation, binary, or unrelated files."
    has_implementation = any(
        path.startswith(IMPLEMENTATION_PREFIXES) or path in ROOT_IMPLEMENTATION_FILES
        for path in changed_files
    )
    if not has_implementation:
        return False, "Test-only changes are not viewer implementation."
    return True, "Implementation commit is eligible for isolated destination validation."


def is_already_ported_source(sha: str) -> bool:
    return any(sha.startswith(prefix) for prefix in ALREADY_PORTED_SOURCE_PREFIXES)


def repo_name() -> str:
    return os.environ.get("GITHUB_REPOSITORY", "Hegodly/Black-Dragon-Viewer")


def api_request(
    path: str,
    payload: dict[str, Any] | None = None,
    method: str | None = None,
) -> dict[str, Any]:
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if not token:
        raise RuntimeError("GITHUB_TOKEN is required for guarded repository integration.")
    url = "https://api.github.com/repos/" + repo_name() + "/" + path.lstrip("/")
    actual_method = method or ("GET" if payload is None else "POST")
    body = None if payload is None else json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=body,
        method=actual_method,
        headers={
            "Authorization": "Bearer " + token,
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "Content-Type": "application/json",
            "User-Agent": "KAMI-Unified-Integration",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            raw = response.read()
            return json.loads(raw.decode("utf-8")) if raw else {}
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"GitHub API {exc.code} for {path}: {detail[:1200]}") from exc


def git(args: list[str], check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["git", *args], text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    if check and result.returncode:
        raise RuntimeError("git " + " ".join(args) + " failed:\n" + result.stdout[-5000:])
    return result


def ref_sha(branch: str) -> str:
    encoded = urllib.parse.quote(branch, safe="/")
    result = api_request("git/ref/heads/" + encoded)
    return result.get("object", {}).get("sha", "")


def runs_for_branch(branch: str, status: str | None = None) -> list[dict[str, Any]]:
    query: dict[str, str] = {"branch": branch, "per_page": "100"}
    if status:
        query["status"] = status
    result = api_request("actions/runs?" + urllib.parse.urlencode(query))
    return result.get("workflow_runs", [])


def runs_for_sha(sha: str) -> list[dict[str, Any]]:
    query = urllib.parse.urlencode({"head_sha": sha, "per_page": "100"})
    return api_request("actions/runs?" + query).get("workflow_runs", [])


def active_destination_runs() -> list[dict[str, Any]]:
    current_id = os.environ.get("GITHUB_RUN_ID", "")
    active_by_id: dict[str, dict[str, Any]] = {}
    for status in ("queued", "in_progress"):
        for run in runs_for_branch(DESTINATION_BRANCH, status):
            active_by_id[str(run.get("id", ""))] = run

        # Destination PR builds have the source branch as head_branch, not
        # DESTINATION_BRANCH. Inspect PR base refs so competing promotions
        # and other destination-targeted work are visible before any write.
        query = urllib.parse.urlencode({"status": status, "per_page": "100"})
        for run in api_request("actions/runs?" + query).get("workflow_runs", []):
            head_branch = str(run.get("head_branch", ""))
            # These runs are either skipped source-branch Windows jobs or the
            # no-op unified workflow on a temporary promotion PR. Neither
            # writes the destination branch.
            if run.get("name") in {"Pull Request Labeler", "Check CLA"}:
                continue
            if (
                run.get("workflow_id") == WINDOWS_WORKFLOW_ID
                and head_branch in ALLOWED_SOURCE_BRANCHES
            ):
                continue
            if (
                run.get("name") == "KAMI Unified Viewer Integration"
                and head_branch.startswith("kami/promotion-")
            ):
                continue
            targets_destination = any(
                (pull.get("base") or {}).get("ref") == DESTINATION_BRANCH
                for pull in (run.get("pull_requests") or [])
            )
            if targets_destination:
                active_by_id[str(run.get("id", ""))] = run

    return [
        run for run_id, run in active_by_id.items()
        if run_id and run_id != current_id
    ]


def state_path() -> Path:
    root = Path(os.environ.get("RUNNER_TEMP", os.environ.get("TEMP", ".")))
    return root / "kami-unified-integration-state.json"


def save_state(state: dict[str, Any]) -> None:
    state_path().write_text(json.dumps(state, indent=2), encoding="utf-8")


def load_state() -> dict[str, Any]:
    path = state_path()
    if not path.exists():
        raise RuntimeError("Promotion state is missing; preflight did not complete.")
    return json.loads(path.read_text(encoding="utf-8"))


def write_output(eligible: bool, reason: str, destination_sha: str = "") -> None:
    output = os.environ.get("GITHUB_OUTPUT")
    if output:
        with open(output, "a", encoding="utf-8") as stream:
            stream.write(f"eligible={'true' if eligible else 'false'}\n")
            stream.write("reason=" + reason.replace("\n", " ") + "\n")
            stream.write("destination_sha=" + destination_sha + "\n")


def summary(markdown: str) -> None:
    target = os.environ.get("GITHUB_STEP_SUMMARY")
    if target:
        with open(target, "a", encoding="utf-8") as stream:
            stream.write(markdown.rstrip() + "\n")


def event_context() -> tuple[str, str, int, bool] | None:
    event_path = os.environ.get("GITHUB_EVENT_PATH")
    if not event_path:
        raise RuntimeError("GITHUB_EVENT_PATH is unavailable.")
    event = json.loads(Path(event_path).read_text(encoding="utf-8"))
    if os.environ.get("GITHUB_EVENT_NAME") != "pull_request":
        return None
    pr = event.get("pull_request", {})
    if pr.get("draft"):
        return None
    head = pr.get("head", {})
    source_repo = (head.get("repo") or {}).get("full_name")
    if source_repo != repo_name():
        return None
    branch = str(head.get("ref", ""))
    sha = str(head.get("sha", ""))
    number = int(pr.get("number", 0))
    if not number:
        return None
    return branch, sha, number, True


def source_windows_runs(
    source_branch: str, source_sha: str
) -> list[dict[str, Any]]:
    return [
        run for run in runs_for_sha(source_sha)
        if run.get("workflow_id") == WINDOWS_WORKFLOW_ID
        and run.get("head_branch") == source_branch
        and run.get("event") in SOURCE_CI_EVENTS
    ]


def successful_run_for_sha(
    source_branch: str, source_sha: str
) -> dict[str, Any] | None:
    # A prior green run must not mask a newer failed or still-running attempt.
    latest = latest_successful_workflow_run(
        source_windows_runs(source_branch, source_sha)
    )
    return latest if latest is not None and windows_job_succeeded(latest) else None


def wait_for_source_head_ci(
    source_branch: str, source_sha: str, after_run_id: int | None = None
) -> dict[str, Any] | None:
    deadline = time.monotonic() + 30 * 60
    while time.monotonic() < deadline:
        latest = latest_workflow_run(source_windows_runs(source_branch, source_sha))
        if latest is not None and (
            after_run_id is None or int(latest.get("id", 0)) != after_run_id
        ):
            if latest.get("status") == "completed":
                if (
                    latest.get("conclusion") == "success"
                    and windows_job_succeeded(latest)
                ):
                    return latest
                return None
        time.sleep(15)
    return None


def ensure_source_head_ci(
    source_branch: str, source_sha: str
) -> dict[str, Any] | None:
    latest = latest_workflow_run(source_windows_runs(source_branch, source_sha))
    if latest is not None and latest.get("status") in {"queued", "in_progress"}:
        return wait_for_source_head_ci(source_branch, source_sha)
    if (
        latest is not None
        and latest.get("status") == "completed"
        and latest.get("conclusion") == "success"
        and windows_job_succeeded(latest)
    ):
        return latest

    previous_run_id = int(latest["id"]) if latest and latest.get("id") else None
    # Feature branches without a current Windows build are tested on their
    # exact source ref. The destination candidate is built separately later.
    api_request(
        f"actions/workflows/{WINDOWS_WORKFLOW_ID}/dispatches",
        {"ref": source_branch},
        method="POST",
    )
    return wait_for_source_head_ci(source_branch, source_sha, previous_run_id)


def is_ancestor(ancestor: str, descendant: str) -> bool:
    return git(["merge-base", "--is-ancestor", ancestor, descendant], check=False).returncode == 0


def choose_verified_implementations(
    source_branch: str, source_head: str, destination_sha: str,
    head_run: dict[str, Any],
) -> tuple[list[tuple[dict[str, Any], dict[str, Any]]], bool] | None:
    query = urllib.parse.urlencode({"sha": source_branch, "per_page": "100"})
    history = api_request("commits?" + query)
    patch_equivalent = parse_patch_equivalent_commits(
        git([
            "cherry",
            "refs/remotes/origin/kami-destination",
            "refs/remotes/origin/kami-source",
        ]).stdout
    )
    selected: list[tuple[dict[str, Any], dict[str, Any]]] = []
    more_candidates = False

    # GitHub returns newest-first. Promote in source ancestry order so a later
    # caps/reparenting fix cannot be cherry-picked before the helpers it uses.
    for item in reversed(history):
        sha = item.get("sha", "")
        if (
            not SHA_RE.fullmatch(sha)
            or is_already_ported_source(sha)
            or sha in patch_equivalent
        ):
            continue
        if is_ancestor(sha, destination_sha):
            continue
        details = api_request("commits/" + sha)
        files = [entry.get("filename", "") for entry in details.get("files", [])]
        if len(files) >= 300:
            continue
        eligible, _ = promotion_decision(
            source_branch, files, len(details.get("parents", []))
        )
        if not eligible:
            continue
        if sha == source_head:
            run = head_run
        else:
            run = successful_run_for_sha(source_branch, sha)
            if run is None and not source_windows_runs(source_branch, sha):
                # The green source-head build includes this ancestor commit;
                # the destination candidate build then verifies the cherry-pick.
                run = head_run
        if run is None:
            continue
        selected.append((details, run))
        if len(selected) > PROMOTION_BATCH_LIMIT:
            more_candidates = True
            selected.pop()
            break

    has_regression_tests = any(
        entry.get("filename", "").startswith(TEST_PREFIX)
        for details, _ in selected
        for entry in details.get("files", [])
    )
    if not selected or not has_regression_tests:
        return None
    return selected, more_candidates


def preflight() -> None:
    context = event_context()
    if context is None:
        reason = "Only ready, same-repository promotion pull requests targeting the destination are eligible."
        write_output(False, reason)
        summary("## KAMI promotion skipped\n\n" + reason)
        return
    source_branch, source_head, source_pr, require_head = context
    if (
        not source_branch or not SHA_RE.fullmatch(source_head)
        or source_branch not in ALLOWED_SOURCE_BRANCHES
    ):
        reason = "Source branch or commit is invalid or outside the reviewed allowlist."
        write_output(False, reason)
        summary("## KAMI promotion skipped\n\n" + reason)
        return
    if source_branch == DESTINATION_BRANCH:
        reason = "The destination branch cannot promote itself."
        write_output(False, reason)
        summary("## KAMI promotion skipped\n\n" + reason)
        return

    manifest = json.loads(Path("docs/KAMI_FEATURE_ACCEPTANCE.json").read_text(encoding="utf-8"))
    acceptance_path = Path("docs/KAMI_LIVE_SL_ACCEPTANCE.md")
    acceptance = acceptance_path.read_text(encoding="utf-8") if acceptance_path.exists() else ""
    errors = manifest_errors(manifest, acceptance)
    if errors:
        raise RuntimeError("Feature manifest validation failed: " + "; ".join(errors))

    if ref_sha(source_branch) != source_head:
        reason = "The source branch advanced after the pull-request event; this stale run will not promote."
        write_output(False, reason)
        summary("## KAMI promotion skipped\n\n" + reason)
        return
    if active_destination_runs():
        reason = "A queued or running destination workflow exists; promotion is deferred to avoid a race."
        write_output(False, reason)
        summary("## KAMI promotion skipped\n\n" + reason)
        return

    head_run = ensure_source_head_ci(source_branch, source_head)
    if head_run is None:
        reason = "The exact source branch head has no successful KAMI Windows CI run."
        write_output(False, reason)
        summary("## KAMI promotion skipped\n\n" + reason)
        return
    if ref_sha(source_branch) != source_head:
        reason = "The source branch advanced while its CI was running; stale promotion was cancelled."
        write_output(False, reason)
        summary("## KAMI promotion skipped\n\n" + reason)
        return

    git(["fetch", "origin", f"refs/heads/{source_branch}:refs/remotes/origin/kami-source"])
    git(["fetch", "origin", f"refs/heads/{DESTINATION_BRANCH}:refs/remotes/origin/kami-destination"])
    destination_sha = ref_sha(DESTINATION_BRANCH)
    remote_destination = git(["rev-parse", "refs/remotes/origin/kami-destination"]).stdout.strip()
    if destination_sha != remote_destination:
        reason = "Destination changed during preflight; retry against its new head."
        write_output(False, reason, destination_sha)
        summary("## KAMI promotion skipped\n\n" + reason)
        return
    if is_ancestor(source_head, destination_sha):
        reason = "The source branch head is already contained in destination history."
        write_output(False, reason, destination_sha)
        summary("## KAMI promotion skipped\n\n" + reason)
        return

    chosen = choose_verified_implementations(
        source_branch, source_head, destination_sha, head_run
    )
    if chosen is None:
        reason = (
            "No ordered batch of unported implementation commits with exact "
            "successful push CI and at least one regression test was found."
        )
        write_output(False, reason, destination_sha)
        summary("## KAMI promotion skipped\n\n" + reason)
        return
    implementations, more_candidates = chosen
    source_shas = [details["sha"] for details, _ in implementations]
    source_sha = source_shas[-1]
    source_runs = [
        {
            "sha": details["sha"],
            "run_id": run.get("id"),
            "run_url": run.get("html_url", ""),
        }
        for details, run in implementations
    ]
    files = sorted({
        entry.get("filename", "")
        for details, _ in implementations
        for entry in details.get("files", [])
    })
    state = {
        "eligible": True,
        "source_branch": source_branch,
        "source_head": source_head,
        "source_pr": source_pr,
        "source_sha": source_sha,
        "source_shas": source_shas,
        "source_runs": source_runs,
        "more_candidates": more_candidates,
        "source_head_run_url": head_run.get("html_url", ""),
        "destination_sha": destination_sha,
        "files": files,
    }
    save_state(state)
    write_output(
        True,
        f"{len(source_shas)} ordered implementation commits and the source head have green Windows CI.",
        destination_sha,
    )
    commit_lines = "\n".join(
        f"  - {item['sha']} — {item['run_url']}"
        for item in source_runs
    )
    summary(
        "## KAMI promotion preflight passed\n\n"
        f"- Source PR: #{source_pr} from {source_branch}\n"
        f"- Source head: {source_head}\n"
        f"- Ordered implementation batch:\n{commit_lines}\n"
        f"- Source-head CI: {head_run.get('html_url', '')}\n"
        f"- Destination head lease: {destination_sha}\n"
        f"- More eligible commits remain: {str(more_candidates).lower()}\n"
        f"- Changed files: {', '.join(files)}"
    )


def cherry_pick() -> None:
    state = load_state()
    if not state.get("eligible"):
        raise RuntimeError("Preflight did not authorize promotion.")
    expected = state["destination_sha"]
    source_branch = state["source_branch"]
    if ref_sha(DESTINATION_BRANCH) != expected or active_destination_runs():
        raise RuntimeError("Destination moved or another workflow started after preflight.")
    if ref_sha(source_branch) != state["source_head"]:
        raise RuntimeError("Source branch changed after preflight.")
    git(["fetch", "origin", f"refs/heads/{source_branch}:refs/remotes/origin/kami-source"])
    git(["fetch", "origin", f"refs/heads/{DESTINATION_BRANCH}:refs/remotes/origin/kami-destination"])
    for source_sha in state["source_shas"]:
        if not is_ancestor(source_sha, "refs/remotes/origin/kami-source"):
            raise RuntimeError(f"Selected source commit {source_sha} is no longer reachable from its branch.")
    if git(["rev-parse", "refs/remotes/origin/kami-destination"]).stdout.strip() != expected:
        raise RuntimeError("Destination remote ref changed; no cherry-pick was made.")
    git(["checkout", "--detach", expected])
    git(["config", "user.name", "github-actions[bot]"])
    git(["config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com"])
    applied: list[str] = []
    for source_sha in state["source_shas"]:
        result = git(["cherry-pick", "-x", source_sha], check=False)
        if result.returncode:
            git(["cherry-pick", "--abort"], check=False)
            raise RuntimeError(
                "The ordered source implementation batch does not apply cleanly "
                f"at {source_sha}; no destination branch was changed:\n"
                + result.stdout[-5000:]
            )
        applied.append(git(["rev-parse", "HEAD"]).stdout.strip())
    new_sha = git(["rev-parse", "HEAD"]).stdout.strip()
    state["new_shas"] = applied
    state["new_sha"] = new_sha
    save_state(state)
    summary(
        "## KAMI implementation batch applied to a local validation tree\n\n"
        f"- Source implementations: {', '.join(state['source_shas'])}\n"
        f"- Destination cherry-picks: {', '.join(applied)}\n"
        f"- Local destination candidate: {new_sha}\n"
        "- No destination branch has been changed; full native build, CTest and WARP run next."
    )


def wait_for_promotion_pr_ci(pr_number: int, branch: str) -> tuple[dict[str, Any], dict[str, Any]]:
    deadline = time.monotonic() + 55 * 60
    while time.monotonic() < deadline:
        matches = [
            run for run in runs_for_branch(branch)
            if run.get("workflow_id") == WINDOWS_WORKFLOW_ID
            and any(int(pr.get("number", 0)) == pr_number for pr in run.get("pull_requests", []))
        ]
        if matches:
            latest = max(matches, key=lambda item: item.get("created_at", ""))
            if latest.get("status") == "completed":
                if latest.get("conclusion") != "success":
                    raise RuntimeError(
                        "Destination-targeted Windows CI failed for promotion PR: "
                        + latest.get("html_url", "")
                    )
                artifacts = api_request(
                    f"actions/runs/{latest['id']}/artifacts"
                ).get("artifacts", [])
                artifact = next(
                    (
                        item for item in artifacts
                        if item.get("name") == VIEWER_ARTIFACT
                        and not item.get("expired")
                        and item.get("size_in_bytes", 0) > 0
                    ),
                    None,
                )
                if artifact is None:
                    raise RuntimeError(
                        "Promotion PR CI passed but did not publish the native viewer candidate artifact: "
                        + latest.get("html_url", "")
                    )
                return latest, artifact
        time.sleep(15)
    raise RuntimeError("Timed out waiting for destination-targeted Windows CI on the promotion PR.")


def open_pr_and_verify() -> None:
    state = load_state()
    if not state.get("eligible") or not state.get("new_sha"):
        raise RuntimeError("No locally validated promotion commit exists.")
    expected = state["destination_sha"]
    source_branch = state["source_branch"]
    new_sha = state["new_sha"]
    if ref_sha(DESTINATION_BRANCH) != expected or active_destination_runs():
        raise RuntimeError("Destination moved or another workflow started before validation PR creation.")
    if ref_sha(source_branch) != state["source_head"]:
        raise RuntimeError("Source branch changed before validation PR creation.")
    if git(["rev-parse", "HEAD"]).stdout.strip() != new_sha:
        raise RuntimeError("Local candidate commit changed after build and WARP validation.")
    if git(["rev-parse", "HEAD^"]).stdout.strip() != expected:
        raise RuntimeError("Candidate is not a direct child of the checked destination head.")

    run_id = os.environ.get("GITHUB_RUN_ID", str(int(time.time())))
    promotion_branch = f"kami/promotion-{state['source_sha'][:10]}-{run_id}"
    git(["switch", "-c", promotion_branch])
    git(["push", "origin", f"HEAD:refs/heads/{promotion_branch}"])
    state["promotion_branch"] = promotion_branch
    save_state(state)

    source_commit_lines = "\\n".join(
        f"- {item['sha']} — [source Windows CI]({item['run_url']})"
        for item in state["source_runs"]
    )
    body = (
        "Automated KAMI source promotion.\n\n"
        f"- Source branch: {source_branch}\n"
        f"- Ordered source implementation commits:\\n{source_commit_lines}\n"
        f"- Source head: {state['source_head']}\n"
        f"- Source-head Windows CI: {state['source_head_run_url']}\n"
        f"- Expected destination base: {expected}\n"
        f"- Local build/CTest/WARP candidate: {new_sha}\n\n"
        "This PR exists so the destination branch's own Windows workflow can validate "
        "the exact candidate tree before it is merged. It contains the actual KAMI "
        "implementation source, not a diagnostic executable."
    )
    pr = api_request("pulls", {
        "title": f"KAMI promotion: {state['source_sha'][:10]} from {source_branch}",
        "head": promotion_branch,
        "base": DESTINATION_BRANCH,
        "body": body,
        "draft": False,
    })
    pr_number = int(pr["number"])
    state["promotion_pr"] = pr_number
    state["promotion_pr_url"] = pr.get("html_url", "")
    save_state(state)
    summary(
        "## KAMI destination validation PR opened\n\n"
        f"- Promotion PR: {state['promotion_pr_url']}\n"
        f"- Candidate commit: {new_sha}\n"
        "- Waiting for the destination-targeted Windows build, CTest, WARP and native artifact."
    )

    ci_run, artifact = wait_for_promotion_pr_ci(pr_number, promotion_branch)
    current_pr = api_request(f"pulls/{pr_number}")
    if current_pr.get("state") != "open":
        raise RuntimeError("Promotion PR was closed before the destination CI gate completed.")
    if current_pr.get("head", {}).get("sha") != new_sha:
        raise RuntimeError("Promotion PR head changed after its CI run.")
    if current_pr.get("base", {}).get("sha") != expected:
        raise RuntimeError("Destination base changed while promotion CI was running; refusing merge.")
    if ref_sha(DESTINATION_BRANCH) != expected or active_destination_runs():
        raise RuntimeError("Destination head or active-run state changed before merge.")

    tested_sha = ci_run.get("head_sha", "")
    if tested_sha:
        tested_commit = api_request("commits/" + tested_sha)
        candidate_commit = api_request("commits/" + new_sha)
        if tested_commit.get("commit", {}).get("tree", {}).get("sha") != candidate_commit.get("commit", {}).get("tree", {}).get("sha"):
            raise RuntimeError("Destination PR CI did not test the exact candidate source tree.")

    merged = api_request(
        f"pulls/{pr_number}/merge",
        {"sha": new_sha, "merge_method": "merge"},
    )
    if not merged.get("merged"):
        raise RuntimeError("GitHub refused the validated promotion PR merge: " + str(merged.get("message", "")))
    merge_sha = str(merged.get("sha", ""))
    if not SHA_RE.fullmatch(merge_sha) or ref_sha(DESTINATION_BRANCH) != merge_sha:
        raise RuntimeError("Destination branch did not advance to the expected promotion merge commit.")

    candidate_commit = api_request("commits/" + new_sha)
    merge_commit = api_request("commits/" + merge_sha)
    if candidate_commit.get("commit", {}).get("tree", {}).get("sha") != merge_commit.get("commit", {}).get("tree", {}).get("sha"):
        raise RuntimeError("Merged destination tree differs from the CI-validated candidate tree.")

    artifact_url = (
        f"https://github.com/{repo_name()}/actions/runs/{ci_run['id']}/artifacts/{artifact['id']}"
    )
    state["destination_sha"] = merge_sha
    state["destination_ci_url"] = ci_run.get("html_url", "")
    state["artifact_url"] = artifact_url
    save_state(state)
    summary(
        "## KAMI original-viewer source promotion verified\n\n"
        f"- Source branch head: {state['source_head']}\n"
        f"- Ordered source implementations: {', '.join(state['source_shas'])}\n"
        f"- Source implementation CI: {', '.join(item['run_url'] for item in state['source_runs'])}\n"
        f"- Destination cherry-picks: {', '.join(state['new_shas'])}\n"
        f"- Promotion PR: {state['promotion_pr_url']}\n"
        f"- Destination merge commit: {merge_sha}\n"
        f"- Destination-targeted Windows CI: {state['destination_ci_url']}\n"
        f"- Native viewer candidate artifact: [{VIEWER_ARTIFACT}]({artifact_url})\n"
        "- The PR CI validated the exact merged tree. The candidate remains explicitly incomplete "
        "until authenticated live SL acceptance passes."
    )
    if state.get("more_candidates"):
        try:
            source_pr = api_request(f"pulls/{state['source_pr']}")
            if source_pr.get("state") == "open":
                api_request(
                    f"pulls/{state['source_pr']}",
                    {"state": "closed"},
                    method="PATCH",
                )
                api_request(
                    f"pulls/{state['source_pr']}",
                    {"state": "open"},
                    method="PATCH",
                )
                summary(
                    "\\nThe source PR was safely reopened to queue the next ordered "
                    "batch of independently CI-verified implementation commits."
                )
            else:
                summary(
                    "\\nAdditional source commits remain, but the source PR was closed "
                    "manually; it was not reopened."
                )
        except Exception as exc:
            summary("\\nAdditional source commits remain; automatic follow-up could not be queued: " + str(exc))
    try:
        api_request(
            "git/refs/heads/" + urllib.parse.quote(promotion_branch, safe="/"),
            method="DELETE",
        )
    except Exception:
        pass


def validate_manifest() -> None:
    manifest = json.loads(Path("docs/KAMI_FEATURE_ACCEPTANCE.json").read_text(encoding="utf-8"))
    acceptance_path = Path("docs/KAMI_LIVE_SL_ACCEPTANCE.md")
    acceptance = acceptance_path.read_text(encoding="utf-8") if acceptance_path.exists() else ""
    errors = manifest_errors(manifest, acceptance)
    if errors:
        raise RuntimeError("Feature manifest invalid: " + "; ".join(errors))
    print(
        f"KAMI manifest valid: {len(manifest['features'])} features, "
        f"{len(REQUIRED_FEATURES)} required gates, "
        f"release_allowed={str(manifest['release_allowed']).lower()}."
    )


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: kami_unified_integration.py validate-manifest|preflight|cherry-pick|open-pr-and-verify")
        return 2
    command = sys.argv[1]
    try:
        if command == "validate-manifest":
            validate_manifest()
        elif command == "preflight":
            preflight()
        elif command == "cherry-pick":
            cherry_pick()
        elif command == "open-pr-and-verify":
            open_pr_and_verify()
        else:
            print("unknown command: " + command)
            return 2
    except Exception as exc:
        summary("## KAMI integration blocked\n\n" + str(exc))
        print(str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
