import unittest

from tools.security_gate import scan_workflow_text


class SecurityGateYamlVariantTests(unittest.TestCase):
    def test_inline_pull_request_target_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request_target
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps: []
"""
        findings = scan_workflow_text(workflow, ".github/workflows/unsafe.yml")
        self.assertIn("WORKFLOW_PULL_REQUEST_TARGET", {finding.code for finding in findings})

    def test_list_inline_pull_request_target_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: [pull_request, pull_request_target]
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps: []
"""
        findings = scan_workflow_text(workflow, ".github/workflows/unsafe.yml")
        self.assertIn("WORKFLOW_PULL_REQUEST_TARGET", {finding.code for finding in findings})

    def test_job_inline_write_permission_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request
permissions:
  contents: read
jobs:
  test:
    permissions: { contents: write }
    runs-on: ubuntu-latest
    steps: []
"""
        findings = scan_workflow_text(workflow, ".github/workflows/unsafe.yml")
        self.assertIn("WORKFLOW_WRITE_PERMISSION", {finding.code for finding in findings})

    def test_job_write_all_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request
permissions:
  contents: read
jobs:
  test:
    permissions: write-all
    runs-on: ubuntu-latest
    steps: []
"""
        findings = scan_workflow_text(workflow, ".github/workflows/unsafe.yml")
        self.assertIn("WORKFLOW_WRITE_PERMISSION", {finding.code for finding in findings})

    def test_spaced_unpinned_action_key_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses : actions/checkout@v7
"""
        findings = scan_workflow_text(workflow, ".github/workflows/unsafe.yml")
        self.assertIn("WORKFLOW_UNPINNED_ACTION", {finding.code for finding in findings})

    def test_quoted_full_sha_action_is_allowed(self) -> None:
        sha = "b" * 40
        workflow = f"""
name: safe
on: pull_request
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: "actions/checkout@{sha}"
"""
        findings = scan_workflow_text(workflow, ".github/workflows/safe.yml")
        codes = {finding.code for finding in findings}
        self.assertNotIn("WORKFLOW_UNPINNED_ACTION", codes)
        self.assertNotIn("WORKFLOW_WRITE_PERMISSION", codes)

    def test_bracket_secret_reference_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    env:
      TOKEN: ${{ secrets['TEST_TOKEN'] }}
    steps: []
"""
        findings = scan_workflow_text(workflow, ".github/workflows/unsafe.yml")
        self.assertIn("WORKFLOW_SECRET_REFERENCE", {finding.code for finding in findings})

    def test_spaced_run_key_with_untrusted_event_is_rejected(self) -> None:
        workflow = """
name: unsafe
on: pull_request
permissions:
  contents: read
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - run : echo "${{ github.event.pull_request.title }}"
"""
        findings = scan_workflow_text(workflow, ".github/workflows/unsafe.yml")
        self.assertIn("WORKFLOW_UNTRUSTED_EVENT_IN_RUN", {finding.code for finding in findings})


if __name__ == "__main__":
    unittest.main()
