#include "Instrumentation.h"

// break visitors apart to avoid conflicts in rewriting

// must be called first to handle non compound cases and to record line informations
class StmtVisitor : public InstruVisitor {
    using InstruVisitor::InstruVisitor;

   public:
    bool VisitFunctionDecl(clang::FunctionDecl* FD);
    bool VisitStmt(clang::Stmt* stmt);
};
class FunctionDeclVisitor : public InstruVisitor {
    using InstruVisitor::InstruVisitor;

   public:
    bool VisitFunctionDecl(clang::FunctionDecl* FD);
};
class FunctionCallVisitor : public InstruVisitor {
    using InstruVisitor::InstruVisitor;

   public:
    bool VisitCallExpr(clang::CallExpr* ce);
};

class StmtInstruAction : public clang::ASTFrontendAction {
   public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI,
                                                          clang::StringRef InFile) final {
        llvm::outs() << "Instrument source file '" << InFile.str() << "'.\n";
        return std::unique_ptr<clang::ASTConsumer>(new Instrumentation<StmtVisitor>());
    }
};
class FuncDeclInstruAction : public clang::ASTFrontendAction {
   public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI,
                                                          clang::StringRef InFile) final {
        return std::unique_ptr<clang::ASTConsumer>(new Instrumentation<FunctionDeclVisitor>());
    }
};
class CallExprInstruAction : public clang::ASTFrontendAction {
   public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI,
                                                          clang::StringRef InFile) final {
        return std::unique_ptr<clang::ASTConsumer>(new Instrumentation<FunctionCallVisitor>());
    }
};