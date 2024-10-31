#include "GlobalAddBack.h"
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

static int currentVisitingDeclStartLine = -1;

void GlobalAddBack::Initialize(clang::ASTContext &Ctx) {
    Reduction::Initialize(Ctx);
    CollectionVisitor = new GlobalAddBackElementCollectionVisitor(this);
}

void GlobalAddBack::HandleTranslationUnit(clang::ASTContext &Ctx) {
    CollectionVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());

    // filter FunctionDecls to only include those that contains debloated lines AND ARE IN DEBLOATED FILE
    DDElementVector filteredGlobalDecls;
    for (auto const &element : globalDecls) {
        // Why will there be (-1, -1)?
        // Because some function declarations are using definitions in other files (such as "extern printf").
        if (element.first < 0 || element.second < 0) {
            llvm::errs() << "Error in HandleTranslationUnit: element is null\n";
            continue;
        }

        // only add back functions that are also in the debloated program
        if (debloatedLines.find(element.first) != debloatedLines.end())
            continue;

        for (int line : debloatedLines) {
            if (line >= element.first && line <= element.second) {
                filteredGlobalDecls.emplace_back(element);
                break;
            }
        }
    }

    // As for TypeDecl: it is too complicated to analyze dependencies of types, so we just include them all in DD
    for (auto const &element : typeDecls) {
        if (element.first < 0 || element.second < 0) {
            llvm::errs() << "Error in HandleTranslationUnit: element is null\n";
            continue;
        }

        for (int line : debloatedLines) {
            if (line >= element.first && line <= element.second) {
                filteredGlobalDecls.emplace_back(element);
                break;
            }
        }
    }

    doDeltaDebugging(filteredGlobalDecls);
}

extern bool reduction_dirty_flag;
DDElementVector GlobalAddBack::doDeltaDebugging(const DDElementVector &lineGroups) {
    std::set<DDElementVector> visited;
    DDElementVector lineGroupsToAddBack = lineGroups;

    // get the "fallback" result of this round of delta debugging (meaning all lines are added back)
    applyFixAndOutputToFile(lineGroupsToAddBack, false);

    if (opt_add_back_all)
        exit(0);

    int chunkSize = (lineGroupsToAddBack.size() + 1) / 2;
    llvm::outs() << "Running delta debugging - Size: " << lineGroupsToAddBack.size() << "\n";

    while (lineGroupsToAddBack.size() > 0) {
        bool success = false;
        auto candidates = getCandidates(lineGroupsToAddBack, chunkSize);
        for (auto candidate : candidates) {
            if (std::find(visited.begin(), visited.end(), candidate) != visited.end()) {
                // llvm::outs() << "Cache hit.\n";
                continue;
            }
            visited.insert(candidate);

            if (test(candidate)) {
                lineGroupsToAddBack = toVector(toSet(candidate));
                success = true;
                break;
            }
        }
        if (success) {
            reduction_dirty_flag = true;
            llvm::outs() << "                Success - Size: " << lineGroupsToAddBack.size() << "\n";
            // llvm::outs() << "Added Back: ";
            // for (auto R : lineGroupsToAddBack)
            //     llvm::outs() << R.first << "-" << R.second << " ";
            // llvm::outs() << "\n";
            chunkSize = (lineGroupsToAddBack.size() + 1) / 2;
            // persist the intermediate result (in case of interruption)
            applyFixAndOutputToFile(lineGroupsToAddBack, false);
        } else {
            if (chunkSize == 1)
                break;
            chunkSize = (chunkSize + 1) / 2;
        }
    }

    // get the "final" result of this round of delta debugging
    // must be before reducing debloatedLines
    applyFixAndOutputToFile(lineGroupsToAddBack, false);

    // initialize addedBackLines
    //   (cannot just use debloatedLines, or lines that are originally in debloated source will also be
    //   included in local reduction)
    std::set<DDElement> ranges, dependenciesRanges;
    for (auto const &element : lineGroupsToAddBack) {
        // if (element.isNull())
        //     continue;
        if (element.first < 0 || element.second < 0) {
            // llvm::errs() << "Error in applyFixAndOutputToFile: (-1, -1)\n";
            continue;
        }
        ranges.insert(element);
    }
    // find all dependencies (RECURSIVELY)
    for (auto const &element : ranges) {
        for (auto const &dep : mapLineToDependencies[element.first]) {
            if (dep.first > 0 && dep.second > 0) {
                if (debloatedLines.find(dep.first) != debloatedLines.end()) {
                    if (dependenciesRanges.find(dep) == dependenciesRanges.end()) {
                        dependenciesRanges.insert(dep);
                    }
                }
            }
        }
    }
    for (bool dirty_flag = true; dirty_flag;) {
        dirty_flag = false;
        for (auto const &element : dependenciesRanges) {
            for (auto const &dep : mapLineToDependencies[element.first]) {
                if (dep.first > 0 && dep.second > 0) {
                    if (debloatedLines.find(dep.first) != debloatedLines.end()) {
                        if (dependenciesRanges.find(dep) == dependenciesRanges.end()) {
                            dirty_flag = true;
                            dependenciesRanges.insert(dep);
                        }
                    }
                }
            }
        }
    }
    for (auto const &element : ranges) {
        for (int line = element.first; line <= element.second; line++) {
            if (debloatedLines.find(line) != debloatedLines.end()) {
                addedBackLines.insert(line);
                addedBackLinesWithoutDependencies.insert(line);
            }
        }
    }
    for (auto const &element : dependenciesRanges) {
        for (int line = element.first; line <= element.second; line++) {
            if (debloatedLines.find(line) != debloatedLines.end()) {
                addedBackLines.insert(line);
                addedBackDependencies.insert(line);
            }
        }
    }

    return lineGroupsToAddBack;
}

