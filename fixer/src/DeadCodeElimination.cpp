#include "DeadCodeElimination.h"

#include "clang/Basic/Builtins.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/LangStandard.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/TargetSelect.h"

#include <vector>
#include <set>

#include "FileManager.h"
#include "SourceManager.h"

extern clang::TextDiagnosticBuffer diagnosticConsumer;

int FrontendDCE::run(const std::vector<std::string> &inputFiles,
                  clang::tooling::CompilationDatabase &compilations,
                  clang::tooling::ToolAction *toolAction) {
    clang::tooling::ClangTool tool(compilations, inputFiles);
    tool.setDiagnosticConsumer(&diagnosticConsumer);
    return tool.run(toolAction);
}

void ClangDeadcodeElimination::Initialize(clang::ASTContext &Ctx) {
    Reduction::Initialize(Ctx);
    CollectionVisitor = new DeadcodeElementCollectionVisitor(this);
}

void ClangDeadcodeElimination::HandleTranslationUnit(clang::ASTContext &Ctx) {
    CollectionVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());

    // llvm::outs() << "Diagnostics:\n";
    for (clang::TextDiagnosticBuffer::const_iterator diagnosticIterator = diagnosticConsumer.warn_begin();
         diagnosticIterator != diagnosticConsumer.warn_end(); ++diagnosticIterator) {
        // llvm::outs() << diagnosticIterator->second;
        if (diagnosticIterator->second.find("unused variable") == 0 ||
            diagnosticIterator->second.find("unused label") == 0) {
            // llvm::outs() << "unused variable/label";
            UnusedLocations.emplace_back(diagnosticIterator->first);
        }
        // llvm::outs() << "\n";
    }

    for (auto VD : allGlobalVarDecls) {
        if (varDeclIsUsed.find(VD) == varDeclIsUsed.end()) {
            // llvm::outs() << "unused global variable: " << VD->getNameAsString() << "\n";
            UnusedLocations.emplace_back(VD->getBeginLoc());
        }
    }

    removeUnusedElements();
}

clang::SourceRange ClangDeadcodeElimination::getRemoveRange(clang::SourceLocation Loc) {
    const clang::SourceManager &SM = Context->getSourceManager();
    for (auto Entry : LocationMapping) {
        clang::SourceLocation Begin = Entry.second.getBegin();
        clang::SourceLocation End;
        // if (clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(Entry.first)) {
        //     if (VD->hasInit()) {
        //         if (isConstant(VD->getInit()))
        //             End = SourceManager::GetEndLocationUntil(SM, VD->getEndLoc(), clang::tok::semi);
        //     } else
        //         End = SourceManager::GetEndLocationUntil(SM, VD->getEndLoc(), clang::tok::semi);
        // } else if (clang::LabelDecl *LD = llvm::dyn_cast<clang::LabelDecl>(Entry.first))
        //     End = LD->getStmt()->getSubStmt()->getBeginLoc().getLocWithOffset(-1);

        End = SourceManager::getStartAndEnd(Context, Entry.first).second;

        if ((Begin < Loc || Begin == Loc) && (Loc < End || Loc == End))
            return clang::SourceRange(Begin, End);
    }
    return clang::SourceRange();
}

bool ClangDeadcodeElimination::isConstant(clang::Stmt *S) {
    if (clang::StringLiteral *L = llvm::dyn_cast<clang::StringLiteral>(S))
        return true;
    if (clang::IntegerLiteral *L = llvm::dyn_cast<clang::IntegerLiteral>(S))
        return true;
    if (clang::CharacterLiteral *L = llvm::dyn_cast<clang::CharacterLiteral>(S))
        return true;
    if (clang::CompoundLiteralExpr *L = llvm::dyn_cast<clang::CompoundLiteralExpr>(S))
        return true;
    if (clang::FloatingLiteral *L = llvm::dyn_cast<clang::FloatingLiteral>(S))
        return true;
    if (clang::ImaginaryLiteral *L = llvm::dyn_cast<clang::ImaginaryLiteral>(S))
        return true;
    if (clang::CastExpr *L = llvm::dyn_cast<clang::CastExpr>(S)) {
        clang::Stmt *FirstChild;
        for (auto Child : S->children()) {
            FirstChild = Child;
            break;
        }
        return isConstant(FirstChild);
    }
    return false;
}

