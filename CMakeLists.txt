cmake_minimum_required(VERSION 3.26)
project(BibbleLSP)

include(FetchContent)

FetchContent_Declare(BibbleC
    GIT_REPOSITORY https://github.com/JannikBulow/BibbleC
    GIT_TAG master
)
FetchContent_MakeAvailable(BibbleC)


set(SOURCES
    src/dispatcher.cpp
    src/document_store.cpp
    src/main.cpp
    src/semantic_tokens.cpp
    src/server.cpp
    src/transport.cpp
)

set(HEADERS
    include/lsp/dispatcher.h
    include/lsp/document_store.h
    include/lsp/json.h
    include/lsp/semantic_tokens.h
    include/lsp/server.h
    include/lsp/transport.h
)

source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})

add_executable(BibbleLSP ${SOURCES} ${HEADERS})

target_include_directories(BibbleLSP PUBLIC include)

target_compile_features(BibbleLSP PUBLIC cxx_std_20)

target_link_libraries(BibbleLSP PRIVATE BibbleC-framework-static)