#!/bin/sh

cc -O0 -g -o test_events -DTEST -I doom/ -I . -I .. \
    $(pkg-config --cflags SDL2_net SDL2_mixer) \
    m_argv.c z_native.c cJSON.c i_system.c m_misc.c i_timer.c m_config.c tables.c \
    doom/x_events.c x_events_test.c \
    $(pkg-config --libs SDL2_net SDL2_mixer)
