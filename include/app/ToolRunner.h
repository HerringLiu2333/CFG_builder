// 本文件用于声明工具驱动入口，负责启动单 TU 分析流程。

#pragma once

#include "app/Config.h"

#include <string>

namespace cfgbuilder::frontend {
enum class JsonOutputMode;
}

namespace cfgbuilder::app {

// ToolRunner 负责：加载编译数据库并执行单 TU 的最小前端动作。
class ToolRunner {
public:
	explicit ToolRunner(const Config& config);

	// 成功返回 true；失败时在 errorMessage 中写入原因。
	bool Run(std::string& errorMessage) const;

	// 仅导出 AST JSON，结果写入 jsonOutput。
	bool RunAstJson(std::string& jsonOutput, std::string& errorMessage) const;

	// 仅导出 CFG JSON，结果写入 jsonOutput。
	bool RunCfgJson(std::string& jsonOutput, std::string& errorMessage) const;

private:
	bool RunWithMode(
		cfgbuilder::frontend::JsonOutputMode outputMode,
		std::string* jsonOutput,
		std::string& errorMessage) const;

	static std::string ResolveCompdbDirectory(const std::string& compdbPath);

	const Config& config_;
};

} // namespace cfgbuilder::app