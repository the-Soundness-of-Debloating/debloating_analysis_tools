#ifndef INSTRU_FRONTEND_H
#define INSTRU_FRONTEND_H

#include <string>
#include <vector>

#include "clang/Parse/ParseAST.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"

/// \brief Provides an independent frontend for any action on AST
class Frontend {
   public:
    static int run(const std::vector<std::string> &inputFiles,
                   const clang::tooling::CompilationDatabase &compilations,
                   clang::tooling::ToolAction *toolAction);
    static bool runWithoutCompilation(std::string &inputFile, clang::ASTConsumer *C);
};

#endif  // INSTRU_FRONTEND_H
