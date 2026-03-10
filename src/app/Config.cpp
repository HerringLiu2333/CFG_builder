// 本文件用于实现运行配置项的解析与管理逻辑。

#include "app/Config.h"

#include <sstream>

namespace cfgbuilder::app {
namespace {

bool IsOption(const std::string& token, const std::string& longName) {
	return token == longName || token.rfind(longName + "=", 0) == 0;
}

bool ReadOptionValue(const std::vector<std::string>& args,
					 int& i,
					 const std::string& longName,
					 std::string& out,
					 std::string& errorMessage) {
	const std::string& token = args[static_cast<std::size_t>(i)];
	const std::string prefix = longName + "=";

	if (token.rfind(prefix, 0) == 0) {
		out = token.substr(prefix.size());
		if (out.empty()) {
			errorMessage = "参数值不能为空: " + longName;
			return false;
		}
		return true;
	}

	if (i + 1 >= static_cast<int>(args.size())) {
		errorMessage = "缺少参数值: " + longName;
		return false;
	}

	const std::string& next = args[static_cast<std::size_t>(i + 1)];
	if (!next.empty() && next[0] == '-') {
		errorMessage = "缺少参数值: " + longName;
		return false;
	}

	out = next;
	++i;
	return true;
}

std::vector<std::string> SplitFunctions(const std::string& input) {
	std::vector<std::string> result;
	std::stringstream ss(input);
	std::string item;
	while (std::getline(ss, item, ',')) {
		if (!item.empty()) {
			result.push_back(item);
		}
	}
	return result;
}

} // namespace

bool ConfigParser::Parse(int argc, char** argv, Config& config, std::string& errorMessage) {
	if (argc <= 1) {
		config.showHelp = true;
		return true;
	}

	std::vector<std::string> args;
	args.reserve(static_cast<std::size_t>(argc));
	for (int i = 0; i < argc; ++i) {
		args.emplace_back(argv[i]);
	}

	for (int i = 1; i < argc; ++i) {
		const std::string& token = args[static_cast<std::size_t>(i)];

		if (token == "-h" || token == "--help") {
			config.showHelp = true;
			continue;
		}

		if (IsOption(token, "--compdb")) {
			if (!ReadOptionValue(args, i, "--compdb", config.compdbPath, errorMessage)) {
				return false;
			}
			continue;
		}

		if (IsOption(token, "--file")) {
			if (!ReadOptionValue(args, i, "--file", config.sourceFile, errorMessage)) {
				return false;
			}
			continue;
		}

		if (IsOption(token, "--output")) {
			if (!ReadOptionValue(args, i, "--output", config.outputPath, errorMessage)) {
				return false;
			}
			continue;
		}

		if (IsOption(token, "--functions")) {
			std::string value;
			if (!ReadOptionValue(args, i, "--functions", value, errorMessage)) {
				return false;
			}
			config.functions = SplitFunctions(value);
			continue;
		}

		errorMessage = "未知参数: " + token;
		return false;
	}

	if (config.showHelp) {
		return true;
	}

	return Validate(config, errorMessage);
}

void ConfigParser::PrintUsage(std::ostream& os, const char* programName) {
	os << "用法:\n"
	   << "  " << programName << " --compdb <path> --file <source.c> [options]\n\n"
	   << "必选参数:\n"
	   << "  --compdb <path>        compile_commands.json 所在路径\n"
	   << "  --file <source.c>      需要分析的单个源码文件\n\n"
	   << "可选参数:\n"
	   << "  --functions <a,b,c>    仅分析指定函数名（逗号分隔）\n"
	   << "  --output <path>        输出文件路径（默认标准输出）\n"
	   << "  -h, --help             显示帮助\n";
}

bool ConfigParser::Validate(const Config& config, std::string& errorMessage) {
	if (config.compdbPath.empty()) {
		errorMessage = "缺少必选参数: --compdb";
		return false;
	}

	if (config.sourceFile.empty()) {
		errorMessage = "缺少必选参数: --file";
		return false;
	}

	return true;
}

} // namespace cfgbuilder::app