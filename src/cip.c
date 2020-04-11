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

#include <stdint.h>
#include <inttypes.h>
#include "cip.h"
#include "context.h"
#include "eip.h"
#include "slice.h"
#include "utils.h"


/* tag commands */
const uint8_t CIP_MULTI[] = { 0x0A, 0x02, 0x20, 0x02, 0x24, 0x01 }; 
const uint8_t CIP_READ[] = { 0x4C, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_WRITE[] = { 0x4D, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_RMW[] = { 0x4E, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_READ_FRAG[] = { 0x52, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_WRITE_FRAG[] = { 0x53, 0x02, 0x20, 0x02, 0x24, 0x01 };


/* non-tag commands */
const uint8_t CIP_PCCC_EXECUTE[] = { 0x4B, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_FORWARD_CLOSE[] = { 0x4E, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_FORWARD_OPEN[] = { 0x54, 0x02, 0x20, 0x06, 0x24, 0x01 };
const uint8_t CIP_LIST_TAGS[] = { 0x55, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_FORWARD_OPEN_EX[] = { 0x5B, 0x02, 0x20, 0x02, 0x24, 0x01 };


#define CIP_DONE               ((uint8_t)0x80)

/* CIP Errors */

#define CIP_ERR_UNSUPPORTED     ((uint8_t)0x08)

typedef struct {
    uint8_t service_code;   /* why is the operation code _before_ the path? */
    uint8_t path_size;      /* size in 16-bit words of the path */
    slice_s path;           /* store this in a slice to avoid copying */
} cip_header_s;

static slice_s handle_forward_open(slice_s input, slice_s output, context_s *context);

slice_s cip_dispatch_request(slice_s input, slice_s output, context_s *context)
{
    cip_header_s header;

    info("Got packet:");
    slice_dump(input);

    /* match the prefix and dispatch. */
    if(slice_match_bytes(input, CIP_FORWARD_OPEN, sizeof(CIP_FORWARD_OPEN))) {
        return handle_forward_open(slice_from_slice(input, sizeof(CIP_FORWARD_OPEN), slice_len(input) - sizeof(CIP_FORWARD_OPEN)), output, context);
    } else {
            /* build a CIP unsupported error response */
            slice_at_put(output, 0, slice_at(input, 0) | CIP_DONE);
            slice_at_put(output, 1, 0); /* reserved, must be zero. */
            slice_at_put(output, 2, CIP_ERR_UNSUPPORTED);
            slice_at_put(output, 3, 0); /* no additional bytes of sub-error. */

            return slice_from_slice(output, 0, 4);
    }
}


/* a handy structure to hold all the parameters we need to receive in a Forward Open request. */
typedef struct {
    uint8_t secs_per_tick;                  /* seconds per tick */
    uint8_t timeout_ticks;                  /* timeout = srd_secs_per_tick * src_timeout_ticks */
    uint32_t server_conn_id;                /* 0, returned by target in reply. */
    uint32_t client_conn_id;                /* what is _our_ ID for this connection, use ab_connection ptr as id ? */
    uint16_t conn_serial_number;            /* client connection ID/serial number */
    uint16_t orig_vendor_id;                /* client unique vendor ID */
    uint32_t orig_serial_number;            /* client unique serial number */
    uint8_t conn_timeout_multiplier;        /* timeout = mult * RPI */
    uint8_t reserved[3];                    /* reserved, set to 0 */
    uint32_t client_to_server_rpi;          /* us to target RPI - Request Packet Interval in microseconds */
    uint32_t client_to_server_conn_params;  /* some sort of identifier of what kind of PLC we are??? */
    uint32_t server_to_client_rpi;          /* target to us RPI, in microseconds */
    uint32_t server_to_client_params;       /* some sort of identifier of what kind of PLC the target is ??? */
    uint8_t transport_class;                /* ALWAYS 0xA3, server transport, class 3, application trigger */
    slice_s path;                           /* connection path. */
} forward_open_s;


slice_s handle_forward_open(slice_s input, slice_s output, context_s *context)
{

}
