// 本文件用于声明函数收集器，从 AST 中筛选待构建 CFG 的函数。

#pragma once

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include <string>
#include <vector>

namespace cfgbuilder::ast {

// 收集“主文件中且有函数体”的函数定义，可按函数名白名单过滤。
class FunctionCollector : public clang::RecursiveASTVisitor<FunctionCollector> {
public:
	FunctionCollector(clang::ASTContext& context, const std::vector<std::string>& targetFunctions);

	void Collect(std::vector<const clang::FunctionDecl*>& outFunctions);
	bool VisitFunctionDecl(clang::FunctionDecl* functionDecl);

private:
	bool IsInMainFile(const clang::FunctionDecl* functionDecl) const;
	bool IsTargetFunction(const clang::FunctionDecl* functionDecl) const;

	clang::ASTContext& context_;
	const std::vector<std::string>& targetFunctions_;
	std::vector<const clang::FunctionDecl*> collectedFunctions_;
};

} // namespace cfgbuilder::ast