#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${project_root}"

fail() {
    printf 'release candidate: %s\n' "$1" >&2
    exit 1
}

if [[ -n "$(git status --porcelain --untracked-files=all)" ]]; then
    git status --short >&2
    fail 'working tree must be clean before a release-candidate check'
fi

git diff --check
make clean
make all img
make smoke
make regression

if [[ -n "$(git status --porcelain --untracked-files=all)" ]]; then
    git status --short >&2
    fail 'verification left tracked or untracked source changes'
fi

printf 'release candidate: source commit %s\n' "$(git rev-parse --verify HEAD)"
printf 'release candidate: artifacts\n'
sha256sum myos.iso myos.img
printf 'release candidate: automated checks passed\n'
