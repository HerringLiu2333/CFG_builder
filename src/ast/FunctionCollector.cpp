// 本文件用于实现函数收集逻辑，筛选并回调可分析函数。

#include "ast/FunctionCollector.h"

#include "clang/Basic/SourceManager.h"

namespace cfgbuilder::ast {

FunctionCollector::FunctionCollector(clang::ASTContext& context, const std::vector<std::string>& targetFunctions)
	: context_(context), targetFunctions_(targetFunctions) {}

void FunctionCollector::Collect(std::vector<const clang::FunctionDecl*>& outFunctions) {
	collectedFunctions_.clear();
	TraverseDecl(context_.getTranslationUnitDecl());
	outFunctions = collectedFunctions_;
}

bool FunctionCollector::VisitFunctionDecl(clang::FunctionDecl* functionDecl) {
	if (functionDecl == nullptr) {
		return true;
	}

	if (!functionDecl->doesThisDeclarationHaveABody()) {
		return true;
	}

	if (!functionDecl->isThisDeclarationADefinition()) {
		return true;
	}

	if (!IsInMainFile(functionDecl)) {
		return true;
	}

	if (!IsTargetFunction(functionDecl)) {
		return true;
	}

	collectedFunctions_.push_back(functionDecl);
	return true;
}

bool FunctionCollector::IsInMainFile(const clang::FunctionDecl* functionDecl) const {
	const clang::SourceManager& sourceManager = context_.getSourceManager();
	const clang::SourceLocation location = functionDecl->getLocation();
	return sourceManager.isWrittenInMainFile(sourceManager.getExpansionLoc(location));
}

bool FunctionCollector::IsTargetFunction(const clang::FunctionDecl* functionDecl) const {
	if (targetFunctions_.empty()) {
		return true;
	}

	const std::string currentName = functionDecl->getNameAsString();
	for (const std::string& targetName : targetFunctions_) {
		if (currentName == targetName) {
			return true;
		}
	}

	return false;
}

} // namespace cfgbuilder::ast