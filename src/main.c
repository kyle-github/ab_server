#include <stdio.h>
#include <stdint.h>
#include "slice.h"
#include "tcp_server.h"


static slice_s client_handler(slice_s buffer);

int main(int argc, const char **argv)
{
    tcp_server_p server = NULL;
    uint8_t buf[4200];
    slice_s server_buf = slice_make(buf, sizeof(buf));
    tcp_server_config_s server_config = {
        .host = "0.0.0.0",
        .port = 44818,
        .buffer = server_buf,
        .handler = client_handler
    };

    server = tcp_server_create(&server_config);

    tcp_server_start(server);

    tcp_server_destroy(server);

    return 0;
}


slice_s client_handler(slice_s buffer)
{
    return slice_make_err(-1);
}