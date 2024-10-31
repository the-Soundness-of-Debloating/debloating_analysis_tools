#include "CovAugment.h"

#include <optional>
#include <vector>

#include "FileManager.h"
#include "SourceManager.h"
#include "clang/AST/ParentMapContext.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"

static int currentVisitingDeclStartLine = -1;

const int n_keywords = 7;
std::string keywords[n_keywords] = {"abort", "exit", "free", "error", "die", "usage", "close"};

void CovAugment::Initialize(clang::ASTContext &Ctx) {
    Context = &Ctx;
    collectionVisitor = new CovAugmentAstVisitor(this);
}

void CovAugment::HandleTranslationUnit(clang::ASTContext &Ctx) {
    collectionVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());

    std::vector<LineRange> to_replace_with_exit;
    std::vector<LineRange> to_add_back;

    // filter out if statements that are in empty functions
    for (auto const &emptyFunction : emptyFunctions) {
        // llvm::outs() << "Empty function: " << emptyFunction.first << "-" << emptyFunction.second << "\n";
        ifStmts.erase(std::remove_if(ifStmts.begin(), ifStmts.end(),
                                     [emptyFunction, this](const IfStmt &if_stmt) {
                                         auto range = getStartAndEnd(if_stmt.if_stmt);
                                         return range.first >= emptyFunction.first &&
                                                range.second <= emptyFunction.second;
                                     }),
                      ifStmts.end());
    }

    // add exit to empty functions with __noreturn__ attribute
    // TODO: change exit message
    for (auto const &func : emptyNoReturnFunctions) {
        if (func.second - func.first >= 2) {
            to_replace_with_exit.push_back(std::make_pair(func.second - 1, func.second - 1));
        } else {
            to_replace_with_exit.push_back(std::make_pair(func.first, func.first));
        }
    }

    // first pass: add back
    for (auto const &if_stmt : ifStmts) {
        // if if-condition got removed, skip
        auto range = getStartAndEnd(if_stmt.if_stmt->getCond());
        if (range.first < 0 || range.second < 0) continue;
        if (lineIsRemoved(range.first)) continue;

        if (if_stmt.is_symmetrical) {
            if (opt_augmentation_strategies.find("sym_assign") != opt_augmentation_strategies.end()) {
                // add back all deleted branches (if at least one branch is not deleted)
                bool all_deleted = true;
                for (auto const &branch : if_stmt.branches) {
                    auto range = getStartAndEnd(branch);
                    if (range.first < 0 || range.second < 0) continue;
                    // only consider compound statements
                    if (clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(branch)) {
                        // only check the first statement
                        if (CS->size() > 0) {
                            clang::Stmt *first_stmt = CS->body_front();
                            if (first_stmt && !llvm::isa<clang::NullStmt>(first_stmt)) {
                                auto range = getStartAndEnd(first_stmt);
                                if (range.first < 0 || range.second < 0) continue;
                                if (!lineIsRemoved(range.first)) {
                                    all_deleted = false;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!all_deleted) {
                    for (auto const &branch : if_stmt.branches) {
                        auto range = getStartAndEnd(branch);
                        if (range.first < 0 || range.second < 0) continue;
                        to_add_back.push_back(range);
                    }
                }
            }
        } else if (if_stmt.is_keyword) {
            if (opt_augmentation_strategies.find("keyword") != opt_augmentation_strategies.end()) {
                // add back all deleted branches
                // FIXME: need to add back if condition (for multi-line conditions)
                auto range = getStartAndEnd(if_stmt.if_stmt);
                if (range.first < 0 || range.second < 0) continue;
                to_add_back.push_back(range);
                // for (auto const &branch : if_stmt.branches) {
                //     auto range = getStartAndEnd(branch);
                //     if (range.first < 0 || range.second < 0) continue;
                //     to_add_back.push_back(range);
                // }
            }
        }
    }

    // second pass: insert exit
    if (opt_augmentation_strategies.find("exit") != opt_augmentation_strategies.end()) {
        for (auto const &if_stmt : ifStmts) {
            // if if-condition got removed, skip
            auto range = getStartAndEnd(if_stmt.if_stmt->getCond());
            if (range.first < 0 || range.second < 0) continue;
            if (lineIsRemoved(range.first)) continue;

            if (if_stmt.is_symmetrical && opt_augmentation_strategies.find("sym_assign") != opt_augmentation_strategies.end())
                continue;
            if (if_stmt.is_keyword && opt_augmentation_strategies.find("keyword") != opt_augmentation_strategies.end())
                continue;
            // replace deleted branches with exit
            for (auto const &branch : if_stmt.branches) {
                // only consider compound statements
                if (clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(branch)) {
                    // only check the first statement
                    clang::Stmt *last_stmt = nullptr;
                    if (CS->size() > 0) {
                        clang::Stmt *first_stmt = CS->body_front();
                        if (first_stmt && !llvm::isa<clang::NullStmt>(first_stmt)) {
                            auto range = getStartAndEnd(first_stmt);
                            if (range.first < 0 || range.second < 0) continue;
                            if (lineIsRemoved(range.first)) {
                                last_stmt = CS->body_back();
                            } else if (llvm::isa<clang::CompoundStmt>(first_stmt)) {
                                // TODO for nested compound statements (if () { {} })
                                clang::Stmt *_first_stmt = llvm::dyn_cast<clang::CompoundStmt>(first_stmt)->body_front();
                                if (_first_stmt && !llvm::isa<clang::NullStmt>(_first_stmt)) {
                                    auto range = getStartAndEnd(_first_stmt);
                                    if (range.first < 0 || range.second < 0) continue;
                                    if (lineIsRemoved(range.first)) {
                                        last_stmt = CS->body_back();
                                    }
                                }
                            }
                        }
                    }
                    if (last_stmt) {
                        auto range = getStartAndEnd(last_stmt);
                        if (range.first < 0 || range.second < 0) continue;
                        // for multi-line statements, we need to insert exit at the end of the last line
                        to_replace_with_exit.push_back(std::make_pair(range.second, range.second));
                    }
                }
            }
            if (if_stmt.branch_after_if) {
                auto range = getStartAndEnd(if_stmt.branch_after_if);
                if (range.first < 0 || range.second < 0) continue;
                to_replace_with_exit.push_back(std::make_pair(range.first, range.first));
            }
        }
    }

    // also add back dependencies (FIXME: currently only declarations but no assignments)
    for (bool dirty_flag = true; dirty_flag;) {
        dirty_flag = false;
        for (int i = 0; i < to_add_back.size(); i++) {
            auto range = to_add_back[i];
            for (int line = range.first; line <= range.second; line++) {
                for (auto const &dep : mapLineToDependencies[line]) {
                    if (std::find(to_add_back.begin(), to_add_back.end(), dep) == to_add_back.end()) {
                        dirty_flag = true;
                        to_add_back.push_back(dep);
                    }
                }
            }
        }
    }

    llvm::sys::fs::copy_file(opt_debloated_file, outputFileName);
    applyAugmentationToFile(to_add_back, false);
    applyAugmentationToFile(to_replace_with_exit, true);
}

void CovAugment::applyAugmentationToFile(const std::vector<LineRange> &ranges, bool is_exit_replacement) {
    // CANNOT use `sed -i 10c\ "$(sed -n 10p original_file)" outputFileName` (when 10th line is empty, it
    // fails) use python instead
    std::string rangesStr;
    for (auto const &range : ranges)
        rangesStr += std::to_string(range.first) + "-" + std::to_string(range.second) + ",";
    std::string programDir = FileManager::getParentDir(llvm::sys::fs::getMainExecutable(nullptr, nullptr));
    int ret;
    if (is_exit_replacement) {
        // llvm::outs() << "Apply exit replacement: " << rangesStr << "\n";
        ret = llvm::sys::ExecuteAndWait("/usr/bin/python3",
                                        {"/usr/bin/python3", programDir + "/apply-augmentation.py", "--exit",
                                         "--lines", rangesStr, outputFileName, opt_original_file});
    } else {
        // llvm::outs() << "Apply add-back: " << rangesStr << "\n";
        ret = llvm::sys::ExecuteAndWait("/usr/bin/python3",
                                        {"/usr/bin/python3", programDir + "/apply-augmentation.py", "--lines",
                                         rangesStr, outputFileName, opt_original_file});
    }
    if (ret != 0) {
        llvm::errs() << "Failed to execute apply-augmentation.py (with args " << rangesStr << ")\n";
        exit(1);
    }
}

void CovAugment::addDependency(clang::Stmt *stmt) {
    if (stmt == nullptr) {
        return;
    } else if (llvm::isa<clang::DeclRefExpr>(stmt)) {
        clang::DeclRefExpr *DRE = llvm::dyn_cast<clang::DeclRefExpr>(stmt);
        clang::Decl *D = DRE->getDecl();
        if (D == nullptr) return;
        if (clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(D)) {
            addDependency(DRE, VD);
            addDependency(DRE, VD->getCanonicalDecl());
            addDependency(DRE, VD->getInitializingDeclaration());
            addDependency(DRE, VD->getDefinition());
        } else if (clang::FunctionDecl *FD = llvm::dyn_cast<clang::FunctionDecl>(D)) {
            // check if the first line of the function body is deleted
            clang::FunctionDecl *def = FD->getDefinition();
            if (def) {
                clang::Stmt *body = def->getBody();
                if (body && llvm::isa<clang::CompoundStmt>(body)) {
                    clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(body);
                    clang::Stmt *first_stmt = CS->body_front();
                    if (first_stmt && !llvm::isa<clang::NullStmt>(first_stmt)) {
                        auto range = getStartAndEnd(first_stmt);
                        auto range_def = getStartAndEnd(def);
                        if (range.first > 0 && range.second > 0) {
                            if (lineIsRemoved(range.first)) {
                                addDependency(DRE, getStartAndEnd(FD));
                                addDependency(DRE, getStartAndEnd(FD->getDefinition()));
                                addDependency(DRE, getStartAndEnd(FD->getCanonicalDecl()));
                            }
                        }
                    }
                }
            }
        } else {
            addDependency(DRE, D);
            addDependency(DRE, D->getCanonicalDecl());
        }
    } else if (llvm::isa<clang::GotoStmt>(stmt)) {
        clang::GotoStmt *GS = llvm::dyn_cast<clang::GotoStmt>(stmt);
        clang::LabelStmt *LS = GS->getLabel()->getStmt();
        if (LS) {
            clang::Stmt *sub_stmt = LS->getSubStmt();
            if (sub_stmt && !llvm::isa<clang::NullStmt>(sub_stmt)) {
                auto range = getStartAndEnd(sub_stmt);
                if (range.first > 0 && range.second > 0) {
                    if (lineIsRemoved(range.first)) {
                        addDependency(GS, getRangeOfGotoLabel(LS));
                    }
                }
            }
        }
    }
}
void CovAugment::addDependency(clang::Stmt *stmt, clang::Decl *decl) {
    auto range_decl = getStartAndEnd(decl);
    if (range_decl.first > 0 && range_decl.second > 0) {
        if (lineIsRemoved(range_decl.first)) {
            addDependency(stmt, range_decl);
        }
    }
}
void CovAugment::addDependency(clang::Stmt *stmt, LineRange range_decl) {
    auto range_stmt = getStartAndEnd(stmt);
    if (range_stmt.first > 0 && range_stmt.second > 0 && range_decl.first > 0 && range_decl.second > 0) {
        mapLineToDependencies[range_stmt.first].insert(range_decl);
    }
}

LineRange CovAugment::getStartAndEnd(clang::SourceRange range) {
    const clang::SourceManager &SM = Context->getSourceManager();

    clang::SourceLocation Start = range.getBegin(), End = range.getEnd();

    if (Start.isMacroID()) Start = SM.getFileLoc(Start);
    if (End.isMacroID()) End = SM.getFileLoc(End);

    if (End.isInvalid() || Start.isInvalid()) return std::make_pair(-1, -1);

    // a pair of start and end line numbers
    return std::make_pair(SM.getSpellingLineNumber(Start), SM.getSpellingLineNumber(End));
}
LineRange CovAugment::getStartAndEnd(clang::Decl *decl) {
    if (decl == nullptr) {
        // Why will there be some decls that are null?
        // Because some function declarations are using definitions in other files (such as "extern printf").
        return std::make_pair(-1, -1);
    }

    return getStartAndEnd(decl->getSourceRange());
}
LineRange CovAugment::getStartAndEnd(clang::Stmt *stmt) {
    if (stmt == nullptr) {
        return std::make_pair(-1, -1);
    }

    return getStartAndEnd(stmt->getSourceRange());
}
LineRange CovAugment::getRangeOfGotoLabel(clang::LabelStmt *LS) {
    if (LS == nullptr) return std::make_pair(-1, -1);

    const clang::SourceManager &SM = Context->getSourceManager();

    auto parents = Context->getParents(*LS);
    if (parents.size() == 0) return std::make_pair(-1, -1);
    auto parent = parents[0].get<clang::Stmt>();

    // find until next label statement or end of parent compound statement
    LineRange ret = std::make_pair(-1, -1);
    bool started = false;
    for (auto const &stmt : parent->children()) {
        if (stmt == LS) {
            auto range = getStartAndEnd(const_cast<clang::Stmt *>(stmt));
            if (range.first <= 0 || range.second <= 0) return std::make_pair(-1, -1);
            ret.first = range.first;
            started = true;
        } else if (started) {
            auto range = getStartAndEnd(const_cast<clang::Stmt *>(stmt));
            if (range.first <= 0 || range.second <= 0) return std::make_pair(-1, -1);
            if (llvm::isa<clang::LabelStmt>(stmt)) {
                // found ending statement (next label statement in the same level)
                break;
            } else {
                ret.second = range.second;
            }
        }
    }
    if (ret.first > 0 && ret.second > 0)
        return ret;
    else
        return std::make_pair(-1, -1);
}

bool CovAugment::lineIsRemoved(int line) {
    return debloatedLines.find(line) != debloatedLines.end();
}
bool CovAugment::functionIsRemoved(clang::FunctionDecl *FD) {
    if (!FD) return false;
    if (FD->isThisDeclarationADefinition()) {
        clang::Stmt *body = FD->getBody();
        // check if body is empty
        if (body && llvm::isa<clang::CompoundStmt>(body)) {
            clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(body);
            clang::Stmt *first_stmt = CS->body_front();
            if (first_stmt && !llvm::isa<clang::NullStmt>(first_stmt)) {
                auto range = getStartAndEnd(first_stmt);
                if (range.first > 0 && range.second > 0) {
                    if (lineIsRemoved(range.first)) {
                        return true;
                    }
                }
            }
            return false;
        }
    } else {
        auto range = getStartAndEnd(FD);
        if (range.first < 0 || range.second < 0) return false;
        return lineIsRemoved(range.first);
    }
    return false;
}
bool CovAugment::ifBranchContainsKeyword(clang::Stmt *if_branch, std::string *keywords, int n_keywords) {
    if (!if_branch || llvm::isa<clang::NullStmt>(if_branch)) return false;

    // iterate over all first-level statements
    // FIXME: only consider compound statements
    if (clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(if_branch)) {
        for (auto const &stmt : CS->body()) {
            if (!stmt || llvm::isa<clang::NullStmt>(stmt)) continue;
            // ignore compound-like statements (except normal compound statements)
            if (llvm::isa<clang::IfStmt>(stmt))
                continue;
            else if (llvm::isa<clang::SwitchStmt>(stmt))
                continue;
            else if (llvm::isa<clang::WhileStmt>(stmt))
                continue;
            else if (llvm::isa<clang::ForStmt>(stmt))
                continue;
            else if (llvm::isa<clang::DoStmt>(stmt))
                continue;
            // break on labelstmt (FIXME:) (in chisel-bench programs, labelstmts are used for switch-cases)
            if (llvm::isa<clang::LabelStmt>(stmt)) break;
            // check for keywords
            if (llvm::isa<clang::CompoundStmt>(stmt)) {
                if (ifBranchContainsKeyword(stmt, keywords, n_keywords)) {
                    return true;
                }
            } else {
                if (rangeContainsKeyword(stmt->getSourceRange(), keywords, n_keywords)) {
                    return true;
                }
            }
        }
    }

    return false;
}
bool CovAugment::rangeContainsKeyword(clang::SourceRange range, std::string *keywords, int n_keywords) {
    return rangeContainsKeyword(getStartAndEnd(range), keywords, n_keywords);
}
bool CovAugment::rangeContainsKeyword(LineRange range, std::string *keywords, int n_keywords) {
    const clang::SourceManager &SM = Context->getSourceManager();

    if (range.first < 0 || range.second < 0) return false;

    for (int line = range.first; line <= range.second; line++) {
        if (!lineIsRemoved(line)) continue;
        std::string src =
            SourceManager::GetSourceText(
                SM, SM.translateLineCol(SM.getFileID(SM.getLocForStartOfFile(SM.getMainFileID())), line, 1),
                SM.translateLineCol(SM.getFileID(SM.getLocForStartOfFile(SM.getMainFileID())), line, 1000))
                .str();
        for (int i = 0; i < n_keywords; i++) {
            if (src.find(keywords[i]) != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}

bool CovAugmentAstVisitor::VisitDeclRefExpr(clang::DeclRefExpr *DRE) {
    covAugment->addDependency(DRE);
    return true;
}
bool CovAugmentAstVisitor::VisitGotoStmt(clang::GotoStmt *GS) {
    covAugment->addDependency(GS);
    return true;
}
bool CovAugmentAstVisitor::VisitFunctionDecl(clang::FunctionDecl *FD) {
    if (FD->isThisDeclarationADefinition()) {
        if (covAugment->functionIsRemoved(FD)) {
            auto range = covAugment->getStartAndEnd(FD);
            if (range.first > 0 && range.second > 0) {
                clang::FunctionType const *FT = FD->getType()->getAs<clang::FunctionType>();
                if (FT && FT->getNoReturnAttr()) {
                    // if function is "__noreturn__", need to add exit if its body is removed
                    covAugment->emptyNoReturnFunctions.push_back(range);
                } else {
                    covAugment->emptyFunctions.push_back(range);
                }
            }
        }
    }

    return true;
}
bool CovAugmentAstVisitor::VisitIfStmt(clang::IfStmt *IS) {
    const clang::SourceManager &SM = covAugment->Context->getSourceManager();

    IfStmt if_stmt;
    if_stmt.if_stmt = IS;

    auto if_range = covAugment->getStartAndEnd(IS);

    for (clang::IfStmt *_if = IS; _if;) {
        if (_if->getThen() && !llvm::isa<clang::NullStmt>(_if->getThen()))
            if_stmt.branches.push_back(_if->getThen());
        if (_if->getElse() && !llvm::isa<clang::NullStmt>(_if->getElse())) {
            if_stmt.branches.push_back(_if->getElse());
            _if = llvm::dyn_cast<clang::IfStmt>(_if->getElse());
        } else {
            break;
        }
    }

    // the code after this ifstmt could be considered as a branch, if this is if-return/if-exit/...
    if (!covAugment->lineIsRemoved(if_range.first)) {
        auto parents = covAugment->Context->getParents(*IS);
        if (!parents.empty() && parents[0].get<clang::Stmt>()) {
            auto *parent = parents[0].get<clang::Stmt>();
            clang::Stmt *prev = nullptr, *next = nullptr;
            for (const clang::Stmt *child : parent->children()) {
                if (prev == IS) next = const_cast<clang::Stmt *>(child);
                prev = const_cast<clang::Stmt *>(child);
            }
            auto range = covAugment->getStartAndEnd(next);
            if (range.first > 0 && range.second > 0) {
                if (covAugment->lineIsRemoved(range.first)) {
                    if_stmt.branch_after_if = next;
                    // llvm::outs() << "Stmt after if-return: " << SM.getSpellingLineNumber(IS->getBeginLoc())
                    //     << " -> " << SM.getSpellingLineNumber(next->getBeginLoc()) << "\n";
                }
            }
        }
    }

    // symmetrical assignments
    clang::DeclRefExpr *common_LHS = nullptr;
    for (auto const &branch : if_stmt.branches) {
        if (branch && !llvm::isa<clang::NullStmt>(branch)) {
            if (clang::CompoundStmt *CS = llvm::dyn_cast<clang::CompoundStmt>(branch)) {
                if (CS->size() == 1) {
                    if (clang::BinaryOperator *BO = llvm::dyn_cast<clang::BinaryOperator>(CS->body_front())) {
                        if (BO->isAssignmentOp()) {
                            // llvm::outs() << "Possible symmetrical assignment: " <<
                            // SM.getSpellingLineNumber(IS->getBeginLoc()) << "\n";
                            if (clang::DeclRefExpr *LHS = llvm::dyn_cast<clang::DeclRefExpr>(BO->getLHS())) {
                                if (!common_LHS) {
                                    common_LHS = LHS;
                                } else if (common_LHS->getDecl() == LHS->getDecl()) {
                                    if_stmt.is_symmetrical = true;
                                    covAugment->ifStmts.push_back(if_stmt);
                                    // llvm::outs() << "Symmetrical assignment: "
                                    //              << SM.getSpellingLineNumber(IS->getBeginLoc()) << "\n";
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // keyword matching
    for (auto const &branch : if_stmt.branches) {
        if (branch && !llvm::isa<clang::NullStmt>(branch)) {
            if (covAugment->ifBranchContainsKeyword(branch, keywords, n_keywords)) {
                // llvm::outs() << "Found keyword in "
                //              << SM.getSpellingLineNumber(IS->getBeginLoc()) << "\n";
                if_stmt.is_keyword = true;
                covAugment->ifStmts.push_back(if_stmt);
                return true;
            } else if (llvm::isa<clang::GotoStmt>(branch) ||
                       (llvm::isa<clang::CompoundStmt>(branch) &&
                        llvm::dyn_cast<clang::CompoundStmt>(branch)->size() == 1 &&
                        llvm::isa<clang::GotoStmt>(
                            llvm::dyn_cast<clang::CompoundStmt>(branch)->body_front()))) {
                // llvm::outs() << "Found if-stmt containing goto-stmt "
                //              << SM.getSpellingLineNumber(IS->getBeginLoc()) << "\n";
                clang::GotoStmt *goto_stmt =
                    llvm::isa<clang::GotoStmt>(branch)
                        ? llvm::dyn_cast<clang::GotoStmt>(branch)
                        : llvm::dyn_cast<clang::GotoStmt>(
                              llvm::dyn_cast<clang::CompoundStmt>(branch)->body_front());
                auto range = covAugment->getRangeOfGotoLabel(goto_stmt->getLabel()->getStmt());
                if (range.first > 0 && range.second > 0) {
                    if (covAugment->rangeContainsKeyword(range, keywords, n_keywords)) {
                        // llvm::outs() << "Found keyword in goto label "
                        //              << range.first << "-" << range.second << "\n";
                        if_stmt.is_keyword = true;
                        covAugment->ifStmts.push_back(if_stmt);
                        return true;
                    }
                }
            }
        }
    }

    covAugment->ifStmts.push_back(if_stmt);

    return true;
}
