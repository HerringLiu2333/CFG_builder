// 本文件用于实现 CFG 输出逻辑，按统一格式打印图结构。

#include "output/CFGPrinter.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <string>
#include <vector>

namespace cfgbuilder::output {
namespace {

std::string GetLocationString(const clang::SourceManager& sourceManager, clang::SourceLocation location) {
	const clang::SourceLocation expansionLocation = sourceManager.getExpansionLoc(location);
	const clang::PresumedLoc presumedLoc = sourceManager.getPresumedLoc(expansionLocation);
	if (!presumedLoc.isValid()) {
		return "<invalid>";
	}

	return std::string(presumedLoc.getFilename()) + ":" +
		std::to_string(presumedLoc.getLine()) + ":" +
		std::to_string(presumedLoc.getColumn());
}

std::string ToSingleLine(std::string text) {
	for (char& c : text) {
		if (c == '\n' || c == '\r' || c == '\t') {
			c = ' ';
		}
	}

	// 简单压缩连续空格，提升可读性。
	std::string compact;
	compact.reserve(text.size());
	bool lastWasSpace = false;
	for (char c : text) {
		if (c == ' ') {
			if (!lastWasSpace) {
				compact.push_back(c);
			}
			lastWasSpace = true;
		} else {
			compact.push_back(c);
			lastWasSpace = false;
		}
	}
	return compact;
}

std::string RenderStmt(const clang::Stmt* stmt, const clang::LangOptions& langOptions) {
	if (stmt == nullptr) {
		return "<null-stmt>";
	}

	std::string rendered;
	llvm::raw_string_ostream ros(rendered);
	clang::PrintingPolicy policy(langOptions);
	policy.SuppressTagKeyword = false;
	policy.FullyQualifiedName = false;
	stmt->printPretty(ros, nullptr, policy);
	ros.flush();

	rendered = ToSingleLine(rendered);
	if (rendered.empty()) {
		rendered = stmt->getStmtClassName();
	}
	return rendered;
}

llvm::json::Object BuildAstNodeJson(const clang::Stmt* stmt, const clang::ASTContext& context) {
	llvm::json::Object node;
	if (stmt == nullptr) {
		node["kind"] = "NullStmt";
		node["location"] = "<invalid>";
		node["text"] = "<null-stmt>";
		node["children"] = llvm::json::Array();
		return node;
	}

	node["kind"] = stmt->getStmtClassName();
	node["location"] = GetLocationString(context.getSourceManager(), stmt->getBeginLoc());
	node["text"] = RenderStmt(stmt, context.getLangOpts());

	llvm::json::Object attrs;
	if (const auto* binaryOp = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
		attrs["op"] = binaryOp->getOpcodeStr().str();
	}
	if (const auto* unaryOp = llvm::dyn_cast<clang::UnaryOperator>(stmt)) {
		attrs["op"] = clang::UnaryOperator::getOpcodeStr(unaryOp->getOpcode()).str();
	}
	if (const auto* declRef = llvm::dyn_cast<clang::DeclRefExpr>(stmt)) {
		attrs["name"] = declRef->getNameInfo().getAsString();
	}
	if (const auto* intLiteral = llvm::dyn_cast<clang::IntegerLiteral>(stmt)) {
		std::string valueText;
		llvm::raw_string_ostream valueStream(valueText);
		valueStream << intLiteral->getValue();
		valueStream.flush();
		attrs["value"] = valueText;
	}
	node["attrs"] = std::move(attrs);

	llvm::json::Array children;
	for (const clang::Stmt* child : stmt->children()) {
		if (child != nullptr) {
			children.push_back(BuildAstNodeJson(child, context));
		}
	}
	node["children"] = std::move(children);
	return node;
}

} // namespace

llvm::json::Object CFGPrinter::BuildFunctionAstJson(
	const clang::FunctionDecl& functionDecl,
	clang::ASTContext& context) {
	const clang::SourceManager& sourceManager = context.getSourceManager();
	llvm::json::Object ast;
	ast["function"] = functionDecl.getNameAsString();
	ast["return_type"] = functionDecl.getReturnType().getAsString();
	ast["location"] = GetLocationString(sourceManager, functionDecl.getLocation());

	llvm::json::Array params;
	for (unsigned i = 0; i < functionDecl.getNumParams(); ++i) {
		const clang::ParmVarDecl* param = functionDecl.getParamDecl(i);
		llvm::json::Object p;
		p["name"] = param->getNameAsString();
		p["type"] = param->getType().getAsString();
		params.push_back(std::move(p));
	}
	ast["params"] = std::move(params);

	ast["body"] = BuildAstNodeJson(functionDecl.getBody(), context);
	return ast;
}

llvm::json::Object CFGPrinter::BuildFunctionCfgJson(
	const clang::FunctionDecl& functionDecl,
	const clang::CFG* cfg,
	clang::ASTContext& context) {
	(void)functionDecl;
	llvm::json::Object cfgJson;
	if (cfg == nullptr) {
		cfgJson["status"] = "failed";
		cfgJson["blocks"] = llvm::json::Array();
		return cfgJson;
	}
	cfgJson["status"] = "ok";

	std::vector<const clang::CFGBlock*> blocks;
	blocks.reserve(cfg->size());
	for (const clang::CFGBlock* block : *cfg) {
		blocks.push_back(block);
	}

	std::sort(blocks.begin(), blocks.end(), [](const clang::CFGBlock* lhs, const clang::CFGBlock* rhs) {
		return lhs->getBlockID() < rhs->getBlockID();
	});

	llvm::json::Array blocksJson;
	for (const clang::CFGBlock* block : blocks) {
		llvm::json::Object b;
		b["id"] = static_cast<int64_t>(block->getBlockID());
		b["is_entry"] = (block == &cfg->getEntry());
		b["is_exit"] = (block == &cfg->getExit());

		llvm::json::Array preds;
		for (auto it = block->pred_begin(); it != block->pred_end(); ++it) {
			if (*it == nullptr) {
				preds.push_back(nullptr);
			} else {
				preds.push_back(static_cast<int64_t>((*it)->getBlockID()));
			}
		}
		b["preds"] = std::move(preds);

		llvm::json::Array succs;
		for (auto it = block->succ_begin(); it != block->succ_end(); ++it) {
			if (it->getReachableBlock() == nullptr) {
				succs.push_back(nullptr);
			} else {
				succs.push_back(static_cast<int64_t>(it->getReachableBlock()->getBlockID()));
			}
		}
		b["succs"] = std::move(succs);

		if (const clang::Stmt* terminator = block->getTerminatorStmt()) {
			b["terminator"] = RenderStmt(terminator, context.getLangOpts());
		} else {
			b["terminator"] = nullptr;
		}

		llvm::json::Array stmts;
		for (clang::CFGElement element : *block) {
			if (auto cfgStmt = element.getAs<clang::CFGStmt>()) {
				const clang::Stmt* stmt = cfgStmt->getStmt();
				stmts.push_back(RenderStmt(stmt, context.getLangOpts()));
			}
		}
		b["stmts"] = std::move(stmts);
		blocksJson.push_back(std::move(b));
	}

	cfgJson["blocks"] = std::move(blocksJson);
	return cfgJson;
}

} // namespace cfgbuilder::output