void ClangDeadcodeElimination::removeUnusedElements() {
    std::set<DDElement> ranges;
    for (auto Loc : UnusedLocations) {
        // llvm::outs() << "unused location: " << Loc.printToString(Context->getSourceManager()) << "\n";
        clang::SourceRange Range = getRemoveRange(Loc);
        DDElement range = getStartAndEnd(Range);
        if (range.first > 0 && range.second > 0) {
            if (addedBackLines.find(range.first) != addedBackLines.end())
                ranges.insert(range);
        }
    }

    auto removed = doDeltaDebugging(toVector(ranges));
    llvm::outs() << "DCE: " << removed.size() << " out of " << ranges.size() << " ranges successfully DCE'd\n";
}

std::string ClangDeadcodeElimination::applyFixAndOutputToFile(const DDElementVector &toAddBack, bool isTemp) {
    const clang::SourceManager &SM = Context->getSourceManager();

    std::set<DDElement> ranges;
    for (auto const &element : toAddBack) {
        // if (element.isNull())
        //     continue;
        if (element.first < 0 || element.second < 0) {
            // llvm::errs() << "Error in applyFixAndOutputToFile: (-1, -1)\n";
            continue;
        }
        ranges.insert(element);
    }

    // replace ranges of lines in the debloated (temp) file with lines in the original file
    std::string outputFile =
        isTemp ? FileManager::getStemName(opt_result_file) + ".temp.c" : outputFileName;
    llvm::sys::fs::copy_file(opt_result_file, outputFile);
    // CANNOT use `sed -i 10c\ "$(sed -n 10p original_file)" outputFile` (when 10th line is empty, it fails)
    // use python instead
    std::string rangesStr;
    for (auto const &range : ranges)
        rangesStr += std::to_string(range.first) + "-" + std::to_string(range.second) + ",";
    // llvm::outs() << "rangesStr: " << rangesStr << "\n";
    std::string programDir = FileManager::getParentDir(llvm::sys::fs::getMainExecutable(nullptr, nullptr));
    if (llvm::sys::ExecuteAndWait("/usr/bin/python3",
                                  {"/usr/bin/python3", programDir + "/apply-fix.py", "--remove", "--lines",
                                   rangesStr, outputFile, opt_debloated_file}) != 0) {
        llvm::errs() << "Failed to execute apply-fix.py (with args " << rangesStr << ")\n";
        exit(1);
    }

    return outputFile;
}

bool DeadcodeElementCollectionVisitor::VisitVarDecl(clang::VarDecl *VD) {
    if (clang::ParmVarDecl *PVD = llvm::dyn_cast<clang::ParmVarDecl>(VD))
        return true;
    else if (SourceManager::IsInHeader(Consumer->Context->getSourceManager(), VD))
        return true;

    Consumer->LocationMapping.insert(std::make_pair(VD, VD->getSourceRange()));

    // if varible is global or extern
    if (VD->hasGlobalStorage() || VD->hasExternalStorage())
        Consumer->allGlobalVarDecls.insert(VD);

    return true;
}

bool DeadcodeElementCollectionVisitor::VisitLabelStmt(clang::LabelStmt *LS) {
    Consumer->LocationMapping.insert(std::make_pair(LS->getDecl(), LS->getDecl()->getSourceRange()));
    return true;
}

bool DeadcodeElementCollectionVisitor::VisitDeclRefExpr(clang::DeclRefExpr *DRE) {
    auto D = DRE->getDecl();
    if (D == nullptr)
        return true;

    if (clang::VarDecl *VD = llvm::dyn_cast<clang::VarDecl>(D)) {
        if (VD->hasGlobalStorage()) {
            // add back the variable declaration and definition
            //   (1) `extern int x` & `x = 1`
            //   (2) `int x` & `x = 1`
            Consumer->varDeclIsUsed.insert(VD);
            Consumer->varDeclIsUsed.insert(VD->getCanonicalDecl());
            Consumer->varDeclIsUsed.insert(VD->getInitializingDeclaration()); // TODO: needed?
            Consumer->varDeclIsUsed.insert(VD->getDefinition());              // TODO: needed?
        }
    }

    return true;
}
