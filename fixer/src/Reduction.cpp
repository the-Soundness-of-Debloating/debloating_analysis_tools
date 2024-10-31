#include "Reduction.h"
#include "SourceManager.h"

#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/Support/Program.h"

#include <queue>

std::vector<clang::Stmt *> Reduction::getAllChildren(clang::Stmt *S) {
    std::queue<clang::Stmt *> ToVisit;
    std::vector<clang::Stmt *> AllChildren;
    ToVisit.push(S);
    while (!ToVisit.empty()) {
        auto C = ToVisit.front();
        ToVisit.pop();
        AllChildren.emplace_back(C);
        for (auto const &Child : C->children()) {
            if (Child != NULL)
                ToVisit.push(Child);
        }
    }
    return AllChildren;
}

DDElement Reduction::getStartAndEnd(clang::SourceRange range) {
    const clang::SourceManager &SM = Context->getSourceManager();

    clang::SourceLocation Start = range.getBegin(), End = range.getEnd();

    if (Start.isMacroID())
        Start = SM.getFileLoc(Start);
    if (End.isMacroID())
        End = SM.getFileLoc(End);

    if (End.isInvalid() || Start.isInvalid())
        return std::make_pair(-1, -1);

    // a pair of start and end line numbers
    return std::make_pair(SM.getSpellingLineNumber(Start), SM.getSpellingLineNumber(End));
}
DDElement Reduction::getStartAndEnd(clang::Decl *decl) {
    if (decl == nullptr) {
        // Why will there be some decls that are null?
        // Because some function declarations are using definitions in other files (such as "extern printf").
        return std::make_pair(-1, -1);
    }

    return getStartAndEnd(decl->getSourceRange());
}
DDElement Reduction::getStartAndEnd(clang::Stmt *stmt) {
    if (stmt == nullptr) {
        return std::make_pair(-1, -1);
    }

    return getStartAndEnd(stmt->getSourceRange());
}

DDElementSet Reduction::toSet(const DDElementVector &Vec) {
    DDElementSet S(Vec.begin(), Vec.end());
    return S;
}
DDElementVector Reduction::toVector(const DDElementSet &Set) {
    DDElementVector Vec(Set.begin(), Set.end());
    return Vec;
}
DDElementSet Reduction::setDifference(const DDElementSet &A, const DDElementSet &B) {
    DDElementSet Result;
    std::set_difference(A.begin(), A.end(), B.begin(), B.end(), std::inserter(Result, Result.begin()));
    return Result;
}

bool Reduction::test(const DDElementVector &toAddBack) {
    llvm::Optional<llvm::StringRef> redirect_to_null[] = {llvm::None, llvm::StringRef("/dev/null"),
                                                          llvm::StringRef("/dev/null")};

    // replace ranges of lines in the debloated (temp) file with lines in the original file
    std::string temp_file = applyFixAndOutputToFile(toAddBack);
    std::string temp_bin_file = temp_file + ".out";
    if (temp_file.empty())
        return false;

    // compile and test the temp file
    bool success = false;
    int retcode;
    retcode =
        llvm::sys::ExecuteAndWait("/bin/bash", {"/bin/bash", opt_compile_script, temp_file, temp_bin_file},
                                  llvm::None, redirect_to_null);
    if (retcode < 0) {
        llvm::errs() << "Fatal error in running compile script.\n";
        llvm::errs() << "Using command: /bin/bash " << opt_compile_script << " " << temp_file << " "
                     << temp_bin_file << "\n";
        exit(1);
    }
    if (retcode == 0) {
        // check if it crashes (segmentation fault)

        // std::string cmd = "./" + temp_file + ".out " + opt_crash_args + " < " + opt_crash_input_file;
        // int retcode =
        //     llvm::sys::ExecuteAndWait("/bin/bash", {"/bin/bash", "-c", cmd}, llvm::None, redirect_to_null);

        if (opt_no_redir) {
            retcode = llvm::sys::ExecuteAndWait("/bin/bash", {"/bin/bash", opt_reproduce_script, temp_bin_file});
        } else {
            retcode = llvm::sys::ExecuteAndWait("/bin/bash", {"/bin/bash", opt_reproduce_script, temp_bin_file},
                                                llvm::None, redirect_to_null);
        }
        if (retcode < 0) {
            // a common reason is that the reproduce script does not use the real path of the binary file
            llvm::errs() << "Fatal error in running reproduce script.\n";
            llvm::errs() << "Using command: /bin/bash " << opt_reproduce_script << " " << temp_bin_file
                         << "\n";
            exit(1);
        }
        bool crash = ((retcode >= 131 && retcode <= 136) || retcode == 139);
        bool hang = (retcode == 124 || retcode == 137);
        success = !(crash || hang);

        // check if it passes other tests
        if (success && !opt_other_test_script.empty()) {
            // llvm::outs() << "---------------------------------Testing with other tests...\n";
            if (opt_no_redir) {
                retcode = llvm::sys::ExecuteAndWait("/bin/bash", {"/bin/bash", opt_other_test_script, temp_file});
            } else {
                retcode = llvm::sys::ExecuteAndWait("/bin/bash", {"/bin/bash", opt_other_test_script, temp_file},
                                                    llvm::None, redirect_to_null);
            }
            if (retcode < 0) {
                llvm::errs() << "Fatal error in running other test script.\n";
                llvm::errs() << "Using command: /bin/bash " << opt_other_test_script << " " << temp_file
                             << "\n";
                exit(1);
            }
            success = (retcode == 0);
        }
    }

    // remove temp files
    llvm::sys::fs::remove(temp_file);
    llvm::sys::fs::remove(temp_bin_file);

    return success;
}

