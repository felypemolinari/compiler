#!/usr/bin/env bash
# Compiles every sample program and checks it ends the way it is supposed to.
#
#   tests/*.txt                valid programs, must compile
#   tests/erros/*.txt          one lexical or syntactic error each, must fail
#   tests/semantica/*.txt      T3; the header of each file says which it is
#                              (APRESENTACAO.txt is a cheat sheet, not a program)
#   tests/semantica/demo/*.txt one per presentation topic, same convention
#
# Usage: tests/run_tests.sh [path/to/compiler-cli]

set -u

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cli="${1:-$root/build/compiler-cli}"

if [[ ! -x "$cli" ]]; then
    echo "compiler-cli not found at $cli -- build it first:"
    echo "    cmake -S . -B build -G Ninja && cmake --build build"
    exit 2
fi

# Semantic samples that are meant to be rejected; the others must compile.
must_fail_semantic="s2_escopos_invalidos s3_unicidade s6_tipos_expressoes s7_tipos_atribuicoes"
must_fail_demo="02_item2_escopo 03_item3_unicidade 06_item7a_expressoes 07_item7b_atribuicoes 08_item7c_parametros 09_extra_vetor 12_extra_void"

passed=0
failed=0

check() {
    local file="$1" expected="$2" label
    label="${file#"$root"/}"

    if "$cli" "$file" > /dev/null 2>&1; then
        local actual="ok"
    else
        local actual="fail"
    fi

    if [[ "$actual" == "$expected" ]]; then
        printf '  pass  %-44s (%s)\n' "$label" "$expected"
        passed=$((passed + 1))
    else
        printf '  FAIL  %-44s expected %s, got %s\n' "$label" "$expected" "$actual"
        failed=$((failed + 1))
    fi
}

echo "valid programs"
for file in "$root"/tests/*.txt; do
    check "$file" ok
done

echo "programs with a lexical or syntactic error"
for file in "$root"/tests/erros/*.txt; do
    check "$file" fail
done

echo "semantic analysis (T3)"
for file in "$root"/tests/semantica/*.txt; do
    name="$(basename "$file" .txt)"
    # A roteiro of separate blocks, not a single program: nothing to compile.
    [[ "$name" == "APRESENTACAO" ]] && continue
    if [[ " $must_fail_semantic " == *" $name "* ]]; then
        check "$file" fail
    else
        check "$file" ok
    fi
done

echo "presentation samples (T3)"
for file in "$root"/tests/semantica/demo/*.txt; do
    name="$(basename "$file" .txt)"
    if [[ " $must_fail_demo " == *" $name "* ]]; then
        check "$file" fail
    else
        check "$file" ok
    fi
done

echo
echo "$passed passed, $failed failed"
[[ "$failed" -eq 0 ]]
