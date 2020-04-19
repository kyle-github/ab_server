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
const uint8_t CIP_READ[] = { 0x4C };
const uint8_t CIP_WRITE[] = { 0x4D, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_RMW[] = { 0x4E, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_READ_FRAG[] = { 0x52 };
const uint8_t CIP_WRITE_FRAG[] = { 0x53, 0x02, 0x20, 0x02, 0x24, 0x01 };


/* non-tag commands */
const uint8_t CIP_PCCC_EXECUTE[] = { 0x4B, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_FORWARD_CLOSE[] = { 0x4E, 0x02, 0x20, 0x06, 0x24, 0x01 };
const uint8_t CIP_FORWARD_OPEN[] = { 0x54, 0x02, 0x20, 0x06, 0x24, 0x01 };
const uint8_t CIP_LIST_TAGS[] = { 0x55, 0x02, 0x20, 0x02, 0x24, 0x01 };
const uint8_t CIP_FORWARD_OPEN_EX[] = { 0x5B, 0x02, 0x20, 0x06, 0x24, 0x01 };

/* path to match. */
// uint8_t LOGIX_CONN_PATH[] = { 0x03, 0x00, 0x00, 0x20, 0x02, 0x24, 0x01 };
// uint8_t MICRO800_CONN_PATH[] = { 0x02, 0x20, 0x02, 0x24, 0x01 };

#define CIP_DONE               ((uint8_t)0x80)

#define CIP_SYMBOLIC_SEGMENT_MARKER ((uint8_t)0x91)

/* CIP Errors */

#define CIP_OK                  ((uint8_t)0x00)
#define CIP_ERR_FRAG            ((uint8_t)0x06)
#define CIP_ERR_UNSUPPORTED     ((uint8_t)0x08)
#define CIP_ERR_EXTENDED        ((uint8_t)0xff)

#define CIP_ERR_EX_TOO_LONG     ((uint16_t)0x2105)

typedef struct {
    uint8_t service_code;   /* why is the operation code _before_ the path? */
    uint8_t path_size;      /* size in 16-bit words of the path */
    slice_s path;           /* store this in a slice to avoid copying */
} cip_header_s;

static slice_s handle_forward_open(slice_s input, slice_s output, plc_s *plc);
static slice_s handle_forward_close(slice_s input, slice_s output, plc_s *plc);
static slice_s handle_read_request(slice_s input, slice_s output, plc_s *plc);
static bool process_tag_segment(plc_s *plc, slice_s input, tag_def_s **tag, size_t *start_read_offset);
static slice_s make_cip_error(slice_s output, uint8_t cip_cmd, uint8_t cip_err, bool extend, uint16_t extended_error);
static bool match_path(slice_s input, bool need_pad, uint8_t *path, uint8_t path_len);

slice_s cip_dispatch_request(slice_s input, slice_s output, plc_s *plc)
{
    cip_header_s header;

    info("Got packet:");
    slice_dump(input);

    /* match the prefix and dispatch. */
    if(slice_match_bytes(input, CIP_READ, sizeof(CIP_READ))) {
        return handle_read_request(input, output, plc);
    } else if(slice_match_bytes(input, CIP_READ_FRAG, sizeof(CIP_READ_FRAG))) {
        return handle_read_request(input, output, plc);
    } else if(slice_match_bytes(input, CIP_FORWARD_OPEN, sizeof(CIP_FORWARD_OPEN))) {
        return handle_forward_open(input, output, plc);
    } else if(slice_match_bytes(input, CIP_FORWARD_OPEN_EX, sizeof(CIP_FORWARD_OPEN_EX))) {
        return handle_forward_open(input, output, plc);
    } else if(slice_match_bytes(input, CIP_FORWARD_CLOSE, sizeof(CIP_FORWARD_CLOSE))) {
        return handle_forward_close(input, output, plc);
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
    slice_s conn_path;
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
    conn_path = slice_from_slice(input, offset, slice_len(input));

    info("path slice:");
    slice_dump(conn_path);

    if(!match_path(conn_path, ((offset & 0x01) ? false : true), &plc->path[0], plc->path_len)) {
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

    /* store the allowed packet sizes. */
    plc->client_to_server_max_packet = fo_req.client_to_server_conn_params & 
                               ((fo_cmd == CIP_FORWARD_OPEN[0]) ? 0x1FF : 0x0FFF);
    plc->server_to_client_max_packet = fo_req.server_to_client_conn_params & 
                               ((fo_cmd == CIP_FORWARD_OPEN[0]) ? 0x1FF : 0x0FFF);

    /* FIXME - check that the packet sizes are valid 508 or 4002 */

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


/* Forward Close request. */
typedef struct {
    uint8_t secs_per_tick;          /* seconds per tick */
    uint8_t timeout_ticks;          /* timeout = srd_secs_per_tick * src_timeout_ticks */
    uint16_t client_connection_serial_number; /* our connection ID/serial number */
    uint16_t client_vendor_id;      /* our unique vendor ID */
    uint32_t client_serial_number;  /* our unique serial number */
    slice_s path;                   /* path to PLC */
} forward_close_s;

/* the minimal Forward Open with no path */
#define CIP_FORWARD_CLOSE_MIN_SIZE   (16)


slice_s handle_forward_close(slice_s input, slice_s output, plc_s *plc)
{
    slice_s conn_path;
    size_t offset = 0;
    uint8_t fc_cmd = slice_at(input, 0);
    forward_close_s fc_req = {0};

    info("Checking Forward Close request:");
    slice_dump(input);

    /* minimum length check */
    if(slice_len(input) < CIP_FORWARD_CLOSE_MIN_SIZE) {
        /* FIXME - send back the right error. */
        return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    /* get the data. */
    offset = sizeof(CIP_FORWARD_CLOSE); /* step past the path to the CM */
    fc_req.secs_per_tick = slice_at(input, offset); offset++;
    fc_req.timeout_ticks = slice_at(input, offset); offset++;
    fc_req.client_connection_serial_number = get_uint16_le(input, offset); offset += 2;
    fc_req.client_vendor_id = get_uint16_le(input, offset); offset += 2;
    fc_req.client_serial_number = get_uint32_le(input, offset); offset += 4;

    /* check the remaining length */
    if(offset >= slice_len(input)) {
        /* FIXME - send back the right error. */
        info("Forward close request size, %d, too small.   Should be greater than %d!", slice_len(input), offset);
        return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    /*
     * why does Rockwell do this?   The path here is _NOT_ a byte-for-byte copy of the path
     * that was used to open the connection.  This one is padded with a zero byte after the path
     * length.
     */

    /* build the path to match. */
    conn_path = slice_from_slice(input, offset, slice_len(input));

    if(!match_path(conn_path, ((offset & 0x01) ? false : true), plc->path, plc->path_len)) {
        info("path does not match stored path!");
        return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    /* Check the values we got. */
    if(plc->client_connection_serial_number != fc_req.client_connection_serial_number) {
        /* FIXME - send back the right error. */
        info("Forward close connection serial number, %x, did not match the connection serial number originally passed, %x!", fc_req.client_connection_serial_number, plc->client_connection_serial_number);
        return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }
    if(plc->client_vendor_id != fc_req.client_vendor_id) {
        /* FIXME - send back the right error. */
        info("Forward close client vendor ID, %x, did not match the client vendor ID originally passed, %x!", fc_req.client_vendor_id, plc->client_vendor_id);
        return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }
    if(plc->client_serial_number != fc_req.client_serial_number) {
        /* FIXME - send back the right error. */
        info("Forward close client serial number, %x, did not match the client serial number originally passed, %x!", fc_req.client_serial_number, plc->client_serial_number);
        return make_cip_error(output, slice_at(input, 0) | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    /* now process the FClose and respond. */
    offset = 0;
    slice_at_put(output, offset, slice_at(input, 0) | CIP_DONE); offset++;
    slice_at_put(output, offset, 0); offset++; /* padding/reserved. */
    slice_at_put(output, offset, 0); offset++; /* no error. */
    slice_at_put(output, offset, 0); offset++; /* no extra error fields. */

    set_uint16_le(output, offset, plc->client_connection_serial_number); offset += 2;
    set_uint16_le(output, offset, plc->client_vendor_id); offset += 2;
    set_uint32_le(output, offset, plc->client_serial_number); offset += 4;

    /* not sure what these do... */
    slice_at_put(output, offset, 0); offset++;
    slice_at_put(output, offset, 0); offset++;

    return slice_from_slice(output, 0, offset);        
}


/*
 * A read request comes in with a symbolic segment first, then zero to three numeric segments. 
 */

#define CIP_READ_MIN_SIZE (6)
#define CIP_READ_FRAG_MIN_SIZE (10)

slice_s handle_read_request(slice_s input, slice_s output, plc_s *plc)
{
    uint8_t read_cmd = slice_at(input, 0);  /*get the type. */
    uint8_t tag_segment_size = 0;
    uint8_t symbolic_marker = 0;
    uint8_t tag_name_size = 0;
    uint16_t element_count = 0;
    uint32_t byte_offset = 0;
    size_t read_start_offset = 0;
    size_t offset = 0;
    tag_def_s *tag = NULL;
    size_t tag_data_length = 0;
    size_t total_request_size = 0;
    size_t remaining_size = 0;
    size_t packet_capacity = 0;
    bool need_frag = false;
    size_t amount_to_copy = 0;

    if(slice_len(input) < (read_cmd == CIP_READ[0] ? CIP_READ_MIN_SIZE : CIP_READ_FRAG_MIN_SIZE)) {
        info("Insufficient data in the CIP read request!");
        return make_cip_error(output, read_cmd | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    offset = 1;
    tag_segment_size = slice_at(input, offset); offset++;

    /* check that we have enough space. */
    if((slice_len(input) + (read_cmd == CIP_READ[0] ? 2 : 6) - 2) < (tag_segment_size * 2)) {
        info("Request does not have enough space for element count and byte offset!");
        return make_cip_error(output, read_cmd | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    if(!process_tag_segment(plc, slice_from_slice(input, offset, tag_segment_size * 2), &tag, &read_start_offset)) {
        return make_cip_error(output, read_cmd | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    /* step past the tag segment. */
    offset += (tag_segment_size * 2);

    element_count = get_uint16_le(input, offset); offset += 2;

    if(read_cmd == CIP_READ_FRAG[0]) {
        byte_offset = get_uint32_le(input, offset); offset += 4;
    }

    /* double check the size of the request. */
    if(offset != slice_len(input)) {
        info("Request size does not match CIP request size!");
        return make_cip_error(output, read_cmd | CIP_DONE, CIP_ERR_UNSUPPORTED, false, 0);
    }

    /* check the offset bounds. */
    tag_data_length = tag->elem_count * tag->elem_size;

    info("tag_data_length = %d", tag_data_length);

    /* get the amount requested. */
    total_request_size = element_count * tag->elem_size;

    info("total_request_size = %d", total_request_size);

    /* check the amount */
    if(read_start_offset + total_request_size > tag_data_length) {
        info("request asks for too much data!");
        return make_cip_error(output, read_cmd | CIP_DONE, CIP_ERR_EXTENDED, true, CIP_ERR_EX_TOO_LONG);
    }

    /* check to make sure that the offset passed is within the bounds. */
    if(read_start_offset + byte_offset > tag_data_length) {
        info("request offset is past the end of the tag!");
        return make_cip_error(output, read_cmd | CIP_DONE, CIP_ERR_EXTENDED, true, CIP_ERR_EX_TOO_LONG);
    }

    /* do we need to fragment the result? */
    remaining_size = total_request_size - byte_offset;
    packet_capacity = slice_len(output) - 4; /* MAGIC - CIP header is 4 bytes. */

    info("packet_capacity = %d", packet_capacity);

    if(remaining_size > packet_capacity) { 
        need_frag = true;
    } else {
        need_frag = false;
    }

    info("need_frag = %s", need_frag ? "true" : "false");
     
    /* start making the response. */
    offset = 0;
    slice_at_put(output, offset, read_cmd | CIP_DONE); offset++;
    slice_at_put(output, offset, 0); offset++; /* padding/reserved. */
    slice_at_put(output, offset, (need_frag ? CIP_ERR_FRAG : CIP_OK)); offset++; /* no error. */
    slice_at_put(output, offset, 0); offset++; /* no extra error fields. */

    /* copy the data type. */
    set_uint16_le(output, offset, tag->tag_type); offset += 2;

    /* how much data to copy? */
    amount_to_copy = (remaining_size < packet_capacity ? remaining_size : packet_capacity);
    if(amount_to_copy > 8) {
        /* align to 8-byte chunks */
        amount_to_copy &= 0xFFFFC;
    }

    info("amount_to_copy = %d", amount_to_copy);

    /* FIXME - use memcpy */
    for(size_t i=0; i < amount_to_copy; i++) {
        slice_at_put(output, offset + i, tag->data[byte_offset + i]);
    }

    offset += amount_to_copy;

    return slice_from_slice(output, 0, offset);
}


/*
 * we should see:
 *  0x91 <name len> <name bytes> (<numeric segment>){0-3} 
 *
 * find the tag name, then check the numeric segments, if any, against the
 * tag dimensions.
 */

bool process_tag_segment(plc_s *plc, slice_s input, tag_def_s **tag, size_t *start_read_offset)
{
    size_t offset = 0;
    uint8_t symbolic_marker = slice_at(input, offset); offset++;
    uint8_t name_len = 0;
    slice_s tag_name;
    int dimensions[3] = { 0, 0, 0};
    size_t dimension_index = 0;

    if(symbolic_marker != CIP_SYMBOLIC_SEGMENT_MARKER)  {
        info("Expected symbolic segment but found %x!", symbolic_marker);
        return false;
    }

    /* get and check the length of the symbolic name part. */
    name_len = slice_at(input, offset); offset++;
    if(name_len >= slice_len(input)) {
        info("Insufficient space in symbolic segment for name.   Needed %d bytes but only had %d bytes!", name_len, slice_len(input)-1);
        return false;
    }

    /* bump the offset.   Must be 16-bit aligned, so pad if needed. */
    offset += name_len + ((name_len & 0x01) ? 1 : 0);

    /* try to find the tag. */
    tag_name = slice_from_slice(input, 2, name_len);
    *tag = plc->tags;

    while(*tag) {
        if(slice_match_string(tag_name, (*tag)->name)) {
            info("Found tag %s", (*tag)->name);
            break;
        }

        (*tag) = (*tag)->next_tag;
    }

    if(*tag) {
        slice_s numeric_segments = slice_from_slice(input, offset, slice_len(input));

        dimension_index = 0;

        info("Numeric segment(s):");
        slice_dump(numeric_segments);

        while(slice_len(numeric_segments) > 0) {
            uint8_t segment_type = slice_at(numeric_segments, 0);

            if(dimension_index >= 3) {
                info("More numeric segments than expected!   Remaining request:");
                slice_dump(numeric_segments);
                return false;
            }

            switch(segment_type) {
                case 0x28: /* single byte value. */
                    dimensions[dimension_index] = (int)slice_at(numeric_segments, 1);
                    dimension_index++;
                    numeric_segments = slice_from_slice(numeric_segments, 2, slice_len(numeric_segments));
                    break;

                case 0x29: /* two byte value */
                    dimensions[dimension_index] = (int)get_uint16_le(numeric_segments, 2);
                    dimension_index++;
                    numeric_segments = slice_from_slice(numeric_segments, 4, slice_len(numeric_segments));
                    break;

                case 0x2A: /* four byte value */
                    dimensions[dimension_index] = (int)get_uint32_le(numeric_segments, 2);
                    dimension_index++;
                    numeric_segments = slice_from_slice(numeric_segments, 6, slice_len(numeric_segments));
                    break;

                default:
                    info("Unexpected numeric segment marker %x!", segment_type);
                    return false;
                    break;
            }
        }

        /* calculate the element offset. */
        if(dimension_index > 0) {
            size_t element_offset = 0;

            if(dimension_index != (*tag)->num_dimensions) {
                info("Required %d numeric segments, but only found %d!", (*tag)->num_dimensions, dimension_index);
                return false;
            }

            /* check in bounds. */
            for(size_t i=0; i < dimension_index; i++) {
                if(dimensions[i] < 0 || dimensions[i] >= (*tag)->dimensions[i]) {
                    info("Dimension %d is out of bounds, must be 0 <= %d < %d", (int)i, dimensions[i], (*tag)->dimensions[i]);
                    return false;
                }
            }
            
            /* calculate the offset. */
            element_offset = dimensions[0] * ((*tag)->dimensions[1] * (*tag)->dimensions[2]) + 
                             dimensions[1] * (*tag)->dimensions[2] +
                             dimensions[2];

            *start_read_offset = (*tag)->elem_size * element_offset;
        } else {
            *start_read_offset = 0;
        }
    } else {
        info("Tag %.*s not found!", slice_len(tag_name), (const char *)(tag_name.data));
        return false;
    }



    return true;
}

/* match a path.   This is tricky, thanks, Rockwell. */
bool match_path(slice_s input, bool need_pad, uint8_t *path, uint8_t path_len)
{
    ssize_t input_len = slice_len(input);
    ssize_t input_path_len = 0;
    size_t path_start = 0;

    info("Starting with request path:");
    slice_dump(input);
    info("and stored path:");
    slice_dump(slice_make(path, (ssize_t)path_len));

    if(input_len < path_len) {
        info("path does not match lengths.   Input length %d, path length %d", input_len, path_len);
        return false;
    }

    /* the first byte of the path input is the length byte in 16-bit words */
    input_path_len = slice_at(input, 0);

    /* check it against the passed path length */
    if((input_path_len * 2) != path_len) {
        info("path is wrong length.   Got %d but expected %d!", input_path_len*2, path_len);
        return false;
    }

    /* where does the path start? */
    if(need_pad) {
        path_start = 2;
    } else {
        path_start = 1;
    }

    info("Comparing slice:");
    slice_dump(slice_from_slice(input, path_start, slice_len(input)));
    info("with slice:");
    slice_dump(slice_make(path, (ssize_t)path_len));

    return slice_match_bytes(slice_from_slice(input, path_start, slice_len(input)), path, path_len);
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

