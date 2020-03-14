cmake_minimum_required(VERSION 3.0.0)
project(ab_server VERSION 0.1.0)

#include(CTest)
#enable_testing()

add_executable(ab_server "src/eip.h"
                         "src/main.c"
                         "src/slice.h"
                         "src/socket.c"
                         "src/socket.h"
                         "src/tcp_server.c"
                         "src/tcp_server.h"
                         "src/utils.c"
                         "src/utils.h"
)

#set(CPACK_PROJECT_NAME ${PROJECT_NAME})
#set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
#include(CPack)
