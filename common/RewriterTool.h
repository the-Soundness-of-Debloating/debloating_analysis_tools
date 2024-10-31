#ifndef REWRITERTOOL_H
#define REWRITERTOOL_H

#include <sstream>
#include <string>

#include "clang/AST/AST.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Rewrite/Core/Rewriter.h"

class RewriterTool {
    clang::Rewriter &theRewriter;
    clang::SourceManager &theSM;
    const clang::LangOptions &theLO;
    bool is_modified;

    void EmitError(clang::SourceLocation sl, std::string msg);

   public:
    RewriterTool(clang::Rewriter &rewriter)
        : theRewriter(rewriter),
          theSM(rewriter.getSourceMgr()),
          theLO(rewriter.getLangOpts()),
          is_modified(false) {}

    bool InsertTextBeforeWithMacroExpansion(clang::SourceLocation sl, std::string txt);
    bool InsertTextBefore(clang::SourceLocation sl, std::string txt);
    bool ReplaceText(clang::SourceLocation sl, unsigned length, std::string txt);
    bool ReplaceText(clang::SourceRange rng, clang::StringRef newstr);
    bool InsertTextAfterWithMacroExpansion(clang::SourceLocation sl, std::string txt);
    bool InsertTextAfterStmtWithMacroExpansion(clang::SourceLocation sl, std::string txt);
    bool InsertTextAfter(clang::SourceLocation sl, std::string txt);
    bool isInMainFile(clang::SourceLocation sl) const;
    bool WriteChangesToFiles();
    std::string GetSourceText(clang::SourceLocation start, clang::SourceLocation end) const;
    std::string GetSourceText(const clang::SourceRange &rng) const;
    llvm::Optional<clang::Token> FindNextToken(clang::SourceLocation sl) const;
    std::string GetSrcFilename(clang::FunctionDecl *fn) const;
    std::string GetSrcFilename(clang::SourceLocation sl) const;
    std::pair<unsigned, unsigned> getSrcLocation(clang::SourceLocation sl) const;
    std::string getSrcLocationAsString(clang::SourceLocation sl) const;
    clang::SourceLocation GetBeginningofLine(clang::SourceLocation sl) const;
    bool isModified() const { return is_modified; }
    clang::SourceLocation FindLocationAfterToken(clang::SourceLocation Loc,
                                                 clang::tok::TokenKind TKind);
    std::string GetPrettyPrintText(const clang::Stmt *stmt) const;
    clang::SourceManager &GetSourceManager() const { return theSM; }
    const clang::LangOptions &GetLangOptions() const { return theLO; }
};

#endif  // REWRITERTOOL_H
