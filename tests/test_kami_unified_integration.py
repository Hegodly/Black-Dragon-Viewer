import unittest

from tools.kami_unified_integration import (
    ALREADY_PORTED_SOURCE_PREFIXES,
    DESTINATION_BRANCH,
    REQUIRED_FEATURES,
    batch_has_implementation_and_tests,
    has_successful_windows_job,
    is_pull_request_event,
    is_already_ported_source,
    latest_successful_workflow_run,
    manifest_errors,
    parse_patch_equivalent_commits,
    promotion_decision,
    source_commit_validation_run,
)


def valid_manifest():
    features = {
        name: {
            "status": "partial" if name == "ui" else "unverified",
            "dependencies": [],
            "acceptance": f"Verify {name} in the actual KAMIViewer application.",
            "evidence": [],
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



def live_evidence():
    return {
        "commit": "a" * 40,
        "ci_url": "https://github.com/Hegodly/Black-Dragon-Viewer/actions/runs/123",
        "native_app": True,
        "live_sl": True,
        "notes": "Authenticated live region checked in the native KAMIViewer.exe.",
    }


class FeatureEvidenceTests(unittest.TestCase):
    def test_verified_feature_requires_native_and_live_sl_evidence(self):
        manifest = valid_manifest()
        manifest["features"]["ui"]["status"] = "verified"
        errors = manifest_errors(manifest)
        self.assertTrue(any("native KAMIViewer.exe CI" in item for item in errors))

    def test_rejects_local_or_warp_only_evidence(self):
        manifest = valid_manifest()
        manifest["features"]["ui"]["status"] = "verified"
        evidence = live_evidence()
        evidence["live_sl"] = False
        manifest["features"]["ui"]["evidence"] = [evidence]
        self.assertTrue(any("authenticated live SL evidence" in item for item in manifest_errors(manifest)))

    def test_verified_feature_cannot_outpace_its_dependencies(self):
        manifest = valid_manifest()
        manifest["features"]["dx12_world"]["dependencies"] = ["sl_transport"]
        manifest["features"]["dx12_world"]["status"] = "verified"
        manifest["features"]["dx12_world"]["evidence"] = [live_evidence()]
        self.assertTrue(any("dependency sl_transport is not verified" in item for item in manifest_errors(manifest)))

    def test_verified_feature_passes_with_live_evidence_and_verified_dependencies(self):
        manifest = valid_manifest()
        manifest["features"]["sl_transport"]["status"] = "verified"
        manifest["features"]["sl_transport"]["evidence"] = [live_evidence()]
        manifest["features"]["dx12_world"]["dependencies"] = ["sl_transport"]
        manifest["features"]["dx12_world"]["status"] = "verified"
        manifest["features"]["dx12_world"]["evidence"] = [live_evidence()]
        self.assertEqual(manifest_errors(manifest), [])


class PromotionPolicyTests(unittest.TestCase):
    branch = "feature/kami-r72-black-dragon-geometry-port"

    def test_allows_single_source_change_with_regression_test(self):
        eligible, _ = promotion_decision(
            self.branch,
            ["src/protocol/simulator_transport.cpp", "tests/object_spatial_tests.cpp"],
            1,
        )
        self.assertTrue(eligible)

    def test_test_only_commit_is_allowed_only_as_batch_companion(self):
        test_files = ["tests/object_spatial_tests.cpp"]
        implementation_files = ["src/protocol/object_spatial.cpp"]
        self.assertTrue(promotion_decision(self.branch, test_files, 1)[0])
        self.assertFalse(batch_has_implementation_and_tests([test_files]))
        self.assertTrue(batch_has_implementation_and_tests(
            [implementation_files, test_files]
        ))

    def test_latest_r72_parent_promotion_fix_remains_eligible(self):
        source_head = "78267ff76ff32edb948d769dfd50e35b4f7489f6"
        self.assertFalse(is_already_ported_source(source_head))
        eligible, reason = promotion_decision(
            self.branch,
            [
                "include/kami/protocol/simulator_transport.hpp",
                "src/protocol/simulator_transport.cpp",
                "tests/simulator_transport_tests.cpp",
            ],
            1,
        )
        self.assertTrue(eligible, reason)

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


    def test_rejects_superseded_creator_photo_branch(self):
        self.assertFalse(
            promotion_decision(
                "feature/kami-original-creator-photo-studios",
                ["src/features/creator_studio.cpp", "tests/creator_studio_tests.cpp"],
                1,
            )[0]
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

    def test_known_r74_material_worker_port_is_recorded_as_already_ported(self):
        for sha in (
            "76849e6f6647b4be016961f3940c65c842928fba",
            "4ea64bab90673a1de9433725783c70896a447d0e",
            "1d43302d818d5f85d19ca1f3993641cf97162bc8",
            "a568c0213f3940da288ad3b7fcf440f32fcb3fc3",
            "67a11ef3b72332417d41a9b35ef226950a26cc8f",
        ):
            self.assertTrue(is_already_ported_source(sha), sha)

    def test_known_r72_fixes_are_recorded_as_already_ported(self):
        self.assertTrue(ALREADY_PORTED_SOURCE_PREFIXES)
        self.assertTrue(is_already_ported_source("cfb1071f96" + "0" * 30))
        already_ported = (
            "611953eabb1f49841da417cf3ad8040eab1a8237",
            "b45229c509c59884eb77ff8c4dfde1d4ae749308",
            "c9af8c4c9c8f6a35129cc78e399e8c446934bb84",
            "b8bd9a404da0bca53f753c03aa8d33f132504b6c",
            "09bde30d7467814945f0c128e9e1b1690a71e7fe",
            "b8f155ec60da5ea87e27f9f3a0535782a40b8613",
            "207723ab770040d2f7d06bbc9831c04e9f0eed76",
            "4e0f4ccadbd9046666c46ff5eadea0298c7ab12e",
            "df149710be03208014f0aa741157fe5e08388b19",
            "8f8feff8c040afead5ef54b58bd274f72f77a933",
            "3465e6daa84487cfcbc85ad226ad31cd897ec7b8",
            "1c2baca100dde790843d398d9c1d84bc273c9e67",
            "4bb04eba922e36bb1dace7ffca09fc8d53122b8e",
            "cd9fb7c9abd573808069155b2e49cc2bb6594ed9",
            "e2d030c34315c6090c0b5aba87779cff6f547784",
            "24f86786663e4cb67370584f60c1c5c5d155bf4",
            "56d19c09417b91b788dd58f9724d0bb91b508ba6",
            "028a0feb7b3588e8d1320a501e1025c59c325c91",
            "39d73fe41a6ea76efde60fabece6f81555127b4d",
            "6287d818d3ca6f6a7052176c7486db083f4dd445",
            "9e73a77eb4b3cce3bd1a5e0e7bac7260c0565953",
            "4de0d5d8f0a124b69eedbe729f6f6eaa4077cb22",
            "fbf845a18f0a0ccdc3ce26e2cd7d058b7ff9f57e",
        )
        for sha in already_ported:
            self.assertTrue(is_already_ported_source(sha), sha)




class PullRequestEventTests(unittest.TestCase):
    def test_accepts_only_trusted_base_branch_event(self):
        self.assertTrue(is_pull_request_event("pull_request_target"))
        self.assertFalse(is_pull_request_event("pull_request"))

    def test_rejects_non_pr_events(self):
        self.assertFalse(is_pull_request_event("workflow_call"))
        self.assertFalse(is_pull_request_event("push"))


class SourceCiSelectionTests(unittest.TestCase):
    def test_green_head_ci_covers_failed_ancestor_attempt(self):
        head_run = {"id": 42, "status": "completed", "conclusion": "success"}
        self.assertEqual(source_commit_validation_run(None, head_run), head_run)

    def test_successful_exact_commit_ci_is_preferred(self):
        exact_run = {"id": 41, "status": "completed", "conclusion": "success"}
        head_run = {"id": 42, "status": "completed", "conclusion": "success"}
        self.assertEqual(source_commit_validation_run(exact_run, head_run), exact_run)

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
