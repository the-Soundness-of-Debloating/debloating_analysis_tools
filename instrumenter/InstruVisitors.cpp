#include "InstruVisitors.h"

#include "SourceManager.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/Lex/Lexer.h"

using BinaryOperator = clang::BinaryOperator;
using BreakStmt = clang::BreakStmt;
using CallExpr = clang::CallExpr;
using ContinueStmt = clang::ContinueStmt;
using CompoundStmt = clang::CompoundStmt;
using DeclGroupRef = clang::DeclGroupRef;
using DeclStmt = clang::DeclStmt;
using FunctionDecl = clang::FunctionDecl;
using GotoStmt = clang::GotoStmt;
using IfStmt = clang::IfStmt;
using LabelStmt = clang::LabelStmt;
using ReturnStmt = clang::ReturnStmt;
using SourceRange = clang::SourceRange;
using SourceLocation = clang::SourceLocation;
using Stmt = clang::Stmt;
using UnaryOperator = clang::UnaryOperator;
using WhileStmt = clang::WhileStmt;
using VarDecl = clang::VarDecl;
using Decl = clang::Decl;
using LabelDecl = clang::LabelDecl;
using Expr = clang::Expr;
using DeclRefExpr = clang::DeclRefExpr;
using ForStmt = clang::ForStmt;
using SwitchStmt = clang::SwitchStmt;
using DoStmt = clang::DoStmt;
using NullStmt = clang::NullStmt;
using SwitchCase = clang::SwitchCase;

bool FunctionDeclVisitor::VisitFunctionDecl(FunctionDecl* fd) {
    if (fd->isThisDeclarationADefinition() && fd->hasBody()) {
        if (RwTool.isInMainFile(fd->getLocation())) {
            // Insert function-end instru code
            std::string fd_sig = getFuncSignature(fd);
            if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(fd->getBody())) {
                SourceLocation loc = cs->getEndLoc();
                if (loc.isMacroID()) {
                    clang::SourceManager& theSM = RwTool.GetSourceManager();
                    const clang::LangOptions& theLO = RwTool.GetLangOptions();
                    clang::Lexer::isAtEndOfMacroExpansion(loc, theSM, theLO, &loc);
                    loc = clang::Lexer::getLocForEndOfToken(loc, 0, theSM, theLO);
                    // isAtEndOfMacroExpansion(loc, theSM, theLO, &loc);
                    // loc = getLocForEndOfToken(loc, 0, theSM, theLO);
                }
                if (loc.isValid()) {
                    RwTool.InsertTextBefore(loc, generateInsertionString("FUNC_RETURN", "", ""));
                }
            }

            // Insert function-begin instru code
            std::string fd_sig_print_str1 = generateInsertionString("FUNC_CALL", fd_sig, "");
            if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(fd->getBody())) {
                if (cs->body_empty()) {
                    SourceLocation loc = cs->getEndLoc();
                    if (loc.isMacroID()) {
                        clang::SourceManager& theSM = RwTool.GetSourceManager();
                        const clang::LangOptions& theLO = RwTool.GetLangOptions();
                        clang::Lexer::isAtEndOfMacroExpansion(loc, theSM, theLO, &loc);
                        loc = clang::Lexer::getLocForEndOfToken(loc, 0, theSM, theLO);
                        // isAtEndOfMacroExpansion(loc, theSM, theLO, &loc);
                        // loc = getLocForEndOfToken(loc, 0, theSM, theLO);
                    }
                    if (loc.isValid()) {
                        RwTool.InsertTextBefore(loc, fd_sig_print_str1);
                    }
                } else {
                    const Stmt* first_child = cs->body_front();
                    SourceLocation loc = first_child->getBeginLoc();
                    if (loc.isMacroID()) {
                        clang::SourceManager& theSM = RwTool.GetSourceManager();
                        const clang::LangOptions& theLO = RwTool.GetLangOptions();
                        clang::Lexer::isAtStartOfMacroExpansion(loc, theSM, theLO, &loc);
                        loc = clang::Lexer::GetBeginningOfToken(loc, theSM, theLO);
                        // isAtStartOfMacroExpansion(loc, theSM, theLO, &loc);
                        // loc = GetBeginningOfToken(loc, theSM, theLO);
                    }
                    if (loc.isValid()) {
                        RwTool.InsertTextBefore(loc, fd_sig_print_str1);
                    }
                }
            }
        }
    }
    return true;
}

