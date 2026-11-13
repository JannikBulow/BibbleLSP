// Copyright 2026 Jannik Laugmand Bülow

#include "lsp/document_store.h"

#include <BibbleC/lexer/lexer.h>

#include <BibbleC/parser/parser.h>

#include <regex>

namespace bibblec::lsp {
    static LspPosition ToLspStartPosition(uint32_t lexerLine, uint32_t lexerColumn) {
        return {lexerLine > 0 ? lexerLine - 1 : 0, lexerColumn > 0 ? lexerColumn - 1 : 0} ;
    }

    static LspPosition ToLspEndPosition(uint32_t lexerLine, uint32_t lexerColumn) {
        return {lexerLine > 0 ? lexerLine - 1 : 0, lexerColumn};
    }

    static LspRange TokenRange(const lexer::SourceLocation& start, const lexer::SourceLocation& end) {
        return {ToLspStartPosition(start.line, start.col), ToLspEndPosition(end.line, end.col)};
    }

    const ParsedDocument& DocumentStore::open(std::string uri, std::string text) {
        return parse(std::move(uri), std::move(text));
    }

    const ParsedDocument& DocumentStore::update(std::string uri, std::string newText) {
        return parse(std::move(uri), std::move(newText));
    }

    void DocumentStore::close(const std::string& uri) {
        mDocuments.erase(uri);
    }

    const ParsedDocument* DocumentStore::get(const std::string& uri) {
        auto it = mDocuments.find(uri);
        if (it == mDocuments.end()) return nullptr;
        return &it->second;
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

    class NullBuffer : public std::streambuf {
    protected:
        int overflow(int c) override {
            return traits_type::not_eof(c);
        }
    };

    ParsedDocument& DocumentStore::parse(std::string uri, std::string text) {
        ParsedDocument& doc = mDocuments[uri];
        doc.uri = std::move(uri);
        doc.text = std::move(text);
        doc.text += '\n'; // I am too lazy to fix the lexer      ... that comment was from the basm lsp and i'm too scared to remove this line cuz idk if the bibblec lexer works without this
        doc.tokens.clear();
        doc.diagnostics.clear();

        NullBuffer nullBuffer;
        std::ostream nullStream(&nullBuffer);

        diagnostic::Diagnostics diag(nullStream, nullStream);
        diag.addListener(std::make_unique<ErrorListener>(doc));
        diag.addText(doc.uri, doc.text);

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

        doc.globalScope = std::make_unique<scope::Scope>(parseModuleName(doc, diag));

        std::vector<lexer::Token> tokensCopy = doc.tokens;
        parser::Parser parser(tokensCopy, diag, doc.globalScope.get(), "");
        parser.parse();

        return doc;
    }

    // copied from bibblec-compiler
    std::string DocumentStore::parseModuleName(ParsedDocument& doc, diagnostic::Diagnostics& diag) {
        auto& tokens = doc.tokens;

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
}
