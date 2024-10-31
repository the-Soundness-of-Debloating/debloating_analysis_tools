#ifndef INSTRU_FILE_MANAGER_H
#define INSTRU_FILE_MANAGER_H

#include <string>
#include <vector>

/// \brief Wrapper for low-level file manipulations
class FileManager {
   public:
    static std::string readLink(const std::string &fileName);
    static std::string getParentDir(const std::string &fileName);
    static std::string getBaseName(const std::string &fileName);
    static std::string getStemName(const std::string &fileName);

    // static std::vector<std::string> getOutputFiles(const std::vector<std::string> &tempFiles);
};

#endif  // INSTRU_FILE_MANAGER_H
