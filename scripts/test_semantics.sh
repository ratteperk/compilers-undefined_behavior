#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BIN="$ROOT_DIR/fcompiler"

if [[ ! -x "$BIN" ]]; then
    echo "Error: $BIN not found. Compile project first via 'task default'." >&2
    exit 1
fi

pass=0
fail=0

echo "Running Semantic Rejection Tests (tests/sema/fail/):"
for src in "$ROOT_DIR"/tests/sema/fail/*.f; do
    [[ -e "$src" ]] || continue
    name="$(basename "$src" .f)"
    if "$BIN" --check-only "$src" >/dev/null 2>&1; then
        echo "FAIL (expected error, but passed): $name"
        ((fail++))
    else
        echo "PASS (rejected as expected): $name"
        ((pass++))
    fi
done

echo
echo "Running AST Optimization Tests (tests/opt/):"
for src in "$ROOT_DIR"/tests/opt/*.f; do
    [[ -e "$src" ]] || continue
    name="$(basename "$src" .f)"
    expected_out="$ROOT_DIR/tests/opt/$name.out"

    if [[ ! -f "$expected_out" ]]; then
        echo "SKIP: $name (no matching .out file)"
        continue
    fi

    actual="$("$BIN" "$src" 2>/dev/null)"
    if diff -u <(printf '%s\n' "$actual") "$expected_out" >/dev/null; then
        echo "PASS (opt): $name"
        ((pass++))
    else
        echo "FAIL (opt): $name"
        diff -u "$expected_out" <(printf '%s\n' "$actual") | sed 's/^/        /'
        ((fail++))
    fi
done

echo
echo "Results: $pass passed, $fail failed."
[[ $fail -eq 0 ]]