std::vector<DDElementVector> Reduction::getCandidates(DDElementVector &Decls, int ChunkSize) {
    if (Decls.size() == 1)
        return {Decls};
    std::vector<DDElementVector> Result;
    int Partitions = Decls.size() / ChunkSize;
    for (int Idx = 0; Idx < Partitions; Idx++) {
        DDElementVector Target;
        Target.insert(Target.end(), Decls.begin() + Idx * ChunkSize, Decls.begin() + (Idx + 1) * ChunkSize);
        if (Target.size() > 0)
            Result.emplace_back(Target);
    }
    for (int Idx = 0; Idx < Partitions; Idx++) {
        DDElementVector Complement;
        Complement.insert(Complement.end(), Decls.begin(), Decls.begin() + Idx * ChunkSize);
        Complement.insert(Complement.end(), Decls.begin() + (Idx + 1) * ChunkSize, Decls.end());
        if (Complement.size() > 0)
            Result.emplace_back(Complement);
    }

    return Result;
}

extern bool reduction_dirty_flag;
DDElementVector Reduction::doDeltaDebugging(const DDElementVector &lineGroups) {
    std::set<DDElementVector> visited;
    DDElementVector lineGroupsToKeep = lineGroups;
    DDElementSet lineGroupsToRemove;

    // get the "fallback" result of this round of delta debugging (meaning all lines are added back)
    applyFixAndOutputToFile(toVector(lineGroupsToRemove), false);

    int chunkSize = (lineGroupsToKeep.size() + 1) / 2;
    llvm::outs() << "Running delta debugging - Size: " << lineGroupsToKeep.size() << "\n";

    while (lineGroupsToKeep.size() > 0) {
        bool success = false;
        auto candidates = getCandidates(lineGroupsToKeep, chunkSize);
        for (auto candidate : candidates) {
            if (std::find(visited.begin(), visited.end(), candidate) != visited.end()) {
                // llvm::outs() << "Cache hit.\n";
                continue;
            }
            visited.insert(candidate);

            // cumulative remove
            candidate.insert(candidate.end(), lineGroupsToRemove.begin(), lineGroupsToRemove.end());
            if (test(candidate)) {
                lineGroupsToKeep = toVector(setDifference(toSet(lineGroups), toSet(candidate)));
                lineGroupsToRemove.insert(candidate.begin(), candidate.end());
                success = true;
                break;
            }
        }
        if (success) {
            reduction_dirty_flag = true;
            llvm::outs() << "                Success - Size: " << lineGroupsToKeep.size() << "\n";
            // llvm::outs() << "Removed: ";
            // for (auto R : lineGroupsToRemove)
            //     llvm::outs() << R.first << "-" << R.second << " ";
            // llvm::outs() << "\n";
            chunkSize = (lineGroupsToKeep.size() + 1) / 2;
            // persist the intermediate result (in case of interruption)
            applyFixAndOutputToFile(toVector(lineGroupsToRemove), false);
        } else {
            if (chunkSize == 1)
                break;
            chunkSize = (chunkSize + 1) / 2;
        }
    }

    // get the "final" result of this round of delta debugging
    applyFixAndOutputToFile(toVector(lineGroupsToRemove), false);

    // remove removed lines in addedBackLines
    for (auto it = addedBackLines.begin(); it != addedBackLines.end();) {
        bool found = false;
        for (auto const &element : lineGroupsToRemove) {
            if (*it >= element.first && *it <= element.second) {
                it = addedBackLines.erase(it);
                found = true;
                break;
            }
        }
        if (!found)
            ++it;
    }

    return toVector(lineGroupsToRemove);
}
