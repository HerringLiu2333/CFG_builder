// 本文件用于实现工具驱动，负责加载编译数据库并执行单 TU 分析。

#include "app/ToolRunner.h"

#include "frontend/CFGFrontendAction.h"

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/JSONCompilationDatabase.h"
#include "clang/Tooling/Tooling.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace cfgbuilder::app {
namespace {

// 负责将配置对象注入 FrontendAction。
class CFGFrontendActionFactory : public clang::tooling::FrontendActionFactory {
public:
	CFGFrontendActionFactory(
		const Config& config,
		cfgbuilder::frontend::JsonOutputMode outputMode,
		std::string* jsonBuffer)
		: config_(config), outputMode_(outputMode), jsonBuffer_(jsonBuffer) {}

	std::unique_ptr<clang::FrontendAction> create() override {
		return std::make_unique<cfgbuilder::frontend::CFGFrontendAction>(config_, outputMode_, jsonBuffer_);
	}

private:
	const Config& config_;
	cfgbuilder::frontend::JsonOutputMode outputMode_;
	std::string* jsonBuffer_;
};

} // namespace

ToolRunner::ToolRunner(const Config& config) : config_(config) {}

bool ToolRunner::Run(std::string& errorMessage) const {
	return RunWithMode(cfgbuilder::frontend::JsonOutputMode::AstAndCfg, nullptr, errorMessage);
}

bool ToolRunner::RunAstJson(std::string& jsonOutput, std::string& errorMessage) const {
	jsonOutput.clear();
	return RunWithMode(cfgbuilder::frontend::JsonOutputMode::AstOnly, &jsonOutput, errorMessage);
}

bool ToolRunner::RunCfgJson(std::string& jsonOutput, std::string& errorMessage) const {
	jsonOutput.clear();
	return RunWithMode(cfgbuilder::frontend::JsonOutputMode::CfgOnly, &jsonOutput, errorMessage);
}

bool ToolRunner::RunWithMode(
	cfgbuilder::frontend::JsonOutputMode outputMode,
	std::string* jsonOutput,
	std::string& errorMessage) const {
	Config runConfig = config_;
	if (jsonOutput != nullptr) {
		// 作为接口调用时，不写文件，直接返回 JSON 字符串。
		runConfig.outputPath.clear();
	}

	const std::string compdbDir = ResolveCompdbDirectory(runConfig.compdbPath);

	std::string loadError;
	std::unique_ptr<clang::tooling::CompilationDatabase> compdb =
		clang::tooling::JSONCompilationDatabase::loadFromDirectory(
			compdbDir,
			loadError);

	if (!compdb) {
		errorMessage = "加载编译数据库失败: " + loadError + " (目录: " + compdbDir + ")";
		return false;
	}

	std::vector<std::string> sourceFiles{runConfig.sourceFile};
	clang::tooling::ClangTool tool(*compdb, sourceFiles);
	CFGFrontendActionFactory actionFactory(runConfig, outputMode, jsonOutput);

	// 执行自定义前端动作，输出函数级 AST 与 CFG。
	const int code = tool.run(&actionFactory);
	if (code != 0) {
		errorMessage = "ClangTool 执行失败，返回码: " + std::to_string(code);
		return false;
	}

	return true;
}

std::string ToolRunner::ResolveCompdbDirectory(const std::string& compdbPath) {
	const std::filesystem::path path(compdbPath);

	if (path.filename() == "compile_commands.json") {
		return path.parent_path().string();
	}

	return path.string();
}

} // namespace cfgbuilder::app