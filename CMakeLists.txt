cmake_minimum_required(VERSION 4.2)
project(Crestline)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-D_WIN32_WINNT=0x0A00)

include_directories(include)

add_executable(Crestline main.cpp)

target_link_libraries(Crestline wsock32 ws2_32)

add_custom_target(copy_public ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/public
        ${CMAKE_BINARY_DIR}/public
)

file(COPY ${CMAKE_SOURCE_DIR}/crestline.conf DESTINATION ${CMAKE_BINARY_DIR})
