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
#include "context.h"
#include "eip.h"
#include "slice.h"
#include "utils.h"


#define CIP_MULTI            ((uint8_t)0x0A)
#define CIP_READ             ((uint8_t)0x4C)
#define CIP_WRITE            ((uint8_t)0x4D)
#define CIP_READ_FRAG        ((uint8_t)0x52)
#define CIP_WRITE_FRAG       ((uint8_t)0x53)
#define CIP_LIST_TAGS        ((uint8_t)0x55)

#define CIP_DONE               ((uint8_t)0x80)

/* CIP Errors */

#define CIP_ERR_UNSUPPORTED     ((uint8_t)0x08)

typedef struct {
    uint8_t service_code;   /* why is the operation code _before_ the path? */
    uint8_t path_size;      /* size in 16-bit words of the path */
    slice_s path;           /* store this in a slice to avoid copying */
} cip_header_s;



slice_s cip_dispatch_request(slice_s input, slice_s output, context_s *context)
{
    cip_header_s header;

    /* get the cip header. */
    header.service_code = slice_at(input, 0);
    header.path_size = slice_at(input, 1);

    /* quick sanity check.  We need at least 6 bytes: service code, path size, 4 bytes of path. */
    if(slice_len(input) < 6 || header.path_size != 2) {
        info("Malformed CIP request of %d bytes with path size %u words!", slice_len(input), header.path_size);
        
        return slice_make_err(EIP_ERR_BAD_REQUEST);
    }

    /* get the path */
    header.path = slice_from_slice(input, 2, 2 * header.path_size);

    switch(header.service_code) {
        default:
            /* build a CIP error response */
            slice_at_put(output, 0, header.service_code | CIP_DONE);
            slice_at_put(output, 1, 0); /* reserved, must be zero. */
            slice_at_put(output, 2, CIP_ERR_UNSUPPORTED);
            slice_at_put(output, 3, 0); /* no additional bytes of sub-error. */

            return slice_from_slice(output, 0, 4);
            break;
    }
}
