cmake_minimum_required(VERSION 3.10)
project(webagent)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WEBAGENT_CORE_SOURCES
    src/agent.cpp
    src/agent_video_utils.cpp
    src/screamer_open.cpp
    src/video_loader.cpp
    src/frontend_launcher.cpp
    src/local_control_http_utils.cpp
    src/local_control_server.cpp
)

add_library(webagent_core ${WEBAGENT_CORE_SOURCES})
target_include_directories(webagent_core PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(webagent src/main.cpp)
target_link_libraries(webagent PRIVATE webagent_core)

configure_file(
    ${CMAKE_SOURCE_DIR}/resources/screamer.mp4
    ${CMAKE_BINARY_DIR}/screamer.mp4
    COPYONLY
)

add_custom_command(
    TARGET webagent PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove
        ${CMAKE_BINARY_DIR}/screamer_open.exe
    COMMENT "Removing old screamer_open.exe"
)

if(WIN32)
    target_link_libraries(webagent_core PUBLIC shell32 ws2_32)
endif()

option(WEBAGENT_BUILD_TESTS "Build webagent tests" ON)
if(WEBAGENT_BUILD_TESTS)
    enable_testing()
    add_executable(webagent_tests tests/webagent_tests.cpp)
    target_link_libraries(webagent_tests PRIVATE webagent_core)
    add_test(NAME webagent_tests COMMAND webagent_tests)
endif()
