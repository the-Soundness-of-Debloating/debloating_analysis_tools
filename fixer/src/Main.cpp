#include <fstream>
#include <memory>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <vector>

#include "FileManager.h"
#include "Frontend.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"

#include "GlobalAddBack.h"
#include "GlobalReduction.h"
#include "LocalReduction.h"
#include "Reduction.h"
#include "DeadCodeElimination.h"

using namespace clang::tooling;

void reduceOneFile(CommonOptionsParser &options, std::set<int> &debloatedLines);

llvm::cl::OptionCategory fixerOptionsCategory("Fixer Options");
// llvm::cl::opt<bool> opt_no_compilation("no-compilation", llvm::cl::desc("Do not compile during crash
//     repair"), llvm::cl::cat(fixerOptionsCategory));
std::string opt_debloated_file;
llvm::cl::opt<std::string> opt_original_file("original-src",
                                             llvm::cl::desc("file path to the original source file"),
                                             llvm::cl::value_desc("FILEPATH"), llvm::cl::Required,
                                             llvm::cl::cat(fixerOptionsCategory));
llvm::cl::opt<std::string> opt_result_file("result-file",
                                           llvm::cl::desc("file path to the result source file (defualt: debloated-file-name.fixed.c)"),
                                           llvm::cl::value_desc("FILEPATH"),
                                           llvm::cl::cat(fixerOptionsCategory));
llvm::cl::opt<std::string> opt_compile_script("compile-script",
                                              llvm::cl::desc("file path to the compile script"),
                                              llvm::cl::value_desc("FILEPATH"), llvm::cl::Required,
                                              llvm::cl::cat(fixerOptionsCategory));
llvm::cl::opt<std::string> opt_reproduce_script("reproduce-script",
                                                llvm::cl::desc("file path to the reproduce script"),
                                                llvm::cl::value_desc("FILEPATH"), llvm::cl::Required,
                                                llvm::cl::cat(fixerOptionsCategory));
llvm::cl::opt<std::string>
    opt_debloated_lines_file("debloated-lines", llvm::cl::init("debloatedLines.txt"),
                             llvm::cl::desc("file path to the text file containing all debloated lines"),
                             llvm::cl::value_desc("FILEPATH"), llvm::cl::Required,
                             llvm::cl::cat(fixerOptionsCategory));
// optional
llvm::cl::opt<std::string>
    opt_other_test_script("other-test-script",
                          llvm::cl::desc("file path to the other test script (such as runDebInputs.sh)"),
                          llvm::cl::value_desc("FILEPATH"), llvm::cl::cat(fixerOptionsCategory));
llvm::cl::opt<bool> opt_no_reduction("skip-reduction", llvm::cl::desc("Do not perform reduction after add-back"),
                                     llvm::cl::cat(fixerOptionsCategory));
llvm::cl::opt<bool> opt_no_redir("no-redir",
                                 llvm::cl::desc("Do not redirect output to null to please isatty() function"),
                                 llvm::cl::cat(fixerOptionsCategory));
llvm::cl::opt<bool> opt_add_back_all("add-back-all",
                                     llvm::cl::desc("Add back all functions and global variables and types (for debugging)"),
                                     llvm::cl::cat(fixerOptionsCategory));

int main(int argc, const char **argv) {
    llvm::cl::SetVersionPrinter([](llvm::raw_ostream &OS) { OS << "Fixer version 0.1\n"; });
    // FIXME: make sure each line can contain at most one statement
    auto expectedOptions = CommonOptionsParser::create(
        argc, argv, fixerOptionsCategory, llvm::cl::OneOrMore,
        "An baseline crash repair tool for debloated program (only supports C language)");
    if (!expectedOptions) {
        llvm::errs() << expectedOptions.takeError();
        return 1;
    }
    CommonOptionsParser &options = expectedOptions.get();
    // FIXME: currently only supports one input file
    opt_debloated_file = options.getSourcePathList()[0];

    // input from file
    // file contains single line with numbers separated by space
    std::set<int> debloatedLines;
    std::ifstream debloatedLinesFile(opt_debloated_lines_file);
    for (int line; debloatedLinesFile >> line;)
        debloatedLines.insert(line);
    debloatedLinesFile.close();

    reduceOneFile(options, debloatedLines);

    return 0;
}

// dirty flag and dirty global variable...
bool reduction_dirty_flag = true;
// dependencies are functions that are not in the debloated program
std::set<int> addedBackLines, addedBackLinesWithoutDependencies, addedBackDependencies;
// used to store diagnostic messages (for deadcode elimination)
clang::TextDiagnosticBuffer diagnosticConsumer;
std::string tempFile;
void reduceOneFile(CommonOptionsParser &options, std::set<int> &debloatedLines) {
    // only do one round of global add-back
    llvm::outs() << "Add-Back\n";
    if (opt_result_file.empty())
        opt_result_file = FileManager::getStemName(opt_debloated_file) + ".fixed.c";
    llvm::sys::fs::copy_file(opt_debloated_file, opt_result_file);
    // the REDUCE is based on the original file
    Frontend::runWithoutCompilation(opt_original_file, new GlobalAddBack(debloatedLines, opt_result_file));

    if (opt_no_reduction)
        return;

    // after doing global add-back, do reduction to remove redundant statements
    llvm::outs() << "Reduction\n";
    tempFile = FileManager::getStemName(opt_debloated_file) + ".reduction.temp.c";
    llvm::outs() << "Iteration 0\n";
    llvm::outs() << "Local Reduction (limited range)\n";
    Frontend::runWithoutCompilation(opt_result_file, new LocalReduction(addedBackLinesWithoutDependencies, tempFile));
    llvm::sys::fs::copy_file(tempFile, opt_result_file);
    for (int i = 1; reduction_dirty_flag; i++) {
        llvm::outs() << "Iteration " << i << "\n";
        reduction_dirty_flag = false;
        llvm::outs() << "Global Reduction\n";
        Frontend::runWithoutCompilation(opt_result_file, new GlobalReduction(addedBackLines, tempFile));
        llvm::sys::fs::copy_file(tempFile, opt_result_file);
        llvm::outs() << "Local Reduction\n";
        Frontend::runWithoutCompilation(opt_result_file, new LocalReduction(addedBackLines, tempFile));
        llvm::sys::fs::copy_file(tempFile, opt_result_file);
        llvm::outs() << "Dead Code Elimination\n";
        FrontendDCE::run({opt_result_file}, options.getCompilations(), newFrontendActionFactory<DCEAction<tempFile>>().get());
        llvm::sys::fs::copy_file(tempFile, opt_result_file);
        // FrontendDCE::runDCE(opt_result_file, new ClangDeadcodeElimination(debloatedLines, tempFile));
        // FrontendDCE::run({opt_result_file}, options.getCompilations(), newFrontendActionFactory<DCEAction<tempFile>>().get());
    }
    llvm::sys::fs::remove(tempFile);

    llvm::outs() << "\n";
    llvm::outs() << "Finished repairing " << opt_debloated_file << "\n";
    llvm::outs() << "Added back " << addedBackLines.size() << " lines\n";
}
