#include "Instrumentation.h"

#include "SourceManager.h"
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

void InstruVisitor::instruStmt(const Stmt* stmt, std::string src_fname, bool is_return_stmt,
                               bool wrap_with_braces) {
    clang::SourceManager& theSM = RwTool.GetSourceManager();
    std::string line_number = getLineNumber(stmt->getBeginLoc());
    std::string instru_str0 = generateInsertionString("STMT_EXEC", "", line_number);
    if (is_return_stmt) instru_str0 += generateInsertionString("FUNC_RETURN", "", "");
    if (wrap_with_braces) instru_str0 = "{\n" + instru_str0;
    std::string instru_str1 = wrap_with_braces ? "\n}\n" : "";
    wrapWithStrings(stmt, instru_str0, instru_str1);
}

// FIXME: Actually, it should be AFTER the semicolon token, not BEFORE the next token
void InstruVisitor::wrapWithStrings(const Stmt* stmt, std::string instru_str0,
                                    std::string instru_str1) {
    clang::SourceManager& theSM = RwTool.GetSourceManager();
    const clang::LangOptions& theLO = RwTool.GetLangOptions();

    SourceLocation stmt_begin = stmt->getBeginLoc();
    SourceLocation stmt_end = stmt->getEndLoc();

    if (stmt_begin.isValid() && stmt_end.isValid()) {
        // Adjust to not use macro locations
        if (stmt_begin.isMacroID()) {
            clang::Lexer::isAtStartOfMacroExpansion(stmt_begin, theSM, theLO, &stmt_begin);
            stmt_begin = clang::Lexer::GetBeginningOfToken(stmt_begin, theSM, theLO);
            // isAtStartOfMacroExpansion(stmt_begin, theSM, theLO, &stmt_begin);
            // stmt_begin = GetBeginningOfToken(stmt_begin, theSM, theLO);
        }

        if (stmt_end.isMacroID()) {
            clang::Lexer::isAtEndOfMacroExpansion(stmt_end, theSM, theLO, &stmt_end);
            stmt_end = clang::Lexer::getLocForEndOfToken(stmt_end, 0, theSM, theLO);
            // isAtEndOfMacroExpansion(stmt_end, theSM, theLO, &stmt_end);
            // stmt_end = getLocForEndOfToken(stmt_end, 0, theSM, theLO);
        }

        // skip semicolon token (Stmt may be an Expr without a semicolon)
        if (RwTool.FindLocationAfterToken(stmt_end, clang::tok::semi).isValid()) {
            // RwTool.InsertTextBefore(stmt_end1, instru_str1);
            if (auto token = clang::Lexer::findNextToken(stmt_end, theSM, theLO)) {
                // insert AFTER the semicolon token, rather than BEFORE the next token
                stmt_end = (*token).getEndLoc();
            }
        }

        if (stmt_begin.isValid()) RwTool.InsertTextBefore(stmt_begin, instru_str0);
        if (stmt_end.isValid()) RwTool.InsertTextAfter(stmt_end, instru_str1);
    }
}

void InstruVisitor::instruNonCompoundStmtAsStmtBody(const Stmt* stmt, std::string src_fname) {
    if (llvm::isa<NullStmt>(stmt)) {
    }  // Do nothing
    else if (llvm::isa<LabelStmt>(stmt)) {
    }  // Do nothing
    else if (llvm::isa<SwitchCase>(stmt)) {
    }  // Do nothing
    else if (llvm::isa<ReturnStmt>(stmt)) {
        instruStmt(stmt, src_fname, true, true);
    } else {
        instruStmt(stmt, src_fname, false, true);
    }
}

void InstruVisitor::instruCompoundStmtAsStmtBody(const CompoundStmt* cs, std::string src_fname,
                                                 bool instru_break) {
    CompoundStmt::const_body_iterator bi, be;
    // Instrument every stmt in the body (but don't look into the stmt)
    for (bi = cs->body_begin(), be = cs->body_end(); bi != be; bi++) {
        // NOTE: a NULLStmt here can be either (1) a real null stmt or (2) a
        // parsing failure For either case, we ignore its instrumentation
        if (llvm::isa<NullStmt>(*bi)) {
            continue;
        } else if (llvm::isa<BreakStmt>(*bi) && !instru_break) {
            continue;
        } else if (llvm::isa<SwitchCase>(*bi)) {
            continue;
        } else if (llvm::isa<LabelStmt>(*bi)) {
            continue;
        } else if (llvm::isa<CompoundStmt>(*bi)) {
            const CompoundStmt* cs0 = llvm::dyn_cast<CompoundStmt>(*bi);
            instruCompoundStmtAsStmtBody(cs0, src_fname,
                                         true);  // This is not a switch body
        } else if (llvm::isa<ReturnStmt>(*bi)) {
            instruStmt(*bi, src_fname, true, false);
        } else {
            instruStmt(*bi, src_fname, false, false);
        }
    }
}

