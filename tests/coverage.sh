#!/bin/bash
# ============================================================
# tests/coverage.sh — 构建并运行带覆盖率的核心层测试，输出行覆盖率汇总。
#
# 用法:
#   ./tests/coverage.sh [build-dir]      # 默认 build-cov
#
# 依赖: cmake, g++, gcov, python3
# 说明: CMake 生成的覆盖率对象命名为 <src>.gcno/.gcda，而 gcov 默认查找
#       <base>.gcno；脚本通过符号链接解决该命名差异后，用 python3 聚合。
# ============================================================
set -euo pipefail

BUILD_DIR="${1:-build-cov}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "=== [1/4] Configure with coverage ==="
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON

echo "=== [2/4] Build ==="
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "=== [3/4] Run tests ==="
cd "$BUILD_DIR"
ctest --output-on-failure

echo "=== [4/4] Coverage summary ==="
TESTS_DIR="$PWD/tests"
cd "$TESTS_DIR"   # 测试的 CMakeFiles 位于 build/tests 下
SCRATCH="$(mktemp -d)"
trap 'rm -rf "$SCRATCH" "$TESTS_DIR/gcov_out"' EXIT

SOURCES=(
  "core/buffer/AppendBuffer"
  "core/buffer/DoubleBuffer"
  "core/buffer/RingBuffer"
  "service/EventBus"
  "service/Session"
  "service/ProtocolEngine"
  "plugin/PluginRegistry"
  "protocol/LineProtocol"
  "protocol/CsvProtocol"
  "protocol/ProtocolRegistry"
)
mkdir -p "$TESTS_DIR/gcov_out"
for src in "${SOURCES[@]}"; do
  base="$(basename "$src")"
  for testbin in core_buffer_test core_session_test core_plugin_test core_integration_test core_protocol_test; do
    gcno="$(find "CMakeFiles/${testbin}.dir" -name "${base}.cpp.gcno" 2>/dev/null | head -1 || true)"
    [ -z "$gcno" ] && continue
    gcda="$(find "CMakeFiles/${testbin}.dir" -name "${base}.cpp.gcda" 2>/dev/null | head -1 || true)"
    ln -sf "$PWD/$gcno" "$SCRATCH/${base}.gcno"
    if [ -n "$gcda" ]; then
      ln -sf "$PWD/$gcda" "$SCRATCH/${base}.gcda"
    fi
    outdir="$TESTS_DIR/gcov_out/${testbin}"
    mkdir -p "$outdir"
    (cd "$SCRATCH" && gcov -o "$SCRATCH" "${base}.cpp" >/dev/null 2>&1) || true
    for g in "$SCRATCH"/*.gcov; do [ -e "$g" ] && cp "$g" "$outdir/$(basename "$g")"; done
    rm -f "$SCRATCH"/*.gcov
  done
done

python3 - "$TESTS_DIR/gcov_out" <<'PY'
import glob, os, re, sys
cov_root = sys.argv[1]
sources = ["AppendBuffer","DoubleBuffer","RingBuffer","EventBus","Session","ProtocolEngine","PluginRegistry","LineProtocol","CsvProtocol","ProtocolRegistry"]
total = covered = 0
print(f"{'file':<18}{'cov':>7}{'total':>7}{'%':>8}")
for base in sources:
    cov_map = {}
    for g in glob.glob(os.path.join(cov_root, "*", base + ".cpp.gcov")):
        with open(g) as fh:
            for line in fh:
                m = re.match(r"^\s*(\d+|#####):\s*(\d+):", line)
                if not m:
                    continue
                ln = int(m.group(2))
                covered_line = m.group(1) != "#####"
                cov_map[ln] = cov_map.get(ln, False) or covered_line
    t = len(cov_map); c = sum(1 for v in cov_map.values() if v)
    total += t; covered += c
    print(f"{base+'.cpp':<18}{c:>7}{t:>7}{(100.0*c/t if t else 0):>7.1f}%")
print("-" * 40)
print(f"{'TOTAL':<18}{covered:>7}{total:>7}{(100.0*covered/total if total else 0):>7.1f}%")
PY