// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_ANALYSIS_H
#define BIBBLELSP_LSP_ANALYSIS_H

#include "lsp/document_store.h"
#include "lsp/module_index.h"

#include <BibbleC/scope/symbol.h>

#include <optional>

namespace bibblec::lsp {
    struct HoverInfo {
        std::string title;
        std::string code;
        std::string description;
        std::optional<LspRange> range;
    };

    struct DefinitionInfo {
        std::string uri;
        LspRange range;
    };

    enum class CompletionKind {
        Variable,
        Function,
        Type,
        Module,
        Keyword
    };

    struct CompletionCandidate {
        std::string label;
        CompletionKind kind;
        std::string detail;
        std::string insertText;
    };

    class Analysis {
    public:
        void analyze(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules);

        std::optional<HoverInfo> getHover(const ParsedDocument& document, LspPosition position);
        std::optional<DefinitionInfo> getDefinition(const ParsedDocument& document, LspPosition position);

    private:
        void handleImports(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules);

        std::string formatSymbol(const scope::Symbol& symbol);

        parser::ASTNode* findNodeAtPosition(const ParsedDocument& document, LspPosition position);
        parser::ASTNode* findNodeAtPosition(parser::ASTNode* node, LspPosition position);

        scope::Symbol* resolveSymbolAtPosition(const ParsedDocument& document, LspPosition position);
    };
}

#endif //BIBBLELSP_LSP_ANALYSIS_H
