// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_ANALYSIS_H
#define BIBBLELSP_LSP_ANALYSIS_H

#include "lsp/document_store.h"
#include "lsp/module_index.h"

#include <BibbleC/scope/symbol.h>

#include <optional>

namespace bibblec::lsp {
    struct HoverInfo {
        std::string contents;
        std::optional<LspRange> range;
    };

    class Analysis {
    public:
        void analyze(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules);

        std::optional<HoverInfo> getHover(const ParsedDocument& document, LspPosition position);

    private:
        void handleImports(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules);

        std::string formatSymbol(const scope::Symbol& symbol);

        const lexer::Token* findTokenAt(const ParsedDocument& document, LspPosition position);
        parser::ASTNode* findNodeAtPosition(const ParsedDocument& document, LspPosition position);
        parser::ASTNode* findNodeAtPosition(parser::ASTNode* node, LspPosition position);
    };
}

#endif //BIBBLELSP_LSP_ANALYSIS_H
