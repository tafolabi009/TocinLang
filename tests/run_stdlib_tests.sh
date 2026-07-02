#!/usr/bin/env bash
#
# run_stdlib_tests.sh — JIT end-to-end runner for tests that need the Tocin
# runtime (vectors/maps/strings/alloc, module imports, std.testing). These
# can't run under the lli-based scripts/run_to_tests.sh because lli cannot
# resolve the __tocin_* runtime symbols, so they live in tests/jit/ and run
# via `tocin --run` (the JIT registers the runtime in-process).
#
# Pass criteria per file: exit code 0, unless it declares `// expect: N`
# (then exit must equal N). std.testing's testSummary() already returns
# nonzero on any failed check, so those files self-report.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOCIN="${TOCIN:-$ROOT/build/tocin}"
DIR="${1:-$ROOT/tests/jit}"
export TOCIN_PATH="${TOCIN_PATH:-$ROOT/stdlib}"

fail=0
for t in "$DIR"/*.to; do
  [ -e "$t" ] || continue
  name="$(basename "$t")"
  want="$(grep -m1 -oE '// *expect: *[0-9]+' "$t" | grep -oE '[0-9]+' || true)"
  [ -n "$want" ] || want=0
  out="$("$TOCIN" "$t" --run 2>&1)"; got=$?
  if [ "$got" -eq "$want" ]; then
    echo "PASS  $name  ($(printf '%s\n' "$out" | tail -1))"
  else
    echo "FAIL  $name  (exit $got, expected $want)"
    printf '%s\n' "$out" | tail -6
    fail=1
  fi
done
exit $fail
