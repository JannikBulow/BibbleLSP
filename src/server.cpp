// Copyright 2026 Jannik Laugmand Bülow

#include "lsp/semantic_tokens.h"
#include "lsp/server.h"
#include "lsp/uri.h"

namespace bibblec::lsp {
    Server::Server(std::function<void(const std::string&)> send)
        : mSend(std::move(send)) {}

    void Server::registerHandlers(Dispatcher& dispatcher) {
        dispatcher.onRequest("initialize", [this](const json& p) -> std::optional<json> {
            return handleInitialize(p);
        });

        dispatcher.onRequest("textDocument/semanticTokens/full", [this](const json& p) -> std::optional<json> {
            if (mState != ServerState::Running) return std::nullopt;
            return handleSemanticTokensFull(p);
        });

        dispatcher.onRequest("shutdown", [this](const json& p) -> std::optional<json> {
            return handleShutdown(p);
        });

        dispatcher.onRequest("textDocument/hover", [this](const json& p) {
            return handleHover(p);
        });

        dispatcher.onRequest("textDocument/definition", [this](const json& p) {
            return handleDefinition(p);
        });


        dispatcher.onNotification("initialized", [this](const json& p) {
            handleInitialized(p);
        });

        dispatcher.onNotification("textDocument/didOpen", [this](const json& p) {
            if (mState != ServerState::Running) return;
            handleDidOpen(p);
        });

        dispatcher.onNotification("textDocument/didChange", [this](const json& p) {
            if (mState != ServerState::Running) return;
            handleDidChange(p);
        });

        dispatcher.onNotification("textDocument/didClose", [this](const json& p) {
            if (mState != ServerState::Running) return;
            handleDidClose(p);
        });

        dispatcher.onNotification("exit", [this](const json& p) {
            handleExit(p);
        });

        dispatcher.onNotification("$/cancelRequest", [](const json&) {});

        dispatcher.onNotification("$/setTrace", [](const json&) {});

        dispatcher.onNotification("$/logTrace", [](const json&) {});
    }

    json Server::handleInitialize(const json& params) {
        mState = ServerState::Initializing;

        if (params.contains("workspaceFolders") && !params["workspaceFolders"].is_null()) {
            for (const auto& folder : params["workspaceFolders"]) {
                std::string uri = folder["uri"];
                mWorkspaceRoots.push_back(UriToPath(uri));
            }
        } else if (params.contains("rootUri") && !params["rootUri"].is_null()) {
            std::string uri = params["rootUri"];
            mWorkspaceRoots.push_back(UriToPath(uri));
        }

        return {
            {"serverInfo", {
                {"name", "bibble-lsp"},
                {"version", "0.1.0"}
            }},
            {"capabilities", {
                {"textDocumentSync", 1},
                {"hoverProvider", true},
                {"definitionProvider", true},

                {"semanticTokensProvider", {
                    {"legend", MakeSemanticTokenLegend()},
                    {"full", true},
                    {"range", false}
                }}
            }}
        };
    }

    json Server::handleSemanticTokensFull(const json& params) {
        std::string uri = params["textDocument"]["uri"].get<std::string>();

        const ParsedDocument* doc = mDocuments.get(uri);
        if (!doc) {
            return MakeSemanticTokensResponse({});
        }

        auto encoded = EncodeTokens(doc->tokens);
        return MakeSemanticTokensResponse(encoded);
    }

    json Server::handleShutdown(const json& params) {
        mState = ServerState::ShuttingDown;
        mExitCode = 0;
        return nullptr;
    }

