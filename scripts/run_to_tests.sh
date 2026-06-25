#!/usr/bin/env bash
#
# run_to_tests.sh - Tocin end-to-end test runner.
#
# Compiles every tests/cases/*.to program with the `tocin` compiler and runs
# the resulting program, checking the actual exit code and/or stdout against an
# expectation encoded as a leading comment in each file:
#
#   // expect: <int>             expected process exit code (0-255)
#   // expect-output: <text>     expected exact stdout (single line)
#
# A file must declare at least one of the two. Both may be given.
#
# How a .to program is "run":
#   `tocin FILE -o OUT` emits LLVM IR to OUT.ll. We then execute that IR with
#   the LLVM interpreter (`lli`), whose process exit code is the value returned
#   by the program's main() and whose stdout is the program's stdout. This
#   exercises the full pipeline: lexer -> parser -> type checker -> codegen,
#   plus real runtime behaviour.
#
# Exit status: 0 if every case passes, non-zero otherwise.
#
# Environment overrides:
#   TOCIN   path to the tocin binary  (default: <build>/tocin)
#   LLI     path to the lli binary    (default: autodetected lli-18/lli)
#   CASES_DIR  directory of .to cases (default: tests/cases relative to repo)
#   TIMEOUT    per-program run timeout in seconds (default: 20)

set -u

# --- Locate repository root and key paths --------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CASES_DIR="${CASES_DIR:-${REPO_ROOT}/tests/cases}"
TIMEOUT="${TIMEOUT:-20}"

# --- Locate the tocin binary --------------------------------------------
find_tocin() {
    if [ -n "${TOCIN:-}" ] && [ -x "${TOCIN}" ]; then
        echo "${TOCIN}"; return 0
    fi
    for cand in \
        "${REPO_ROOT}/build/tocin" \
        "${REPO_ROOT}/build/Release/tocin" \
        "${REPO_ROOT}/build/Debug/tocin" \
        "${REPO_ROOT}/build/tocin.exe"; do
        if [ -x "${cand}" ]; then echo "${cand}"; return 0; fi
    done
    return 1
}

# --- Locate an IR runner (lli) ------------------------------------------
find_lli() {
    if [ -n "${LLI:-}" ] && command -v "${LLI}" >/dev/null 2>&1; then
        echo "${LLI}"; return 0
    fi
    for cand in lli-18 lli-17 lli /usr/lib/llvm-18/bin/lli; do
        if command -v "${cand}" >/dev/null 2>&1; then echo "${cand}"; return 0; fi
    done
    return 1
}

TOCIN_BIN="$(find_tocin || true)"
if [ -z "${TOCIN_BIN}" ]; then
    echo "ERROR: could not find the 'tocin' binary. Build it first:" >&2
    echo "  cmake -S . -B build -G Ninja -DWITH_V8=OFF -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm" >&2
    echo "  ninja -C build tocin" >&2
    exit 2
fi

LLI_BIN="$(find_lli || true)"
if [ -z "${LLI_BIN}" ]; then
    echo "ERROR: could not find 'lli' to execute compiled programs." >&2
    echo "  Install LLVM 18 (package 'llvm-18' provides lli-18) or set LLI=<path>." >&2
    exit 2
fi

# --- Locate the shared Tocin runtime (for channels/goroutines/exceptions) ---
# lli resolves libc symbols from its own process, but the __tocin_* runtime
# lives in our own library; -load it so programs using those features run.
find_runtime_so() {
    if [ -n "${TOCIN_RUNTIME_SO:-}" ] && [ -f "${TOCIN_RUNTIME_SO}" ]; then
        echo "${TOCIN_RUNTIME_SO}"; return 0
    fi
    local bindir; bindir="$(dirname "${TOCIN_BIN}")"
    for cand in \
        "${bindir}/libtocin_runtime.so" \
        "${bindir}/libtocin_runtime.dylib" \
        "${REPO_ROOT}/build/libtocin_runtime.so"; do
        if [ -f "${cand}" ]; then echo "${cand}"; return 0; fi
    done
    return 1
}
RUNTIME_SO="$(find_runtime_so || true)"

if [ ! -d "${CASES_DIR}" ]; then
    echo "ERROR: cases directory not found: ${CASES_DIR}" >&2
    exit 2
fi

echo "Tocin .to test runner"
echo "  tocin : ${TOCIN_BIN}"
echo "  lli   : ${LLI_BIN}"
echo "  rtlib : ${RUNTIME_SO:-<none found; runtime-dependent tests may fail>}"
echo "  cases : ${CASES_DIR}"
echo "========================================================"

# lli flag to load the runtime shared library, when available.
LLI_LOAD_ARGS=()
if [ -n "${RUNTIME_SO}" ]; then
    LLI_LOAD_ARGS=(-load "${RUNTIME_SO}")
fi

# --- Work directory for generated IR ------------------------------------
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/tocin_to_tests.XXXXXX")"
cleanup() { rm -rf "${WORK_DIR}"; }
trap cleanup EXIT

