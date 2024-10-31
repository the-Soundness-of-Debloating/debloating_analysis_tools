#ifndef GLOBAL_ADD_BACK_H
#define GLOBAL_ADD_BACK_H

#include <map>
#include <queue>
#include <set>
#include <vector>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/CommandLine.h"

// Delta debugging elements
// make sure each line can contain at most one statement
using LineRange = std::pair<int, int>;
using LineRangeVector = std::vector<LineRange>;
using LineRangeSet = std::set<LineRange>;

extern std::string opt_original_file;
extern llvm::cl::opt<std::string> opt_debloated_file;
extern std::set<std::string> opt_augmentation_strategies;

struct IfStmt {
    clang::IfStmt *if_stmt = nullptr;
    std::vector<clang::Stmt*> branches;
    clang::Stmt *branch_after_if = nullptr;

    bool is_symmetrical = false;
    bool is_keyword = false;
};

class CovAugment;

class CovAugmentAstVisitor : public clang::RecursiveASTVisitor<CovAugmentAstVisitor> {
   public:
    CovAugmentAstVisitor(CovAugment *R) : covAugment(R) {}

    bool VisitDeclRefExpr(clang::DeclRefExpr *DRE);
    bool VisitGotoStmt(clang::GotoStmt *GS);
    bool VisitFunctionDecl(clang::FunctionDecl *FD);
    bool VisitIfStmt(clang::IfStmt *IS);

   private:
    CovAugment *covAugment;
};

class CovAugment : public clang::ASTConsumer {
    friend class CovAugmentAstVisitor;

   public:
    CovAugment(std::set<int> &debloatedLines, std::string outputFileName)
        : debloatedLines(debloatedLines), outputFileName(outputFileName), collectionVisitor(NULL) {}
    ~CovAugment() { delete collectionVisitor; }

   private:
    void Initialize(clang::ASTContext &Ctx);
    void HandleTranslationUnit(clang::ASTContext &Ctx);

    void applyAugmentationToFile(const std::vector<LineRange> &ranges, bool is_exit_replacement);

    void addDependency(clang::Stmt *stmt);
    void addDependency(clang::Stmt *stmt, clang::Decl *decl);
    void addDependency(clang::Stmt *stmt, LineRange range_decl);

    LineRange getStartAndEnd(clang::SourceRange range);
    LineRange getStartAndEnd(clang::Decl *decl);
    LineRange getStartAndEnd(clang::Stmt *stmt);
    LineRange getRangeOfGotoLabel(clang::LabelStmt *LS);

    bool lineIsRemoved(int line);
    bool functionIsRemoved(clang::FunctionDecl *FD);
    bool ifBranchContainsKeyword(clang::Stmt* if_branch, std::string *keywords, int n_keywords);
    bool rangeContainsKeyword(clang::SourceRange range, std::string *keywords, int n_keywords);
    bool rangeContainsKeyword(LineRange range, std::string *keywords, int n_keywords);

    std::vector<IfStmt> ifStmts;
    std::vector<LineRange> emptyFunctions;
    std::vector<LineRange> emptyNoReturnFunctions;

    CovAugmentAstVisitor *collectionVisitor;

    // only record removed dependencies
    std::map<int, std::set<LineRange>> mapLineToDependencies;

    clang::ASTContext *Context;
    std::set<int> &debloatedLines;

    std::string outputFileName;
};

#endif  // GLOBAL_ADD_BACK_H
