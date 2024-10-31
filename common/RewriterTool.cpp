#include "RewriterTool.h"

#include "clang/Lex/Lexer.h"

using namespace clang;

bool RewriterTool::InsertTextBeforeWithMacroExpansion(SourceLocation sl, std::string txt) {
    if (sl.isValid()) {
        if (sl.isMacroID()) {
            Lexer::isAtStartOfMacroExpansion(sl, theSM, theLO, &sl);
            sl = Lexer::GetBeginningOfToken(sl, theSM, theLO);
        }
    }

    if (sl.isValid()) {
        return InsertTextBefore(sl, txt);
    }
    return false;
}

bool RewriterTool::InsertTextBefore(SourceLocation sl, std::string txt) {
    // Lexer::isAtStartOfMacroExpansion(sl, theSM, theLO, &sl); //Got error,
    // don't know why...
    if (Rewriter::isRewritable(sl)) {
        theRewriter.InsertTextBefore(sl, txt);
        is_modified = true;
        return true;
    }

    EmitError(theSM.getSpellingLoc(sl), "non-writable source location");
    return false;
}

bool RewriterTool::ReplaceText(SourceLocation sl, unsigned length, std::string txt) {
    // Lexer::isAtStartOfMacroExpansion(sl, theSM, theLO, &sl);
    if (Rewriter::isRewritable(sl)) {
        theRewriter.ReplaceText(sl, length, txt);
        is_modified = true;
        return true;
    }

    EmitError(theSM.getSpellingLoc(sl), "non-writable source location");
    return false;
}

bool RewriterTool::ReplaceText(clang::SourceRange rng, clang::StringRef newstr) {
    // Lexer::isAtStartOfMacroExpansion(...);
    if (Rewriter::isRewritable(rng.getBegin())) {
        theRewriter.ReplaceText(rng, newstr);
        is_modified = true;
        return true;
    }

    EmitError(theSM.getSpellingLoc(rng.getBegin()), "non-writable source location");
    return false;
}

bool RewriterTool::InsertTextAfterWithMacroExpansion(SourceLocation sl, std::string txt) {
    if (sl.isValid()) {
        if (sl.isMacroID()) {
            Lexer::isAtEndOfMacroExpansion(sl, theSM, theLO, &sl);
            sl = Lexer::getLocForEndOfToken(sl, 0, theSM, theLO);
        }
    }

    if (sl.isValid()) {
        return InsertTextAfter(sl, txt);
    }

    return false;
}

bool RewriterTool::InsertTextAfterStmtWithMacroExpansion(SourceLocation sl, std::string txt) {
    if (sl.isValid()) {
        if (sl.isMacroID()) {
            Lexer::isAtEndOfMacroExpansion(sl, theSM, theLO, &sl);
            sl = Lexer::getLocForEndOfToken(sl, 0, theSM, theLO);
        }
    }

    SourceLocation sl1 = FindLocationAfterToken(sl, clang::tok::semi);
    if (sl1.isValid()) {
        sl = sl1;
    }

    if (sl.isValid()) {
        return InsertTextAfter(sl, txt);
    }

    return false;
}

bool RewriterTool::InsertTextAfter(SourceLocation sl, std::string txt) {
    // Lexer::isAtEndOfMacroExpansion(sl, theSM, theLO, &sl);
    if (Rewriter::isRewritable(sl)) {
        theRewriter.InsertTextAfterToken(sl, txt);
        is_modified = true;
        return true;
    }

    EmitError(theSM.getSpellingLoc(sl), "non-writable source location");
    return false;
}

bool RewriterTool::isInMainFile(clang::SourceLocation sl) const {
    return theSM.isWrittenInMainFile(sl);
}

bool RewriterTool::WriteChangesToFiles() { return theRewriter.overwriteChangedFiles(); }

std::string RewriterTool::GetSourceText(clang::SourceLocation start,
                                        clang::SourceLocation end) const {
    // adjust for macro expansion
    // Lexer::isAtStartOfMacroExpansion(start, theSM, theLO, &start);
    start = Lexer::GetBeginningOfToken(start, theSM, theLO);
    // Lexer::isAtEndOfMacroExpansion(end, theSM, theLO, &end);
    end = Lexer::getLocForEndOfToken(end, 0, theSM, theLO);

    // get the source text of the in the sl range
    const CharSourceRange src = Lexer::getAsCharRange(SourceRange(start, end), theSM, theLO);
    return Lexer::getSourceText(src, theSM, theLO).str();
}

llvm::Optional<clang::Token> RewriterTool::FindNextToken(clang::SourceLocation sl) const {
    return Lexer::findNextToken(sl, theSM, theLO);
}

std::string RewriterTool::GetSourceText(const clang::SourceRange& rng) const {
    return GetSourceText(rng.getBegin(), rng.getEnd());
}

std::string RewriterTool::GetSrcFilename(clang::FunctionDecl* fn) const {
    return GetSrcFilename(fn->getBeginLoc());
}

std::pair<unsigned, unsigned> RewriterTool::getSrcLocation(clang::SourceLocation sl) const {
    std::pair<unsigned, unsigned> result;
    result.first = theSM.getSpellingLineNumber(sl);
    result.second = theSM.getSpellingColumnNumber(sl);
    return result;
}

std::string RewriterTool::getSrcLocationAsString(clang::SourceLocation sl) const {
    std::string filename = GetSrcFilename(sl);
    std::pair<unsigned, unsigned> loc = getSrcLocation(sl);
    std::stringstream ss;

    ss << filename << ":" << loc.first << ":" << loc.second;
    return ss.str();
}

std::string RewriterTool::GetSrcFilename(clang::SourceLocation sl) const {
    return theSM.getFilename(sl).str();
}

clang::SourceLocation RewriterTool::GetBeginningofLine(clang::SourceLocation sl) const {
    FileID fID = theSM.getFileID(sl);
    unsigned line = theSM.getSpellingLineNumber(sl);
    return theSM.translateLineCol(fID, line, 1);
}

void RewriterTool::EmitError(SourceLocation sl, std::string msg) {
    llvm::errs() << theSM.getFilename(sl).str() << ":" << theSM.getSpellingLineNumber(sl) << ":"
                 << theSM.getSpellingColumnNumber(sl) << ":"
                 << " " << msg << "\n";
}

SourceLocation RewriterTool::FindLocationAfterToken(SourceLocation Loc, tok::TokenKind TKind) {
    return Lexer::findLocationAfterToken(Loc, TKind, theSM, theLO, true);
}

std::string RewriterTool::GetPrettyPrintText(const clang::Stmt* stmt) const {
    std::string stmt_str;
    llvm::raw_string_ostream stream(stmt_str);
    stmt->printPretty(stream, NULL, PrintingPolicy(theLO));
    stream.flush();
    return stmt_str;
}
