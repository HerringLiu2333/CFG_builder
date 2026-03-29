#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_JSON="/tmp/cfg_builder_stmt_begin_end_test.json"

cd "$ROOT_DIR"

cmake --build build --target cfg_builder -j >/dev/null

./build/cfg_builder \
  --compdb testdata/compile_commands.json \
  --file testdata/stmt_begin_end.c \
  --functions demo_stmt_begin_end \
  --output "$OUT_JSON" >/dev/null

if ! grep -q '"kind": "CallExpr"' "$OUT_JSON"; then
  echo "[FAIL] CallExpr node not found in AST output"
  exit 1
fi

if ! grep -q '"text": "f2fs_err(sbi, \\"Inconsistent segment (%u) type \[%d, %d\] in SSA and SIT\\", segno, type, sum_footer_entry_type)"' "$OUT_JSON"; then
  echo "[FAIL] target call stmt text not found in AST output"
  exit 1
fi

if ! grep -q '"begin": 13' "$OUT_JSON"; then
  echo "[FAIL] begin line for multiline call stmt is not 13"
  exit 1
fi

if ! grep -q '"end": 14' "$OUT_JSON"; then
  echo "[FAIL] end line for multiline call stmt is not 14"
  exit 1
fi

echo "[PASS] AST stmt begin/end test passed (multiline call begin=13, end=14)"
