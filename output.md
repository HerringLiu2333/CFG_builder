# CFG_builder 输出 JSON 字段文档（完整）

本文档描述当前工具输出 JSON 的字段语义、层级结构与常见取值，覆盖：

- 顶层字段
- `functions[]` 字段
- `ast` 结构
- `cfg` 结构
- `normalized_ir` 结构（重点）

---

## 1. 输出整体结构

```json
{
	"source_file": "...",
	"functions_requested": 1,
	"requested_function_names": ["..."],
	"functions_found": 1,
	"functions": [
		{
			"name": "...",
			"ast": { ... },
			"cfg": { ... },
			"normalized_ir": { ... }
		}
	]
}
```

说明：

- `ast` / `cfg` / `normalized_ir` 的出现与输出模式有关。
- 当前实现中，`normalized_ir` 与 `cfg` 同时输出（即包含 CFG 时会包含 IR）。

---

## 2. 顶层字段

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `source_file` | string | 当前分析的主文件路径 | `io_uring/io_uring.c`、`/abs/path/file.c` |
| `functions_requested` | integer | 用户请求分析的函数数量 | `0`、`1`、`N` |
| `requested_function_names` | string[] | 用户请求的函数名列表 | `[]`、`["main"]` |
| `functions_found` | integer | 实际在主文件中命中的函数数量 | `0..N` |
| `functions` | object[] | 每个函数的分析结果 | 见下节 |

---

## 3. `functions[]` 字段

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `name` | string | 函数名 | `main`、`io_get_ext_arg_reg` |
| `ast` | object | 函数 AST 结果 | 见第 4 节 |
| `cfg` | object | 函数 CFG 结果 | 见第 5 节 |
| `normalized_ir` | object | 标准化语句级 IR 结果 | 见第 6 节 |

---

## 4. `ast` 字段说明

### 4.1 `ast` 根对象

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `function` | string | 函数名 | `io_get_ext_arg_reg` |
| `return_type` | string | 返回类型（Clang 文本） | `int`、`struct io_uring_reg_wait *` |
| `location` | string | 函数位置 `file:line:column` | `io_uring/io_uring.c:3199:34` |
| `params` | object[] | 形参列表 | 见 4.2 |
| `body` | object | AST 根语句节点（通常 `CompoundStmt`） | 见 4.3 |

### 4.2 `params[]`

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `name` | string | 参数名 | `ctx` |
| `type` | string | 参数类型 | `struct io_ring_ctx *` |

### 4.3 AST 节点（递归）

`body` 和 `children[]` 中的每个节点都遵循以下结构：

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `kind` | string | AST 节点类型名 | `IfStmt`、`CallExpr`、`DeclRefExpr` |
| `location` | string | 节点位置 `file:line:column` | `io_uring/io_uring.c:3210:6` |
| `text` | string | 节点可读文本（单行） | `end > ctx->cq_wait_size` |
| `attrs` | object | 额外属性（按节点类型填充） | 见下 |
| `children` | object[] | 子节点数组 | 递归 AST |

`attrs` 常见键：

- `op`：操作符（如 `+`、`%`、`>`、`-`）
- `name`：标识符名（如 `ctx`、`offset`）
- `value`：字面量值（如 `0`、`14`）

---

## 5. `cfg` 字段说明

### 5.1 `cfg` 根对象

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `status` | string | CFG 构建状态 | `ok`、`failed` |
| `blocks` | object[] | 基本块列表 | 见 5.2 |

### 5.2 `cfg.blocks[]`

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `id` | integer | 基本块 ID | `0..N` |
| `is_entry` | boolean | 是否入口块 | `true/false` |
| `is_exit` | boolean | 是否出口块 | `true/false` |
| `preds` | (integer\|null)[] | 前驱块 ID 列表 | `[3]`、`[1,2]`、`[null]` |
| `succs` | (integer\|null)[] | 后继块 ID 列表 | `[2,1]`、`[]`、`[null]` |
| `terminator` | string\|null | 块终结语句文本 | `"if (...) ..."`、`null` |
| `stmts` | string[] | 块内语句文本（CFG 层） | `"x = y"`、`"return 0;"` |

说明：

- `id=0` 常是 exit block（依 Clang CFG 生成而定）。
- `preds/succs` 中 `null` 代表不可达或空分支位。

---

## 6. `normalized_ir` 字段说明（重点）

### 6.1 `normalized_ir` 根对象

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `version` | string | IR 版本号 | `v1` |
| `status` | string | IR 构建状态 | `ok`、`failed` |
| `nodes` | object[] | 语句级 IR 节点列表 | 见 6.2 |
| `edges` | object | 语句级控制流边集合 | 见 6.3 |

---

