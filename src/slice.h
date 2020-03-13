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

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


typedef enum {
    SLICE_OK,
    SLICE_OUT_OF_BOUNDS,
} slice_status_t;

typedef struct {
    int len;
    uint8_t *data;
} slice_s;

inline static slice_s slice_make(uint8_t *data, int len) { return (slice_s){ .len = len, .data = data }; }
inline static slice_s slice_make_err(int err) { return slice_make(NULL, err); }
inline static bool slice_in_bounds(slice_s s, int index) { if(index >= 0 || index < s.len) { return true; } else { return false; } }
inline static uint16_t slice_at(slice_s s, int index) { if(slice_in_bounds(s, index)) { return s.data[index]; } else { return UINT16_MAX; } }
inline static bool slice_at_put(slice_s s, int index, uint8_t val) { if(slice_in_bounds(s, index)) { s.data[index] = val; return true; } else { return false; } }
inline static int slice_err(slice_s s) { if(s.data == NULL) { return s.len; } else { return 0; } }

inline static slice_s slice_from_slice(slice_s src, int start, int len) {
    if(start < 0 || start > src.len || (start + len) > src.len) {
        return slice_make_err(SLICE_OUT_OF_BOUNDS);
    } else {
    return (slice_s){ .len = len, .data = &(src.data[start]) };
    }
}


/* helper functions to get and set data in a slice. */

inline static uint16_t get_uint16_le(slice_s input_buf, int offset) {
    uint16_t res = 0;

    if(offset >= 0 && slice_in_bounds(input_buf, (offset + 1))) {
        res = ((uint16_t)slice_at(input_buf, offset) + (uint16_t)(slice_at(input_buf, offset + 1) << 8));
    }

    return res;
}


inline static uint32_t get_uint32_le(slice_s input_buf, int offset) {
    uint32_t res = 0;

    if(offset >= 0 && slice_in_bounds(input_buf, (offset + 3))) {
        res =  (uint32_t)(slice_at(input_buf, offset))
             + (uint32_t)(slice_at(input_buf, offset + 1) << 8)
             + (uint32_t)(slice_at(input_buf, offset + 2) << 16)
             + (uint32_t)(slice_at(input_buf, offset + 3) << 24);
    }

    return res;
}


inline static uint64_t get_uint64_le(slice_s input_buf, int offset) {
    uint64_t res = 0;

    if(offset >= 0 && slice_in_bounds(input_buf, (offset + 7))) {
        res =  ((uint64_t)slice_at(input_buf, offset))
             + ((uint64_t)slice_at(input_buf, offset + 1) << 8)
             + ((uint64_t)slice_at(input_buf, offset + 2) << 16)
             + ((uint64_t)slice_at(input_buf, offset + 3) << 24)
             + ((uint64_t)slice_at(input_buf, offset + 4) << 32)
             + ((uint64_t)slice_at(input_buf, offset + 5) << 40)
             + ((uint64_t)slice_at(input_buf, offset + 6) << 48)
             + ((uint64_t)slice_at(input_buf, offset + 7) << 56);
    }

    return res;
}


/* FIXME - these probably should not just fail silently.  They are safe though. */

inline static void set_uint16_le(slice_s output_buf, int offset, uint16_t val) {
    if(offset >= 0 && slice_in_bounds(output_buf, (offset + 1))) {
        slice_at_put(output_buf, offset + 0, (uint8_t)(val & 0xFF));
        slice_at_put(output_buf, offset + 1, (uint8_t)((val >> 8) & 0xFF));
    }
}


inline static void set_uint32_le(slice_s output_buf, int offset, uint32_t val) {
    if(offset >= 0 && slice_in_bounds(output_buf, (offset + 3))) {
        slice_at_put(output_buf, offset + 0, (uint8_t)(val & 0xFF));
        slice_at_put(output_buf, offset + 1, (uint8_t)((val >> 8) & 0xFF));
        slice_at_put(output_buf, offset + 2, (uint8_t)((val >> 16) & 0xFF));
        slice_at_put(output_buf, offset + 3, (uint8_t)((val >> 24) & 0xFF));
    }
}


inline static void set_uint64_le(slice_s output_buf, int offset, uint64_t val) {
    if(offset >= 0 && slice_in_bounds(output_buf, (offset + 7))) {
        slice_at_put(output_buf, offset + 0, (uint8_t)(val & 0xFF));
        slice_at_put(output_buf, offset + 1, (uint8_t)((val >> 8) & 0xFF));
        slice_at_put(output_buf, offset + 2, (uint8_t)((val >> 16) & 0xFF));
        slice_at_put(output_buf, offset + 3, (uint8_t)((val >> 24) & 0xFF));
        slice_at_put(output_buf, offset + 4, (uint8_t)((val >> 32) & 0xFF));
        slice_at_put(output_buf, offset + 5, (uint8_t)((val >> 40) & 0xFF));
        slice_at_put(output_buf, offset + 6, (uint8_t)((val >> 48) & 0xFF));
        slice_at_put(output_buf, offset + 7, (uint8_t)((val >> 56) & 0xFF));
    }
}

