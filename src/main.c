#include <stdio.h>

int main(int argc, const char **argv)
{
    tcp_server_p server = NULL;
    byte buf[4200];
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


int client_handler()