std::string InstruVisitor::getFuncSignature(const FunctionDecl* fd) {
    // std::string src_name = RwTool.GetSrcFilename(fd);
    std::string fd_name = fd->getNameInfo().getAsString();
    std::string param_types = "(";
    unsigned fd_param_num = fd->getNumParams();
    for (unsigned i = 0; i < fd_param_num; i++) {
        if (i != 0) {
            param_types += ",";
        }
        param_types += clang::QualType::getAsString(fd->getParamDecl(i)->getType().split(),
                                                    clang::PrintingPolicy{{}});
    }
    param_types += ")";
    // return src_name + ":" + fd_name + param_types;
    return fd_name + param_types;
}

std::string InstruVisitor::generateInsertionString(std::string type, std::string func_signature,
                                                   std::string stmt_line) {
    std::string content = type + ";" + func_signature + ";" + stmt_line;
    // add a newline at the beginning to separate from original outputs of the instrumented program
    return "printf(\"\\n" + content + "\\n\");\n";
}

std::string InstruVisitor::getLineNumber(const clang::SourceLocation loc) {
    clang::SourceManager& theSM = RwTool.GetSourceManager();
    if (loc.isValid()) {
        return std::to_string(theSM.getSpellingLineNumber(loc));
    } else {
        return "?";
    }
}

// parent / leaf
bool InstruVisitor::isParentStmt(const Stmt* stmt) {
    if (const IfStmt* if_stmt = llvm::dyn_cast<IfStmt>(stmt)) {
        return true;
    } else if (const DoStmt* ds = llvm::dyn_cast<DoStmt>(stmt)) {
        return true;
    } else if (const ForStmt* fs = llvm::dyn_cast<ForStmt>(stmt)) {
        return true;
    } else if (const SwitchStmt* ss = llvm::dyn_cast<SwitchStmt>(stmt)) {
        return true;
    } else if (const WhileStmt* ws = llvm::dyn_cast<WhileStmt>(stmt)) {
        return true;
    } else if (const SwitchCase* sc = llvm::dyn_cast<SwitchCase>(stmt)) {
        return true;
    } else if (const LabelStmt* ls = llvm::dyn_cast<LabelStmt>(stmt)) {
        return true;
    } else {
        return false;
    }
}

std::vector<clang::Stmt*> InstruVisitor::getAllChildren(clang::Stmt* S) {
    std::queue<clang::Stmt*> ToVisit;
    std::vector<clang::Stmt*> AllChildren;
    ToVisit.push(S);
    while (!ToVisit.empty()) {
        auto C = ToVisit.front();
        ToVisit.pop();
        AllChildren.emplace_back(C);
        for (auto const& Child : C->children()) {
            if (Child != NULL) ToVisit.push(Child);
        }
    }
    return AllChildren;
}

std::vector<clang::Stmt*> InstruVisitor::getAllPrimitiveChildrenStmts(clang::CompoundStmt* S) {
    std::queue<clang::Stmt*> ToVisit;
    std::vector<clang::Stmt*> AllPrimitiveChildrenStmts;
    ToVisit.push(S);
    while (!ToVisit.empty()) {
        auto C = ToVisit.front();
        ToVisit.pop();

        if (clang::CompoundStmt* CS = llvm::dyn_cast<clang::CompoundStmt>(C)) {
            for (auto ChildS : CS->body())
                if (ChildS != NULL) ToVisit.push(ChildS);
        } else if (clang::DoStmt* DS = llvm::dyn_cast<clang::DoStmt>(C)) {
            ToVisit.push(DS->getBody());
        } else if (clang::ForStmt* FS = llvm::dyn_cast<clang::ForStmt>(C)) {
            ToVisit.push(FS->getBody());
        } else if (clang::IfStmt* IS = llvm::dyn_cast<clang::IfStmt>(C)) {
            ToVisit.push(IS->getThen());
            if (IS->hasElseStorage()) {
                ToVisit.push(IS->getElse());
            }
        } else if (clang::LabelStmt* LS = llvm::dyn_cast<clang::LabelStmt>(C)) {
            ToVisit.push(LS->getSubStmt());
        } else if (clang::SwitchStmt* SS = llvm::dyn_cast<clang::SwitchStmt>(C)) {
            ToVisit.push(SS->getBody());
        } else if (clang::SwitchCase* SC = llvm::dyn_cast<clang::SwitchCase>(C)) {
            ToVisit.push(SC->getSubStmt());
        } else if (clang::WhileStmt* WS = llvm::dyn_cast<clang::WhileStmt>(C)) {
            ToVisit.push(WS->getBody());
        } else {
            AllPrimitiveChildrenStmts.emplace_back(C);
        }
    }
    return AllPrimitiveChildrenStmts;
}
