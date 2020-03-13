cmake_minimum_required(VERSION 3.0.0)
project(ab_server VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(ab_server "src/main.c"
                         "src/slice.h"
                         "src/tcp_server.h"
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
