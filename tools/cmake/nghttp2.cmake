#add_definitions(-DBUILDING_NGHTTP2)
include_directories(
    vendor/nghttp2/src/lib/includes
)

set(NGHTTP2_SOURCES
        vendor/nghttp2/src/lib/nghttp2_pq.c
        vendor/nghttp2/src/lib/nghttp2_map.c
        vendor/nghttp2/src/lib/nghttp2_queue.c
        vendor/nghttp2/src/lib/nghttp2_frame.c
        vendor/nghttp2/src/lib/nghttp2_buf.c
        vendor/nghttp2/src/lib/nghttp2_stream.c
        vendor/nghttp2/src/lib/nghttp2_outbound_item.c
        vendor/nghttp2/src/lib/nghttp2_session.c
        vendor/nghttp2/src/lib/nghttp2_submit.c
        vendor/nghttp2/src/lib/nghttp2_helper.c
        vendor/nghttp2/src/lib/nghttp2_npn.c
        vendor/nghttp2/src/lib/nghttp2_hd.c
        vendor/nghttp2/src/lib/nghttp2_hd_huffman.c
        vendor/nghttp2/src/lib/nghttp2_hd_huffman_data.c
        vendor/nghttp2/src/lib/nghttp2_version.c
        vendor/nghttp2/src/lib/nghttp2_priority_spec.c
        vendor/nghttp2/src/lib/nghttp2_option.c
        vendor/nghttp2/src/lib/nghttp2_callbacks.c
        vendor/nghttp2/src/lib/nghttp2_mem.c
        vendor/nghttp2/src/lib/nghttp2_http.c
        vendor/nghttp2/src/lib/nghttp2_rcbuf.c
        vendor/nghttp2/src/lib/nghttp2_debug.c
        )
add_library(nghttp2 STATIC ${NGHTTP2_SOURCES})
set_target_properties(nghttp2 PROPERTIES COMPILE_FLAGS -DBUILDING_NGHTTP2)
