// 本文件用于程序入口，解析参数并启动分析流程。

#include "app/Config.h"
#include "app/ToolRunner.h"

#include <iostream>

int main(int argc, char** argv) {
	cfgbuilder::app::Config config;
	std::string errorMessage;

	if (!cfgbuilder::app::ConfigParser::Parse(argc, argv, config, errorMessage)) {
		std::cerr << "[CFGBuilder] 参数错误: " << errorMessage << std::endl;
		cfgbuilder::app::ConfigParser::PrintUsage(std::cerr, argv[0]);
		return 1;
	}

	if (config.showHelp) {
		cfgbuilder::app::ConfigParser::PrintUsage(std::cout, argv[0]);
		return 0;
	}

	std::cerr << "[CFGBuilder] 启动成功：" << std::endl;
	std::cerr << "[CFGBuilder] compdb: " << config.compdbPath << std::endl;
	std::cerr << "[CFGBuilder] file: " << config.sourceFile << std::endl;
	if (!config.outputPath.empty()) {
		std::cerr << "[CFGBuilder] output: " << config.outputPath << std::endl;
	}
	if (!config.functions.empty()) {
		std::cerr << "[CFGBuilder] functions: ";
		for (std::size_t i = 0; i < config.functions.size(); ++i) {
			if (i > 0) {
				std::cerr << ',';
			}
			std::cerr << config.functions[i];
		}
		std::cerr << std::endl;
	}

	cfgbuilder::app::ToolRunner runner(config);
	if (!runner.Run(errorMessage)) {
		std::cerr << "[CFGBuilder] ToolRunner 失败: " << errorMessage << std::endl;
		return 1;
	}

	std::cerr << "[CFGBuilder] ToolRunner 执行成功：单 TU 前端链路已跑通" << std::endl;

	return 0;
}