### 6.2 `normalized_ir.nodes[]`

每个元素对应“一个标准化语句节点”。

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `stmt_id` | integer | 函数内唯一语句 ID | `1..N` |
| `bb_id` | integer | 所在 CFG 基本块 ID | `0..N` |
| `index_in_bb` | integer | 语句在块内顺序（0-based） | `0`、`1`、`2` |
| `loc` | object | 源码位置 | 见下 |
| `type` | string | 归一化语义类型 | `decl`、`assign`、`call`、`branch`、`return`、`expr`、`stmt`、`unknown` |
| `ast_kind` | string | 原始 AST 节点类型 | `DeclStmt`、`CallExpr`、`IfStmt`、`ImplicitCastExpr` |
| `text` | string | 节点可读文本 | `"unsigned long end; "` |
| `is_terminator` | boolean | 是否块终结语句 | `true/false` |
| `defs` | string[] | 本语句定义（写入）的变量集合 | `[]`、`["offset"]` |
| `uses` | string[] | 本语句使用（读取）的变量集合 | `[]`、`["ctx","end"]` |
| `call` | object (optional) | 调用信息，仅调用节点存在 | 见下 |
| `ctrl_pred` | integer[] | 语句级控制流前驱 `stmt_id` | `[]`、`[20]` |
| `ctrl_succ` | integer[] | 语句级控制流后继 `stmt_id` | `[]`、`[10,14]` |

`loc` 子字段：

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `file` | string | 源文件路径 | `io_uring/io_uring.c` |
| `line` | integer | 行号（1-based） | `3206` |
| `column` | integer | 列号（1-based） | `2` |

`call` 子字段（仅调用节点）：

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `callee` | string | 被调函数名 | `__builtin_expect`、`ERR_PTR` |
| `args` | string[] | 参数表达式文本列表 | `["offset", "size", "&end"]` |
| `ret_target` | string | 接收返回值目标变量，无则空串 | `"ptr"`、`""` |

---

### 6.3 `normalized_ir.edges.control_flow[]`

表示语句级控制流边（`stmt_id -> stmt_id`）。

| 字段 | 类型 | 含义 | 可能内容 |
|---|---|---|---|
| `from` | integer | 起点语句 ID | `21` |
| `to` | integer | 终点语句 ID | `10` |
| `kind` | string | 边类型 | `fallthrough`、`branch`、`backedge` |

边类型解释：

- `fallthrough`：同块内顺序执行边。
- `branch`：分支跳转到后续路径。
- `backedge`：回边（常见于循环或按块编号推断的回跳）。

---

## 7. 字段关系与使用建议

### 7.1 `cfg` 与 `normalized_ir` 的关系

- `bb_id` 可把 IR 节点映射回 CFG 基本块。
- `index_in_bb` 可恢复块内执行顺序。
- `ctrl_pred/ctrl_succ` + `edges.control_flow` 可直接做语句级图分析。

### 7.2 Def-Use 分析建议

- 用 `defs`/`uses` 构建变量读写关系。
- 结合控制流边过滤不可达路径。
- 调用语句优先使用 `call.callee/args/ret_target`，避免只依赖 `text` 字符串解析。

### 7.3 PDG / Slicing 建议

- 控制依赖：由 `branch` 节点及其 `ctrl_succ` 建立。
- 数据依赖：由 `defs -> uses` 追踪并沿控制流传播。
- 切片起点可选：某个 `stmt_id`、某变量名、或某行号对应节点。

---

## 8. 示例：`nodes[]` 单节点

```json
{
	"stmt_id": 21,
	"bb_id": 7,
	"index_in_bb": 4,
	"loc": {
		"file": "io_uring/io_uring.c",
		"line": 3206,
		"column": 2
	},
	"type": "branch",
	"ast_kind": "IfStmt",
	"text": "if (__builtin_expect(!!(offset % sizeof(long)), 0)) return ERR_PTR(-14); ",
	"is_terminator": true,
	"defs": [],
	"uses": ["__builtin_expect", "offset", "ERR_PTR"],
	"ctrl_pred": [20],
	"ctrl_succ": [10, 14]
}
```

---

## 9. 当前版本注意事项

1. `uses` 为语法级提取（基于标识符引用），不是别名分析结果。  
2. `ret_target` 仅在常见“赋值接收返回值”场景可提取，复杂场景可能为空。  
3. 某些宏/内建函数展开后，会出现多个语句节点（如 `ImplicitCastExpr`）。  
4. `text` 主要用于可读性，不建议作为唯一机器可解析字段。  

---

## 10. 向后兼容性

- 已有 `ast` 与 `cfg` 字段保持兼容。
- 新增 `normalized_ir` 不影响已有消费者；下游可渐进式接入。

