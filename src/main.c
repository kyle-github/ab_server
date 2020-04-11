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

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "context.h"
#include "eip.h"
#include "slice.h"
#include "tcp_server.h"
#include "utils.h"


static void usage(void);
static void process_args(int argc, const char **argv, context_s *context);
static void parse_path(const char *path, context_s *context);
static void parse_tag(const char *tag, context_s *context);
static slice_s request_handler(slice_s input, slice_s output, void *context);

int main(int argc, const char **argv)
{
    tcp_server_p server = NULL;
    uint8_t buf[4200];  /* CIP only allows 4002 for the CIP request, but there is overhead. */
    slice_s server_buf = slice_make(buf, sizeof(buf));
    context_s context;

    debug_off();

    /* clear out context to make sure we do not get gremlins */
    memset(&context, 0, sizeof(context));

    /* set the random seed. */
    srand(time(NULL));

    process_args(argc, argv, &context);

    /* open a server connection and listen on the right port. */
    server = tcp_server_create("0.0.0.0", "44818", server_buf, request_handler, &context);

    tcp_server_start(server);

    tcp_server_destroy(server);

    return 0;
}


void usage(void)
{
    fprintf(stderr, "Usage: ab_server --plc=<plc_type> [--path=<path>] --tag=<tag>\n"
                    "   <plc type> = one of \"ControlLogix\" or \"Micro800\".\n"
                    "   <path> = (required for ControlLogix) internal path to CPU in PLC.  E.g. \"1,0\".\n"
                    "\n"
                    "    Tags are in the format: <name>:<type>[<sizes>] where:\n"
                    "        <name> is alphanumeric, starting with an alpha character.\n"
                    "        <type> is one of:\n"
                    "            INT - 2-byte signed integer.  Requires array size(s).\n"
                    "            DINT - 4-byte signed integer.  Requires array size(s).\n"
                    "            LINT - 8-byte signed integer.  Requires array size(s).\n"
                    "            REAL - 4-byte floating point number.  Requires array size(s).\n"
                    "            LREAL - 8-byte floating point number.  Requires array size(s).\n"
                    "\n"
                    "        <sizes>> field is one or more (up to 3) numbers separated by commas.\n"
                    "\n"
                    "Example: ab_server --plc=ControlLogix --path=1,0 --tag=MyTag:DINT[10,10]\n");

    exit(1);
}


void process_args(int argc, const char **argv, context_s *context)
{
    bool has_path = false;
    bool needs_path = false;
    bool has_plc = false;
    bool has_tag = false;

    for(int i=0; i < argc; i++) {
        if(strncmp(argv[i],"--plc=",6) == 0) {
            if(has_plc) {
                fprintf(stderr, "PLC type can only be specified once!\n");
                usage();
            }

            if(strcasecmp(&(argv[i][6]), "ControlLogix") == 0) {
                fprintf(stderr, "Selecting ControlLogix simulator.\n");
                context->plc_type = PLC_CONTROL_LOGIX;
                needs_path = true;
                has_plc = true;
            } else if(strcasecmp(&(argv[i][6]), "Micro800") == 0) {
                fprintf(stderr, "Selecting Micro8xx simulator.\n");
                context->plc_type = PLC_MICRO800;
                needs_path = false;
                has_plc = true;
            } else {
                fprintf(stderr, "Unsupported PLC type %s!\n", &(argv[i][6]));
                usage();
            }
        }

        if(strncmp(argv[i],"--path=",7) == 0) {
            parse_path(&(argv[i][7]), context);
            has_path = true;
        }

        if(strncmp(argv[i],"--tag=",6) == 0) {
            parse_tag(&(argv[i][6]), context);
            has_tag = true;
        }

        if(strcmp(argv[i],"--debug") == 0) {
            debug_on();
            has_tag = true;
        }
    }

    if(needs_path && !has_path) {
        fprintf(stderr, "This PLC type requires a path argument.\n");
        usage();
    }

    if(!has_plc) {
        fprintf(stderr, "You must pass a --plc= argument!\n");
        usage();
    }

    if(!has_tag) {
        fprintf(stderr, "You must define at least one tag.\n");
        usage();
    }
}



