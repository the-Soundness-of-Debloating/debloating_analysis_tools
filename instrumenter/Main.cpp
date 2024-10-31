#include <stdlib.h>
#include <time.h>

#include <memory>
#include <string>

#include "FileManager.h"
#include "Frontend.h"
#include "InstruVisitors.h"
#include "Instrumentation.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang::tooling;

void checkCommandLineArgs();
void runTool(const std::vector<std::string> &inputFiles, const CompilationDatabase &compilations);

llvm::cl::OptionCategory instrumenterOptionsCategory("Instrumenter Options");
std::vector<std::string> opt_input_files;
llvm::cl::opt<bool> opt_no_compilation("no-compilation",
                                       llvm::cl::desc("Do not compile during instrumentation"),
                                       llvm::cl::cat(instrumenterOptionsCategory));
// llvm::cl::opt<std::string> opt_granu("granularity", llvm::cl::init("statement"),
//                                      llvm::cl::desc("Instrumentation Granularity"),
//                                      llvm::cl::value_desc("GRANU"),
//                                      llvm::cl::cat(instrumenterOptionsCategory));
// llvm::cl::alias _opt_granu("g", llvm::cl::desc("Alias for -granularity"),
//                            llvm::cl::aliasopt(opt_granu),
//                            llvm::cl::cat(instrumenterOptionsCategory));

int main(int argc, const char **argv) {
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream &OS) { OS << "Instrumenter version 0.1\n"; });
    auto expectedOptions =
        CommonOptionsParser::create(argc, argv, instrumenterOptionsCategory, llvm::cl::OneOrMore,
                                    "An instrumentation tool (only supports C language)");
    if (!expectedOptions) {
        llvm::errs() << expectedOptions.takeError();
        return 1;
    }
    CommonOptionsParser &options = expectedOptions.get();

    opt_input_files = options.getSourcePathList();
    checkCommandLineArgs();

    // generate output files
    // FIXME: don't know if changing the filename will make "options.getCompilations()" useless
    std::vector<std::string> outputFiles;
    for (auto &inputFile : options.getSourcePathList()) {
        std::string outputFile = FileManager::getStemName(inputFile) + ".instru.c";
        llvm::sys::fs::copy_file(inputFile, outputFile);
        outputFiles.push_back(outputFile);
    }
    runTool(outputFiles, options.getCompilations());

    return 0;
}

void checkCommandLineArgs() {
    for (auto inputFile : opt_input_files) {
        if (!llvm::sys::fs::exists(inputFile)) {
            llvm::errs() << "The specified input file '" << inputFile << "' does not exist.\n";
            exit(1);
        }
    }
}

void runTool(const std::vector<std::string> &sourceFiles, const CompilationDatabase &compilations) {
    if (!opt_no_compilation) {
        Frontend::run(sourceFiles, compilations, newFrontendActionFactory<StmtInstruAction>().get());
        Frontend::run(sourceFiles, compilations, newFrontendActionFactory<FuncDeclInstruAction>().get());
        Frontend::run(sourceFiles, compilations, newFrontendActionFactory<CallExprInstruAction>().get());
    } else {
        for (auto f : sourceFiles) {
            llvm::outs() << "Instrument source file '" << f << "'.\n";
            Frontend::runWithoutCompilation(f, new Instrumentation<StmtVisitor>());
            Frontend::runWithoutCompilation(f, new Instrumentation<FunctionDeclVisitor>());
            Frontend::runWithoutCompilation(f, new Instrumentation<FunctionCallVisitor>());
        }
    }
}
