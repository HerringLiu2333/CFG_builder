// 本文件用于实现前端动作，创建并串联 AST 处理流程。

#include "frontend/CFGFrontendAction.h"

#include "ast/FunctionCollector.h"
#include "output/CFGPrinter.h"

#include "clang/Analysis/CFG.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>
#include <system_error>
#include <vector>

namespace cfgbuilder::frontend {
namespace {

class CFGASTConsumer : public clang::ASTConsumer {
public:
	CFGASTConsumer(
		const cfgbuilder::app::Config& config,
		JsonOutputMode outputMode,
		std::string* jsonBuffer)
		: config_(config), outputMode_(outputMode), jsonBuffer_(jsonBuffer) {}

	void HandleTranslationUnit(clang::ASTContext& context) override {
		std::vector<const clang::FunctionDecl*> functions;
		cfgbuilder::ast::FunctionCollector collector(context, config_.functions);
		collector.Collect(functions);

		llvm::json::Object root;
		const clang::SourceManager& sourceManager = context.getSourceManager();
		const clang::SourceLocation mainLoc = sourceManager.getLocForStartOfFile(sourceManager.getMainFileID());
		const std::string mainFile = sourceManager.getFilename(mainLoc).str();
		root["source_file"] = mainFile.empty() ? std::string("<unknown>") : mainFile;
		root["functions_requested"] = static_cast<int64_t>(config_.functions.size());

		llvm::json::Array requested;
		for (const std::string& name : config_.functions) {
			requested.push_back(name);
		}
		root["requested_function_names"] = std::move(requested);

		llvm::json::Array functionArray;

		for (const clang::FunctionDecl* functionDecl : functions) {
			if (functionDecl == nullptr || functionDecl->getBody() == nullptr) {
				continue;
			}

			clang::CFG::BuildOptions buildOptions;
			std::unique_ptr<clang::CFG> cfg = clang::CFG::buildCFG(
				functionDecl,
				functionDecl->getBody(),
				&context,
				buildOptions);

			llvm::json::Object functionJson;
			functionJson["name"] = functionDecl->getNameAsString();
			if (outputMode_ == JsonOutputMode::AstOnly || outputMode_ == JsonOutputMode::AstAndCfg) {
				functionJson["ast"] = cfgbuilder::output::CFGPrinter::BuildFunctionAstJson(*functionDecl, context);
			}
			if (outputMode_ == JsonOutputMode::CfgOnly || outputMode_ == JsonOutputMode::AstAndCfg) {
				functionJson["cfg"] = cfgbuilder::output::CFGPrinter::BuildFunctionCfgJson(*functionDecl, cfg.get(), context);
				functionJson["normalized_ir"] = cfgbuilder::output::CFGPrinter::BuildFunctionNormalizedIrJson(*functionDecl, cfg.get(), context);
			}
			functionArray.push_back(std::move(functionJson));
		}
		root["functions_found"] = static_cast<int64_t>(functionArray.size());
		root["functions"] = std::move(functionArray);

		std::string jsonText;
		llvm::raw_string_ostream jsonStream(jsonText);
		jsonStream << llvm::formatv("{0:2}", llvm::json::Value(std::move(root)));
		jsonStream.flush();

		if (jsonBuffer_ != nullptr) {
			*jsonBuffer_ = jsonText;
			return;
		}

		if (!config_.outputPath.empty()) {
			std::error_code ec;
			llvm::raw_fd_ostream fileOut(config_.outputPath, ec, llvm::sys::fs::OF_Text);
			if (ec) {
				llvm::errs() << "[CFGBuilder] JSON 输出文件写入失败: " << ec.message() << "\n";
				llvm::outs() << jsonText << "\n";
			} else {
				fileOut << jsonText << "\n";
			}
		} else {
			llvm::outs() << jsonText << "\n";
		}
	}

private:
	const cfgbuilder::app::Config& config_;
	JsonOutputMode outputMode_;
	std::string* jsonBuffer_;
};

} // namespace

CFGFrontendAction::CFGFrontendAction(
	const cfgbuilder::app::Config& config,
	JsonOutputMode outputMode,
	std::string* jsonBuffer)
	: config_(config), outputMode_(outputMode), jsonBuffer_(jsonBuffer) {}

std::unique_ptr<clang::ASTConsumer> CFGFrontendAction::CreateASTConsumer(
	clang::CompilerInstance& compilerInstance,
	llvm::StringRef inFile) {
	(void)compilerInstance;
	(void)inFile;
	return std::make_unique<CFGASTConsumer>(config_, outputMode_, jsonBuffer_);
}

} // namespace cfgbuilder::frontend