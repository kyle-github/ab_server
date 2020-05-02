cmake_minimum_required(VERSION 3.0.0)
project(ab_server VERSION 0.7.0)

add_executable(ab_server 
                         "src/cip.h"
                         "src/cip.c"
                         "src/cpf.h"
                         "src/cpf.c"
                         "src/eip.h"
                         "src/eip.c"
                         "src/main.c"
                         "src/plc.h"
                         "src/slice.h"
                         "src/socket.c"
                         "src/socket.h"
                         "src/tcp_server.c"
                         "src/tcp_server.h"
                         "src/utils.c"
                         "src/utils.h"
)
