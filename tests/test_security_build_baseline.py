from pathlib import Path
import re
import unittest


BUILD_WORKFLOW = Path(".github/workflows/build-baseline.yml")
ROOT_CMAKE = Path("CMakeLists.txt")
CHECKOUT_SHA = "3d3c42e5aac5ba805825da76410c181273ba90b1"


class BuildBaselineSecurityContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.workflow = BUILD_WORKFLOW.read_text(encoding="utf-8")
        cls.cmake = ROOT_CMAKE.read_text(encoding="utf-8")
        cls.cmake_casefold = cls.cmake.casefold()

    def test_build_workflow_is_least_privilege_and_pinned(self) -> None:
        self.assertIn("permissions:\n  contents: read", self.workflow)
        self.assertEqual(2, self.workflow.count(f"actions/checkout@{CHECKOUT_SHA}"))
        self.assertEqual(2, self.workflow.count("persist-credentials: false"))
        self.assertNotIn("secrets.", self.workflow)
        self.assertNotIn("pull_request_target", self.workflow)

    def test_build_workflow_uses_explicit_runner_major(self) -> None:
        self.assertEqual(2, self.workflow.count("runs-on: ubuntu-24.04"))
        self.assertNotIn("ubuntu-latest", self.workflow)

    def test_current_main_observer_is_pr_only_and_explicit(self) -> None:
        self.assertIn("current-main-build-test:", self.workflow)
        self.assertIn("if: github.event_name == 'pull_request'", self.workflow)
        self.assertIn("ref: main", self.workflow)

    def test_build_workflow_runs_candidate_and_current_main_builds(self) -> None:
        self.assertIn("Verify in-source build is rejected", self.workflow)
        self.assertIn("Verify current-main in-source build is rejected", self.workflow)
        self.assertEqual(2, self.workflow.count('cmake -S "$scratch" -B "$scratch"'))
        self.assertEqual(2, self.workflow.count("cmake -S . -B build"))
        self.assertEqual(2, self.workflow.count("cmake --build build"))
        self.assertEqual(2, self.workflow.count("ctest --test-dir build"))

    def test_baseline_cmake_has_no_network_or_process_bootstrap(self) -> None:
        forbidden_casefold = (
            "fetchcontent",
            "externalproject",
            "execute_process",
            "curl ",
            "wget ",
        )
        for token in forbidden_casefold:
            with self.subTest(token=token):
                self.assertNotIn(token, self.cmake_casefold)

        self.assertIsNone(
            re.search(r"file\s*\(\s*download\b", self.cmake, flags=re.IGNORECASE),
            "CMake file(DOWNLOAD ...) is prohibited by the baseline",
        )

    def test_cmake_requires_out_of_source_before_project_and_cxx20(self) -> None:
        guard = "CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR"
        self.assertIn(guard, self.cmake)
        self.assertIn("project(STMusicWorkstation", self.cmake)
        self.assertLess(self.cmake.index(guard), self.cmake.index("project(STMusicWorkstation"))
        self.assertIn("cxx_std_20", self.cmake)
        self.assertIn("CXX_EXTENSIONS NO", self.cmake)
        self.assertIn("include(CTest)", self.cmake)


if __name__ == "__main__":
    unittest.main()
