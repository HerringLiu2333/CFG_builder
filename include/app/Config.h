// 本文件用于声明项目运行配置项与命令行解析接口。

#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace cfgbuilder::app {

// 阶段 A 最小配置：单 TU、少量函数、文本输出路径。
struct Config {
	std::string compdbPath;
	std::string sourceFile;
	std::string outputPath;
	std::vector<std::string> functions;
	bool showHelp = false;
};

// 负责解析命令行并做基础校验。
class ConfigParser {
public:
	static bool Parse(int argc, char** argv, Config& config, std::string& errorMessage);
	static void PrintUsage(std::ostream& os, const char* programName);

private:
	static bool Validate(const Config& config, std::string& errorMessage);
};

} // namespace cfgbuilder::app