std::string GlobalAddBack::applyFixAndOutputToFile(const DDElementVector &toAddBack, bool isTemp) {
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
    // also add back all dependencies (recursively)
    for (bool dirty_flag = true; dirty_flag;) {
        dirty_flag = false;
        for (auto const &element : ranges) {
            for (auto const &dep : mapLineToDependencies[element.first]) {
                if (dep.first > 0 && dep.second > 0) {
                    if (debloatedLines.find(dep.first) != debloatedLines.end()) {
                        if (ranges.find(dep) == ranges.end()) {
                            dirty_flag = true;
                            ranges.insert(dep);
                        }
                    }
                }
            }
        }
    }

    // replace ranges of lines in the debloated (temp) file with lines in the original file
    std::string outputFile =
        isTemp ? FileManager::getStemName(opt_result_file) + ".temp.c" : outputFileName;
    llvm::sys::fs::copy_file(opt_debloated_file, outputFile);
    // CANNOT use `sed -i 10c\ "$(sed -n 10p original_file)" outputFile` (when 10th line is empty, it fails)
    // use python instead
    std::string rangesStr;
    for (auto const &range : ranges)
        rangesStr += std::to_string(range.first) + "-" + std::to_string(range.second) + ",";
    std::string programDir = FileManager::getParentDir(llvm::sys::fs::getMainExecutable(nullptr, nullptr));
    if (llvm::sys::ExecuteAndWait("/usr/bin/python3",
                                  {"/usr/bin/python3", programDir + "/apply-fix.py", "--lines", rangesStr,
                                   outputFile, opt_original_file}) != 0) {
        llvm::errs() << "Failed to execute apply-fix.py (with args " << rangesStr << ")\n";
        exit(1);
    }

    return outputFile;
}

void GlobalAddBack::addDependencies(clang::Decl *decl) {
    auto range = getStartAndEnd(decl);
    if (range.first > 0 && range.second > 0)
        mapLineToDependencies[currentVisitingDeclStartLine].insert(range);
}

bool GlobalAddBackElementCollectionVisitor::VisitFunctionDecl(clang::FunctionDecl *FD) {
    if (SourceManager::IsInHeader(globalAddBack->Context->getSourceManager(), FD))
        return true;

    // TODO: currently only considers FD's definition because Chisel only removes function definitions
    DDElement range = globalAddBack->getStartAndEnd(FD->getDefinition());
    if (range.first < 0 || range.second < 0)
        return true;
    // globalAddBack->mapLineToRange[range.first] = range;
    currentVisitingDeclStartLine = range.first;

    globalAddBack->globalDecls.emplace_back(range);

    return true;
}

bool GlobalAddBackElementCollectionVisitor::VisitVarDecl(clang::VarDecl *VD) {
    if (SourceManager::IsInHeader(globalAddBack->Context->getSourceManager(), VD))
        return true;

    // only global variables
    if (VD->hasGlobalStorage()) {
        DDElement range = globalAddBack->getStartAndEnd(VD);
        if (range.first < 0 || range.second < 0)
            return true;
        // globalAddBack->mapLineToRange[range.first] = range;
        currentVisitingDeclStartLine = range.first;

        // only do delta-debugging on functions (not variable declarations)
        // globalAddBack->globalDecls.emplace_back(globalAddBack->getStartAndEnd(VD));
    }

    return true;
}

bool GlobalAddBackElementCollectionVisitor::VisitTypeDecl(clang::TypeDecl *TD) {
    if (SourceManager::IsInHeader(globalAddBack->Context->getSourceManager(), TD))
        return true;

    DDElement range = globalAddBack->getStartAndEnd(TD);
    if (range.first < 0 || range.second < 0)
        return true;
    // globalAddBack->mapLineToRange[range.first] = range;
    currentVisitingDeclStartLine = range.first;

    globalAddBack->typeDecls.emplace_back(range);

    return true;
}

bool GlobalAddBackElementCollectionVisitor::VisitDeclRefExpr(clang::DeclRefExpr *DRE) {
    auto D = DRE->getDecl();
    if (D == nullptr)
        return true;

    if (clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(D)) {
        if (VD->hasGlobalStorage()) {
            // add back the variable declaration and definition
            //   (1) `extern int x` & `x = 1`
            //   (2) `int x` & `x = 1`
            globalAddBack->addDependencies(VD);
            globalAddBack->addDependencies(VD->getCanonicalDecl());
            globalAddBack->addDependencies(VD->getInitializingDeclaration()); // TODO: needed?
            globalAddBack->addDependencies(VD->getDefinition());              // TODO: needed?
        }
    } else if (clang::FunctionDecl *FD = llvm::dyn_cast<clang::FunctionDecl>(D)) {
        // add back the function declaration and definition
        globalAddBack->addDependencies(FD);
        globalAddBack->addDependencies(FD->getDefinition());
        globalAddBack->addDependencies(FD->getCanonicalDecl()); // TODO: needed?
    } else {
        globalAddBack->addDependencies(D);
        globalAddBack->addDependencies(D->getCanonicalDecl()); // TODO: needed?
    }

    return true;
}
