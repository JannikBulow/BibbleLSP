// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_ANALYSIS_H
#define BIBBLELSP_LSP_ANALYSIS_H

#include "lsp/document_store.h"
#include "lsp/module_index.h"

namespace bibblec::lsp {
    class Analysis {
    public:
        void analyze(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules);

    private:
        void handleImports(ParsedDocument& document, DocumentStore& documents, const ModuleIndex& modules);
    };
}

#endif //BIBBLELSP_LSP_ANALYSIS_H
