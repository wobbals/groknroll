//
//  main.c
//  groknroll
//
//  Created by Charley Robinson on 3/30/15.
//  Copyright (c) 2015 TokBox, Inc. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mongoose.h"
#include <zlib.h>
#include <stdint.h>

static char decompressor_input[4096];
static z_stream decompressor;

static int try_flush_decompressor_output(int flush) {
    if (0 == decompressor.avail_out || flush) {
        printf("%s", decompressor_input);
        bzero(decompressor_input, sizeof(decompressor_input));
        decompressor.avail_out = sizeof(decompressor_input) - 1;
        decompressor.next_out = (Bytef*)decompressor_input;
        return 1;
    }
    return 0;
}

static int decompress(Bytef* bytes, size_t length) {
    int flush = 0;
    // cycle compressor input & output
    decompressor.next_in = bytes;
    decompressor.avail_in = (uInt)length;
    while (0 < decompressor.avail_in) {
        try_flush_decompressor_output(flush);
        int result = inflate(&decompressor, Z_NO_FLUSH);
        if (Z_OK != result) {
            printf("deflate error %d: %s\n", result, decompressor.msg);
        }
    }
    
    return 0;
}

static int handle_request(struct mg_connection *conn) {
    if (conn->is_websocket) {
        // This handler is called for each incoming websocket frame, one or more
        // times for connection lifetime.
        decompress((Bytef*)conn->content, conn->content_len);
        return conn->content_len == 4 && !memcmp(conn->content, "exit", 4) ?
        MG_FALSE : MG_TRUE;
    }
    
    return MG_FALSE;
}

static int ev_handler(struct mg_connection *conn, enum mg_event ev) {
    switch (ev) {
        case MG_AUTH: return MG_TRUE;
        case MG_REQUEST: return handle_request(conn);
        default: return MG_FALSE;
    }
}

int main(void) {
    decompressor.zalloc = Z_NULL;
    decompressor.zfree = Z_NULL;
    decompressor.opaque = Z_NULL;
    bzero(&decompressor, sizeof(z_stream));
    bzero(decompressor_input, sizeof(decompressor_input));
    int status = inflateInit(&decompressor);
    if (Z_OK != status) {
        printf("defalateInit returned %d: %s\n", status, decompressor.msg);
    }
    
    struct mg_server *server = mg_create_server(NULL, ev_handler);
    time_t current_timer = 0, last_timer = time(NULL);
    
    mg_set_option(server, "listening_port", "8080");
    
    printf("Started on port %s\n", mg_get_option(server, "listening_port"));
    for (;;) {
        mg_poll_server(server, 100);
        current_timer = time(NULL);
        if (current_timer - last_timer > 0) {
            last_timer = current_timer;
        }
    }
    
    mg_destroy_server(&server);
    return 0;
}
