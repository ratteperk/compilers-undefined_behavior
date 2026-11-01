#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
TESTS_DIR="$ROOT_DIR/tests/runtime"
BIN="$ROOT_DIR/fcompiler"

INTERPRETER="${1:-}"
PARSER_MODE=0
if [[ "$INTERPRETER" == "--parser" ]]; then
    PARSER_MODE=1
    INTERPRETER=""
fi
pass=0
fail=0

if [[ -n "$INTERPRETER" ]]; then
    mode="interpreter"
    if [[ ! -x "$INTERPRETER" ]]; then
        echo "Error: '$INTERPRETER' is not executable." >&2
        exit 1
    fi
elif [[ $PARSER_MODE -eq 1 ]]; then
    mode="parser"
    echo "Running parser check on $TESTS_DIR..."
else
    mode="lexer"
    echo "Running lexer check on $TESTS_DIR..."
fi

for src in "$TESTS_DIR"/*.f; do
    [[ -e "$src" ]] || continue
    name="$(basename "$src" .f)"
    expected_out="$TESTS_DIR/$name.out"
    expected_err="$TESTS_DIR/$name.err"

    if [[ "$mode" == "lexer" ]]; then
        if "$BIN" --lex-only "$src" >/dev/null 2>&1; then
            echo "PASS  $name (lexer)"
            ((pass++))
        else
            echo "FAIL  $name (lexer error)"
            ((fail++))
        fi
        continue
    fi

    if [[ "$mode" == "parser" ]]; then
        if "$BIN" --parse-only "$src" >/dev/null 2>&1; then
            echo "PASS  $name (parser)"
            ((pass++))
        else
            echo "FAIL  $name (parser error)"
            ((fail++))
        fi
        continue
    fi

    # Режим интерпретатора
    if [[ -f "$expected_err" ]]; then
        if "$INTERPRETER" "$src" >/dev/null 2>&1; then
            echo "FAIL  $name (expected exit != 0)"
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
    fi
done

echo
echo "Results: $pass passed, $fail failed."
[[ $fail -eq 0 ]]