    json Server::handleHover(const json& params) {
        std::string uri = params["textDocument"]["uri"].get<std::string>();
        LspPosition position(params["position"]["line"].get<uint32_t>(), params["position"]["character"].get<uint32_t>());

        auto* document = mDocuments.get(uri);
        std::optional<HoverInfo> hover;

        if (document) {
            hover = mAnalysis.getHover(*document, position);
        }

        json result;
        if (hover) {
            result = {
                {"contents", {
                    {
                        {"kind", "markdown"},
                        {"value", hover->title}
                    },
                    {
                        {"language", "bibble"},
                        {"value", hover->code}
                    },
                    {
                        {"kind", "markdown"},
                        {"value", hover->description}
                    }
                }}
            };

            if (hover->range) {
                result["range"] = {
                    {"start", {
                        {"line", hover->range->start.line},
                        {"character", hover->range->start.character}
                    }},
                    {"end", {
                        {"line", hover->range->end.line},
                        {"character", hover->range->end.character}
                    }}
                };
            }
        }

        return result;
    }

    json Server::handleDefinition(const json& params) {
        std::string uri = params["textDocument"]["uri"].get<std::string>();
        LspPosition position(params["position"]["line"].get<uint32_t>(), params["position"]["character"].get<uint32_t>());

        auto* document = mDocuments.get(uri);
        std::optional<DefinitionInfo> definition;

        if (document) {
            definition = mAnalysis.getDefinition(*document, position);
        }

        json result;
        if (definition) {
            result = {
                {"uri", definition->uri},
                {"range", {
                    {"start", {
                        {"line", definition->range.start.line},
                        {"character", definition->range.start.character}
                    }},
                    {"end", {
                        {"line", definition->range.end.line},
                        {"character", definition->range.end.character}
                    }}
                }}
            };
        }

        return result;
    }

    void Server::handleInitialized(const json& params) {
        for (const auto& root : mWorkspaceRoots) {
            mModuleIndex.indexDirectory(root, mDocuments);
        }

        mState = ServerState::Running;
    }

    void Server::handleDidOpen(const json& params) {
        std::string uri = params["textDocument"]["uri"].get<std::string>();
        std::string text = params["textDocument"]["text"].get<std::string>();

        ParsedDocument& doc = mDocuments.open(std::move(uri), std::move(text));

        mModuleIndex.indexFile(UriToPath(doc.uri), mDocuments);

        mAnalysis.analyze(doc, mDocuments, mModuleIndex);

        publishDiagnostics(doc);
    }

    void Server::handleDidChange(const json& params) {
        std::string uri = params["textDocument"]["uri"].get<std::string>();

        const json& changes = params["contentChanges"];
        if (changes.empty()) return;

        std::string newText = changes.back()["text"].get<std::string>();

        ParsedDocument& doc = mDocuments.update(std::move(uri), std::move(newText));

        mModuleIndex.indexFile(UriToPath(doc.uri), mDocuments);

        mAnalysis.analyze(doc, mDocuments, mModuleIndex);

        publishDiagnostics(doc);
    }

    void Server::handleDidClose(const json& params) {
        std::string uri = params["textDocument"]["uri"].get<std::string>();
        mDocuments.close(uri);

        mSend(Dispatcher::MakeNotification("textDocument/publishDiagnostics", {
            {"uri", std::move(uri)},
            {"diagnostics", json::array()}
        }));
    }

    void Server::handleExit(const json& params) {
        mExit = true;
    }

    void Server::publishDiagnostics(const ParsedDocument& doc) {
        json diagArray = json::array();

        for (const Diagnostic& diag : doc.diagnostics) {
            diagArray.push_back({
                {"range", {
                    {"start", {
                        {"line", diag.range.start.line},
                        {"character", diag.range.start.character},
                    }},
                    {"end", {
                        {"line", diag.range.end.line},
                        {"character", diag.range.end.character},
                    }}
                }},
                {"severity", diag.severity},
                {"message", diag.message},
                {"source", "bibble-lsp"}
            });
        }

        mSend(Dispatcher::MakeNotification("textDocument/publishDiagnostics", {
            {"uri", doc.uri},
            {"diagnostics", std::move(diagArray)}
        }));
    }
}
