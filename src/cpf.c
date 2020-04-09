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
#include "cip.h"
#include "cpf.h"
#include "eip.h"
#include "utils.h"

#define CPF_ITEM_NAI ((uint16_t)0x0000) /* NULL Address Item */
#define CPF_ITEM_CAI ((uint16_t)0x00A1) /* connected address item */
#define CPF_ITEM_CDI ((uint16_t)0x00B1) /* connected data item */
#define CPF_ITEM_UDI ((uint16_t)0x00B2) /* Unconnected data item */


typedef struct {
    uint16_t item_count;        /* should be 2 for now. */
    uint16_t item_addr_type;
    uint16_t item_addr_length;
    uint16_t item_data_type;
    uint16_t item_data_length;
} cpf_uc_header_s;

#define CPF_UCONN_HEADER_SIZE (10)

typedef struct {
    uint16_t item_count;        /* should be 2 for now. */
    uint16_t item_addr_type;
    uint16_t item_addr_length;
    uint32_t conn_id;
    uint16_t item_data_type;
    uint16_t item_data_length;
    uint16_t conn_seq;
} cpf_co_header_s;

#define CPF_CONN_HEADER_SIZE (16)



slice_s handle_cpf_unconnected(slice_s input, slice_s output, context_s *context)
{
    slice_s result;
    cpf_uc_header_s header;

    /* we must have some sort of payload. */
    if(slice_len(input) <= CPF_UCONN_HEADER_SIZE) {
        info("Unusable size of unconnected CPF packet!");
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    /* unpack the request. */
    header.item_count = get_uint16_le(input, 0);

    /* sanity check the number of items. */
    if(header.item_count != (uint16_t)2) {
        info("Unsupported unconnected CPF packet, expected two items but found %u!", header.item_count);
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    header.item_addr_type = get_uint16_le(input, 2);
    header.item_addr_length = get_uint16_le(input, 4);
    header.item_data_type = get_uint16_le(input, 6);
    header.item_data_length = get_uint16_le(input, 8);

    /* sanity check the data. */
    if(header.item_addr_type != CPF_ITEM_NAI) {
        info("Expected null address item but found %x!", header.item_addr_type);
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    if(header.item_addr_length != 0) {
        info("Expected zero address item length but found %d bytes!", header.item_addr_length);
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    if(header.item_data_type != CPF_ITEM_UDI) {
        info("Expected unconnected data item but found %x!", header.item_data_type);
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }


    /* dispatch and handle the result. */
    result = cip_dispatch_request(slice_from_slice(input, CPF_UCONN_HEADER_SIZE, slice_len(input) - CPF_UCONN_HEADER_SIZE),
                                slice_from_slice(output, CPF_UCONN_HEADER_SIZE, slice_len(output) - CPF_UCONN_HEADER_SIZE),
                                context);

    if(!slice_has_err(result)) {
        /* build outbound header. */
        set_uint16_le(output, 0, 2); /* two items. */
        set_uint16_le(output, 2, CPF_ITEM_NAI); /* connected address type. */
        set_uint16_le(output, 4, 0); /* No connection ID. */
        set_uint16_le(output, 6, CPF_ITEM_UDI); /* connected data type */
        set_uint16_le(output, 8, slice_len(result)); /* result from CIP processing downstream. */

        /* create a new slice with the CPF header and the response packet in it. */
        result = slice_from_slice(output, 0, slice_len(result) + CPF_CONN_HEADER_SIZE);
    }

    /* errors are pass through. */

    return result;

}



slice_s handle_cpf_connected(slice_s input, slice_s output, context_s *context)
{
    slice_s result;
    cpf_co_header_s header;

    /* we must have some sort of payload. */
    if(slice_len(input) <= CPF_UCONN_HEADER_SIZE) {
        info("Unusable size of connected CPF packet!");
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    /* unpack the request. */
    header.item_count = get_uint16_le(input, 0);

    /* sanity check the number of items. */
    if(header.item_count != (uint16_t)2) {
        info("Unsupported connected CPF packet, expected two items but found %u!", header.item_count);
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    header.item_addr_type = get_uint16_le(input, 2);
    header.item_addr_length = get_uint16_le(input, 4);
    header.conn_id = get_uint32_le(input, 6);
    header.item_data_type = get_uint16_le(input, 10);
    header.item_data_length = get_uint16_le(input, 12);
    header.conn_seq = get_uint16_le(input, 14);

    /* sanity check the data. */
    if(header.item_addr_type != CPF_ITEM_CAI) {
        info("Expected connected address item but found %x!", header.item_addr_type);
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    if(header.item_addr_length != 4) {
        info("Expected address item length of 4 but found %d bytes!", header.item_addr_length);
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    if(header.conn_id != context->server_connection_id) {
        info("Expected connection ID %x but found connection ID %x!", context->server_connection_id, header.conn_id);
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    if(header.item_data_type != CPF_ITEM_CDI) {
        info("Expected connected data item but found %x!", header.item_data_type);
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    /* do we care about the sequence ID?   Should check. */
    context->server_connection_seq = header.conn_seq;

    /* dispatch and handle the result. */
    result = cip_dispatch_request(slice_from_slice(input, CPF_CONN_HEADER_SIZE, slice_len(input) - CPF_CONN_HEADER_SIZE),
                                slice_from_slice(output, CPF_CONN_HEADER_SIZE, slice_len(output) - CPF_CONN_HEADER_SIZE),
                                context);

    if(!slice_has_err(result)) {
        /* build outbound header. */
        set_uint16_le(output, 0, 2); /* two items. */
        set_uint16_le(output, 2, CPF_ITEM_CAI); /* connected address type. */
        set_uint16_le(output, 4, 4); /* connection ID is 4 bytes. */
        set_uint32_le(output, 6, context->client_connection_id);
        set_uint16_le(output, 10, CPF_ITEM_CDI); /* connected data type */
        set_uint16_le(output, 12, slice_len(result) + 2); /* result from CIP processing downstream.  Plus 2 bytes for sequence number. */
        set_uint16_le(output, 14, context->client_connection_seq);

        /* create a new slice with the CPF header and the response packet in it. */
        result = slice_from_slice(output, 0, slice_len(result) + CPF_CONN_HEADER_SIZE);
    }

    /* errors are pass through. */

    return result;
}

