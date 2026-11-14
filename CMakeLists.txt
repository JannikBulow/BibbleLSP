cmake_minimum_required(VERSION 3.26)
project(BibbleLSP)

include(FetchContent)

set(URIPARSER_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(URIPARSER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(URIPARSER_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(URIPARSER_BUILD_WCHAR_T OFF CACHE BOOL "" FORCE)
set(URIPARSER_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(
    uriparser
    GIT_REPOSITORY https://github.com/uriparser/uriparser.git
    GIT_TAG        uriparser-1.0.3
)
FetchContent_MakeAvailable(uriparser)

FetchContent_Declare(BibbleC
    GIT_REPOSITORY https://github.com/JannikBulow/BibbleC
    GIT_TAG master
)
FetchContent_MakeAvailable(BibbleC)


set(SOURCES
    src/dispatcher.cpp
    src/document_store.cpp
    src/main.cpp
    src/module_index.cpp
    src/semantic_tokens.cpp
    src/server.cpp
    src/transport.cpp
)

set(HEADERS
    include/lsp/dispatcher.h
    include/lsp/document_store.h
    include/lsp/json.h
    include/lsp/module_index.h
    include/lsp/semantic_tokens.h
    include/lsp/server.h
    include/lsp/transport.h
    include/lsp/uri.h
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

add_executable(BibbleLSP ${SOURCES} ${HEADERS})

target_include_directories(BibbleLSP PUBLIC include)

target_compile_features(BibbleLSP PUBLIC cxx_std_20)

target_link_libraries(BibbleLSP PRIVATE BibbleC-framework-static uriparser::uriparser)