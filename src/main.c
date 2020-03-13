#include <stdio.h>
#include <stdint.h>
#include "slice.h"
#include "tcp_server.h"


static slice_s check_eip_packet(slice_s buffer);
static slice_s request_handler(slice_s input, slice_s output);

int main(int argc, const char **argv)
{
    tcp_server_p server = NULL;
    uint8_t buf[4200];
    slice_s server_buf = slice_make(buf, sizeof(buf));
    tcp_server_config_s server_config = {
        .host = "0.0.0.0",
        .port = 44818,
        .buffer = server_buf,
        .checker = check_eip_packet,
        .handler = request_handler
    };

    server = tcp_server_create(&server_config);

    tcp_server_start(server);

    tcp_server_destroy(server);

    return 0;
}


/*
 * check to see if we have a complete EIP packet. 
 * 
 * If we do not return an error slice with TCP_SERVER_INCOMPLETE
 * as the status.
 * 
 * If we do have a complete packet, return the slice containing the
 * entire packet.
 */

slice_s check_eip_packet(slice_s buffer)
{
    return slice_make_err(-1);
}

/*
 * Process each request.  Dispatch to the correct 
 * request type handler.
 */

slice_s request_handler(slice_s input, slice_s output)
{
    return slice_make_err(-1);
}
