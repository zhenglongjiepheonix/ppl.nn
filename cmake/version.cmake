file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION __PPLNN_VERSION_STR__)
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" __PPLNN__UNUSED__ ${__PPLNN_VERSION_STR__})

set(PPLNN_VERSION_MAJOR ${CMAKE_MATCH_1})
set(PPLNN_VERSION_MINOR ${CMAKE_MATCH_2})
set(PPLNN_VERSION_PATCH ${CMAKE_MATCH_3})

list(APPEND PPLNN_COMPILE_DEFINITIONS
    PPLNN_VERSION_MAJOR=${PPLNN_VERSION_MAJOR}
    PPLNN_VERSION_MINOR=${PPLNN_VERSION_MINOR}
    PPLNN_VERSION_PATCH=${PPLNN_VERSION_PATCH})

hpcc_get_git_info(
    GIT_HASH_OUTPUT PPLNN_COMMIT_HASH
    GIT_TAG_OUTPUT PPLNN_COMMIT_TAG)

set(__PPLNN_COMMIT_ID_SRC__ ${CMAKE_CURRENT_BINARY_DIR}/generated/commit.cc)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ppl/nn/utils/commit.cc.in
    ${__PPLNN_COMMIT_ID_SRC__}
    @ONLY)

list(APPEND PPLNN_SOURCES ${__PPLNN_COMMIT_ID_SRC__})

unset(__PPLNN_COMMIT_ID_SRC__)
unset(__PPLNN__UNUSED__)
unset(__PPLNN_VERSION_STR__)
