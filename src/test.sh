#!/bin/sh

cc -O0 -g -o test_events -DTEST -DX_LOG_MOVEMENT -I doom/ -I . -I .. \
    $(pkg-config --cflags --libs SDL2_net SDL2_mixer) \
    m_argv.c z_native.c cJSON.c i_system.c m_misc.c i_timer.c m_config.c tables.c \
    doom/x_events.c x_events_test.c
