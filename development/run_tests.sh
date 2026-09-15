#!/usr/bin/env bash
# Test runner for Project F.
#
#   ./run_tests.sh              lexer mode: every tests/*.f must tokenize without errors
#   ./run_tests.sh ./f_interpreter
#                               interpreter mode: stdout is diffed against tests/*.out,
#                               and *.err tests only have to exit non-zero

set -uo pipefail
cd "$(dirname "$0")"

TESTS_DIR="tests"
INTERPRETER="${1:-}"
pass=0
fail=0

if [[ -n "$INTERPRETER" ]]; then
    mode="interpreter"
    if [[ ! -x "$INTERPRETER" ]]; then
        echo "Error: '$INTERPRETER' is not executable." >&2
        exit 1
    fi
else
    mode="lexer"
    BIN="$(mktemp -d)/lexer"
    trap 'rm -rf "$(dirname "$BIN")"' EXIT
    echo "Building lexer..."
    g++ -std=c++17 -Wall -Wextra -O2 -o "$BIN" lexer.cpp || exit 1
fi

echo "Running $mode tests against $TESTS_DIR/"
echo

for src in "$TESTS_DIR"/*.f; do
    name="$(basename "$src" .f)"
    expected_out="$TESTS_DIR/$name.out"
    expected_err="$TESTS_DIR/$name.err"

    if [[ "$mode" == "lexer" ]]; then
        # The lexer only decides whether the source tokenizes. A test whose
        # failure is semantic (division by zero) still has to lex cleanly.
        if stderr="$("$BIN" "$src" 2>&1 >/dev/null)"; then
            echo "PASS  $name (lexes)"
            ((pass++))
        else
            echo "FAIL  $name (lexer errors)"
            echo "$stderr" | sed 's/^/        /'
            ((fail++))
        fi
        continue
    fi

    if [[ -f "$expected_err" ]]; then
        # Negative test: only the exit status matters.
        if "$INTERPRETER" "$src" >/dev/null 2>&1; then
            echo "FAIL  $name (expected a non-zero exit code, got 0)"
            ((fail++))
        else
            echo "PASS  $name (fails as expected)"
            ((pass++))
        fi
    elif [[ -f "$expected_out" ]]; then
        actual="$("$INTERPRETER" "$src" 2>/dev/null)"
        if diff -u <(printf '%s\n' "$actual") "$expected_out" >/dev/null; then
            echo "PASS  $name"
            ((pass++))
        else
            echo "FAIL  $name"
            diff -u "$expected_out" <(printf '%s\n' "$actual") | sed 's/^/        /'
            ((fail++))
        fi
    else
        echo "SKIP  $name (no .out or .err file)"
    fi
done

echo
echo "$pass passed, $fail failed"
[[ $fail -eq 0 ]]
