// Copyright 2026 Jannik Laugmand Bülow

#include "lsp/analysis.h"
#include "lsp/uri.h"

#include <BibbleC/parser/ast/global/import_statement.h>

namespace bibblec::lsp {
    static std::filesystem::path NormalizePath(const std::filesystem::path& path) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);

        if (ec) {
            canonical = path.lexically_normal();
        }

        return canonical;
    }

    void Analysis::analyze(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules) {
        handleImports(document, documents, modules);

        bool exit = false;
        for (auto& node : document.ast) {
            node->typeCheck(document.diag, exit);
        }
    }

    void Analysis::handleImports(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules) {
        if (document.importResolvedAst) return;

        std::vector<std::string> moduleNames{document.moduleName};

        for (const auto& node : document.ast) {
            auto* import = dynamic_cast<parser::ImportStatement*>(node.get());

            if (!import) continue;

            std::string moduleName;

            for (const auto& name : import->getModule()) {
                moduleName += name;
                moduleName += '.';
            }

            if (!moduleName.empty()) moduleName.pop_back();

            moduleNames.push_back(std::move(moduleName));
        }

        auto currentPath = NormalizePath(UriToPath(document.uri));

        for (const auto& moduleName : moduleNames) {
            const auto* files = modules.find(moduleName);

            if (!files) {
                continue;
            }

            for (const auto& file : *files) {
                if (NormalizePath(file) == currentPath) continue;

                ParsedDocument* imported = documents.loadFromDisk(file);

                if (!imported) continue;

                for (const auto& node : imported->ast) {
                    if (auto cloned = node->cloneExternal(document.globalScope.get())) {
                        document.ast.insert(document.ast.begin(), std::move(cloned));
                    }
                }
            }
        }

        document.importResolvedAst = true;
    }
}
