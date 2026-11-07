import unittest

from tools.kami_unified_integration import (
    ALREADY_PORTED_SOURCE_PREFIXES,
    DESTINATION_BRANCH,
    REQUIRED_FEATURES,
    has_successful_windows_job,
    is_already_ported_source,
    latest_successful_workflow_run,
    manifest_errors,
    parse_patch_equivalent_commits,
    promotion_decision,
)


def valid_manifest():
    features = {
        name: {
            "status": "partial" if name == "ui" else "unverified",
            "dependencies": [],
            "acceptance": f"Verify {name} in the actual KAMIViewer application.",
        }
        for name in REQUIRED_FEATURES
    }
    return {
        "application": "KAMIViewer.exe",
        "release_allowed": False,
        "release_blockers": ["Live SL acceptance is pending."],
        "features": features,
    }


class FeatureManifestTests(unittest.TestCase):
    def test_accepts_explicit_incomplete_native_viewer_manifest(self):
        self.assertEqual(manifest_errors(valid_manifest()), [])

    def test_rejects_wrong_application_target(self):
        manifest = valid_manifest()
        manifest["application"] = "SevenInOneTest.exe"
        self.assertTrue(any("KAMIViewer.exe" in item for item in manifest_errors(manifest)))

    def test_rejects_unknown_dependency(self):
        manifest = valid_manifest()
        manifest["features"]["dx12_world"]["dependencies"] = ["missing"]
        self.assertTrue(any("unknown feature" in item for item in manifest_errors(manifest)))

    def test_rejects_dependency_cycle(self):
        manifest = valid_manifest()
        manifest["features"]["ui"]["dependencies"] = ["dx12_world"]
        manifest["features"]["dx12_world"]["dependencies"] = ["ui"]
        self.assertTrue(any("cycle" in item.lower() for item in manifest_errors(manifest)))

    def test_release_requires_verified_features_and_live_acceptance(self):
        manifest = valid_manifest()
        manifest["release_allowed"] = True
        manifest["release_blockers"] = []
        errors = manifest_errors(manifest)
        self.assertTrue(any("verified" in item for item in errors))
        self.assertTrue(any("live SL" in item for item in errors))


class PromotionPolicyTests(unittest.TestCase):
    branch = "feature/kami-r72-black-dragon-geometry-port"

    def test_allows_single_source_change_with_regression_test(self):
        eligible, _ = promotion_decision(
            self.branch,
            ["src/protocol/simulator_transport.cpp", "tests/object_spatial_tests.cpp"],
            1,
        )
        self.assertTrue(eligible)

    def test_rejects_test_only_commit(self):
        self.assertFalse(
            promotion_decision(self.branch, ["tests/object_spatial_tests.cpp"], 1)[0]
        )

    def test_rejects_docs_workflow_and_mixed_scope(self):
        for paths in (
            ["README.md"],
            [".github/workflows/windows.yml"],
            ["src/world/renderer.cpp", "docs/R72_STATUS.md"],
        ):
            with self.subTest(paths=paths):
                self.assertFalse(promotion_decision(self.branch, paths, 1)[0])

    def test_rejects_merge_commits(self):
        self.assertFalse(
            promotion_decision(self.branch, ["src/world/renderer.cpp"], 2)[0]
        )

    def test_rejects_unreviewed_source_branch(self):
        self.assertFalse(
            promotion_decision("feature/unknown", ["src/world/renderer.cpp"], 1)[0]
        )

    def test_destination_is_never_its_own_source(self):
        self.assertFalse(
            promotion_decision(
                DESTINATION_BRANCH, ["src/world/renderer.cpp"], 1
            )[0]
        )

    def test_known_r72_fixes_are_recorded_as_already_ported(self):
        self.assertTrue(ALREADY_PORTED_SOURCE_PREFIXES)
        self.assertTrue(is_already_ported_source("cfb1071f96" + "0" * 30))




class SourceCiSelectionTests(unittest.TestCase):
    def test_newer_failure_overrides_an_older_green_run(self):
        runs = [
            {
                "created_at": "2026-11-07T08:00:00Z",
                "status": "completed",
                "conclusion": "success",
            },
            {
                "created_at": "2026-11-07T08:05:00Z",
                "status": "completed",
                "conclusion": "failure",
            },
        ]
        self.assertIsNone(latest_successful_workflow_run(runs))

    def test_newer_in_progress_attempt_blocks_an_older_green_run(self):
        runs = [
            {
                "created_at": "2026-11-07T08:00:00Z",
                "status": "completed",
                "conclusion": "success",
            },
            {
                "created_at": "2026-11-07T08:05:00Z",
                "status": "in_progress",
                "conclusion": None,
            },
        ]
        self.assertIsNone(latest_successful_workflow_run(runs))

    def test_latest_completed_green_attempt_is_accepted(self):
        latest = {
            "created_at": "2026-11-07T08:05:00Z",
            "status": "completed",
            "conclusion": "success",
        }
        self.assertEqual(
            latest_successful_workflow_run([
                {
                    "created_at": "2026-11-07T08:00:00Z",
                    "status": "completed",
                    "conclusion": "failure",
                },
                latest,
            ]),
            latest,
        )


class PatchEquivalenceTests(unittest.TestCase):
    def test_git_cherry_equivalent_commits_are_parsed(self):
        a = "a" * 40
        b = "b" * 40
        output = f"- {a}\n+ {b}\n"
        self.assertEqual(parse_patch_equivalent_commits(output), {a})




class WindowsJobAcceptanceTests(unittest.TestCase):
    def test_requires_actual_windows_job_success(self):
        self.assertTrue(has_successful_windows_job([
            {"name": "source-boundary", "status": "completed", "conclusion": "success"},
            {"name": "windows", "status": "completed", "conclusion": "success"},
        ]))

    def test_skipped_windows_job_does_not_count_as_ci(self):
        self.assertFalse(has_successful_windows_job([
            {"name": "source-boundary", "status": "completed", "conclusion": "success"},
            {"name": "windows", "status": "completed", "conclusion": "skipped"},
        ]))


if __name__ == "__main__":
    unittest.main()
