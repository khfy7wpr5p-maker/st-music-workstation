from pathlib import Path
import unittest


WORKFLOW = Path(".github/workflows/security-baseline.yml")
CHECKOUT_SHA = "3d3c42e5aac5ba805825da76410c181273ba90b1"


class SecurityWorkflowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.workflow_text = WORKFLOW.read_text(encoding="utf-8")

    def test_workflow_keeps_pr_and_main_push_triggers(self) -> None:
        self.assertIn("pull_request:", self.workflow_text)
        self.assertIn("push:", self.workflow_text)
        self.assertIn("- main", self.workflow_text)

    def test_workflow_keeps_read_only_permissions(self) -> None:
        self.assertIn("permissions:\n  contents: read", self.workflow_text)

    def test_both_checkouts_use_immutable_reviewed_sha(self) -> None:
        self.assertEqual(2, self.workflow_text.count(f"actions/checkout@{CHECKOUT_SHA}"))
        self.assertEqual(2, self.workflow_text.count("persist-credentials: false"))

    def test_current_main_observer_is_pr_only_and_explicit(self) -> None:
        self.assertIn("current-main-security-baseline:", self.workflow_text)
        self.assertIn("if: github.event_name == 'pull_request'", self.workflow_text)
        self.assertIn("ref: main", self.workflow_text)

    def test_security_test_discovery_includes_all_security_tests(self) -> None:
        self.assertEqual(2, self.workflow_text.count("test_security_*.py"))


if __name__ == "__main__":
    unittest.main()