// FIXME: Why need this???
bool StmtVisitor::VisitFunctionDecl(FunctionDecl* fd) {
    if (fd->isThisDeclarationADefinition() && fd->hasBody()) {
        if (RwTool.isInMainFile(fd->getLocation())) {
            // Instru stmt
            if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(fd->getBody())) {
                std::string src_fname = RwTool.GetSrcFilename(fd->getLocation());
                instruCompoundStmtAsStmtBody(cs, src_fname, true);
            }
        }
    }
    return true;
}

bool StmtVisitor::VisitStmt(Stmt* stmt) {
    if (!RwTool.isInMainFile(stmt->getBeginLoc())) {
        return true;
    }

    std::string src_fname = RwTool.GetSrcFilename(stmt->getBeginLoc());

    // Look at every non-primitive stmt (in post-order)
    if (const IfStmt* if_stmt = llvm::dyn_cast<IfStmt>(stmt)) {
        if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(if_stmt->getThen())) {
            instruCompoundStmtAsStmtBody(cs, src_fname, true);
        } else {
            instruNonCompoundStmtAsStmtBody(if_stmt->getThen(), src_fname);
        }

        if (if_stmt->hasElseStorage()) {
            if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(if_stmt->getElse())) {
                instruCompoundStmtAsStmtBody(cs, src_fname, true);
            } else {
                instruNonCompoundStmtAsStmtBody(if_stmt->getElse(), src_fname);
            }
        }
    }

    else if (const DoStmt* ds = llvm::dyn_cast<DoStmt>(stmt)) {
        if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(ds->getBody())) {
            instruCompoundStmtAsStmtBody(cs, src_fname, true);
        } else {
            instruNonCompoundStmtAsStmtBody(ds->getBody(), src_fname);
        }
    }

    else if (const ForStmt* fs = llvm::dyn_cast<ForStmt>(stmt)) {
        if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(fs->getBody())) {
            instruCompoundStmtAsStmtBody(cs, src_fname, true);
        } else {
            instruNonCompoundStmtAsStmtBody(fs->getBody(), src_fname);
        }
    }

    else if (const SwitchStmt* ss = llvm::dyn_cast<SwitchStmt>(stmt)) {
        if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(ss->getBody())) {
            // Do NOT instrument break stmts (at the top level in the body).
            // Why? Because we don't instrument any case lines (e.g., case:
            // 'A'), so if a case branch is not taken, and the break stmt would
            // be removed, and the program semantics would be changed -- the
            // execution would fall through the branch.
            instruCompoundStmtAsStmtBody(cs, src_fname, false);
        } else {
            instruNonCompoundStmtAsStmtBody(ss->getBody(), src_fname);
        }
    }

    else if (const WhileStmt* ws = llvm::dyn_cast<WhileStmt>(stmt)) {
        if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(ws->getBody())) {
            instruCompoundStmtAsStmtBody(cs, src_fname, true);
        } else {
            instruNonCompoundStmtAsStmtBody(ws->getBody(), src_fname);
        }
    }

    else if (const SwitchCase* sc = llvm::dyn_cast<SwitchCase>(stmt)) {
        if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(sc->getSubStmt())) {
            instruCompoundStmtAsStmtBody(cs, src_fname, true);
        } else {
            // Insert a null stmt (i.e., a semi-colon) after the colon
            // Why? Because the code after the switch case colon might all be
            // removed, and the compiler would complain this (for having no code
            // b/w the last switch case and the end of the switch stmt).
            SourceLocation colon_loc = sc->getColonLoc();
            if (colon_loc.isValid()) {
                if (colon_loc.isMacroID()) {
                    clang::SourceManager& theSM = RwTool.GetSourceManager();
                    const clang::LangOptions& theLO = RwTool.GetLangOptions();
                    clang::Lexer::isAtEndOfMacroExpansion(colon_loc, theSM, theLO, &colon_loc);
                    colon_loc = clang::Lexer::getLocForEndOfToken(colon_loc, 0, theSM, theLO);
                    // isAtEndOfMacroExpansion(colon_loc, theSM, theLO,
                    // &colon_loc); colon_loc = getLocForEndOfToken(colon_loc,
                    // 0, theSM, theLO);
                }
                if (colon_loc.isValid()) {
                    RwTool.InsertTextAfter(colon_loc,
                                           ";");  // Insert a semi-colon
                }
            }

            const Stmt* sc_sub = sc->getSubStmt();
            if (!llvm::isa<NullStmt>(sc_sub) &&
                !llvm::isa<LabelStmt>(sc_sub) &&   // e.g., case '1': failure: ...
                !llvm::isa<BreakStmt>(sc_sub) &&   // e.g., case '1': break;
                !llvm::isa<SwitchCase>(sc_sub)) {  // e.g., case '1': case '2': ...
                // No need to add braces here
                instruStmt(sc_sub, src_fname, llvm::isa<ReturnStmt>(sc_sub), false);
            }
        }
    }

    else if (const LabelStmt* ls = llvm::dyn_cast<LabelStmt>(stmt)) {
        if (const CompoundStmt* cs = llvm::dyn_cast<CompoundStmt>(ls->getSubStmt())) {
            instruCompoundStmtAsStmtBody(cs, src_fname, true);
        } else {
            instruNonCompoundStmtAsStmtBody(ls->getSubStmt(),
                                            src_fname);  // Need to add braces here
        }
    }

    // TODO: what else stmt to consider?

    return true;
}

