// 本文件用于声明 CFG 输出器，负责打印或导出 CFG 结果。

#pragma once

#include "clang/Analysis/CFG.h"
#include "llvm/Support/JSON.h"

#include <unordered_map>
#include <vector>

namespace clang {
class ASTContext;
class FunctionDecl;
}

namespace llvm {
class raw_ostream;
}

namespace cfgbuilder::output {

// 以稳定 JSON 结构导出函数 AST 与 CFG。
class CFGPrinter {
public:
	static llvm::json::Object BuildFunctionAstJson(
		const clang::FunctionDecl& functionDecl,
		clang::ASTContext& context,
		const std::unordered_map<const clang::Stmt*, std::vector<int64_t>>* stmtIdsByStmt = nullptr,
		std::unordered_map<const clang::Stmt*, std::vector<int64_t>>* astNodeIdsByStmt = nullptr);

	static llvm::json::Object BuildFunctionCfgJson(
		const clang::FunctionDecl& functionDecl,
		const clang::CFG* cfg,
		clang::ASTContext& context,
		const std::unordered_map<const clang::Stmt*, std::vector<int64_t>>* astNodeIdsByStmt = nullptr);

	static llvm::json::Object BuildFunctionNormalizedIrJson(
		const clang::FunctionDecl& functionDecl,
		const clang::CFG* cfg,
		clang::ASTContext& context,
		std::unordered_map<const clang::Stmt*, std::vector<int64_t>>* stmtIdsByStmt = nullptr);
};

} // namespace cfgbuilder::output
