// Copyright 2026 Jannik Laugmand Bülow

#include "lsp/document_store.h"
#include "lsp/uri.h"

#include <BibbleC/lexer/lexer.h>

#include <BibbleC/parser/parser.h>

#include <fstream>
#include <regex>

namespace bibblec::lsp {
    // copied from bibblec-compiler
    static std::string ExtractModuleName(const std::vector<lexer::Token>& tokens, diagnostic::Diagnostics& diag) {
        if (tokens[0].getTokenType() == lexer::TokenType::ModuleKeyword) {
            if (tokens[1].getTokenType() == lexer::TokenType::Identifier) {
                int pos = 1;
                std::string moduleName;
                while (tokens[pos].getTokenType() == lexer::TokenType::Identifier) {
                    moduleName += tokens[pos++].getText();
                    if (tokens[pos].getTokenType() == lexer::TokenType::Semicolon) break;

                    if (tokens[pos].getTokenType() == lexer::TokenType::Dot) {
                        pos++;
                        moduleName += '.';
                    }
                }
                return moduleName;
            } else {
                diag.reportCompilerError(
                    tokens[1].getStartLocation(),
                    tokens[1].getEndLocation(),
                    std::format("expected module name after '{}module{}' keyword", fmt::bold, fmt::reset));
            }
        } else {
            diag.reportCompilerError(
                    tokens[0].getStartLocation(),
                    tokens[0].getEndLocation(),
                    std::format("expected '{}module{}' keyword", fmt::bold, fmt::reset));
        }

        return "<error>";
    }

    LspPosition ToLspStartPosition(uint32_t line, uint32_t col) {
        return {line - 1, col - 1};
    }

    LspPosition ToLspEndPosition(uint32_t line, uint32_t col) {
        return {line - 1, col - 1};
    }

    LspRange TokenRange(const lexer::SourceLocation& start, const lexer::SourceLocation& end) {
        return {ToLspStartPosition(start.line, start.col), ToLspEndPosition(end.line, end.col)};
    }

    LspRange TokenRange(const SourcePair& source) {
        return TokenRange(source.start, source.end);
    }

    bool PositionInRange(LspPosition position, LspRange range) {
        const auto& start = range.start;
        const auto& end = range.end;

        if (position.line < start.line || position.line > end.line) return false;
        if (position.line == start.line &&  position.character < start.character) return false;
        if (position.line == end.line && position.character >= end.character) return false;

        return true;
    }

    bool IsSmallerRange(LspRange a, LspRange b) {
        if (a.start.line != b.start.line) return a.start.line > b.start.line;
        if (a.start.character != b.start.character) return a.start.character > b.start.character;
        if (a.end.line != b.end.line) return a.end.line < b.end.line;
        return a.end.character < b.end.character;
    }

    NullBuffer nullBuffer;
    std::ostream nullStream(&nullBuffer);

    ParsedDocument::ParsedDocument()
        : diag(nullStream, nullStream) {
        diag.addListener(std::make_unique<DocumentStore::ErrorListener>(*this));
    }

    ParsedDocument& DocumentStore::open(std::string uri, std::string text) {
        mOpenDocuments.insert(uri);
        return parse(std::move(uri), std::move(text));
    }

    ParsedDocument& DocumentStore::update(std::string uri, std::string newText) {
        mOpenDocuments.insert(uri);
        return parse(std::move(uri), std::move(newText));
    }

    void DocumentStore::close(const std::string& uri) {
        mOpenDocuments.erase(uri);
        mDocuments.erase(uri);
    }

    ParsedDocument* DocumentStore::get(const std::string& uri) {
        auto it = mDocuments.find(uri);
        if (it == mDocuments.end()) return nullptr;
        return &it->second;
    }

    ParsedDocument* DocumentStore::loadFromDisk(const std::filesystem::path& path) {
        const std::string uri = PathToFileUri(path);

        if (mOpenDocuments.contains(uri)) {
            auto it = mDocuments.find(uri);

            if (it != mDocuments.end()) return &it->second;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) return nullptr;

        std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

        return &parse(uri, std::move(text));
    }

    DocumentStore::ErrorListener::ErrorListener(ParsedDocument& doc)
        : mDoc(doc) {}

    static const std::regex ansi(R"(\x1B\[[0-?]*[ -/]*[@-~])");

    void DocumentStore::ErrorListener::onCompilerError(lexer::SourceLocation start, lexer::SourceLocation end, std::string_view message, std::string_view fileName, std::string_view fileText) {
        mDoc.diagnostics.push_back({
            .range = TokenRange(start, end),
            .severity = 1,
            .message = std::regex_replace(std::string(message), ansi, "")
        });
    }

    void DocumentStore::ErrorListener::onCompilerWarning(lexer::SourceLocation start, lexer::SourceLocation end, std::string_view message, std::string_view fileName, std::string_view fileText) {
        mDoc.diagnostics.push_back({
            .range = TokenRange(start, end),
            .severity = 2,
            .message = std::regex_replace(std::string(message), ansi, "")
        });
    }

    ParsedDocument& DocumentStore::parse(std::string uri, std::string text) {
        ParsedDocument& doc = mDocuments[uri];
        doc.uri = std::move(uri);
        doc.text = std::move(text);
        doc.text += '\n'; // I am too lazy to fix the lexer      ... that comment was from the basm lsp and i'm too scared to remove this line cuz idk if the bibblec lexer works without this
        doc.tokens.clear();
        doc.diagnostics.clear();

        doc.diag.addText(doc.uri, doc.text);

        lexer::Lexer lexer(doc.text, doc.uri);
        doc.tokens = lexer.lex();

        for (const lexer::Token& token : doc.tokens) {
            if (token.getTokenType() == lexer::TokenType::Error) {
                doc.diagnostics.push_back({
                    .range = TokenRange(token.getStartLocation(), token.getEndLocation()),
                    .severity = 1,
                    .message = "unexpected token: '" + std::string(token.getText()) + "'"
                });
            }
        }

        doc.moduleName = ExtractModuleName(doc.tokens, doc.diag);
        doc.globalScope = std::make_unique<scope::Scope>(doc.moduleName);

        std::vector<lexer::Token> tokensCopy = doc.tokens;
        parser::Parser parser(tokensCopy, doc.diag, doc.globalScope.get(), "");
        doc.ast = parser.parse();
        doc.importResolvedAst = false;

        return doc;
    }
}
