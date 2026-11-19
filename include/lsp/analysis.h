// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_ANALYSIS_H
#define BIBBLELSP_LSP_ANALYSIS_H

#include "lsp/document_store.h"
#include "lsp/json.h"
#include "lsp/module_index.h"

#include <BibbleC/scope/symbol.h>

#include <optional>

namespace bibblec::lsp {
    using json = nlohmann::json;

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

    enum class CompletionItemKind : uint32_t {
        Text = 1,
        Method = 2,
        Function = 3,
        Constructor = 4,
        Field = 5,
        Variable = 6,
        Class = 7,
        Interface = 8,
        Module = 9,
        Property = 10,
        Keyword = 14,
        Constant = 21
    };

    enum class InsertTextFormat : uint32_t {
        PlainText = 1,
        Snippet = 2
    };

    struct CompletionItem {
        std::string label;
        CompletionItemKind kind;

        std::optional<std::string> insertText;
        InsertTextFormat insertTextFormat = InsertTextFormat::PlainText;

        json serialize() const;
    };

    struct CompletionResult {
        std::vector<CompletionItem> items;
        bool isIncomplete = false;

        json serialize() const;
    };

    class Analysis {
    public:
        void analyze(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules);

        std::vector<std::string> getTriggerChars() const;

        std::optional<HoverInfo> getHover(const ParsedDocument& document, LspPosition position);
        std::optional<DefinitionInfo> getDefinition(const ParsedDocument& document, LspPosition position);

        CompletionResult complete(const ParsedDocument& document, LspPosition position, uint32_t triggerKind, std::optional<std::string> triggerChar);

    private:
        struct WordAtPosition {
            std::string_view text;
            LspRange range;
        };

        struct CompleteContext {
            const ParsedDocument& document;
            LspPosition position;
            uint32_t triggerKind;
            std::optional<std::string> triggerChar;

            WordAtPosition word;
        };

        void handleImports(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules);

        void completeKeywords(CompletionResult& result, const CompleteContext& ctx);
        void completeSymbols(CompletionResult& result, const CompleteContext& ctx);

        std::string formatSymbol(const scope::Symbol& symbol);

        WordAtPosition getWordAtPosition(const ParsedDocument& document, LspPosition position);

        parser::ASTNode* findNodeAtPosition(const ParsedDocument& document, LspPosition position);
        parser::ASTNode* findNodeAtPosition(parser::ASTNode* node, LspPosition position);

        scope::Symbol* resolveSymbolAtPosition(const ParsedDocument& document, LspPosition position);
    };
}

#endif //BIBBLELSP_LSP_ANALYSIS_H
