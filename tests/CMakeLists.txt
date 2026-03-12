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
)

target_link_libraries(tests PRIVATE
    Catch2::Catch2WithMain
)

add_test(NAME tests COMMAND tests)