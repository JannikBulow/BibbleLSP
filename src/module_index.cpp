// Copyright 2026 Jannik Laugmand Bülow

#include "lsp/document_store.h"
#include "lsp/module_index.h"

#include <BibbleC/lexer/lexer.h>
#include <BibbleC/lexer/token.h>

namespace bibblec::lsp {
    static std::string NormalizePath(const std::filesystem::path& path) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);

        if (ec) {
            canonical = path.lexically_normal();
        }

        return canonical.string();
    }

    void ModuleIndex::indexDirectory(const std::filesystem::path& root, DocumentStore& documents) {
        std::error_code ec;
        for (auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (ec) {
                ec.clear();
                continue;
            }

            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".bibl") continue;

            indexFile(entry.path(), documents);
        }
    }

    void ModuleIndex::indexFile(const std::filesystem::path& path, DocumentStore& documents) {
        removeFile(path);

        ParsedDocument* document = documents.loadFromDisk(path);

        if (!document) return;
        if (document->moduleName.empty()) return;

        const std::string normal = NormalizePath(path);

        mModules[document->moduleName].push_back(path);
        mFileModules[normal] = document->moduleName;
    }

    void ModuleIndex::removeFile(const std::filesystem::path& path) {
        const std::string normal = NormalizePath(path);

        auto fileIt = mFileModules.find(normal);
        if (fileIt == mFileModules.end()) return;

        const std::string& moduleName = fileIt->second;

        auto moduleIt = mModules.find(moduleName);
        if (moduleIt != mModules.end()) {
            auto& files = moduleIt->second;

            std::erase_if(files, [normal](const std::filesystem::path& existing) {
                return NormalizePath(existing) == normal;
            });

            if (files.empty()) {
                mModules.erase(moduleIt);
            }
        }

        mFileModules.erase(fileIt);
    }

    const std::vector<std::filesystem::path>* ModuleIndex::find(const std::string& moduleName) const {
        auto it = mModules.find(moduleName);
        if (it == mModules.end()) return nullptr;
        return &it->second;
    }
}
