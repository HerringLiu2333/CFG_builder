// 本文件用于实现 CFG 输出逻辑，按统一格式打印图结构。

#include "output/CFGPrinter.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

namespace cfgbuilder::output {
namespace {

clang::SourceLocation ResolveOutputLoc(
	const clang::SourceManager& sourceManager,
	clang::SourceLocation location,
	bool useSpellingLoc) {
	if (location.isInvalid()) {
		return location;
	}
	return useSpellingLoc ? sourceManager.getSpellingLoc(location) : sourceManager.getExpansionLoc(location);
}

std::string GetLocationString(
	const clang::SourceManager& sourceManager,
	clang::SourceLocation location,
	bool useSpellingLoc) {
	const clang::SourceLocation outputLocation = ResolveOutputLoc(sourceManager, location, useSpellingLoc);
	const clang::PresumedLoc presumedLoc = sourceManager.getPresumedLoc(outputLocation);
	if (!presumedLoc.isValid()) {
		return "<invalid>";
	}

	return std::string(presumedLoc.getFilename()) + ":" +
		std::to_string(presumedLoc.getLine()) + ":" +
		std::to_string(presumedLoc.getColumn());
}

std::string GetFilenameString(
	const clang::SourceManager& sourceManager,
	clang::SourceLocation location,
	bool useSpellingLoc) {
	const clang::SourceLocation outputLocation = ResolveOutputLoc(sourceManager, location, useSpellingLoc);
	const clang::PresumedLoc presumedLoc = sourceManager.getPresumedLoc(outputLocation);
	if (!presumedLoc.isValid()) {
		return "<invalid>";
	}
	return std::string(presumedLoc.getFilename());
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

std::string GetOriginalText(
	const clang::Stmt* stmt,
	const clang::SourceManager& sourceManager,
	const clang::LangOptions& langOptions) {
	if (stmt == nullptr) {
		return "";
	}

	const clang::CharSourceRange expandedRange = sourceManager.getExpansionRange(stmt->getSourceRange());
	if (expandedRange.isInvalid()) {
		return RenderStmt(stmt, langOptions);
	}

	const llvm::StringRef text = clang::Lexer::getSourceText(expandedRange, sourceManager, langOptions);
	if (text.empty()) {
		return RenderStmt(stmt, langOptions);
	}
	return text.str();
}

int64_t GetExpansionRangeLength(
	const clang::Stmt* stmt,
	const clang::SourceManager& sourceManager,
	const clang::LangOptions& langOptions) {
	if (stmt == nullptr) {
		return -1;
	}

	const clang::CharSourceRange range = sourceManager.getExpansionRange(stmt->getSourceRange());
	if (range.isInvalid()) {
		return -1;
	}

	clang::SourceLocation beginLoc = range.getBegin();
	clang::SourceLocation endLoc = range.getEnd();
	if (beginLoc.isInvalid() || endLoc.isInvalid()) {
		return -1;
	}

	if (sourceManager.getFileID(beginLoc) != sourceManager.getFileID(endLoc)) {
		return -1;
	}

	if (range.isTokenRange()) {
		const clang::SourceLocation tokenEnd =
			clang::Lexer::getLocForEndOfToken(endLoc, 0, sourceManager, langOptions);
		if (tokenEnd.isValid()) {
			endLoc = tokenEnd;
		}
	}

	const uint64_t beginOffset = sourceManager.getFileOffset(beginLoc);
	const uint64_t endOffset = sourceManager.getFileOffset(endLoc);
	if (endOffset < beginOffset) {
		return -1;
	}
	return static_cast<int64_t>(endOffset - beginOffset);
}

std::string RenderBranchCondition(const clang::Stmt* stmt, const clang::LangOptions& langOptions) {
	if (stmt == nullptr) {
		return "<null-stmt>";
	}

	const clang::Stmt* condition = nullptr;
	if (const auto* ifStmt = llvm::dyn_cast<clang::IfStmt>(stmt)) {
		condition = ifStmt->getCond();
	} else if (const auto* whileStmt = llvm::dyn_cast<clang::WhileStmt>(stmt)) {
		condition = whileStmt->getCond();
	} else if (const auto* doStmt = llvm::dyn_cast<clang::DoStmt>(stmt)) {
		condition = doStmt->getCond();
	} else if (const auto* forStmt = llvm::dyn_cast<clang::ForStmt>(stmt)) {
		condition = forStmt->getCond();
	} else if (const auto* switchStmt = llvm::dyn_cast<clang::SwitchStmt>(stmt)) {
		condition = switchStmt->getCond();
	}

	if (condition == nullptr) {
		return RenderStmt(stmt, langOptions);
	}

	return RenderStmt(condition, langOptions);
}

const clang::Stmt* GetBranchConditionStmt(const clang::Stmt* stmt) {
	if (stmt == nullptr) {
		return nullptr;
	}

	if (const auto* ifStmt = llvm::dyn_cast<clang::IfStmt>(stmt)) {
		return ifStmt->getCond();
	}
	if (const auto* whileStmt = llvm::dyn_cast<clang::WhileStmt>(stmt)) {
		return whileStmt->getCond();
	}
	if (const auto* doStmt = llvm::dyn_cast<clang::DoStmt>(stmt)) {
		return doStmt->getCond();
	}
	if (const auto* forStmt = llvm::dyn_cast<clang::ForStmt>(stmt)) {
		return forStmt->getCond();
	}
	if (const auto* switchStmt = llvm::dyn_cast<clang::SwitchStmt>(stmt)) {
		return switchStmt->getCond();
	}

	return nullptr;
}

int64_t GetLine(
	const clang::SourceManager& sourceManager,
	clang::SourceLocation location,
	bool useSpellingLoc) {
	const clang::SourceLocation outputLocation = ResolveOutputLoc(sourceManager, location, useSpellingLoc);
	const clang::PresumedLoc presumedLoc = sourceManager.getPresumedLoc(outputLocation);
	if (!presumedLoc.isValid()) {
		return -1;
	}
	return static_cast<int64_t>(presumedLoc.getLine());
}

int64_t GetColumn(
	const clang::SourceManager& sourceManager,
	clang::SourceLocation location,
	bool useSpellingLoc) {
	const clang::SourceLocation outputLocation = ResolveOutputLoc(sourceManager, location, useSpellingLoc);
	const clang::PresumedLoc presumedLoc = sourceManager.getPresumedLoc(outputLocation);
	if (!presumedLoc.isValid()) {
		return -1;
	}
	return static_cast<int64_t>(presumedLoc.getColumn());
}

int64_t GetStmtStartLine(
	const clang::SourceManager& sourceManager,
	const clang::Stmt* stmt,
	bool useSpellingLoc) {
	if (stmt == nullptr) {
		return -1;
	}
	return GetLine(sourceManager, stmt->getBeginLoc(), useSpellingLoc);
}

int64_t GetStmtEndLine(
	const clang::SourceManager& sourceManager,
	const clang::LangOptions& langOptions,
	const clang::Stmt* stmt,
	bool useSpellingLoc) {
	if (stmt == nullptr) {
		return -1;
	}

	clang::SourceLocation endLocation = ResolveOutputLoc(sourceManager, stmt->getEndLoc(), useSpellingLoc);
	if (endLocation.isInvalid()) {
		return -1;
	}

	const clang::SourceLocation tokenEnd =
		clang::Lexer::getLocForEndOfToken(endLocation, 0, sourceManager, langOptions);
	if (tokenEnd.isValid()) {
		endLocation = tokenEnd;
	}

	return GetLine(sourceManager, endLocation, useSpellingLoc);
}

llvm::json::Object BuildLineColumnLocJson(
	const clang::SourceManager& sourceManager,
	clang::SourceLocation location,
	bool useSpellingLoc) {
	llvm::json::Object loc;
	loc["file"] = GetFilenameString(sourceManager, location, useSpellingLoc);
	loc["line"] = GetLine(sourceManager, location, useSpellingLoc);
	loc["column"] = GetColumn(sourceManager, location, useSpellingLoc);
	return loc;
}

void AddUnique(std::vector<std::string>& values, const std::string& value) {
	if (value.empty()) {
		return;
	}
	if (std::find(values.begin(), values.end(), value) == values.end()) {
		values.push_back(value);
	}
}

struct VarRefInfo {
	std::string name;
	std::string varLoc;
	std::string type;
};

void AddUniqueVarRef(std::vector<VarRefInfo>& values, const VarRefInfo& value) {
	if (value.name.empty()) {
		return;
	}

	for (const VarRefInfo& existing : values) {
		if (existing.name == value.name && existing.varLoc == value.varLoc) {
			return;
		}
	}
	values.push_back(value);
}

std::string GetValueDeclTypeString(const clang::ValueDecl* valueDecl) {
	if (valueDecl == nullptr) {
		return "";
	}
	return valueDecl->getType().getAsString();
}

class DeclRefCollector : public clang::RecursiveASTVisitor<DeclRefCollector> {
public:
	explicit DeclRefCollector(const clang::SourceManager& sourceManager) : sourceManager_(sourceManager) {}

	bool VisitDeclRefExpr(clang::DeclRefExpr* declRefExpr) {
		if (declRefExpr == nullptr) {
			return true;
		}

		const clang::ValueDecl* valueDecl = declRefExpr->getDecl();
		if (valueDecl == nullptr) {
			return true;
		}

		const bool isVariableLike =
			llvm::isa<clang::VarDecl>(valueDecl) ||
			llvm::isa<clang::ParmVarDecl>(valueDecl) ||
			llvm::isa<clang::ImplicitParamDecl>(valueDecl) ||
			llvm::isa<clang::BindingDecl>(valueDecl);
		if (!isVariableLike) {
			return true;
		}

		VarRefInfo ref;
		ref.name = declRefExpr->getNameInfo().getAsString();
		ref.varLoc = GetLocationString(sourceManager_, declRefExpr->getLocation(), true);
		ref.type = GetValueDeclTypeString(valueDecl);
		AddUniqueVarRef(refs_, ref);
		return true;
	}

	const std::vector<VarRefInfo>& Refs() const { return refs_; }

private:
	const clang::SourceManager& sourceManager_;
	std::vector<VarRefInfo> refs_;
};

std::vector<VarRefInfo> CollectDeclRefs(const clang::Stmt* stmt, const clang::SourceManager& sourceManager) {
	if (stmt == nullptr) {
		return {};
	}

	DeclRefCollector collector(sourceManager);
	collector.TraverseStmt(const_cast<clang::Stmt*>(stmt));
	return collector.Refs();
}

std::string GuessNodeType(const clang::Stmt* stmt) {
	if (stmt == nullptr) {
		return "unknown";
	}

	if (llvm::isa<clang::CallExpr>(stmt)) {
		return "call";
	}
	if (llvm::isa<clang::ReturnStmt>(stmt)) {
		return "return";
	}
	if (llvm::isa<clang::IfStmt>(stmt) || llvm::isa<clang::WhileStmt>(stmt) || llvm::isa<clang::DoStmt>(stmt) ||
		llvm::isa<clang::ForStmt>(stmt) || llvm::isa<clang::SwitchStmt>(stmt)) {
		return "branch";
	}
	if (llvm::isa<clang::DeclStmt>(stmt)) {
		return "decl";
	}
	if (const auto* binaryOp = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
		if (binaryOp->isAssignmentOp() || binaryOp->isCompoundAssignmentOp()) {
			return "assign";
		}
		return "expr";
	}
	if (const auto* unaryOp = llvm::dyn_cast<clang::UnaryOperator>(stmt)) {
		if (unaryOp->isIncrementDecrementOp()) {
			return "assign";
		}
		return "expr";
	}

	if (llvm::isa<clang::Expr>(stmt)) {
		return "expr";
	}
	return "stmt";
}

const clang::CallExpr* FindTopCallExpr(const clang::Stmt* stmt) {
	if (stmt == nullptr) {
		return nullptr;
	}

	if (const auto* callExpr = llvm::dyn_cast<clang::CallExpr>(stmt)) {
		return callExpr;
	}

	if (const auto* binaryOp = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
		return llvm::dyn_cast<clang::CallExpr>(binaryOp->getRHS()->IgnoreParenImpCasts());
	}

	if (const auto* declStmt = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
		for (const clang::Decl* decl : declStmt->decls()) {
			const auto* varDecl = llvm::dyn_cast<clang::VarDecl>(decl);
			if (varDecl == nullptr || !varDecl->hasInit()) {
				continue;
			}
			if (const auto* callExpr = llvm::dyn_cast<clang::CallExpr>(varDecl->getInit()->IgnoreParenImpCasts())) {
				return callExpr;
			}
		}
	}

	return nullptr;
}

std::string GetCalleeName(const clang::CallExpr* callExpr) {
	if (callExpr == nullptr) {
		return "";
	}

	const clang::Expr* callee = callExpr->getCallee();
	if (callee == nullptr) {
		return "";
	}

	callee = callee->IgnoreParenImpCasts();
	if (const auto* declRef = llvm::dyn_cast<clang::DeclRefExpr>(callee)) {
		return declRef->getNameInfo().getAsString();
	}
	if (const auto* memberExpr = llvm::dyn_cast<clang::MemberExpr>(callee)) {
		return memberExpr->getMemberDecl() ? memberExpr->getMemberDecl()->getNameAsString() : "";
	}

	return "";
}

std::vector<VarRefInfo> CollectDefs(const clang::Stmt* stmt, const clang::SourceManager& sourceManager) {
	std::vector<VarRefInfo> defs;
	if (stmt == nullptr) {
		return defs;
	}

	if (const auto* declStmt = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
		for (const clang::Decl* decl : declStmt->decls()) {
			if (const auto* varDecl = llvm::dyn_cast<clang::VarDecl>(decl)) {
				VarRefInfo ref;
				ref.name = varDecl->getNameAsString();
				ref.varLoc = GetLocationString(sourceManager, varDecl->getLocation(), true);
				ref.type = varDecl->getType().getAsString();
				AddUniqueVarRef(defs, ref);
			}
		}
		return defs;
	}

	if (const auto* binaryOp = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
		if (binaryOp->isAssignmentOp() || binaryOp->isCompoundAssignmentOp()) {
			for (const VarRefInfo& ref : CollectDeclRefs(binaryOp->getLHS(), sourceManager)) {
				AddUniqueVarRef(defs, ref);
			}
		}
		return defs;
	}

	if (const auto* unaryOp = llvm::dyn_cast<clang::UnaryOperator>(stmt)) {
		if (unaryOp->isIncrementDecrementOp()) {
			for (const VarRefInfo& ref : CollectDeclRefs(unaryOp->getSubExpr(), sourceManager)) {
				AddUniqueVarRef(defs, ref);
			}
		}
		return defs;
	}

	return defs;
}

std::vector<VarRefInfo> CollectUses(const clang::Stmt* stmt, const clang::SourceManager& sourceManager) {
	std::vector<VarRefInfo> uses;
	if (stmt == nullptr) {
		return uses;
	}

	if (const clang::Stmt* branchCond = GetBranchConditionStmt(stmt)) {
		for (const VarRefInfo& ref : CollectDeclRefs(branchCond, sourceManager)) {
			AddUniqueVarRef(uses, ref);
		}
		return uses;
	}

	if (const auto* declStmt = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
		for (const clang::Decl* decl : declStmt->decls()) {
			const auto* varDecl = llvm::dyn_cast<clang::VarDecl>(decl);
			if (varDecl == nullptr || !varDecl->hasInit()) {
				continue;
			}
			for (const VarRefInfo& ref : CollectDeclRefs(varDecl->getInit(), sourceManager)) {
				AddUniqueVarRef(uses, ref);
			}
		}
		return uses;
	}

	if (const auto* binaryOp = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
		if (binaryOp->isAssignmentOp()) {
			for (const VarRefInfo& ref : CollectDeclRefs(binaryOp->getRHS(), sourceManager)) {
				AddUniqueVarRef(uses, ref);
			}
			return uses;
		}

		if (binaryOp->isCompoundAssignmentOp()) {
			for (const VarRefInfo& ref : CollectDeclRefs(binaryOp->getLHS(), sourceManager)) {
				AddUniqueVarRef(uses, ref);
			}
			for (const VarRefInfo& ref : CollectDeclRefs(binaryOp->getRHS(), sourceManager)) {
				AddUniqueVarRef(uses, ref);
			}
			return uses;
		}
	}

	if (const auto* unaryOp = llvm::dyn_cast<clang::UnaryOperator>(stmt)) {
		if (unaryOp->isIncrementDecrementOp()) {
			for (const VarRefInfo& ref : CollectDeclRefs(unaryOp->getSubExpr(), sourceManager)) {
				AddUniqueVarRef(uses, ref);
			}
			return uses;
		}
	}

	for (const VarRefInfo& ref : CollectDeclRefs(stmt, sourceManager)) {
		AddUniqueVarRef(uses, ref);
	}
	return uses;
}

llvm::json::Array BuildVarRefsJson(const std::vector<VarRefInfo>& refs) {
	llvm::json::Array refsJson;
	for (const VarRefInfo& ref : refs) {
		llvm::json::Object refJson;
			refJson["name"] = ref.name;
			refJson["var_loc"] = ref.varLoc;
			refJson["type"] = ref.type;
			refsJson.push_back(std::move(refJson));
	}
	return refsJson;
}

struct NormalizedStmtRef {
	const clang::Stmt* stmt = nullptr;
	const clang::CFGBlock* block = nullptr;
	int64_t indexInBlock = -1;
	bool isTerminator = false;
};

using StmtIdMap = std::unordered_map<const clang::Stmt*, std::vector<int64_t>>;

struct ControlEdge {
	int64_t from = -1;
	int64_t to = -1;
	std::string kind;
};

const clang::CFGBlock* FindFirstStmtBlockOnPath(const clang::CFGBlock* startBlock) {
	if (startBlock == nullptr) {
		return nullptr;
	}

	std::deque<const clang::CFGBlock*> worklist;
	llvm::SmallPtrSet<const clang::CFGBlock*, 32> visited;
	worklist.push_back(startBlock);
	visited.insert(startBlock);

	while (!worklist.empty()) {
		const clang::CFGBlock* current = worklist.front();
		worklist.pop_front();

		bool hasStmt = false;
		for (clang::CFGElement element : *current) {
			if (element.getAs<clang::CFGStmt>().has_value()) {
				hasStmt = true;
				break;
			}
		}
		if (!hasStmt && current->getTerminatorStmt() != nullptr) {
			hasStmt = true;
		}

		if (hasStmt) {
			return current;
		}

		for (auto succIt = current->succ_begin(); succIt != current->succ_end(); ++succIt) {
			const clang::CFGBlock* succBlock = succIt->getReachableBlock();
			if (succBlock == nullptr || visited.contains(succBlock)) {
				continue;
			}
			visited.insert(succBlock);
			worklist.push_back(succBlock);
		}
	}

	return nullptr;
}

std::vector<NormalizedStmtRef> CollectBlockStmtRefs(const clang::CFGBlock* block) {
	std::vector<NormalizedStmtRef> refs;
	if (block == nullptr) {
		return refs;
	}

	int64_t idx = 0;
	for (clang::CFGElement element : *block) {
		if (auto cfgStmt = element.getAs<clang::CFGStmt>()) {
			const clang::Stmt* stmt = cfgStmt->getStmt();
			if (stmt == nullptr) {
				continue;
			}

			refs.push_back(NormalizedStmtRef{stmt, block, idx, false});
			++idx;
		}
	}

	if (const clang::Stmt* terminator = block->getTerminatorStmt()) {
		if (refs.empty() || refs.back().stmt != terminator) {
			refs.push_back(NormalizedStmtRef{terminator, block, idx, true});
		}
	}

	return refs;
}

struct ControlFlowContext {
	std::string owner;
	std::string slot;
};

void AddStmtId(StmtIdMap& stmtIdsByStmt, const clang::Stmt* stmt, int64_t stmtId) {
	if (stmt == nullptr || stmtId <= 0) {
		return;
	}

	auto& stmtIds = stmtIdsByStmt[stmt];
	if (std::find(stmtIds.begin(), stmtIds.end(), stmtId) == stmtIds.end()) {
		stmtIds.push_back(stmtId);
	}
}

void AddNodeId(StmtIdMap& astNodeIdsByStmt, const clang::Stmt* stmt, int64_t nodeId) {
	if (stmt == nullptr || nodeId <= 0) {
		return;
	}

	auto& nodeIds = astNodeIdsByStmt[stmt];
	if (std::find(nodeIds.begin(), nodeIds.end(), nodeId) == nodeIds.end()) {
		nodeIds.push_back(nodeId);
	}
}

llvm::json::Array BuildStmtIdsJson(const clang::Stmt* stmt, const StmtIdMap* stmtIdsByStmt) {
	llvm::json::Array stmtIdsJson;
	if (stmtIdsByStmt == nullptr || stmt == nullptr) {
		return stmtIdsJson;
	}

	auto it = stmtIdsByStmt->find(stmt);
	if (it == stmtIdsByStmt->end()) {
		return stmtIdsJson;
	}

	for (int64_t stmtId : it->second) {
		stmtIdsJson.push_back(stmtId);
	}
	return stmtIdsJson;
}

llvm::json::Object BuildAstNodeJson(
	const clang::Stmt* stmt,
	const clang::ASTContext& context,
	const ControlFlowContext* controlFlowContext,
	int64_t nodeId,
	int64_t parentId,
	int64_t& nextNodeId,
	const StmtIdMap* stmtIdsByStmt,
	StmtIdMap* astNodeIdsByStmt,
	const std::string& parentOriginalText,
	int64_t parentOriginalLength) {
	llvm::json::Object node;
	node["node_id"] = nodeId;
	node["parent_id"] = parentId;
	node["stmt_ids"] = BuildStmtIdsJson(stmt, stmtIdsByStmt);
	if (astNodeIdsByStmt != nullptr) {
		AddNodeId(*astNodeIdsByStmt, stmt, nodeId);
	}
	if (stmt == nullptr) {
		node["kind"] = "NullStmt";
		node["location"] = "<invalid>";
		node["spelling_loc"] = "<invalid>";
		node["expansion_loc"] = "<invalid>";
		node["start_line"] = -1;
		node["end_line"] = -1;
		node["text"] = "<null-stmt>";
		node["original_text"] = "";
		node["children"] = llvm::json::Array();
		return node;
	}

	node["kind"] = stmt->getStmtClassName();
	node["location"] = GetLocationString(context.getSourceManager(), stmt->getBeginLoc(), false);
	node["spelling_loc"] = GetLocationString(context.getSourceManager(), stmt->getBeginLoc(), true);
	node["expansion_loc"] = GetLocationString(context.getSourceManager(), stmt->getBeginLoc(), false);
	node["start_line"] = GetStmtStartLine(context.getSourceManager(), stmt, false);
	node["end_line"] = GetStmtEndLine(context.getSourceManager(), context.getLangOpts(), stmt, false);
	node["text"] = RenderStmt(stmt, context.getLangOpts());
	std::string originalText = GetOriginalText(stmt, context.getSourceManager(), context.getLangOpts());
	const int64_t originalLength = GetExpansionRangeLength(stmt, context.getSourceManager(), context.getLangOpts());
	if (!originalText.empty() &&
		!parentOriginalText.empty() &&
		originalText == parentOriginalText &&
		parentOriginalLength >= 0 &&
		originalLength >= 0 &&
		parentOriginalLength >= originalLength) {
		originalText.clear();
	}
	node["original_text"] = originalText;
	if (controlFlowContext != nullptr) {
		llvm::json::Object cfContext;
		cfContext["owner"] = controlFlowContext->owner;
		cfContext["slot"] = controlFlowContext->slot;
		node["cf_context"] = std::move(cfContext);
	}

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
	const clang::Stmt* branchCondition = GetBranchConditionStmt(stmt);
	const std::string stmtKind = stmt->getStmtClassName();
	ControlFlowContext conditionContext;
	conditionContext.owner = stmtKind;
	conditionContext.slot = "cond";
	ControlFlowContext forInitContext;
	forInitContext.owner = "ForStmt";
	forInitContext.slot = "init";
	ControlFlowContext forIncContext;
	forIncContext.owner = "ForStmt";
	forIncContext.slot = "inc";
	const clang::Stmt* forInit = nullptr;
	const clang::Stmt* forInc = nullptr;
	if (const auto* forStmt = llvm::dyn_cast<clang::ForStmt>(stmt)) {
		forInit = forStmt->getInit();
		forInc = forStmt->getInc();
	}
	for (const clang::Stmt* child : stmt->children()) {
		if (child != nullptr) {
			const bool isConditionChild = (branchCondition != nullptr && child == branchCondition);
			const bool isForInitChild = (forInit != nullptr && child == forInit);
			const bool isForIncChild = (forInc != nullptr && child == forInc);
			const ControlFlowContext* childContext = nullptr;
			if (isConditionChild) {
				childContext = &conditionContext;
			} else if (isForInitChild) {
				childContext = &forInitContext;
			} else if (isForIncChild) {
				childContext = &forIncContext;
			}
			const int64_t childNodeId = nextNodeId++;
			const std::string& nextOriginalText = originalText.empty() ? parentOriginalText : originalText;
			const int64_t nextOriginalLength = originalText.empty() ? parentOriginalLength : originalLength;
			children.push_back(BuildAstNodeJson(
				child,
				context,
				childContext,
				childNodeId,
				nodeId,
				nextNodeId,
				stmtIdsByStmt,
				astNodeIdsByStmt,
				nextOriginalText,
				nextOriginalLength));
		}
	}
	node["children"] = std::move(children);
	return node;
}

} // namespace

llvm::json::Object CFGPrinter::BuildFunctionAstJson(
	const clang::FunctionDecl& functionDecl,
	clang::ASTContext& context,
	const std::unordered_map<const clang::Stmt*, std::vector<int64_t>>* stmtIdsByStmt,
	std::unordered_map<const clang::Stmt*, std::vector<int64_t>>* astNodeIdsByStmt) {
	const clang::SourceManager& sourceManager = context.getSourceManager();
	llvm::json::Object ast;
	ast["function"] = functionDecl.getNameAsString();
	ast["return_type"] = functionDecl.getReturnType().getAsString();
	ast["location"] = GetLocationString(sourceManager, functionDecl.getLocation(), false);
	ast["spelling_loc"] = GetLocationString(sourceManager, functionDecl.getLocation(), true);
	ast["expansion_loc"] = GetLocationString(sourceManager, functionDecl.getLocation(), false);
	ast["start_line"] = GetLine(sourceManager, functionDecl.getBeginLoc(), false);
	ast["end_line"] = GetLine(sourceManager, functionDecl.getEndLoc(), false);

	llvm::json::Array params;
	for (unsigned i = 0; i < functionDecl.getNumParams(); ++i) {
		const clang::ParmVarDecl* param = functionDecl.getParamDecl(i);
		llvm::json::Object p;
		p["name"] = param->getNameAsString();
		p["type"] = param->getType().getAsString();
		params.push_back(std::move(p));
	}
	ast["params"] = std::move(params);

	int64_t nextNodeId = 2;
	ast["body"] = BuildAstNodeJson(
		functionDecl.getBody(),
		context,
		nullptr,
		1,
		-1,
		nextNodeId,
		stmtIdsByStmt,
		astNodeIdsByStmt,
		std::string(),
		-1);
	return ast;
}

llvm::json::Object CFGPrinter::BuildFunctionCfgJson(
	const clang::FunctionDecl& functionDecl,
	const clang::CFG* cfg,
	clang::ASTContext& context,
	const std::unordered_map<const clang::Stmt*, std::vector<int64_t>>* astNodeIdsByStmt) {
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
		std::vector<int64_t> astNodeIds;
		if (astNodeIdsByStmt != nullptr) {
			for (clang::CFGElement element : *block) {
				if (auto cfgStmt = element.getAs<clang::CFGStmt>()) {
					const clang::Stmt* stmt = cfgStmt->getStmt();
					if (stmt == nullptr) {
						continue;
					}
					auto it = astNodeIdsByStmt->find(stmt);
					if (it == astNodeIdsByStmt->end()) {
						continue;
					}
					for (int64_t nodeId : it->second) {
						astNodeIds.push_back(nodeId);
					}
				}
			}
			if (const clang::Stmt* terminator = block->getTerminatorStmt()) {
				auto it = astNodeIdsByStmt->find(terminator);
				if (it != astNodeIdsByStmt->end()) {
					for (int64_t nodeId : it->second) {
						astNodeIds.push_back(nodeId);
					}
				}
			}
			std::sort(astNodeIds.begin(), astNodeIds.end());
			astNodeIds.erase(std::unique(astNodeIds.begin(), astNodeIds.end()), astNodeIds.end());
		}
		llvm::json::Array astNodeIdsJson;
		for (int64_t nodeId : astNodeIds) {
			astNodeIdsJson.push_back(nodeId);
		}
		b["ast_node_ids"] = std::move(astNodeIdsJson);

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

llvm::json::Object CFGPrinter::BuildFunctionNormalizedIrJson(
	const clang::FunctionDecl& functionDecl,
	const clang::CFG* cfg,
	clang::ASTContext& context,
	std::unordered_map<const clang::Stmt*, std::vector<int64_t>>* stmtIdsByStmt) {
	(void)functionDecl;
	llvm::json::Object irJson;
	irJson["version"] = "v1";

	if (cfg == nullptr) {
		irJson["status"] = "failed";
		irJson["nodes"] = llvm::json::Array();
		llvm::json::Object edges;
		edges["control_flow"] = llvm::json::Array();
		irJson["edges"] = std::move(edges);
		return irJson;
	}

	irJson["status"] = "ok";

	std::vector<const clang::CFGBlock*> blocks;
	blocks.reserve(cfg->size());
	for (const clang::CFGBlock* block : *cfg) {
		blocks.push_back(block);
	}
	std::sort(blocks.begin(), blocks.end(), [](const clang::CFGBlock* lhs, const clang::CFGBlock* rhs) {
		return lhs->getBlockID() < rhs->getBlockID();
	});

	std::unordered_map<const clang::CFGBlock*, std::vector<NormalizedStmtRef>> blockToStmts;
	std::unordered_map<const clang::CFGBlock*, std::vector<int64_t>> blockStmtIds;
	std::unordered_map<const clang::CFGBlock*, int64_t> firstStmtIdInBlock;

	struct NodeRecord {
		int64_t stmtId;
		NormalizedStmtRef ref;
	};
	std::vector<NodeRecord> orderedNodes;

	int64_t nextStmtId = 1;
	for (const clang::CFGBlock* block : blocks) {
		std::vector<NormalizedStmtRef> refs = CollectBlockStmtRefs(block);
		if (refs.empty()) {
			blockToStmts.emplace(block, std::move(refs));
			blockStmtIds.emplace(block, std::vector<int64_t>());
			continue;
		}

		std::vector<int64_t> ids;
		ids.reserve(refs.size());

		for (const NormalizedStmtRef& ref : refs) {
			orderedNodes.push_back(NodeRecord{nextStmtId, ref});
			ids.push_back(nextStmtId);
			if (stmtIdsByStmt != nullptr) {
				AddStmtId(*stmtIdsByStmt, ref.stmt, nextStmtId);
			}
			if (!firstStmtIdInBlock.count(block)) {
				firstStmtIdInBlock[block] = nextStmtId;
			}
			++nextStmtId;
		}

		blockToStmts.emplace(block, std::move(refs));
		blockStmtIds.emplace(block, std::move(ids));
	}

	std::vector<ControlEdge> edges;
	for (const clang::CFGBlock* block : blocks) {
		auto it = blockToStmts.find(block);
		if (it == blockToStmts.end() || it->second.empty()) {
			continue;
		}

		const std::vector<NormalizedStmtRef>& refs = it->second;
		auto idIt = blockStmtIds.find(block);
		if (idIt == blockStmtIds.end() || idIt->second.size() != refs.size()) {
			continue;
		}
		const std::vector<int64_t>& ids = idIt->second;

		for (size_t i = 1; i < refs.size(); ++i) {
			edges.push_back(ControlEdge{ids[i - 1], ids[i], "fallthrough"});
		}

		const int64_t fromStmtId = ids.back();
		for (auto succIt = block->succ_begin(); succIt != block->succ_end(); ++succIt) {
			const clang::CFGBlock* succ = succIt->getReachableBlock();
			if (succ == nullptr) {
				continue;
			}

			const clang::CFGBlock* nextBlock = FindFirstStmtBlockOnPath(succ);
			if (nextBlock == nullptr) {
				continue;
			}

			auto firstIt = firstStmtIdInBlock.find(nextBlock);
			if (firstIt == firstStmtIdInBlock.end()) {
				continue;
			}

			edges.push_back(ControlEdge{
				fromStmtId,
				firstIt->second,
				(nextBlock->getBlockID() <= block->getBlockID()) ? "backedge" : "branch"});
		}
	}

	std::set<std::pair<int64_t, int64_t>> dedupEdgeSet;
	std::vector<ControlEdge> dedupEdges;
	for (const ControlEdge& edge : edges) {
		if (edge.from <= 0 || edge.to <= 0) {
			continue;
		}
		if (dedupEdgeSet.insert({edge.from, edge.to}).second) {
			dedupEdges.push_back(edge);
		}
	}

	std::unordered_map<int64_t, std::vector<int64_t>> predMap;
	std::unordered_map<int64_t, std::vector<int64_t>> succMap;
	for (const ControlEdge& edge : dedupEdges) {
		succMap[edge.from].push_back(edge.to);
		predMap[edge.to].push_back(edge.from);
	}

	auto sortAndUnique = [](std::vector<int64_t>& values) {
		std::sort(values.begin(), values.end());
		values.erase(std::unique(values.begin(), values.end()), values.end());
	};

	for (auto& kv : predMap) {
		sortAndUnique(kv.second);
	}
	for (auto& kv : succMap) {
		sortAndUnique(kv.second);
	}

	llvm::json::Array nodesJson;
	const clang::SourceManager& sourceManager = context.getSourceManager();
	for (const NodeRecord& nodeRecord : orderedNodes) {
		const int64_t stmtId = nodeRecord.stmtId;
		const NormalizedStmtRef& ref = nodeRecord.ref;

		llvm::json::Object node;
		node["stmt_id"] = stmtId;
		node["bb_id"] = static_cast<int64_t>(ref.block->getBlockID());
		node["index_in_bb"] = ref.indexInBlock;
		node["start_line"] = GetStmtStartLine(sourceManager, ref.stmt, false);
		node["end_line"] = GetStmtEndLine(sourceManager, context.getLangOpts(), ref.stmt, false);

		llvm::json::Object loc;
		loc["spelling_loc"] = BuildLineColumnLocJson(sourceManager, ref.stmt->getBeginLoc(), true);
		loc["expansion_loc"] = BuildLineColumnLocJson(sourceManager, ref.stmt->getBeginLoc(), false);
		loc["file"] = GetFilenameString(sourceManager, ref.stmt->getBeginLoc(), false);
		loc["line"] = GetLine(sourceManager, ref.stmt->getBeginLoc(), false);
		loc["column"] = GetColumn(sourceManager, ref.stmt->getBeginLoc(), false);
		node["loc"] = std::move(loc);

		const clang::CallExpr* callExpr = FindTopCallExpr(ref.stmt);
		std::string nodeType = GuessNodeType(ref.stmt);
		if (callExpr != nullptr) {
			nodeType = "call";
		}

		node["type"] = nodeType;
		node["ast_kind"] = ref.stmt->getStmtClassName();
		if (nodeType == "branch") {
			node["text"] = RenderBranchCondition(ref.stmt, context.getLangOpts());
		} else {
			node["text"] = RenderStmt(ref.stmt, context.getLangOpts());
		}
		node["is_terminator"] = ref.isTerminator;

		std::vector<VarRefInfo> defs = CollectDefs(ref.stmt, sourceManager);
		std::vector<VarRefInfo> uses = CollectUses(ref.stmt, sourceManager);
		node["defs"] = BuildVarRefsJson(defs);
		node["uses"] = BuildVarRefsJson(uses);

		if (callExpr != nullptr) {
			llvm::json::Object call;
			call["callee"] = GetCalleeName(callExpr);

			llvm::json::Array args;
			for (unsigned i = 0; i < callExpr->getNumArgs(); ++i) {
				args.push_back(RenderStmt(callExpr->getArg(i), context.getLangOpts()));
			}
			call["args"] = std::move(args);

			std::string retTarget;
			if (const auto* binaryOp = llvm::dyn_cast<clang::BinaryOperator>(ref.stmt)) {
				if (binaryOp->isAssignmentOp() || binaryOp->isCompoundAssignmentOp()) {
					std::vector<VarRefInfo> lhsRefs = CollectDeclRefs(binaryOp->getLHS(), sourceManager);
					if (!lhsRefs.empty()) {
						retTarget = lhsRefs.front().name;
					}
				}
			} else if (const auto* declStmt = llvm::dyn_cast<clang::DeclStmt>(ref.stmt)) {
				for (const clang::Decl* decl : declStmt->decls()) {
					if (const auto* varDecl = llvm::dyn_cast<clang::VarDecl>(decl)) {
						if (varDecl->hasInit() && llvm::isa<clang::CallExpr>(varDecl->getInit()->IgnoreParenImpCasts())) {
							retTarget = varDecl->getNameAsString();
							break;
						}
					}
				}
			}
			call["ret_target"] = retTarget;
			node["call"] = std::move(call);
		}

		llvm::json::Array predJson;
		if (predMap.count(stmtId)) {
			for (int64_t p : predMap[stmtId]) {
				predJson.push_back(p);
			}
		}
		node["ctrl_pred"] = std::move(predJson);

		llvm::json::Array succJson;
		if (succMap.count(stmtId)) {
			for (int64_t s : succMap[stmtId]) {
				succJson.push_back(s);
			}
		}
		node["ctrl_succ"] = std::move(succJson);
		if (stmtIdsByStmt != nullptr) {
			AddStmtId(*stmtIdsByStmt, ref.stmt, stmtId);
		}

		nodesJson.push_back(std::move(node));
	}
	irJson["nodes"] = std::move(nodesJson);

	llvm::json::Array edgesJson;
	for (const ControlEdge& edge : dedupEdges) {
		llvm::json::Object edgeJson;
		edgeJson["from"] = edge.from;
		edgeJson["to"] = edge.to;
		edgeJson["kind"] = edge.kind;
		edgesJson.push_back(std::move(edgeJson));
	}

	llvm::json::Object edgesObj;
	edgesObj["control_flow"] = std::move(edgesJson);
	irJson["edges"] = std::move(edgesObj);

	return irJson;
}

} // namespace cfgbuilder::output
