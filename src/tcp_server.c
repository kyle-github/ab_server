
#include <stdlib.h>
#include "slice.h"
#include "tcp_server.h"

struct tcp_server {
    int sock_fd;
    slice_s buffer;
    slice_s (*handler)(slice_s input, slice_s output);
};


tcp_server_p tcp_server_create(tcp_server_config_s *config)
{
    tcp_server_p server = calloc(1, sizeof(*server));

    if(server) {
        int sock_fd = socket_open(config->host, config->port);
    }
}

void tcp_server_start(tcp_server_p server);
void tcp_server_destroy(tcp_server_p server);
