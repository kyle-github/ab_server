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

#include <stdint.h>


typedef uint16_t tag_type_t;

#define TAG_TYPE_SINT        ((tag_type_t)0xC200) /* Signed 8–bit integer value */
#define TAG_TYPE_INT         ((tag_type_t)0xC300) /* Signed 16–bit integer value */
#define TAG_TYPE_DINT        ((tag_type_t)0xC400) /* Signed 32–bit integer value */
#define TAG_TYPE_LINT        ((tag_type_t)0xC500) /* Signed 64–bit integer value */
#define TAG_TYPE_USINT       ((tag_type_t)0xC600) /* Unsigned 8–bit integer value */
#define TAG_TYPE_UINT        ((tag_type_t)0xC700) /* Unsigned 16–bit integer value */
#define TAG_TYPE_UDINT       ((tag_type_t)0xC800) /* Unsigned 32–bit integer value */
#define TAG_TYPE_ULINT       ((tag_type_t)0xC900) /* Unsigned 64–bit integer value */
#define TAG_TYPE_REAL        ((tag_type_t)0xCA00) /* 32–bit floating point value, IEEE format */
#define TAG_TYPE_LREAL       ((tag_type_t)0xCB00) /* 64–bit floating point value, IEEE format */

struct tag_def_s {
    struct tag_def_s *next_tag;
    char *name;
    tag_type_t tag_type;
    int elem_size;
    int elem_count;
    int dimensions[3];
    uint8_t *data;
};

typedef struct tag_def_s tag_def_s;

typedef enum {
    PLC_CONTROL_LOGIX,
    PLC_MICRO800
} plc_type_t;

/* Define the context that is passed around. */
typedef struct {
    plc_type_t plc_type;
    uint8_t path[16];
    uint8_t path_len;

    /* connection info. */
    uint32_t session_handle;
    uint64_t sender_context;
    uint32_t server_connection_id;
    uint16_t server_connection_seq;
    uint32_t server_to_client_rpi;
    uint32_t client_connection_id;
    uint16_t client_connection_seq;
    uint16_t client_connection_serial_number;
    uint16_t client_vendor_id;
    uint32_t client_serial_number;
    uint32_t client_to_server_rpi;

    /* list of tags served by this "PLC" */
    struct tag_def_s *tags;
} plc_s;