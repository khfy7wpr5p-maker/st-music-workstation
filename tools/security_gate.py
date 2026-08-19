#!/usr/bin/env python3
"""Dependency-free repository security baseline checks for ST Music Workstation."""

from __future__ import annotations

from dataclasses import dataclass
from fnmatch import fnmatch
from pathlib import Path
import re
import sys
from typing import Iterable

MAX_FILE_BYTES = 5 * 1024 * 1024

SKIP_DIRS = {
    ".git",
    "__pycache__",
    ".pytest_cache",
}

GENERATED_DIRS = {
    "build",
    "out",
    "dist",
    "CMakeFiles",
}

SENSITIVE_NAME_PATTERNS = (
    ".env",
    ".env.*",
    "*.pem",
    "*.key",
    "*.p12",
    "*.pfx",
    "id_rsa",
    "id_ed25519",
    "credentials*.json",
    "secrets*.json",
)

SENSITIVE_NAME_ALLOWLIST = {
    ".env.example",
}

COMPILED_ARTIFACT_EXTENSIONS = {
    ".a",
    ".class",
    ".dll",
    ".dylib",
    ".exe",
    ".jar",
    ".lib",
    ".o",
    ".obj",
    ".so",
}

SECRET_PATTERNS = (
    (
        "private-key",
        re.compile(
            rb"-----BEGIN (?:RSA |EC |DSA |OPENSSH )?PRIVATE KEY-----"
        ),
    ),
    (
        "aws-access-key",
        re.compile(rb"\b(?:AKIA|ASIA)[0-9A-Z]{16}\b"),
    ),
    (
        "github-token",
        re.compile(rb"\b(?:ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9]{30,}\b"),
    ),
    (
        "github-fine-grained-token",
        re.compile(rb"\bgithub_pat_[A-Za-z0-9_]{20,}\b"),
    ),
    (
        "google-api-key",
        re.compile(rb"\bAIza[0-9A-Za-z_-]{30,}\b"),
    ),
    (
        "slack-token",
        re.compile(rb"\bxox[baprs]-[0-9A-Za-z-]{10,}\b"),
    ),
    (
        "openai-style-key",
        re.compile(rb"\bsk-[A-Za-z0-9_-]{20,}\b"),
    ),
)