# --- Expectation parsing -------------------------------------------------
# Reads leading comment metadata. Sets the globals EXP_EXIT / EXP_OUTPUT and
# HAS_EXIT / HAS_OUTPUT.
parse_expectations() {
    local file="$1"
    EXP_EXIT=""
    EXP_OUTPUT=""
    HAS_EXIT=0
    HAS_OUTPUT=0
    local line
    while IFS= read -r line; do
        case "${line}" in
            *"// expect-output:"*)
                EXP_OUTPUT="${line#*// expect-output:}"
                EXP_OUTPUT="${EXP_OUTPUT# }"
                HAS_OUTPUT=1
                ;;
            *"// expect:"*)
                EXP_EXIT="${line#*// expect:}"
                # strip surrounding whitespace
                EXP_EXIT="$(echo "${EXP_EXIT}" | tr -d '[:space:]')"
                HAS_EXIT=1
                ;;
        esac
    done < "${file}"
}

# --- Counters ------------------------------------------------------------
PASS=0
FAIL=0
FAILED_NAMES=()

shopt -s nullglob
CASE_FILES=("${CASES_DIR}"/*.to)
shopt -u nullglob

if [ "${#CASE_FILES[@]}" -eq 0 ]; then
    echo "ERROR: no .to test cases found in ${CASES_DIR}" >&2
    exit 2
fi

for tofile in "${CASE_FILES[@]}"; do
    name="$(basename "${tofile}")"
    parse_expectations "${tofile}"

    if [ "${HAS_EXIT}" -eq 0 ] && [ "${HAS_OUTPUT}" -eq 0 ]; then
        echo "FAIL  ${name}  (no '// expect:' or '// expect-output:' directive)"
        FAIL=$((FAIL + 1)); FAILED_NAMES+=("${name}")
        continue
    fi

    base="${WORK_DIR}/${name%.to}"
    ll="${base}.ll"
    rm -f "${ll}"

    # Compile to LLVM IR. Pass the explicit ".ll" output path: tocin only
    # appends ".ll" when the output path contains no '.' at all, which is
    # unreliable when the temp directory name contains a dot, so name the
    # target file ourselves.
    if ! "${TOCIN_BIN}" "${tofile}" -o "${ll}" > "${base}.compile.log" 2>&1; then
        echo "FAIL  ${name}  (compiler returned non-zero)"
        sed 's/^/        /' "${base}.compile.log" | head -5
        FAIL=$((FAIL + 1)); FAILED_NAMES+=("${name}")
        continue
    fi
    if [ ! -s "${ll}" ]; then
        echo "FAIL  ${name}  (no IR produced)"
        sed 's/^/        /' "${base}.compile.log" | head -5
        FAIL=$((FAIL + 1)); FAILED_NAMES+=("${name}")
        continue
    fi

    # Execute the IR. Capture stdout and exit code.
    actual_output="$(timeout "${TIMEOUT}" "${LLI_BIN}" "${LLI_LOAD_ARGS[@]}" "${ll}" 2>"${base}.run.err")"
    actual_exit=$?

    if [ "${actual_exit}" -eq 124 ]; then
        echo "FAIL  ${name}  (timed out after ${TIMEOUT}s)"
        FAIL=$((FAIL + 1)); FAILED_NAMES+=("${name}")
        continue
    fi

    ok=1
    reason=""

    if [ "${HAS_EXIT}" -eq 1 ] && [ "${actual_exit}" -ne "${EXP_EXIT}" ]; then
        ok=0
        reason="exit expected=${EXP_EXIT} actual=${actual_exit}"
    fi

    if [ "${HAS_OUTPUT}" -eq 1 ] && [ "${actual_output}" != "${EXP_OUTPUT}" ]; then
        ok=0
        if [ -n "${reason}" ]; then reason="${reason}; "; fi
        reason="${reason}output expected=[${EXP_OUTPUT}] actual=[${actual_output}]"
    fi

    if [ "${ok}" -eq 1 ]; then
        detail=""
        [ "${HAS_EXIT}" -eq 1 ] && detail="exit=${actual_exit}"
        [ "${HAS_OUTPUT}" -eq 1 ] && detail="${detail}${detail:+ }out=[${actual_output}]"
        echo "PASS  ${name}  (${detail})"
        PASS=$((PASS + 1))
    else
        echo "FAIL  ${name}  (${reason})"
        FAIL=$((FAIL + 1)); FAILED_NAMES+=("${name}")
    fi
done

echo "========================================================"
echo "Summary: $((PASS + FAIL)) cases, ${PASS} passed, ${FAIL} failed"

if [ "${FAIL}" -ne 0 ]; then
    echo "Failed cases:"
    for n in "${FAILED_NAMES[@]}"; do echo "  - ${n}"; done
    exit 1
fi

echo "All .to tests passed."
exit 0
