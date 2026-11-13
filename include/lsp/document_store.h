// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLEASM_LSP_DOCUMENT_STORE_H
#define BIBBLEASM_LSP_DOCUMENT_STORE_H 1

#include <BibbleC/diagnostic/diagnostics.h>

#include <BibbleC/lexer/token.h>

#include <BibbleC/scope/scope.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bibblec::lsp {
    struct LspPosition {
        uint32_t line; // 0 indexed
        uint32_t character; // 0 indexed utf16 code unit offset, though we don't use utf16 in basm so it actually refers to an ascii character index
    };

    struct LspRange {
        LspPosition start;
        LspPosition end;
    };

    struct Diagnostic {
        LspRange range;
        int severity; // 1=error, 2=warning, 3=info, 4=hint
        std::string message;
    };

    struct ParsedDocument {
        std::string uri;
        std::string text;
        scope::ScopePtr globalScope;
        std::vector<lexer::Token> tokens;
        std::vector<Diagnostic> diagnostics;
    };

    class DocumentStore {
    public:
        const ParsedDocument& open(std::string uri, std::string text);
        const ParsedDocument& update(std::string uri, std::string newText);

        void close(const std::string& uri);

        const ParsedDocument* get(const std::string& uri);

    private:
        class ErrorListener : public diagnostic::IListener {
        public:
            explicit ErrorListener(ParsedDocument& doc);

            void onCompilerError(lexer::SourceLocation start, lexer::SourceLocation end, std::string_view message, std::string_view fileName, std::string_view fileText) override;
            void onCompilerWarning(lexer::SourceLocation start, lexer::SourceLocation end, std::string_view message, std::string_view fileName, std::string_view fileText) override;

        private:
            ParsedDocument& mDoc;
        };

        std::unordered_map<std::string, ParsedDocument> mDocuments;

        ParsedDocument& parse(std::string uri, std::string text);
        std::string parseModuleName(ParsedDocument& doc, diagnostic::Diagnostics& diag);
    };
}

#endif // BIBBLEASM_LSP_DOCUMENT_STORE_H
