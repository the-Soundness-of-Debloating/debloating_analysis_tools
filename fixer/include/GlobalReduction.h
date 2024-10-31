#ifndef GLOBAL_REDUCTION_H
#define GLOBAL_REDUCTION_H

#include <map>
#include <set>
#include <vector>

#include "clang/AST/RecursiveASTVisitor.h"

#include "Reduction.h"

class GlobalReduction;

class GlobalElementCollectionVisitor : public clang::RecursiveASTVisitor<GlobalElementCollectionVisitor> {
  public:
    GlobalElementCollectionVisitor(GlobalReduction *R) : globalReduction(R) {}

    bool VisitFunctionDecl(clang::FunctionDecl *FD);
    bool VisitVarDecl(clang::VarDecl *VD);
    bool VisitTypeDecl(clang::TypeDecl *TD);

  private:
    GlobalReduction *globalReduction;
};

/// \brief Represents a global reduction phase
///
/// In global reduction phase, global declarations are reduced.
class GlobalReduction : public Reduction {
    friend class GlobalElementCollectionVisitor;

  public:
    GlobalReduction(std::set<int> &debloatedLines, std::string outputFileName)
        : Reduction(debloatedLines, outputFileName), CollectionVisitor(NULL) {}
    ~GlobalReduction() { delete CollectionVisitor; }

  private:
    void Initialize(clang::ASTContext &Ctx);
    void HandleTranslationUnit(clang::ASTContext &Ctx);

    std::string applyFixAndOutputToFile(const DDElementVector &toAddBack, bool isTemp = true);

    void addDependencies(clang::Decl *decl);

    DDElementVector globalDecls;
    GlobalElementCollectionVisitor *CollectionVisitor;
};

#endif // GLOBAL_REDUCTION_H
