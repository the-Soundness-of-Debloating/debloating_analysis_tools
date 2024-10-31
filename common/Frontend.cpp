#include "Frontend.h"

#include "clang/Basic/Builtins.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/LangStandard.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/Support/Host.h"

int Frontend::run(const std::vector<std::string> &inputFiles,
                  const clang::tooling::CompilationDatabase &compilations,
                  clang::tooling::ToolAction *toolAction) {
    clang::tooling::ClangTool tool(compilations, inputFiles);
    return tool.run(toolAction);
}

bool Frontend::runWithoutCompilation(std::string &inputFile, clang::ASTConsumer *R) {
    std::unique_ptr<clang::CompilerInstance> CI(new clang::CompilerInstance);
    CI->createDiagnostics();
    clang::TargetOptions &TO = CI->getTargetOpts();
    TO.Triple = llvm::sys::getDefaultTargetTriple();
    clang::CompilerInvocation &Invocation = CI->getInvocation();
    clang::TargetInfo *Target =
        clang::TargetInfo::CreateTargetInfo(CI->getDiagnostics(), CI->getInvocation().TargetOpts);
    CI->setTarget(Target);

    CI->createFileManager();
    CI->createSourceManager(CI->getFileManager());
    CI->createPreprocessor(clang::TU_Complete);
    CI->createASTContext();

    CI->setASTConsumer(std::unique_ptr<clang::ASTConsumer>(R));
    clang::Preprocessor &PP = CI->getPreprocessor();
    PP.getBuiltinInfo().initializeBuiltins(PP.getIdentifierTable(), PP.getLangOpts());

    if (!CI->InitializeSourceManager(
            // Original Version (clang::InputKind::C auto converts to clang::InputKind):
            //   clang::FrontendInputFile(inputFile, clang::InputKind::C))) {
            clang::FrontendInputFile(inputFile, clang::InputKind(clang::Language::C)))) {
        return false;
    }

    CI->createSema(clang::TU_Complete, 0);
    clang::DiagnosticsEngine &Diag = CI->getDiagnostics();
    Diag.setSuppressAllDiagnostics(true);
    Diag.setIgnoreAllWarnings(true);

    CI->getDiagnosticClient().BeginSourceFile(CI->getLangOpts(), &CI->getPreprocessor());
    ParseAST(CI->getSema());
    CI->getDiagnosticClient().EndSourceFile();
    return true;
}
