#!/usr/bin/env bash
# Run all slideio test executables for the given build configuration.
# Usage: ./run-tests.sh [release|debug]   (default: release)
# Supports Linux, macOS, and Windows (Git Bash / MSYS / Cygwin).

config="${1:-release}"
config_lower="$(printf '%s' "$config" | tr '[:upper:]' '[:lower:]')"

case "$config_lower" in
    release|debug) ;;
    *)
        echo "Error: configuration must be 'release' or 'debug' (got '$config')" >&2
        echo "Usage: $0 [release|debug]" >&2
        exit 2
        ;;
esac

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "$(uname -s)" in
    Linux*|Darwin*)
        bin_dir="$script_dir/build/$config_lower/bin"
        exe_suffix=""
        ;;
    MINGW*|MSYS*|CYGWIN*)
        config_cap="$(printf '%s' "$config_lower" | awk '{print toupper(substr($0,1,1)) substr($0,2)}')"
        bin_dir="$script_dir/build/bin/$config_cap"
        exe_suffix=".exe"
        ;;
    *)
        echo "Error: unsupported OS: $(uname -s)" >&2
        exit 2
        ;;
esac

if [ ! -d "$bin_dir" ]; then
    echo "Error: bin directory not found: $bin_dir" >&2
    exit 2
fi

shopt -s nullglob
tests=( "$bin_dir"/*_tests"$exe_suffix" )
shopt -u nullglob

if [ ${#tests[@]} -eq 0 ]; then
    echo "Error: no *_tests${exe_suffix} executables found in $bin_dir" >&2
    exit 2
fi

echo "Running ${#tests[@]} test binaries from $bin_dir"
echo

passed=()
failed=()

for t in "${tests[@]}"; do
    name="$(basename "$t")"
    echo "=============================================="
    echo "  $name"
    echo "=============================================="
    if "$t"; then
        passed+=("$name")
    else
        failed+=("$name")
    fi
    echo
done

echo "=============================================="
echo "  Summary ($config_lower)"
echo "=============================================="
echo "Passed: ${#passed[@]}"
echo "Failed: ${#failed[@]}"

if [ ${#failed[@]} -gt 0 ]; then
    echo
    echo "Failed tests:"
    for n in "${failed[@]}"; do
        echo "  - $n"
    done
    exit 1
fi

exit 0
