#!/usr/bin/env bash
# Borrow-checker harness. Runs the opt-in `--borrow-check` analysis over the
# samples in tests/borrow/. Files whose name contains "ok" must compile cleanly
# WITH the flag; all others must be REJECTED (non-zero exit + a B001 diagnostic)
# with the flag, and must STILL compile cleanly WITHOUT it (proving opt-in).
set -u
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOCIN="${TOCIN:-$REPO_ROOT/build/tocin}"
DIR="$REPO_ROOT/tests/borrow"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0 fail=0
for f in "$DIR"/*.to; do
    name="$(basename "$f")"
    # 1. Must always compile WITHOUT the flag.
    if ! "$TOCIN" "$f" -o "$TMP/o.ll" >/dev/null 2>&1; then
        echo "FAIL  $name  (should compile WITHOUT --borrow-check, but did not)"
        fail=$((fail+1)); continue
    fi
    # 2. With the flag: "ok" files must pass; others must be rejected with an
    #    ownership diagnostic (B001 use-after-move or B002 borrow conflict).
    out="$("$TOCIN" --borrow-check "$f" -o "$TMP/o2.ll" 2>&1)"; rc=$?
    if [[ "$name" == *ok* ]]; then
        if [[ $rc -eq 0 ]]; then echo "PASS  $name  (accepted, as expected)"; pass=$((pass+1))
        else echo "FAIL  $name  (should be ACCEPTED under --borrow-check): $out"; fail=$((fail+1)); fi
    else
        if [[ $rc -ne 0 && ( "$out" == *B001* || "$out" == *B002* ) ]]; then code=B001; [[ "$out" == *B002* ]] && code=B002; echo "PASS  $name  (rejected: $code)"; pass=$((pass+1))
        else echo "FAIL  $name  (should be REJECTED with B001/B002 under --borrow-check): rc=$rc $out"; fail=$((fail+1)); fi
    fi
done
echo "========================================================"
echo "Borrow-check: $((pass+fail)) cases, $pass passed, $fail failed"
[[ $fail -eq 0 ]]
