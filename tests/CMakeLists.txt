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
    test_placeholder.cpp
    ../src/editor/graphState.cpp
    ../src/editor/settings.cpp
)

target_include_directories(tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/dependencies/imgui-node-editor
    ${CMAKE_BINARY_DIR}/_deps/imgui-src
)

target_link_libraries(tests PRIVATE
    Catch2::Catch2WithMain
    nlohmann_json::nlohmann_json
)

include(CTest)
include(Catch)
catch_discover_tests(tests)