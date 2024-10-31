#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <memory>
#include <queue>
#include <set>
#include <vector>

#include "RewriterTool.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

class InstruVisitor : public clang::RecursiveASTVisitor<InstruVisitor> {
   public:
    InstruVisitor(clang::Rewriter& TheRewriter) : RwTool(TheRewriter) {}
    void setASTContext(clang::ASTContext* astContext) { this->astContext = astContext; }
    void writeChangesToFiles() { RwTool.WriteChangesToFiles(); }
    RewriterTool* getTheRwTool() { return &this->RwTool; }

    void instruStmt(const clang::Stmt* stmt, std::string src_fname, bool is_return_stmt,
                    bool wrap_with_braces);
    void wrapWithStrings(const clang::Stmt* stmt, std::string instru_str0, std::string instru_str1);
    bool isParentStmt(const clang::Stmt* stmt);

    std::string getFuncSignature(const clang::FunctionDecl* fd);
    std::string generateInsertionString(std::string type, std::string func_signature,
                                        std::string stmt_line);
    std::string getLineNumber(const clang::SourceLocation loc);

    void instruNonCompoundStmtAsStmtBody(const clang::Stmt* stmt, std::string src_fname);
    void instruCompoundStmtAsStmtBody(const clang::CompoundStmt* cs, std::string src_fname,
                                      bool instru_break);

    std::vector<clang::Stmt*> getAllChildren(clang::Stmt* S);
    std::vector<clang::Stmt*> getAllPrimitiveChildrenStmts(clang::CompoundStmt* S);

    // must declare them in the base class to let RecursiveASTVisitor know
    virtual bool VisitCallExpr(clang::CallExpr* ce) { return true; }
    virtual bool VisitFunctionDecl(clang::FunctionDecl* FD) { return true; }
    virtual bool VisitStmt(clang::Stmt* stmt) { return true; }

   protected:
    clang::ASTContext* astContext;
    RewriterTool RwTool;
};

template <typename VISITOR_TYPE>
class Instrumentation : public clang::ASTConsumer {
   public:
    Instrumentation() : visitor(nullptr) {}
    ~Instrumentation() { delete visitor; }

   private:
    // https://stackoverflow.com/questions/8752837/undefined-reference-to-template-class-constructor
    void Initialize(clang::ASTContext& Ctx) {
        theRewriter.setSourceMgr(Ctx.getSourceManager(), Ctx.getLangOpts());
        this->visitor = new VISITOR_TYPE(this->theRewriter);
    }
    void HandleTranslationUnit(clang::ASTContext& Ctx) {
        this->visitor->setASTContext(&Ctx);
        this->visitor->TraverseDecl(Ctx.getTranslationUnitDecl());
        this->visitor->writeChangesToFiles();
    }

    InstruVisitor* visitor;
    clang::Rewriter theRewriter;
};

#endif  // INSTRUMENTATION_H
