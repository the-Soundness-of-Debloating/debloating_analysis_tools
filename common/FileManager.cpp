#include "FileManager.h"

#include <libgen.h>
#include <unistd.h>

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

std::string FileManager::readLink(const std::string &fileName) {
    std::string buffer(64, '\0');
    ssize_t len;
    while ((len = ::readlink(fileName.c_str(), &buffer[0], buffer.size())) ==
           static_cast<ssize_t>(buffer.size())) {
        buffer.resize(buffer.size() * 2);
    }
    if (len == -1) {
        return fileName;
    }
    buffer.resize(len);
    return buffer;
}

std::string FileManager::getParentDir(const std::string &fileName) {
    char *Cstr = new char[fileName.length() + 1];
    strcpy(Cstr, fileName.c_str());
    ::dirname(Cstr);
    std::string Dir(Cstr);
    delete[] Cstr;
    return Dir;
}

std::string FileManager::getBaseName(const std::string &fileName) {
    int Idx = fileName.rfind('/', fileName.length());
    if (Idx != std::string::npos) {
        return (fileName.substr(Idx + 1, fileName.length() - Idx));
    }
    return fileName;
}

std::string FileManager::getStemName(const std::string &fileName) {
    std::string base_filename = FileManager::getBaseName(fileName);
    int Idx = base_filename.rfind('.', base_filename.length());
    if (Idx != std::string::npos) {
        return (base_filename.substr(0, Idx));
    }
    return base_filename;
}

// std::vector<std::string> FileManager::getOutputFiles(const std::vector<std::string> &tempFiles) {
//     std::vector<std::string> outputFiles;
//     for (auto &tempFile : tempFiles) {
//         std::string outputFile = FileManager::getStemName(tempFile) + ".instru.c";
//         llvm::outs() << "Output instrumented source file to '" << outputFile << "'.\n";
//         llvm::sys::fs::copy_file(tempFile, outputFile);
//         llvm::sys::fs::remove(tempFile);
//         outputFiles.push_back(outputFile);
//     }
//     return outputFiles;
// }
