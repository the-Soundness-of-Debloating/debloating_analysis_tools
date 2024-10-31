#include "GlobalReduction.h"
#include "FileManager.h"
#include "Reduction.h"
#include "SourceManager.h"

#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"

#include <optional>
#include <vector>

void GlobalReduction::Initialize(clang::ASTContext &Ctx) {
    Reduction::Initialize(Ctx);
    CollectionVisitor = new GlobalElementCollectionVisitor(this);
}

void GlobalReduction::HandleTranslationUnit(clang::ASTContext &Ctx) {
    CollectionVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());

    // filter out the global declarations that do not contain any added back lines
    DDElementVector filteredDecls;
    for (auto const &element : globalDecls) {
        // Why will there be (-1, -1)?
        // Because some function declarations are using definitions in other files (such as "extern printf").
        if (element.first < 0 || element.second < 0) {
            llvm::errs() << "Error in HandleTranslationUnit: element is null\n";
            continue;
        }

        // llvm::outs() << element.first << "-" << element.second << ":  ";
        for (int line : addedBackLines) {
            if (line >= element.first && line <= element.second) {
                // llvm::outs() << "included\n";
                filteredDecls.emplace_back(element);
                break;
            }
        }
    }

    doDeltaDebugging(filteredDecls);
}

std::string GlobalReduction::applyFixAndOutputToFile(const DDElementVector &toAddBack, bool isTemp) {
    const clang::SourceManager &SM = Context->getSourceManager();

    std::set<DDElement> ranges;
    for (auto const &element : toAddBack) {
        // if (element.isNull())
        //     continue;
        if (element.first < 0 || element.second < 0) {
            // llvm::errs() << "Error in applyFixAndOutputToFile: (-1, -1)\n";
            continue;
        }
        ranges.insert(element);
    }

    // replace ranges of lines in the debloated (temp) file with lines in the original file
    std::string outputFile =
        isTemp ? FileManager::getStemName(opt_result_file) + ".temp.c" : outputFileName;
    llvm::sys::fs::copy_file(opt_result_file, outputFile);
    // CANNOT use `sed -i 10c\ "$(sed -n 10p original_file)" outputFile` (when 10th line is empty, it fails)
    // use python instead
    std::string rangesStr;
    for (auto const &range : ranges)
        rangesStr += std::to_string(range.first) + "-" + std::to_string(range.second) + ",";
    std::string programDir = FileManager::getParentDir(llvm::sys::fs::getMainExecutable(nullptr, nullptr));
    if (llvm::sys::ExecuteAndWait("/usr/bin/python3",
                                  {"/usr/bin/python3", programDir + "/apply-fix.py", "--remove", "--lines",
                                   rangesStr, outputFile, opt_debloated_file}) != 0) {
        llvm::errs() << "Failed to execute apply-fix.py (with args " << rangesStr << ")\n";
        exit(1);
    }

    return outputFile;
}

bool GlobalElementCollectionVisitor::VisitFunctionDecl(clang::FunctionDecl *FD) {
    if (SourceManager::IsInHeader(globalReduction->Context->getSourceManager(), FD))
        return true;

    DDElement range = globalReduction->getStartAndEnd(FD);
    if (range.first < 0 || range.second < 0)
        return true;

    globalReduction->globalDecls.emplace_back(range);

    return true;
}

bool GlobalElementCollectionVisitor::VisitVarDecl(clang::VarDecl *VD) {
    if (SourceManager::IsInHeader(globalReduction->Context->getSourceManager(), VD))
        return true;

    // only global variables
    if (VD->hasGlobalStorage()) {
        DDElement range = globalReduction->getStartAndEnd(VD);
        if (range.first < 0 || range.second < 0)
            return true;

        // can't rely on DCE, because there are some circular dependency cases (e.g.: in Blade-debloated date, argmatch_die)
        globalReduction->globalDecls.emplace_back(globalReduction->getStartAndEnd(VD));
    }

    return true;
}

bool GlobalElementCollectionVisitor::VisitTypeDecl(clang::TypeDecl *TD) {
    if (SourceManager::IsInHeader(globalReduction->Context->getSourceManager(), TD))
        return true;

    DDElement range = globalReduction->getStartAndEnd(TD);
    if (range.first < 0 || range.second < 0)
        return true;

    globalReduction->globalDecls.emplace_back(range);

    return true;
}
