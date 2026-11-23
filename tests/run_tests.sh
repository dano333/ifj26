#!/usr/bin/env bash
# IFJ26 test runner.
#
#   tests/valid/*.scala    must be accepted         -> exit code 0
#   tests/errors/eN_*.scala must be rejected        -> exit code N (taken from the file name)
#   tests/ast/*.scala      AST printed by tests/tools/ast_dump.c must equal the .expected file
#
# Usage:  ./tests/run_tests.sh [-v] [-m] [-k] [name-filter]
#   -v  show the compiler's stderr for failing tests
#   -m  also run every test under valgrind (leaks / invalid memory access = failure)
#   -k  keep going is the default; with -k the script exits 0 even if tests fail
#   name-filter  only run tests whose path contains this text
#
# Sources are looked up automatically (folder with main.c); override with  SRC=path ./tests/run_tests.sh
# Build output goes to tests/.build (safe to delete).
#
# The compiler reads the program from stdin and returns the error code as its exit code.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TESTS="$ROOT/tests"
BUILD="$TESTS/build"
BIN="$BUILD/ifj26"
CFLAGS="-Wall -Wextra -std=c11 -g -MMD -MP -D_GNU_SOURCE"

# Source directory: $SRC if given, otherwise the folder holding main.c (project root, src/, ...)
if [ -z "$SRC" ]; then
	for d in "$ROOT" "$ROOT/src" "$ROOT/source" "$ROOT/sources"; do
		[ -f "$d/main.c" ] && SRC="$d" && break
	done
fi
if [ -z "$SRC" ]; then
	SRC="$(dirname "$(find "$ROOT" -name main.c -not -path "$TESTS/*" 2>/dev/null | head -n1)")"
fi
if [ ! -f "$SRC/main.c" ]; then
	echo "Cannot find the compiler sources (main.c). Run as:  SRC=path/to/sources $0"
	exit 99
fi
echo "Sources: $SRC"

VERBOSE=0; VALGRIND=0; KEEP=0; FILTER=""
for a in "$@"; do
	case "$a" in
	-v) VERBOSE=1 ;;
	-m) VALGRIND=1 ;;
	-k) KEEP=1 ;;
	*) FILTER="$a" ;;
	esac
done

if [ -t 1 ]; then G=$'\e[32m'; R=$'\e[31m'; Y=$'\e[33m'; N=$'\e[0m'; else G=""; R=""; Y=""; N=""; fi

# ---------- build ----------
rm -rf "$BUILD"; mkdir -p "$BUILD"
echo "Building..."
if ! (cd "$SRC" && for f in *.c; do
	gcc $CFLAGS -c "$f" -o "$BUILD/${f%.c}.o" || exit 1
done && gcc "$BUILD"/*.o -o "$BIN"); then
	echo "${R}BUILD FAILED${N}"
	exit 99
fi
DUMP="$BUILD/ast_dump"
if ! gcc $CFLAGS -I"$SRC" "$TESTS/tools/ast_dump.c" $(ls "$BUILD"/*.o | grep -v '/main\.o$') -o "$DUMP"; then
	echo "${R}BUILD OF ast_dump FAILED${N}"
	exit 99
fi
if [ "$VALGRIND" = 1 ] && ! command -v valgrind >/dev/null; then
	echo "${Y}valgrind not found, running without it${N}"; VALGRIND=0
fi

pass=0; fail=0; failed=()

# run_one <file> <expected exit code>
run_one() {
	local file="$1" want="$2" name="${1#$TESTS/}" err got out
	err="$(mktemp)"
	if [ "$VALGRIND" = 1 ]; then
		valgrind -q --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=77 \
			"$BIN" <"$file" >/dev/null 2>"$err"
	else
		"$BIN" <"$file" >/dev/null 2>"$err"
	fi
	got=$?
	if [ "$got" = "$want" ]; then
		pass=$((pass + 1)); printf '  %sPASS%s %s\n' "$G" "$N" "$name"
	else
		fail=$((fail + 1)); failed+=("$name")
		if [ "$got" = 77 ] && [ "$VALGRIND" = 1 ]; then
			printf '  %sFAIL%s %s (memory error / leak reported by valgrind)\n' "$R" "$N" "$name"
		else
			printf '  %sFAIL%s %s (expected exit %s, got %s)\n' "$R" "$N" "$name" "$want" "$got"
		fi
		[ "$VERBOSE" = 1 ] && sed 's/^/        | /' "$err" | head -15
	fi
	rm -f "$err"
}

echo; echo "== valid programs (expect exit 0) =="
for f in "$TESTS"/valid/*.scala; do
	[ -e "$f" ] || continue
	[[ -n "$FILTER" && "$f" != *"$FILTER"* ]] && continue
	run_one "$f" 0
done

echo; echo "== invalid programs (expect the exit code in the file name) =="
for f in "$TESTS"/errors/e*.scala; do
	[ -e "$f" ] || continue
	[[ -n "$FILTER" && "$f" != *"$FILTER"* ]] && continue
	code="$(basename "$f" | sed -E 's/^e([0-9]+)_.*/\1/')"
	run_one "$f" "$code"
done

echo; echo "== AST snapshots (tests/ast/*.scala must print exactly tests/ast/*.expected) =="
for f in "$TESTS"/ast/*.scala; do
	[ -e "$f" ] || continue
	[[ -n "$FILTER" && "$f" != *"$FILTER"* ]] && continue
	name="${f#$TESTS/}"; exp="${f%.scala}.expected"; out="$(mktemp)"
	if [ "$VALGRIND" = 1 ]; then
		valgrind -q --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=77 "$DUMP" <"$f" >"$out" 2>/dev/null
	else
		"$DUMP" <"$f" >"$out" 2>/dev/null
	fi
	rc=$?
	if [ "$rc" = 0 ] && diff -q "$exp" "$out" >/dev/null; then
		pass=$((pass + 1)); printf '  %sPASS%s %s\n' "$G" "$N" "$name"
	else
		fail=$((fail + 1)); failed+=("$name")
		printf '  %sFAIL%s %s (AST differs from %s)\n' "$R" "$N" "$name" "$(basename "$exp")"
		[ "$VERBOSE" = 1 ] && diff "$exp" "$out" | head -20 | sed 's/^/        | /'
	fi
	rm -f "$out"
done

echo
total=$((pass + fail))
if [ "$fail" = 0 ]; then
	echo "${G}All $total tests passed.${N}"
else
	echo "${R}$fail of $total tests failed:${N}"
	printf '  - %s\n' "${failed[@]}"
	[ "$KEEP" = 1 ] && exit 0
	exit 1
fi
