# CMakeLists.txt for tests
# Simple UTC Daemon - Test Configuration
# Copyright 2024 SimpleDaemons

# Use FetchContent to get Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Test source files
set(TEST_SOURCES
    test_utc_packet.cpp
    test_utc_config.cpp
    test_logger.cpp
    test_main.cpp
)

# Create test executable
add_executable(simple_utcd_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(simple_utcd_tests
    PRIVATE
    gtest_main
    gtest
    ${CMAKE_THREAD_LIBS_INIT}
)

# Link against the main project sources
# We need to compile the source files for testing (excluding main.cpp)
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/*.cpp")
target_sources(simple_utcd_tests PRIVATE ${CORE_SOURCES})

# Include directories for the core sources
target_include_directories(simple_utcd_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Link OpenSSL and JSONCPP if enabled
if(ENABLE_SSL)
    target_link_libraries(simple_utcd_tests PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(ENABLE_JSON)
    target_link_libraries(simple_utcd_tests PRIVATE ${JSONCPP_LIBRARIES})
    target_include_directories(simple_utcd_tests PRIVATE ${JSONCPP_INCLUDE_DIRS})
    target_link_directories(simple_utcd_tests PRIVATE ${JSONCPP_LIBRARY_DIRS})
    target_compile_options(simple_utcd_tests PRIVATE ${JSONCPP_CFLAGS_OTHER})
endif()

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(simple_utcd_tests)

