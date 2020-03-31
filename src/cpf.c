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
} cpf_header_s;

#define CPF_UCONN_HEADER_SIZE (10)

slice_s handle_cpf_unconnected(slice_s input, slice_s output, context_s *context)
{
    cpf_header_s header;

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

    return cip_dispatch_request(slice_from_slice(input, CPF_UCONN_HEADER_SIZE, slice_len(input) - CPF_UCONN_HEADER_SIZE),
                                slice_from_slice(output, CPF_UCONN_HEADER_SIZE, slice_len(output) - CPF_UCONN_HEADER_SIZE),
                                context);
}