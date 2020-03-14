#include <stdio.h>
#include <stdint.h>
#include "slice.h"
#include "tcp_server.h"


/* EIP header size is 24 bytes. */
#define EIP_HEADER_SIZE (24)

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
        .handler = request_handler
    };

    server = tcp_server_create(&server_config);

    tcp_server_start(server);

    tcp_server_destroy(server);

    return 0;
}



/*
 * Process each request.  Dispatch to the correct 
 * request type handler.
 */

slice_s request_handler(slice_s input, slice_s output)
{
    /* check to see if we have a full packet. */
    if(slice_len(input) >= EIP_HEADER_SIZE) {
        uint16_t eip_len = get_uint16_le(input, 2);

        if(slice_len(input) >= (EIP_HEADER_SIZE + eip_len)) {
            return eip_dispatch_request(input, output);
        } 
    } 
    
    /* we do not have a complete packet, get more data. */
    return slice_make_err(TCP_SERVER_INCOMPLETE);
}
