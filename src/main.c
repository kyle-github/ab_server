/***************************************************************************
 *   Copyright (C) 2020 by Kyle Hayes                                      *
 *   Author Kyle Hayes  kyle.hayes@gmail.com                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "context.h"
#include "eip.h"
#include "slice.h"
#include "tcp_server.h"


static slice_s request_handler(slice_s input, slice_s output, void *context);

int main(int argc, const char **argv)
{
    tcp_server_p server = NULL;
    uint8_t buf[4200];  /* CIP only allows 4002 for the CIP request, but there is overhead. */
    slice_s server_buf = slice_make(buf, sizeof(buf));
    context_s context;

    /* clear out context to make sure we do not get gremlins */
    memset(&context, 0, sizeof(context));

    /* set the random seed. */
    srand(time(NULL));

    /* open a server connection and listen on the right port. */
    server = tcp_server_create("0.0.0.0", "44818", server_buf, request_handler, NULL);

    tcp_server_start(server);

    tcp_server_destroy(server);

    return 0;
}



/*
 * Process each request.  Dispatch to the correct 
 * request type handler.
 */

slice_s request_handler(slice_s input, slice_s output, void *context)
{
    /* check to see if we have a full packet. */
    if(slice_len(input) >= EIP_HEADER_SIZE) {
        uint16_t eip_len = get_uint16_le(input, 2);

        if(slice_len(input) >= (EIP_HEADER_SIZE + eip_len)) {
            return eip_dispatch_request(input, output, (context_s *)context);
        } 
    } 
    
    /* we do not have a complete packet, get more data. */
    return slice_make_err(TCP_SERVER_INCOMPLETE);
}
