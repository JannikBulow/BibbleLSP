// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_SEMANTIC_TOKENS_H
#define BIBBLELSP_LSP_SEMANTIC_TOKENS_H

#include "lsp/json.h"

#include <BibbleC/lexer/token.h>

#include <cstdint>
#include <string>
#include <vector>

namespace bibblec::lsp {
    using json = nlohmann::json;

    enum class SemanticTokenType : uint32_t {
        Keyword = 0,    // Language keywords: module, import, return, if, while, etc.
        Variable = 1,   // Variable declarations and references
        Namespace = 2,  // Module and namespace identifiers
        Function = 3,   // Function declarations and references
        Number = 4,     // Numeric literals
        String = 5,     // String and character literals
        Type = 6,       // Built-in and user-defined types
        Decorator = 7,  // Attributes: noreturn, etc.
        EnumMember = 8, // Enum constants and members
        Comment = 9,    // Line and block comments
        Parameter = 10, // Function parameters and references
        Operator = 11,  // Arithmetic, logical, comparison, and assignment operators
        Class = 12,     // Class declarations and references
    };

    std::optional<SemanticTokenType> ToSemanticTokenType(lexer::TokenType tokenType);

    std::vector<uint32_t> EncodeTokens(const std::vector<lexer::Token>& tokens);

    json MakeSemanticTokensResponse(const std::vector<uint32_t>& encoded);

    json MakeSemanticTokenLegend();
}

#endif // BIBBLELSP_LSP_SEMANTIC_TOKENS_H
