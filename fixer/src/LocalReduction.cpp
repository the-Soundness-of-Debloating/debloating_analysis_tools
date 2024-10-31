#include "LocalReduction.h"

#include "clang/Lex/Lexer.h"
#include "llvm/Support/Program.h"

#include "FileManager.h"
#include "Reduction.h"
#include "SourceManager.h"

void LocalReduction::Initialize(clang::ASTContext &Ctx) {
    Reduction::Initialize(Ctx);
    CollectionVisitor = new LocalElementCollectionVisitor(this);
}

void LocalReduction::HandleTranslationUnit(clang::ASTContext &Ctx) {
    CollectionVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());

    for (auto const &FD : Functions) {
        auto _range = SourceManager::getStartAndEnd(Context, FD);
        auto range = getStartAndEnd(clang::SourceRange(_range.first, _range.second));
        bool shouldSkip = true;
        for (int line : debloatedLines) {
            if (line >= range.first && line <= range.second) {
                shouldSkip = false;
                break;
            }
        }
        if (shouldSkip) continue;

        llvm::outs() << "Reduce " << FD->getNameInfo().getAsString() << "\n";
        stmtQueue.push(FD->getBody());

        while (!stmtQueue.empty()) {
            clang::Stmt *S = stmtQueue.front();
            stmtQueue.pop();

            // if compound-like statement, get body and do delta debugging
            if (clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(S)) {
                std::vector<clang::Stmt *> stmts;
                for (auto S : CS->body()){
                    if (S == NULL) continue;
                    // DeclStmts are removed by DCE (which is more efficient than DD)
                    else if (clang::DeclStmt *DS = llvm::dyn_cast<clang::DeclStmt>(S)) continue;
                    else if (clang::NullStmt *NS = llvm::dyn_cast<clang::NullStmt>(S)) continue;
                    else stmts.push_back(S);
                }

                DDElementVector toRemove;
                for (auto S : stmts) {
                    auto _range = SourceManager::getStartAndEnd(Context, S);
                    auto range = getStartAndEnd(clang::SourceRange(_range.first, _range.second));
                    if (range.first >= 0 && range.second >= 0) {
                        for (int line : debloatedLines) {
                            if (line >= range.first && line <= range.second){
                                // llvm::outs() << "    Found: " << line << "\n";
                                toRemove.push_back(range);
                                break;
                            }
                        }
                    }
                }

                DDElementSet removed;
                if (toRemove.size()) {
                    // llvm::outs() << "  To remove: ";
                    // for (auto R : toRemove)
                    //     llvm::outs() << R.first << "-" << R.second << " ";
                    // llvm::outs() << "\n";
                    removed = toSet(doDeltaDebugging(toRemove));
                    cumulatedRemove.insert(cumulatedRemove.end(), removed.begin(), removed.end());
                }

                for (auto S : stmts) {
                    auto _range = SourceManager::getStartAndEnd(Context, S);
                    auto range = getStartAndEnd(clang::SourceRange(_range.first, _range.second));
                    if (removed.find(range) == removed.end())
                        stmtQueue.push(S);
                }
            } else if (clang::LabelStmt *LS = llvm::dyn_cast<clang::LabelStmt>(S)) {
                // label stmt is special in that we should allow removing the substmt while keeping the label
                clang::Stmt *substmt = LS->getSubStmt();
                if (substmt == NULL) continue;
                else if (clang::DeclStmt *DS = llvm::dyn_cast<clang::DeclStmt>(substmt)) continue;
                else if (clang::NullStmt *NS = llvm::dyn_cast<clang::NullStmt>(substmt)) continue;

                bool canRemove = false;
                auto _range = SourceManager::getStartAndEnd(Context, substmt);
                auto range = getStartAndEnd(clang::SourceRange(_range.first, _range.second));
                if (range.first >= 0 && range.second >= 0) {
                    for (int line : debloatedLines) {
                        if (line >= range.first && line <= range.second){
                            canRemove = true;
                            break;
                        }
                    }
                }
                DDElementSet removed;
                if (canRemove) {
                    removed = toSet(doDeltaDebugging({range}));
                    cumulatedRemove.insert(cumulatedRemove.end(), removed.begin(), removed.end());
                }

                if (removed.size() == 0) stmtQueue.push(substmt);
                continue;
            } else if (clang::IfStmt *IS = llvm::dyn_cast<clang::IfStmt>(S)) {
                stmtQueue.push(IS->getThen());
                if (IS->getElse()) stmtQueue.push(IS->getElse());
                continue;
            } else if (clang::WhileStmt *WS = llvm::dyn_cast<clang::WhileStmt>(S)) {
                stmtQueue.push(WS->getBody());
                continue;
            } else if (clang::DoStmt *DS = llvm::dyn_cast<clang::DoStmt>(S)) {
                stmtQueue.push(DS->getBody());
                continue;
            } else if (clang::ForStmt *FS = llvm::dyn_cast<clang::ForStmt>(S)) {
                stmtQueue.push(FS->getBody());
                continue;
            } else if (clang::SwitchStmt *SS = llvm::dyn_cast<clang::SwitchStmt>(S)) {
                stmtQueue.push(SS->getBody());
                continue;
            }
        }
    }
}

std::string LocalReduction::applyFixAndOutputToFile(const DDElementVector &toAddBack, bool isTemp) {
    const clang::SourceManager &SM = Context->getSourceManager();

    std::set<DDElement> ranges(cumulatedRemove.begin(), cumulatedRemove.end());
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
    if (llvm::sys::ExecuteAndWait("/usr/bin/python3", {"/usr/bin/python3", programDir + "/apply-fix.py", "--remove",
                                                   "--lines", rangesStr, outputFile, opt_debloated_file}) !=
        0) {
        llvm::errs() << "Failed to execute apply-fix.py (with args " << rangesStr << ")\n";
        exit(1);
    }

    return outputFile;
}

bool LocalElementCollectionVisitor::VisitFunctionDecl(clang::FunctionDecl *FD) {
    if (SourceManager::IsInHeader(localReduction->Context->getSourceManager(), FD))
        return true;
    if (FD->isThisDeclarationADefinition())
        localReduction->Functions.emplace_back(FD);
    return true;
}
