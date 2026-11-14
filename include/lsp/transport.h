// Copyright 2026 Jannik Laugmand Bülow

#ifndef BIBBLELSP_LSP_TRANSPORT_H
#define BIBBLELSP_LSP_TRANSPORT_H

#include <iostream>
#include <stdexcept>
#include <string>

namespace bibblec::lsp {
    struct TransportEOF : std::exception {};
    struct TransportError : std::runtime_error { using std::runtime_error::runtime_error; };

    class Transport {
    public:
        Transport(std::istream& in, std::ostream& out);

        std::string readMessage();
        void sendMessage(std::string_view message);

    private:
        std::istream& mIn;
        std::ostream& mOut;

        std::string readLine();
    };
}

#endif // BIBBLELSP_LSP_TRANSPORT_H
