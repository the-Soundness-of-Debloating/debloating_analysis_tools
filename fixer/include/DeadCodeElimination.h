#ifndef DEADCODE_ELIMINATION_H
#define DEADCODE_ELIMINATION_H

#include <queue>
#include <vector>
#include <string>
#include <set>

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"

#include "Reduction.h"

class ClangDeadcodeElimination;

class DeadcodeElementCollectionVisitor : public clang::RecursiveASTVisitor<DeadcodeElementCollectionVisitor> {
  public:
    DeadcodeElementCollectionVisitor(ClangDeadcodeElimination *R) : Consumer(R) {}

    bool VisitVarDecl(clang::VarDecl *VD);
    bool VisitLabelStmt(clang::LabelStmt *LS);
    bool VisitDeclRefExpr(clang::DeclRefExpr *DRE);

  private:
    ClangDeadcodeElimination *Consumer;
};

class ClangDeadcodeElimination : public Reduction {
    friend class DeadcodeElementCollectionVisitor;

  public:
    ClangDeadcodeElimination(std::set<int> debloatedLines, std::string outputFileName)
        : Reduction(debloatedLines, outputFileName), CollectionVisitor(NULL) {}
    ~ClangDeadcodeElimination() { delete CollectionVisitor; }

    void removeUnusedElements();
    std::map<clang::Decl *, clang::SourceRange> LocationMapping;
    std::vector<clang::SourceLocation> UnusedLocations;

  private:
    void Initialize(clang::ASTContext &Ctx);
    void HandleTranslationUnit(clang::ASTContext &Ctx);
    // bool HandleTopLevelDecl(clang::DeclGroupRef D);
    clang::SourceRange getRemoveRange(clang::SourceLocation Loc);
    bool isConstant(clang::Stmt *S);

    std::string applyFixAndOutputToFile(const DDElementVector &toAddBack, bool isTemp = true);

    DeadcodeElementCollectionVisitor *CollectionVisitor;

    std::set<clang::VarDecl*> allGlobalVarDecls;
    std::set<clang::VarDecl*> varDeclIsUsed;
};

class FrontendDCE {
  public:
    // use clang::tooling::ClangTool to run the action (because running without compilation does not support including headers)
    static int run(const std::vector<std::string> &inputFiles,
                   clang::tooling::CompilationDatabase &compilations,
                   clang::tooling::ToolAction *toolAction);
};

template <std::string &outputFileName>
class DCEAction : public clang::ASTFrontendAction {
   public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI,
                                                          clang::StringRef InFile) final {
        return std::unique_ptr<clang::ASTConsumer>(new ClangDeadcodeElimination(std::set<int>(), outputFileName));
    }
};

#endif // DEADCODE_ELIMINATION_H