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

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include "cip.h"
#include "eip.h"
#include "plc.h"
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
const uint8_t CIP_FORWARD_CLOSE[] = { 0x4E, 0x02, 0x20, 0x06, 0x24, 0x01 };
const uint8_t CIP_FORWARD_OPEN[] = { 0x54, 0x02, 0x20, 0x06, 0x24, 0x01 };
const uint8_t CIP_LIST_TAGS[] = { 0x55, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_FORWARD_OPEN_EX[] = { 0x5B, 0x02, 0x20, 0x06, 0x24, 0x01 };

/* path to match. */
uint8_t PLC_CONN_PATH[] = { 0x03, 0x00, 0x00, 0x20, 0x02, 0x24, 0x01 };

#define CIP_DONE               ((uint8_t)0x80)

/* CIP Errors */

#define CIP_ERR_UNSUPPORTED     ((uint8_t)0x08)

typedef struct {
    uint8_t service_code;   /* why is the operation code _before_ the path? */
    uint8_t path_size;      /* size in 16-bit words of the path */
    slice_s path;           /* store this in a slice to avoid copying */
} cip_header_s;

static slice_s handle_forward_open(slice_s input, slice_s output, plc_s *plc);
static slice_s make_cip_error(slice_s output, uint8_t cip_cmd, uint8_t cip_err, bool extend, uint16_t extended_error);

slice_s cip_dispatch_request(slice_s input, slice_s output, plc_s *plc)
{
    cip_header_s header;

    info("Got packet:");
    slice_dump(input);

    /* match the prefix and dispatch. */
    if(slice_match_bytes(input, CIP_FORWARD_OPEN, sizeof(CIP_FORWARD_OPEN))) {
        return handle_forward_open(input, output, plc);
    } else {
            return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }
}


/* a handy structure to hold all the parameters we need to receive in a Forward Open request. */
typedef struct {
    uint8_t secs_per_tick;                  /* seconds per tick */
    uint8_t timeout_ticks;                  /* timeout = srd_secs_per_tick * src_timeout_ticks */
    uint32_t server_conn_id;                /* 0, returned by server in reply. */
    uint32_t client_conn_id;                /* sent by client. */
    uint16_t conn_serial_number;            /* client connection ID/serial number */
    uint16_t orig_vendor_id;                /* client unique vendor ID */
    uint32_t orig_serial_number;            /* client unique serial number */
    uint8_t conn_timeout_multiplier;        /* timeout = mult * RPI */
    uint8_t reserved[3];                    /* reserved, set to 0 */
    uint32_t client_to_server_rpi;          /* us to target RPI - Request Packet Interval in microseconds */
    uint32_t client_to_server_conn_params;  /* some sort of identifier of what kind of PLC we are??? */
    uint32_t server_to_client_rpi;          /* target to us RPI, in microseconds */
    uint32_t server_to_client_conn_params;       /* some sort of identifier of what kind of PLC the target is ??? */
    uint8_t transport_class;                /* ALWAYS 0xA3, server transport, class 3, application trigger */
    slice_s path;                           /* connection path. */
} forward_open_s;

/* the minimal Forward Open with no path */
#define CIP_FORWARD_OPEN_MIN_SIZE   (48)


slice_s handle_forward_open(slice_s input, slice_s output, plc_s *plc)
{
    slice_s result;
    size_t offset = 0;
    uint8_t fo_cmd = slice_at(input, 0);
    forward_open_s fo_req = {0};

    info("Checking Forward Open request:");
    slice_dump(input);

    /* minimum length check */
    if(slice_len(input) < CIP_FORWARD_OPEN_MIN_SIZE) {
        /* FIXME - send back the right error. */
        return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    /* get the data. */
    offset = sizeof(CIP_FORWARD_OPEN); /* step past the path to the CM */
    fo_req.secs_per_tick = slice_at(input, offset); offset++;
    fo_req.timeout_ticks = slice_at(input, offset); offset++;
    fo_req.server_conn_id = get_uint32_le(input, offset); offset += 4;
    fo_req.client_conn_id = get_uint32_le(input, offset); offset += 4;
    fo_req.conn_serial_number = get_uint16_le(input, offset); offset += 2;
    fo_req.orig_vendor_id = get_uint16_le(input, offset); offset += 2;
    fo_req.orig_serial_number = get_uint32_le(input, offset); offset += 4;
    fo_req.conn_timeout_multiplier = slice_at(input, offset); offset += 4; /* byte plus 3-bytes of padding. */
    fo_req.client_to_server_rpi = get_uint32_le(input, offset); offset += 4;
    if(fo_cmd == CIP_FORWARD_OPEN[0]) {
        /* old command uses 16-bit value. */
        fo_req.client_to_server_conn_params = get_uint16_le(input, offset); offset += 2;
    } else {
        /* new command has 32-bit field here. */
        fo_req.client_to_server_conn_params = get_uint32_le(input, offset); offset += 4;
    }
    fo_req.server_to_client_rpi = get_uint32_le(input, offset); offset += 4;
    if(fo_cmd == CIP_FORWARD_OPEN[0]) {
        /* old command uses 16-bit value. */
        fo_req.server_to_client_conn_params = get_uint16_le(input, offset); offset += 2;
    } else {
        /* new command has 32-bit field here. */
        fo_req.server_to_client_conn_params = get_uint32_le(input, offset); offset += 4;
    }
    fo_req.transport_class = slice_at(input, offset); offset++;

    /* check the remaining length */
    if(offset >= slice_len(input)) {
        /* FIXME - send back the right error. */
        info("Forward open request size, %d, too small.   Should be greater than %d!", slice_len(input), offset);
        return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    /* build the path to match. */
    PLC_CONN_PATH[1] = plc->path[0];
    PLC_CONN_PATH[2] = plc->path[1];

    /* does the path match this PLC? */
    info("Checking path, offset=%d, input length=%d", offset, slice_len(input));
    ssize_t len = slice_len(input) - offset;
    slice_s tmp_slice = slice_from_slice(input, offset, slice_len(input));

    info("path slice:");
    slice_dump(tmp_slice);

    if(!slice_match_bytes(slice_from_slice(input, offset, slice_len(input) - offset), &PLC_CONN_PATH[0], sizeof(PLC_CONN_PATH))) {
        /* FIXME - send back the right error. */
        info("Forward open request path did not match the path for this PLC!");
        return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    /* all good if we got here. */
    plc->client_connection_id = fo_req.client_conn_id;
    plc->client_connection_serial_number = fo_req.conn_serial_number;
    plc->client_vendor_id = fo_req.orig_vendor_id;
    plc->client_serial_number = fo_req.orig_serial_number;
    plc->client_to_server_rpi = fo_req.client_to_server_rpi;
    plc->server_to_client_rpi = fo_req.server_to_client_rpi;
    plc->server_connection_id = rand();
    plc->server_connection_seq = (uint16_t)rand();

    /* now process the FO and respond. */
    offset = 0;
    slice_at_put(output, offset, slice_at(input, 0) | CIP_DONE); offset++;
    slice_at_put(output, offset, 0); offset++; /* padding/reserved. */
    slice_at_put(output, offset, 0); offset++; /* no error. */
    slice_at_put(output, offset, 0); offset++; /* no extra error fields. */

    set_uint32_le(output, offset, plc->server_connection_id); offset += 4;
    set_uint32_le(output, offset, plc->client_connection_id); offset += 4;
    set_uint16_le(output, offset, plc->client_connection_serial_number); offset += 2;
    set_uint16_le(output, offset, plc->client_vendor_id); offset += 2;
    set_uint32_le(output, offset, plc->client_serial_number); offset += 4;
    set_uint32_le(output, offset, plc->client_to_server_rpi); offset += 4;
    set_uint32_le(output, offset, plc->server_to_client_rpi); offset += 4;

    /* not sure what these do... */
    slice_at_put(output, offset, 0); offset++;
    slice_at_put(output, offset, 0); offset++;

    return slice_from_slice(output, 0, offset);        
}



slice_s make_cip_error(slice_s output, uint8_t cip_cmd, uint8_t cip_err, bool extend, uint16_t extended_error)
{
    size_t result_size = 0;

    slice_at_put(output, 0, cip_cmd | CIP_DONE); 
    slice_at_put(output, 1, 0); /* reserved, must be zero. */
    slice_at_put(output, 2, CIP_ERR_UNSUPPORTED);

    if(extend) {
        slice_at_put(output, 3, 2); /* two bytes of extended status. */
        set_uint16_le(output, 4, extended_error);
        result_size = 6;
    } else {
        slice_at_put(output, 3, 0); /* no additional bytes of sub-error. */
        result_size = 4;
    }

    return slice_from_slice(output, 0, result_size);    
}

