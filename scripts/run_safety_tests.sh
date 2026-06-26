#!/usr/bin/env bash
# Safety harness for compile-time and codegen-level guarantees.
#  - *reassign*  : reassigning a `const` must be REJECTED with a T013 diagnostic.
#  - *bounds*    : a checked array access must emit the __tocin_oob bounds-check
#                  call in the generated IR (the runtime panic is exercised
#                  separately; here we just confirm the check is inserted).
set -u
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOCIN="${TOCIN:-$REPO_ROOT/build/tocin}"
DIR="$REPO_ROOT/tests/safety"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0 fail=0
for f in "$DIR"/*.to; do
    name="$(basename "$f")"
    case "$name" in
        *reassign*)
            out="$("$TOCIN" "$f" -o "$TMP/o.ll" 2>&1)"; rc=$?
            if [[ $rc -ne 0 && "$out" == *T013* ]]; then
                echo "PASS  $name  (rejected: T013 const reassignment)"; pass=$((pass+1))
            else
                echo "FAIL  $name  (should be REJECTED with T013): rc=$rc $out"; fail=$((fail+1))
            fi
            ;;
        *bounds*)
            "$TOCIN" "$f" -o "$TMP/b.ll" >/dev/null 2>&1
            if grep -q "__tocin_oob" "$TMP/b.ll" 2>/dev/null; then
                echo "PASS  $name  (bounds check emitted)"; pass=$((pass+1))
            else
                echo "FAIL  $name  (no __tocin_oob bounds check in IR)"; fail=$((fail+1))
            fi
            ;;
        *bound*)
            out="$("$TOCIN" "$f" -o "$TMP/g.ll" 2>&1)"; rc=$?
            if [[ $rc -ne 0 && "$out" == *T016* ]]; then
                echo "PASS  $name  (rejected: T016 unsatisfied trait bound)"; pass=$((pass+1))
            else
                echo "FAIL  $name  (should be REJECTED with T016): rc=$rc $out"; fail=$((fail+1))
            fi
            ;;
    esac
done
echo "========================================================"
echo "Safety: $((pass+fail)) cases, $pass passed, $fail failed"
[[ $fail -eq 0 ]]
