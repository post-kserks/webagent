cmake_minimum_required(VERSION 3.10)
project(webagent)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/agent.cpp
    src/screamer_open.cpp
    src/video_loader.cpp
    src/frontend_launcher.cpp
    src/local_control_server.cpp
)
configure_file(
    ${CMAKE_SOURCE_DIR}/resources/screamer.mp4
    ${CMAKE_BINARY_DIR}/screamer.mp4
    COPYONLY
)


add_executable(webagent ${SOURCES})
add_custom_command(
    TARGET webagent PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove
        ${CMAKE_BINARY_DIR}/screamer_open.exe
    COMMENT "Removing old screamer_open.exe"
)

if(WIN32)
    target_link_libraries(webagent shell32 ws2_32)
endif()
