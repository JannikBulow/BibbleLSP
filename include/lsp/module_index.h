// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_MODULE_INDEX_H
#define BIBBLELSP_LSP_MODULE_INDEX_H

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace bibblec::lsp {
    class DocumentStore;

    class ModuleIndex {
    public:
        void indexDirectory(const std::filesystem::path& root, DocumentStore& documents);

        void indexFile(const std::filesystem::path& path, DocumentStore& documents);
        void removeFile(const std::filesystem::path& path);

        const std::vector<std::filesystem::path>* find(const std::string& moduleName) const;

    private:
        std::unordered_map<std::string, std::vector<std::filesystem::path>> mModules;
        std::unordered_map<std::string, std::string> mFileModules;
    };
}

#endif //BIBBLELSP_LSP_MODULE_INDEX_H
