#!/usr/bin/env bash
# Real shell: functions, traps, parameter expansion, pipelines.
set -euo pipefail

readonly ROOT="${1:-$(pwd)}"
declare -a FAILED=()

cleanup() {
    local code=$?
    [[ $code -ne 0 ]] && echo "failed with $code" >&2
    return $code
}
trap cleanup EXIT

check_file() {
    local path="$1"
    if [[ ! -r "$path" ]]; then
        FAILED+=("$path")
        return 1
    fi
    grep -c 'TODO' "$path" || true
}

find "$ROOT" -name '*.cpp' -print0 | while IFS= read -r -d '' f; do
    check_file "$f"
done
