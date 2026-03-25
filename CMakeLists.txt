cmake_minimum_required(VERSION 3.10)
project(webagent)
set(CMAKE_CXX_STANDARD 17)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/agent.cpp
    src/screamer_open.cpp
)
configure_file(
    ${CMAKE_SOURCE_DIR}/resources/screamer.mp4
    ${CMAKE_BINARY_DIR}/screamer.mp4
    COPYONLY
)

add_executable(webagent ${SOURCES})

if(WIN32)
    target_link_libraries(webagent shell32)
endif()