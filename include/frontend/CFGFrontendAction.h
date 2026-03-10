// 本文件用于声明前端动作，连接 Clang 前端与 AST 处理流程。

#pragma once

#include "app/Config.h"

#include "clang/Frontend/FrontendActions.h"

#include <string>

namespace cfgbuilder::frontend {

// 控制 JSON 输出内容：仅 AST、仅 CFG、或两者都输出。
enum class JsonOutputMode {
	AstOnly,
	CfgOnly,
	AstAndCfg,
};

// 在单 TU 中收集函数并打印函数 AST 与 CFG。
class CFGFrontendAction : public clang::ASTFrontendAction {
public:
	CFGFrontendAction(
		const cfgbuilder::app::Config& config,
		JsonOutputMode outputMode,
		std::string* jsonBuffer);

protected:
	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
		clang::CompilerInstance& compilerInstance,
		llvm::StringRef inFile) override;

private:
	const cfgbuilder::app::Config& config_;
	JsonOutputMode outputMode_;
	std::string* jsonBuffer_;
};

} // namespace cfgbuilder::frontend