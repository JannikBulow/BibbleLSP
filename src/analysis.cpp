// Copyright 2026 Jannik Laugmand Bülow

#include "lsp/analysis.h"
#include "lsp/uri.h"

#include <BibbleC/parser/ast/expression/call_expression.h>
#include <BibbleC/parser/ast/expression/variable_expression.h>

#include <BibbleC/parser/ast/global/import_statement.h>

#include <BibbleC/type/function_type.h>

#include <format>

namespace bibblec::lsp {
    static std::filesystem::path NormalizePath(const std::filesystem::path& path) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);

        if (ec) {
            canonical = path.lexically_normal();
        }

        return canonical;
    }

    void Analysis::analyze(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules) {
        handleImports(document, documents, modules);

        bool exit = false;
        for (auto& node : document.ast) {
            node->typeCheck(document.diag, exit);
        }
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
