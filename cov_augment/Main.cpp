#include <stdlib.h>
#include <time.h>

#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "CovAugment.h"
#include "FileManager.h"
#include "Frontend.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang::tooling;

llvm::cl::OptionCategory covAugmentOptionsCategory("CovAugment Options");
std::string opt_original_file;
llvm::cl::opt<std::string> opt_debloated_file("debloated-src",
                                              llvm::cl::desc("file path to the debloated source file"),
                                              llvm::cl::value_desc("FILEPATH"), llvm::cl::Required,
                                              llvm::cl::cat(covAugmentOptionsCategory));
llvm::cl::opt<std::string> opt_debloated_lines_file(
    "debloated-lines", llvm::cl::init("debloatedLines.txt"),
    llvm::cl::desc("file path to the text file containing all debloated lines"),
    llvm::cl::value_desc("FILEPATH"), llvm::cl::Required, llvm::cl::cat(covAugmentOptionsCategory));
// llvm::cl::opt<std::string> opt_result_file(
//     "result-file",
//     llvm::cl::desc("file path to the result source file (default: debloated-file-name.fixed.c)"),
//     llvm::cl::value_desc("FILEPATH"), llvm::cl::cat(covAugmentOptionsCategory));

llvm::cl::opt<std::string> opt_aug_strat(
    "aug-strat",
    llvm::cl::desc("augmentation strategies, separated by comma (default: \"exit,sym_assign,keyword\")"),
    llvm::cl::value_desc("LEVEL"), llvm::cl::cat(covAugmentOptionsCategory));
std::set<std::string> opt_augmentation_strategies;

int main(int argc, const char **argv) {
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream &OS) { OS << "CovAugment version 0.1\n"; });
    auto expectedOptions =
        CommonOptionsParser::create(argc, argv, covAugmentOptionsCategory, llvm::cl::OneOrMore,
                                    "CovAugment: a tool to augment Cov-debloated program\n");
    if (!expectedOptions) {
        llvm::errs() << expectedOptions.takeError();
        return 1;
    }
    CommonOptionsParser &options = expectedOptions.get();
    opt_original_file = options.getSourcePathList()[0];

    if (opt_aug_strat.empty()) {
        opt_augmentation_strategies.insert("exit");
        opt_augmentation_strategies.insert("sym_assign");
        opt_augmentation_strategies.insert("keyword");
    } else {
        std::string strategy;
        for (char c : opt_aug_strat) {
            if (c == ',') {
                opt_augmentation_strategies.insert(strategy);
                strategy.clear();
            } else {
                strategy.push_back(c);
            }
        }
        opt_augmentation_strategies.insert(strategy);
    }

    // input from file
    // file contains single line with numbers separated by space
    std::set<int> debloatedLines;
    std::ifstream debloatedLinesFile(opt_debloated_lines_file);
    for (int line; debloatedLinesFile >> line;) debloatedLines.insert(line);
    debloatedLinesFile.close();

    // Frontend::runWithoutCompilation(opt_result_file, new GlobalReduction(addedBackLines, tempFile));
    // FrontendDCE::run({opt_result_file}, options.getCompilations(),
    //     newFrontendActionFactory<DCEAction<tempFile>>().get());

    std::string outputFile = FileManager::getStemName(opt_debloated_file) + ".augmented.c";
    llvm::outs() << "Output augmented program to file '" << outputFile << "'.\n";
    Frontend::runWithoutCompilation(opt_original_file, new CovAugment(debloatedLines, outputFile));

    return 0;
}
