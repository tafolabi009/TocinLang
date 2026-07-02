#!/usr/bin/env bash
# Run every Tocin-level stdlib test (tests/cases/stdlib_*.to) via the compiler's
# JIT and assert a zero exit code (each uses std.testing's testSummary, which
# returns 0 only if all its checks passed). Fails loudly on the first bad exit.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOCIN="${TOCIN:-$ROOT/build/tocin}"
fail=0
for t in "$ROOT"/tests/cases/stdlib_*.to; do
  name="$(basename "$t")"
  if out="$("$TOCIN" "$t" --run 2>&1)"; then
    summary="$(printf '%s\n' "$out" | tail -1)"
    echo "PASS  $name  ($summary)"
  else
    echo "FAIL  $name"
    printf '%s\n' "$out" | tail -5
    fail=1
  fi
done
exit $fail
