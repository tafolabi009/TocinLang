#!/usr/bin/env bash
# Exhaustiveness harness for algebraic-enum `match`. Files whose name contains
# "ok" must compile cleanly; all others must be REJECTED with a P001
# non-exhaustive-match diagnostic (a match that does not cover every variant and
# has no `default:` arm is a fatal error).
set -u
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOCIN="${TOCIN:-$REPO_ROOT/build/tocin}"
DIR="$REPO_ROOT/tests/match"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0 fail=0
for f in "$DIR"/*.to; do
    name="$(basename "$f")"
    out="$("$TOCIN" "$f" -o "$TMP/o.ll" 2>&1)"; rc=$?
    if [[ "$name" == *ok* ]]; then
        if [[ $rc -eq 0 ]]; then echo "PASS  $name  (accepted, as expected)"; pass=$((pass+1))
        else echo "FAIL  $name  (should be ACCEPTED): $out"; fail=$((fail+1)); fi
    else
        if [[ $rc -ne 0 && "$out" == *P001* ]]; then echo "PASS  $name  (rejected: P001 non-exhaustive)"; pass=$((pass+1))
        else echo "FAIL  $name  (should be REJECTED with P001): rc=$rc $out"; fail=$((fail+1)); fi
    fi
done
echo "========================================================"
echo "Match-exhaustiveness: $((pass+fail)) cases, $pass passed, $fail failed"
[[ $fail -eq 0 ]]