void parse_path(const char *path, context_s *context)
{
    if(sscanf(path, "%d,%d", &(context->path[0]), &(context->path[1])) == 2) {
        info("Processed path %d,%d.", context->path[0], context->path[1]);
    } else {
        fprintf(stderr, "Error processing path \"%s\"!  Path must be two numbers separated by a comma.\n", path);
        usage();
    }
}


/*
 * Tags are in the format:
 *    <name>:<type>[<sizes>]
 * 
 * Where name is alphanumeric, starting with an alpha character.
 * 
 * Type is one of:
 *     INT - 2-byte signed integer.  Requires array size(s).
 *     DINT - 4-byte signed integer.  Requires array size(s).
 *     LINT - 8-byte signed integer.  Requires array size(s).
 *     REAL - 4-byte floating point number.  Requires array size(s).
 *     LREAL - 8-byte floating point number.  Requires array size(s).
 *  
 * Array size field is one or more (up to 3) numbers separated by commas.
 */

void parse_tag(const char *tag_str, context_s *context)
{
    tag_def_s *tag = calloc(1, sizeof(*tag));
    char *type_str = NULL;
    char *dim_str = NULL;
    int num_dims = 0;

    if(!tag) {
        error("Unable to allocate memory for new tag!");
    }

    if(sscanf(tag_str,"%m[a-zA-Z0-9_]:%m[A-Z][%m[0-9,]]", &(tag->name), &type_str, &dim_str) != 3) {
        fprintf(stderr, "Tag format is incorrect in \"%s\"!\n", tag_str);
        if(tag->name) {
            info("Tag name: %s\n", tag->name);
        }

        if(type_str) {
            info("type_str: %s\n", type_str);
        }

        if(dim_str) {
            info("dim_str: %s\n", dim_str);
        }

        usage();
    }

    /* match the type. */
    if(strcasecmp(type_str, "SINT") == 0) {
        tag->tag_type = TAG_TYPE_SINT;
        tag->elem_size = 1;
    } else if(strcasecmp(type_str, "INT") == 0) {
        tag->tag_type = TAG_TYPE_INT;
        tag->elem_size = 2;
    } else if(strcasecmp(type_str, "DINT") == 0) {
        tag->tag_type = TAG_TYPE_DINT;
        tag->elem_size = 4;
    } else if(strcasecmp(type_str, "LINT") == 0) {
        tag->tag_type = TAG_TYPE_LINT;
        tag->elem_size = 8;
    } else if(strcasecmp(type_str, "REAL") == 0) {
        tag->tag_type = TAG_TYPE_REAL;
        tag->elem_size = 4;
    } else if(strcasecmp(type_str, "LREAL") == 0) {
        tag->tag_type = TAG_TYPE_LREAL;
        tag->elem_size = 8;
    } else {
        fprintf(stderr, "Unsupported tag type \"%s\"!", type_str);
        free(type_str);
        free(dim_str);
        usage();
    }

    free(type_str);

    /* match the dimensions. */
    tag->dimensions[0] = 0;
    tag->dimensions[1] = 0;
    tag->dimensions[2] = 0;
    num_dims = sscanf(dim_str, "%d,%d,%d,%*d", &tag->dimensions[0], &tag->dimensions[1], &tag->dimensions[2]);

    free(dim_str);

    if(num_dims < 1 || num_dims > 3) {
        fprintf(stderr, "Tag dimensions must have at least one dimension non-zero and no more than three dimensions.");
        usage();
    }

    /* check the dimensions. */
    if(tag->dimensions[0] <= 0) {
        fprintf(stderr, "The first tag dimension must be at least 1 and may not be negative!\n");
        usage();
    } else {
        tag->elem_count = tag->dimensions[0];
    }

    if(tag->dimensions[1] > 0) {
        tag->elem_count *= tag->dimensions[1];
    }

    if(tag->dimensions[2] > 0) {
        tag->elem_count *= tag->dimensions[2];
    }

    /* allocate the tag data array. */
    info("allocating %d elements of %d bytes each.", tag->elem_count, tag->elem_size);
    tag->data = calloc(tag->elem_count, (size_t)tag->elem_size);

    if(!tag->data) {
        free(tag->name);
    }

    info("Processed \"%s\" into tag %s of type %x with dimensions (%d, %d, %d).", tag_str, tag->name, tag->tag_type, tag->dimensions[0], tag->dimensions[1], tag->dimensions[2]);

    /* add the tag to the list. */
    tag->next_tag = context->tags;
    context->tags = tag;
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
