// Copyright 2026 Jannik Laugmand Bülow

#include "lsp/analysis.h"
#include "lsp/uri.h"

#include <BibbleC/parser/ast/expression/call_expression.h>
#include <BibbleC/parser/ast/expression/variable_expression.h>

#include <BibbleC/parser/ast/global/import_statement.h>

#include <BibbleC/type/function_type.h>

#include <format>
#include <iostream>

namespace bibblec::lsp {
    static std::filesystem::path NormalizePath(const std::filesystem::path& path) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);

        if (ec) {
            canonical = path.lexically_normal();
        }

        return canonical;
    }

    json CompletionItem::serialize() const {
        json result = {
            {"label", label},
            {"kind", static_cast<uint32_t>(kind)}
        };

        if (insertText) {
            result["insertText"] = *insertText;
        }

        result["insertTextFormat"] = static_cast<uint32_t>(insertTextFormat);

        return result;
    }

    json CompletionResult::serialize() const {
        json jsonItems = json::array();

        for (const auto& item : items)
            jsonItems.push_back(item.serialize());

        return {
            {"isIncomplete", isIncomplete},
            {"items", std::move(jsonItems)}
        };
    }

    void Analysis::analyze(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules) {
        handleImports(document, documents, modules);

        bool exit = false;
        for (auto& node : document.ast) {
            node->typeCheck(document.diag, exit);
        }
    }

    std::vector<std::string> Analysis::getTriggerChars() const {
        return {":"};
    }

    std::optional<HoverInfo> Analysis::getHover(const ParsedDocument& document, LspPosition position) {
        scope::Symbol* symbol = resolveSymbolAtPosition(document, position);
        if (!symbol) return std::nullopt;

        return HoverInfo(symbol->type->isFunctionType() ? "function" : "variable", formatSymbol(*symbol), "", TokenRange(symbol->declarationSource));
    }

    std::optional<DefinitionInfo> Analysis::getDefinition(const ParsedDocument& document, LspPosition position) {
        scope::Symbol* symbol = resolveSymbolAtPosition(document, position);
        if (!symbol) return std::nullopt;

        return DefinitionInfo(PathToFileUri(symbol->declarationSource.start.file), TokenRange(symbol->declarationSource));
    }

    CompletionResult Analysis::complete(const ParsedDocument& document, LspPosition position, uint32_t triggerKind, std::optional<std::string> triggerChar) {
        CompletionResult result;
        CompleteContext ctx{document, position, triggerKind, std::move(triggerChar), getWordAtPosition(document, position)};

        completeKeywords(result, ctx);
        completeSymbols(result, ctx);

        return result;
    }

    void Analysis::handleImports(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules) {
        if (document.importResolvedAst) return;

        document.importedNodeCount = 0;

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
                        document.importedNodeCount++;
                    }
                }
            }
        }

        document.importResolvedAst = true;
    }

    void Analysis::completeKeywords(CompletionResult& result, const CompleteContext& ctx) {
        using namespace std::string_view_literals;

        struct Keyword {
            std::string_view name;
            std::optional<std::string_view> snippet;
        };

        static constexpr std::array keywords = {
            Keyword{"module", "module $0;"},
            Keyword{"import", "import $0;"},
            Keyword{"native", "native "},
            Keyword{"class", "class ${1:Name} {\n    $0\n}"},
            Keyword{"new", "new $0"},
            Keyword{"true"},
            Keyword{"false"},
            Keyword{"return"},
            Keyword{"if", "if (${1:condition}) {\n    $0\n}"},
            Keyword{"else", "else {\n    $0\n}"},
            Keyword{"while", "while (${1:condition}) {\n    $0\n}"},
            Keyword{"for", "for (${1:initializer}; ${2:condition}; ${3:increment}) {\n    $0\n}"},
            Keyword{"continue"},
            Keyword{"break"},
            Keyword{"const", "const "},
            Keyword{"auto", "auto "},
            Keyword{"void", "void "},
            Keyword{"byte", "byte "},
            Keyword{"short", "short "},
            Keyword{"int", "int "},
            Keyword{"long", "long "},
            Keyword{"ubyte", "ubyte "},
            Keyword{"ushort", "ushort "},
            Keyword{"uint", "uint "},
            Keyword{"ulong", "ulong "},
            Keyword{"char", "char "},
            Keyword{"bool", "bool "},
            Keyword{"string", "string "},
            Keyword{"handle", "handle "}
        };

        for (const auto keyword : keywords) {
            if (!keyword.name.starts_with(ctx.word.text))
                continue;

            CompletionItem item;
            item.label = std::string(keyword.name);
            item.kind = CompletionItemKind::Keyword;
            if (keyword.snippet) {
                item.insertText = *keyword.snippet;
                item.insertTextFormat = InsertTextFormat::Snippet;
            }

            result.items.push_back(std::move(item));
        }
    }

    //TODO: symbols from other modules
    void Analysis::completeSymbols(CompletionResult& result, const CompleteContext& ctx) {
        auto* node = findNodeAtPosition(ctx.document, {ctx.position.line, ctx.position.character - 1});
        if (!node) return;

        auto* scope = node->getScope();

        for (scope::Symbol* symbol : scope->getVisibleSymbols()) {
            if (!symbol->module.empty() && symbol->module != scope->getModuleName()) continue;
            if (!symbol->name.starts_with(ctx.word.text))
                continue;

            CompletionItem item;
            item.label = symbol->name;
            item.kind = symbol->type->isFunctionType() ? CompletionItemKind::Function : CompletionItemKind::Variable;

            result.items.push_back(std::move(item));
        }
    }

    std::string Analysis::formatSymbol(const scope::Symbol& symbol) {
        std::string name;
        if (!symbol.module.empty()) name = std::format("{}::{}", symbol.module, symbol.name);
        else name = symbol.name;

        if (symbol.type->isFunctionType()) {
            auto* functionType = static_cast<FunctionType*>(symbol.type);
            std::string result = std::format("{} {}(", functionType->getReturnType()->getName(), name);

            if (!functionType->getArgumentTypes().empty()) {
                for (auto it = functionType->getArgumentTypes().begin(); it != functionType->getArgumentTypes().end() - 1; ++it) {
                    Type* argument = *it;
                    result += argument->getName();
                    result += ", ";
                }
                result += functionType->getArgumentTypes().back()->getName();
            }

            result += ')';

            return result;
        }

        return std::format("{} {}", symbol.type->getName(), name);
    }

    Analysis::WordAtPosition Analysis::getWordAtPosition(const ParsedDocument& document, LspPosition position) {
        std::string_view line = document.getLine(position.line);

        size_t start = position.character;
        size_t end = position.character;

        auto isIdentifierChar = [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        };

        while (start > 0 && isIdentifierChar(line[start - 1])) {
            --start;
        }

        while (end < line.size() && isIdentifierChar(line[end])) {
            ++end;
        }

        return WordAtPosition{
            .text = std::string_view(line).substr(start, end - start),
            .range = {
                .start = {position.line, static_cast<uint32_t>(start)},
                .end = {position.line, static_cast<uint32_t>(end)}
            }
        };
    }

    parser::ASTNode* Analysis::findNodeAtPosition(const ParsedDocument& document, LspPosition position) {
        for (size_t i = document.importedNodeCount; i < document.ast.size(); i++) {
            auto& node = document.ast[i];

            if (auto* result = findNodeAtPosition(node.get(), position)) {
                return result;
            }
        }

        return nullptr;
    }

    parser::ASTNode* Analysis::findNodeAtPosition(parser::ASTNode* node, LspPosition position) {
        if (!node) return nullptr;

        parser::ASTNode* bestMatch = nullptr;

        for (auto* child : node->getChildren()) {
            auto* match = findNodeAtPosition(child, position);

            if (!match) continue;

            if (auto* call = dynamic_cast<parser::CallExpression*>(node)) {
                if (call->getCallee() == match) {
                    bestMatch = node;
                    continue;
                }
            }

            if (!bestMatch || IsSmallerRange(TokenRange(match->getSource()), TokenRange(bestMatch->getSource()))) {
                bestMatch = match;
            }
        }

        auto range = TokenRange(node->getSource());

        if (PositionInRange(position, range) && (!bestMatch || IsSmallerRange(range, TokenRange(bestMatch->getSource())))) {
            bestMatch = node;
        }

        return bestMatch;
    }

    scope::Symbol* Analysis::resolveSymbolAtPosition(const ParsedDocument& document, LspPosition position) {
        auto* node = findNodeAtPosition(document, position);

        scope::Symbol* symbol = nullptr;
        if (auto* variable = dynamic_cast<parser::VariableExpression*>(node)) {
            symbol = variable->resolveSymbol();
        } else if (auto* call = dynamic_cast<parser::CallExpression*>(node)) {
            symbol = call->getBestViableFunction(document.diag);
        }

        if (!symbol || !symbol->type) return nullptr;

        return symbol;
    }
}