EXTERNAL_ACTION_REF = re.compile(r"^[^@\s]+@([0-9a-fA-F]{40})$")
USES_LINE = re.compile(
    r"^\s*(?:-\s*)?uses\s*:\s*['\"]?([^#\s'\"]+)",
    re.MULTILINE,
)
PULL_REQUEST_TARGET = re.compile(r"\bpull_request_target\b")
SECRET_REFERENCE = re.compile(r"\$\{\{\s*secrets(?:\.|\s*\[)")
WRITE_PERMISSION = re.compile(r"\b[A-Za-z0-9_-]+\s*:\s*write\b")
TOP_LEVEL_PERMISSIONS = re.compile(r"^permissions\s*:\s*$", re.MULTILINE)
TOP_LEVEL_WRITE_ALL = re.compile(r"\bwrite-all\b")
UNTRUSTED_EVENT_EXPRESSION = re.compile(r"\$\{\{\s*github\.event\.")
REMOTE_PIPE_TO_SHELL = re.compile(
    r"\b(?:curl|wget)\b[^\n|]*\|\s*(?:bash|sh)\b",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class Finding:
    code: str
    path: str
    message: str

    def format(self) -> str:
        return f"{self.code}: {self.path}: {self.message}"


def _relative(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def iter_repository_files(root: Path) -> Iterable[Path]:
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if any(part in SKIP_DIRS for part in relative.parts):
            continue
        yield path


def sensitive_filename(relative_path: str) -> bool:
    name = Path(relative_path).name
    if name in SENSITIVE_NAME_ALLOWLIST:
        return False
    return any(fnmatch(name, pattern) for pattern in SENSITIVE_NAME_PATTERNS)


def scan_secret_bytes(data: bytes, relative_path: str) -> list[Finding]:
    findings: list[Finding] = []
    for code, pattern in SECRET_PATTERNS:
        if pattern.search(data):
            findings.append(
                Finding(
                    code=f"SECRET_{code.upper().replace('-', '_')}",
                    path=relative_path,
                    message="high-confidence credential/private-key pattern detected",
                )
            )
    return findings


def _run_blocks(workflow_text: str) -> list[str]:
    """Extract simple YAML run blocks without requiring a YAML dependency."""
    lines = workflow_text.splitlines()
    blocks: list[str] = []
    index = 0
    while index < len(lines):
        line = lines[index]
        match = re.match(r"^(\s*)(?:-\s*)?run\s*:\s*(.*)$", line)
        if not match:
            index += 1
            continue

        indent = len(match.group(1))
        tail = match.group(2)
        block_lines: list[str] = []
        if tail and tail not in {"|", ">", "|-", ">-"}:
            block_lines.append(tail)

        index += 1
        while index < len(lines):
            next_line = lines[index]
            if not next_line.strip():
                block_lines.append(next_line)
                index += 1
                continue
            next_indent = len(next_line) - len(next_line.lstrip())
            if next_indent <= indent:
                break
            block_lines.append(next_line.strip())
            index += 1

        blocks.append("\n".join(block_lines))
    return blocks


def scan_workflow_text(workflow_text: str, relative_path: str) -> list[Finding]:
    findings: list[Finding] = []

    if PULL_REQUEST_TARGET.search(workflow_text):
        findings.append(
            Finding(
                "WORKFLOW_PULL_REQUEST_TARGET",
                relative_path,
                "pull_request_target is prohibited by the baseline security policy",
            )
        )

    if not TOP_LEVEL_PERMISSIONS.search(workflow_text):
        findings.append(
            Finding(
                "WORKFLOW_PERMISSIONS_MISSING",
                relative_path,
                "workflow must declare explicit top-level permissions",
            )
        )

    if TOP_LEVEL_WRITE_ALL.search(workflow_text) or WRITE_PERMISSION.search(workflow_text):
        findings.append(
            Finding(
                "WORKFLOW_WRITE_PERMISSION",
                relative_path,
                "write permissions require a later explicit security-policy revision",
            )
        )

    if SECRET_REFERENCE.search(workflow_text):
        findings.append(
            Finding(
                "WORKFLOW_SECRET_REFERENCE",
                relative_path,
                "baseline PR validation workflows must not consume repository secrets",
            )
        )

    for match in USES_LINE.finditer(workflow_text):
        action_ref = match.group(1)
        if action_ref.startswith("./"):
            continue
        if action_ref.startswith("docker://"):
            findings.append(
                Finding(
                    "WORKFLOW_UNPINNED_CONTAINER_ACTION",
                    relative_path,
                    f"container action must use a separately reviewed immutable digest: {action_ref}",
                )
            )
            continue
        if not EXTERNAL_ACTION_REF.match(action_ref):
            findings.append(
                Finding(
                    "WORKFLOW_UNPINNED_ACTION",
                    relative_path,
                    f"external action must be pinned to a full 40-character commit SHA: {action_ref}",
                )
            )

    for run_block in _run_blocks(workflow_text):
        if UNTRUSTED_EVENT_EXPRESSION.search(run_block):
            findings.append(
                Finding(
                    "WORKFLOW_UNTRUSTED_EVENT_IN_RUN",
                    relative_path,
                    "github.event data must not be interpolated directly into shell run blocks",
                )
            )
        if REMOTE_PIPE_TO_SHELL.search(run_block):
            findings.append(
                Finding(
                    "WORKFLOW_REMOTE_PIPE_TO_SHELL",
                    relative_path,
                    "remote download piped directly to a shell is prohibited",
                )
            )

    return findings


def scan_repository(root: Path) -> list[Finding]:
    root = root.resolve()
    findings: list[Finding] = []

    for path in iter_repository_files(root):
        relative_path = _relative(path, root)
        relative = Path(relative_path)

        if sensitive_filename(relative_path):
            findings.append(
                Finding(
                    "SENSITIVE_FILENAME",
                    relative_path,
                    "tracked/local repository tree contains a credential-prone filename",
                )
            )

        if any(part in GENERATED_DIRS or part.startswith("cmake-build-") for part in relative.parts):
            findings.append(
                Finding(
                    "GENERATED_OUTPUT",
                    relative_path,
                    "generated/build output must not be committed",
                )
            )

        if path.suffix.lower() in COMPILED_ARTIFACT_EXTENSIONS:
            findings.append(
                Finding(
                    "COMPILED_ARTIFACT",
                    relative_path,
                    "compiled/binary artifact requires an explicit provenance exception",
                )
            )

        try:
            size = path.stat().st_size
        except OSError as exc:
            findings.append(Finding("FILE_STAT_ERROR", relative_path, str(exc)))
            continue

        if size > MAX_FILE_BYTES:
            findings.append(
                Finding(
                    "OVERSIZED_FILE",
                    relative_path,
                    f"file is {size} bytes; baseline limit is {MAX_FILE_BYTES} bytes",
                )
            )
            continue

        try:
            data = path.read_bytes()
        except OSError as exc:
            findings.append(Finding("FILE_READ_ERROR", relative_path, str(exc)))
            continue

        findings.extend(scan_secret_bytes(data, relative_path))

        if relative.parts[:2] == (".github", "workflows") and path.suffix.lower() in {".yml", ".yaml"}:
            try:
                workflow_text = data.decode("utf-8")
            except UnicodeDecodeError:
                findings.append(
                    Finding(
                        "WORKFLOW_ENCODING",
                        relative_path,
                        "workflow must be valid UTF-8 text",
                    )
                )
                continue
            findings.extend(scan_workflow_text(workflow_text, relative_path))

    return findings


def main(argv: list[str]) -> int:
    root = Path(argv[1]) if len(argv) > 1 else Path(".")
    if not root.exists() or not root.is_dir():
        print(f"security gate: invalid repository root: {root}", file=sys.stderr)
        return 2

    findings = scan_repository(root)
    if findings:
        print("security gate: FAIL")
        for finding in sorted(findings, key=lambda item: (item.path, item.code, item.message)):
            print(f"- {finding.format()}")
        return 1

    print("security gate: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
