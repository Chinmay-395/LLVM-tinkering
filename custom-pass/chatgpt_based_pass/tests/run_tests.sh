#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_dir=$(cd -- "$script_dir/.." && pwd)

clang_bin=${CLANG:-clang}
opt_bin=${OPT:-opt}
plugin=${PLUGIN:-"$project_dir/build/SimplePointsTo.so"}

if [[ ! -f "$plugin" ]]; then
  echo "error: plugin not found at $plugin (run 'make' first)" >&2
  exit 1
fi

temporary_dir=$(mktemp -d)
trap 'rm -rf -- "$temporary_dir"' EXIT

pass_count=0

run_analysis() {
  local source_file=$1
  local base_name
  base_name=$(basename -- "$source_file" .c)

  "$clang_bin" -O0 -fno-discard-value-names -emit-llvm -c \
    "$source_file" -o "$temporary_dir/$base_name.bc"
  "$opt_bin" -load-pass-plugin="$plugin" -passes=simple-pta \
    -disable-output "$temporary_dir/$base_name.bc" \
    2>"$temporary_dir/$base_name.out"

  echo "$temporary_dir/$base_name.out"
}

block_text() {
  local output_file=$1
  local block_name=$2

  awk -v wanted="$block_name" '
    $0 == "BasicBlock: " wanted { capture = 1; next }
    /^BasicBlock: / { capture = 0 }
    capture { print }
  ' "$output_file"
}

expect_in_block() {
  local output_file=$1
  local block_name=$2
  local expected_line=$3
  local description=$4
  local actual_block

  actual_block=$(block_text "$output_file" "$block_name")
  if grep -Fqx -- "$expected_line" <<<"$actual_block"; then
    printf 'PASS: %s\n' "$description"
    pass_count=$((pass_count + 1))
    return
  fi

  printf 'FAIL: %s\n' "$description" >&2
  printf 'Expected in block %s: %s\n' "$block_name" "$expected_line" >&2
  printf '%s\n' "$actual_block" >&2
  exit 1
}

output=$(run_analysis "$script_dir/01_simple_assignment.c")
expect_in_block "$output" entry '    %p -> { %x }' \
  'direct assignment records p -> x'

output=$(run_analysis "$script_dir/02_strong_update.c")
expect_in_block "$output" entry '    %p -> { %y }' \
  'single-location store strongly replaces p -> x with p -> y'

output=$(run_analysis "$script_dir/03_branch_merge.c")
expect_in_block "$output" if.end '    %p -> { %x, %y }' \
  'branch join unions p -> x and p -> y'

output=$(run_analysis "$script_dir/04_pointer_copy.c")
expect_in_block "$output" entry '    %p -> { %x }' \
  'pointer-copy source remains p -> x'
expect_in_block "$output" entry '    %q -> { %x }' \
  'pointer load and store produce q -> x'

output=$(run_analysis "$script_dir/05_pointer_to_pointer.c")
expect_in_block "$output" entry '    %p -> { %x }' \
  'pointer variable p contains x'
expect_in_block "$output" entry '    %pp -> { %p }' \
  'pointer-to-pointer variable pp contains p'

output=$(run_analysis "$script_dir/06_indirect_store.c")
expect_in_block "$output" entry '    %p -> { %y }' \
  'indirect store through pp strongly updates p to y'
expect_in_block "$output" entry '    %pp -> { %p }' \
  'indirect store preserves pp -> p'

output=$(run_analysis "$script_dir/07_loop_fixed_point.c")
expect_in_block "$output" while.end '    %p -> { %x, %y }' \
  'loop exit includes zero-iteration and one-or-more-iteration facts'

output=$(run_analysis "$script_dir/08_weak_update.c")
expect_in_block "$output" if.end '    %p -> { %x, %z }' \
  'multi-location store weakly updates possible target p'
expect_in_block "$output" if.end '    %q -> { %y, %z }' \
  'multi-location store weakly updates possible target q'

printf '\nAll %d points-to assertions passed.\n' "$pass_count"
