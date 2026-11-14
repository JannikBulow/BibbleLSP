// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_SERVER_H
#define BIBBLELSP_LSP_SERVER_H

#include "lsp/analysis.h"
#include "lsp/dispatcher.h"
#include "lsp/document_store.h"
#include "lsp/json.h"
#include "lsp/module_index.h"

#include <functional>
#include <string>

namespace bibblec::lsp {
    using json = nlohmann::json;

    enum class ServerState {
        Uninitialized,
        Initializing,
        Running,
        ShuttingDown
    };

    class Server {
    public:
        explicit Server(std::function<void(const std::string&)> send);

        void registerHandlers(Dispatcher& dispatcher);

        bool shouldExit() const { return mExit; }
        int exitCode() const { return mExitCode; }

    private:
        std::function<void(const std::string&)> mSend;
        std::vector<std::filesystem::path> mWorkspaceRoots;
        ModuleIndex mModuleIndex;
        DocumentStore mDocuments;
        Analysis mAnalysis;
        ServerState mState = ServerState::Uninitialized;
        bool mExit = false;
        int mExitCode = 1;

        json handleInitialize(const json& params);
        json handleSemanticTokensFull(const json& params);
        json handleShutdown(const json& params);
        json handleHover(const json& params);

        void handleInitialized(const json& params);
        void handleDidOpen(const json& params);
        void handleDidChange(const json& params);
        void handleDidClose(const json& params);
        void handleExit(const json& params);

        void publishDiagnostics(const ParsedDocument& doc);
    };
}

#endif // BIBBLELSP_LSP_SERVER_H
