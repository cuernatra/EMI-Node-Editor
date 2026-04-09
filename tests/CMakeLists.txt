include(FetchContent)

# Disable unnecessary Catch2 build steps
set(CATCH_INSTALL_DOCS OFF)
set(CATCH_INSTALL_EXTRAS OFF)
set(CATCH_BUILD_TESTING OFF)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests
    test_graph_state.cpp
    test_settings_placeholder.cpp
    test_serializer_placeholder.cpp
    test_graph_compiler.cpp
    ../src/editor/graph/graphState.cpp
    ../src/editor/settings.cpp
    ../src/core/compiler/graphCompiler.cpp
    ../src/core/compiler/pinResolver.cpp
    ../src/core/registry/nodeFactory.cpp
    ../src/core/registry/nodeRegistry.cpp
    ../src/core/registry/nodes/dataNodes.cpp
    ../src/core/registry/nodes/eventNodes.cpp
    ../src/core/registry/nodes/flowNodes.cpp
    ../src/core/registry/nodes/logicNodes.cpp
    ../src/core/registry/nodes/renderNodes.cpp
    ../src/core/registry/nodes/structNodes.cpp
    ../src/core/registry/nodes/demoNodes.cpp
    ../src/core/graphModel/pin.cpp
)

target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor
    ${CMAKE_SOURCE_DIR}/EMI-Script/src
    ${CMAKE_SOURCE_DIR}/EMI-Script/include
    ${CMAKE_BINARY_DIR}/_deps/unordered_dense-src/include
    ${CMAKE_BINARY_DIR}/_deps/imgui-src
)

target_link_libraries(tests PRIVATE
    Catch2::Catch2WithMain
    nlohmann_json::nlohmann_json
)

include(CTest)
include(Catch)
catch_discover_tests(tests)