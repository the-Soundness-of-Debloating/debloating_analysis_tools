#ifndef GLOBAL_ADD_BACK_H
#define GLOBAL_ADD_BACK_H

#include <map>
#include <set>
#include <vector>

#include "clang/AST/RecursiveASTVisitor.h"

#include "Reduction.h"

class GlobalAddBack;

class GlobalAddBackElementCollectionVisitor : public clang::RecursiveASTVisitor<GlobalAddBackElementCollectionVisitor> {
  public:
    GlobalAddBackElementCollectionVisitor(GlobalAddBack *R) : globalAddBack(R) {}

    bool VisitFunctionDecl(clang::FunctionDecl *FD);
    bool VisitVarDecl(clang::VarDecl *VD);
    bool VisitTypeDecl(clang::TypeDecl *TD);
    bool VisitDeclRefExpr(clang::DeclRefExpr *DRE);

  private:
    GlobalAddBack *globalAddBack;
};

/// \brief Represents a global reduction phase
///
/// In global reduction phase, global declarations are reduced.
class GlobalAddBack : public Reduction {
    friend class GlobalAddBackElementCollectionVisitor;

  public:
    GlobalAddBack(std::set<int> &debloatedLines, std::string outputFileName)
        : Reduction(debloatedLines, outputFileName), CollectionVisitor(NULL) {}
    ~GlobalAddBack() { delete CollectionVisitor; }

  private:
    void Initialize(clang::ASTContext &Ctx);
    void HandleTranslationUnit(clang::ASTContext &Ctx);

    DDElementVector doDeltaDebugging(const DDElementVector &lineGroups);

    std::string applyFixAndOutputToFile(const DDElementVector &toAddBack, bool isTemp = true);

    void addDependencies(clang::Decl *decl);

    DDElementVector globalDecls;
    DDElementVector typeDecls;
    GlobalAddBackElementCollectionVisitor *CollectionVisitor;

    // std::map<int, DDElement> mapLineToRange;
    std::map<int, std::set<DDElement>> mapLineToDependencies;
};

#endif // GLOBAL_ADD_BACK_H
