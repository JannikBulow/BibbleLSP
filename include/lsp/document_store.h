// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_DOCUMENT_STORE_H
#define BIBBLELSP_LSP_DOCUMENT_STORE_H

#include "lsp/module_index.h"

#include <BibbleC/diagnostic/diagnostics.h>

#include <BibbleC/lexer/token.h>

#include <BibbleC/parser/ast/node.h>

#include <BibbleC/scope/scope.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

    class NullBuffer : public std::streambuf {
    protected:
        int overflow(int c) override {
            return traits_type::not_eof(c);
        }
    };

    extern NullBuffer nullBuffer;
    extern std::ostream nullStream;

    struct ParsedDocument {
        std::string uri;
        std::string text;
        std::string moduleName;
        scope::ScopePtr globalScope;
        std::vector<lexer::Token> tokens;
        std::vector<parser::ASTNodePtr> ast;
        bool importResolvedAst;
        size_t importedNodeCount;
        std::vector<Diagnostic> diagnostics;
        mutable diagnostic::Diagnostics diag;

        ParsedDocument();

        std::string_view getLine(uint32_t line) const;
    };

    class DocumentStore {
        friend struct ParsedDocument;
    public:
        ParsedDocument& open(std::string uri, std::string text);
        ParsedDocument& update(std::string uri, std::string newText);

        void close(const std::string& uri);

        ParsedDocument* get(const std::string& uri);

        ParsedDocument* loadFromDisk(const std::filesystem::path& path);

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
        std::unordered_set<std::string> mOpenDocuments;

        ParsedDocument& parse(std::string uri, std::string text);
    };

    LspPosition ToLspStartPosition(uint32_t lexerLine, uint32_t lexerColumn);
    LspPosition ToLspEndPosition(uint32_t lexerLine, uint32_t lexerColumn);
    LspRange TokenRange(const lexer::SourceLocation& start, const lexer::SourceLocation& end);
    LspRange TokenRange(const SourcePair& source);
    bool PositionInRange(LspPosition position, LspRange range);
    bool IsPositionBeforeOrEqual(LspPosition lhs, LspPosition rhs);
    bool IsSmallerRange(LspRange a, LspRange b);
}

#endif // BIBBLELSP_LSP_DOCUMENT_STORE_H
