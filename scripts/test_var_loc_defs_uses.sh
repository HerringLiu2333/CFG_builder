#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_JSON="/tmp/cfg_var_loc_defs_uses_test.json"
TARGET_FILE="$ROOT_DIR/testdata/var_loc_case.c"

cd "$ROOT_DIR"

cmake --build build --target cfg_builder -j >/dev/null

./build/cfg_builder \
  --compdb testdata/compile_commands.json \
  --file testdata/var_loc_case.c \
  --functions test_var_loc_case \
  --output "$OUT_JSON" >/dev/null

# 1) 同名 def/use 在一行：x = x + 1;
if ! grep -B40 -A40 '"text": "x = x + 1"' "$OUT_JSON" | grep -q '"var_loc": ".*var_loc_case.c:3:3"'; then
  echo "[FAIL] missing def var_loc for lhs x at line 3 col 3"
  exit 1
fi

if ! grep -B40 -A40 '"text": "x = x + 1"' "$OUT_JSON" | grep -q '"var_loc": ".*var_loc_case.c:3:7"'; then
  echo "[FAIL] missing use var_loc for rhs x at line 3 col 7"
  exit 1
fi

# 2) 变量声明：int y = x; 的 def 应落在 y 的 spelling_loc（line 4 col 7）
if ! grep -B40 -A40 '"text": "int y = x; "' "$OUT_JSON" | grep -q '"name": "y"'; then
  echo "[FAIL] declaration stmt for y not found"
  exit 1
fi

if ! grep -B40 -A40 '"text": "int y = x; "' "$OUT_JSON" | grep -q '"var_loc": ".*var_loc_case.c:4:7"'; then
  echo "[FAIL] missing declaration var_loc for y at line 4 col 7"
  exit 1
fi

echo "[PASS] defs/uses var_loc test passed (same-name def/use disambiguated, declaration loc correct)"