// Only instrument functions that are defined outside the main file (library function calls)
// FIXME: temporarily ignore "printf" function
bool FunctionCallVisitor::VisitCallExpr(CallExpr* ce) {
    if (!RwTool.isInMainFile(ce->getBeginLoc())) {
        return true;
    }

    // no-decl: such as calling a function using its pointer
    // if (!fd) llvm::outs() << "visit call expr: <no-decl>\n";
    // else if (!fd->getIdentifier()) llvm::outs() << "visit call expr: <no-name>\n";
    // else llvm::outs() << "  visit call expr: " << fd->getIdentifier()->getName() << "\n";
    const FunctionDecl* fd = ce->getDirectCallee();
    if (fd) {
        // only instruments library function calls
        if (fd->hasBody(fd)) {
            if (RwTool.isInMainFile(fd->getLocation())) {
                return true;
            }
        }

        if (fd->getNameInfo().getAsString() == "printf") {
            return true;
        }

        clang::SourceManager& theSM = RwTool.GetSourceManager();
        std::string instru_str0 = generateInsertionString("FUNC_CALL", getFuncSignature(fd), "");
        std::string instru_str1 = generateInsertionString("FUNC_RETURN", "", "");

        // Expr can be an individual Stmt while VarDecl must be wrapped in DeclStmt.
        const Stmt* parentStmt = ce;
        for (auto parents = this->astContext->getParents(*ce); !parents.empty();) {
            if (const Expr* expr = parents[0].get<Expr>()) {
                parentStmt = expr;
                parents = this->astContext->getParents(*expr);
                continue;
            } else if (const Decl* decl = parents[0].get<Decl>()) {
                // such as VarDecl...
                parents = this->astContext->getParents(*decl);
                continue;
            } else if (parents[0].get<CompoundStmt>() || parents[0].get<IfStmt>() ||
                       parents[0].get<DoStmt>() || parents[0].get<ForStmt>() ||
                       parents[0].get<SwitchStmt>() || parents[0].get<WhileStmt>() ||
                       parents[0].get<SwitchCase>() || parents[0].get<LabelStmt>()) {
                break;
            } else if (const Stmt* stmt = parents[0].get<Stmt>()) {
                parentStmt = stmt;
                break;
            } else {
                llvm::errs() << "Unknown parent type when finding CallExpr ("
                             << getLineNumber(ce->getBeginLoc()) << ") 's parent.\n";
                return true;
            }
        }

        if (parentStmt != nullptr) {
            // llvm::outs() << "==========CallExpr (" << getLineNumber(ce->getBeginLoc()) << ")\n";
            // llvm::outs() << "==========ParentStmt (" << getLineNumber(parentStmt->getBeginLoc())
            //              << ")\n";
            wrapWithStrings(parentStmt, instru_str0, instru_str1);
        } else {
            llvm::errs() << "Error when finding CallExpr (" << getLineNumber(ce->getBeginLoc())
                         << ") 's parent.\n";
            return true;
        }
    }
    return true;
}
