// Copyright 2026 Jannik Laugmand Bülow

#include "lsp/semantic_tokens.h"

namespace bibblec::lsp {
    std::optional<SemanticTokenType> ToSemanticTokenType(lexer::TokenType tokenType) {
        switch (tokenType) {
            // Keywords
            case lexer::TokenType::ModuleKeyword:
            case lexer::TokenType::ImportKeyword:
            case lexer::TokenType::NativeKeyword:
            case lexer::TokenType::ClassKeyword:
            case lexer::TokenType::NewKeyword:
            case lexer::TokenType::TrueKeyword:
            case lexer::TokenType::FalseKeyword:
            case lexer::TokenType::ReturnKeyword:
            case lexer::TokenType::IfKeyword:
            case lexer::TokenType::ElseKeyword:
            case lexer::TokenType::WhileKeyword:
            case lexer::TokenType::ForKeyword:
            case lexer::TokenType::ContinueKeyword:
            case lexer::TokenType::BreakKeyword:
            case lexer::TokenType::ConstKeyword:
                return SemanticTokenType::Keyword;

            // Types
            case lexer::TokenType::Type:
                return SemanticTokenType::Keyword;

            // Literals
            case lexer::TokenType::IntegerLiteral:
                return SemanticTokenType::Number;

            case lexer::TokenType::StringLiteral:
            case lexer::TokenType::CharacterLiteral:
                return SemanticTokenType::String;

            // Operators
            case lexer::TokenType::BangEqual:
            case lexer::TokenType::Percent:
            case lexer::TokenType::PercentEqual:
            case lexer::TokenType::Ampersand:
            case lexer::TokenType::DoubleAmpersand:
            case lexer::TokenType::AmpersandEqual:
            case lexer::TokenType::Star:
            case lexer::TokenType::StarEqual:
            case lexer::TokenType::Plus:
            case lexer::TokenType::PlusEqual:
            case lexer::TokenType::Minus:
            case lexer::TokenType::MinusEqual:
            case lexer::TokenType::Slash:
            case lexer::TokenType::SlashEqual:
            case lexer::TokenType::LessThan:
            case lexer::TokenType::DoubleLessThan:
            case lexer::TokenType::LessEqual:
            case lexer::TokenType::Equal:
            case lexer::TokenType::DoubleEqual:
            case lexer::TokenType::GreaterThan:
            case lexer::TokenType::GreaterEqual:
            case lexer::TokenType::DoubleGreaterThan:
            case lexer::TokenType::TripleGreaterThan:
            case lexer::TokenType::Caret:
            case lexer::TokenType::CaretEqual:
            case lexer::TokenType::Pipe:
            case lexer::TokenType::PipeEqual:
            case lexer::TokenType::DoublePipe:
            case lexer::TokenType::Tilde:
                return SemanticTokenType::Operator;

            // Identifiers require semantic analysis
            case lexer::TokenType::Identifier:
                return std::nullopt;

            // Punctuation
            case lexer::TokenType::LeftParen:
            case lexer::TokenType::RightParen:
            case lexer::TokenType::Comma:
            case lexer::TokenType::Dot:
            case lexer::TokenType::DoubleColon:
            case lexer::TokenType::Semicolon:
            case lexer::TokenType::LeftBracket:
            case lexer::TokenType::RightBracket:
            case lexer::TokenType::LeftBrace:
            case lexer::TokenType::RightBrace:
                return std::nullopt;

            // Not highlightable
            case lexer::TokenType::EndOfFile:
            case lexer::TokenType::Error:
                return std::nullopt;
        }

        return std::nullopt;
    }

    std::vector<uint32_t> EncodeTokens(const std::vector<lexer::Token>& tokens) {
        std::vector<uint32_t> data;
        data.reserve(tokens.size() * 5);

        uint32_t previousLine = 0;
        uint32_t previousStartChar = 0;

        auto emit = [&](uint32_t line, uint32_t column, uint32_t length, SemanticTokenType type) {
            if (length == 0)
                return;

            uint32_t deltaLine = line - previousLine;
            uint32_t deltaStartChar = deltaLine == 0 ? column - previousStartChar : column;

            data.push_back(deltaLine);
            data.push_back(deltaStartChar);
            data.push_back(length);
            data.push_back(static_cast<uint32_t>(type));
            data.push_back(0); // Token modifiers

            previousLine = line;
            previousStartChar = column;
        };

        for (const lexer::Token& token : tokens) {
            const auto semanticType = ToSemanticTokenType(token.getTokenType());

            if (!semanticType.has_value()) continue;

            auto start = token.getStartLocation();
            auto end = token.getEndLocation();

            uint32_t line = start.line - 1;
            uint32_t column = start.col - 1;
            uint32_t length = end.col - start.col + 1;

            emit(line, column, length, semanticType.value());
        }

        return data;
    }

    json MakeSemanticTokensResponse(const std::vector<uint32_t>& encoded) {
        return {
            {"resultId", "1"},
            {"data", encoded}
        };
    }

    json MakeSemanticTokenLegend() {
        return {
            {"tokenTypes", {
                "keyword",
                "variable",
                "namespace",
                "function",
                "number",
                "string",
                "type",
                "decorator",
                "enumMember",
                "comment",
                "parameter",
                "operator",
                "class"
            }},
            {"tokenModifiers", {
                "declaration",
                "definition",
                "readonly",
                "static",
                "deprecated",
                "defaultLibrary"
            }}
        };
    }
}
