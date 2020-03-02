#!/bin/sh

#cc -O0 -g -o test_events -DTEST -I doom/ -I . -I .. \
#    -L/usr/local/lib -L/usr/X11R6/lib -lSDL2 \
#    m_argv.o z_native.o cJSON.o i_system.o m_misc.o i_timer.o m_config.o tables.o \
#    doom/x_events.c x_events_test.c

cc -O0 -g -o test_events -DTEST -I doom/ -I . -I .. -I /usr/local/include/SDL2 \
    -L/usr/local/lib -L/usr/X11R6/lib -lSDL2 \
    m_argv.c z_native.c cJSON.c i_system.c m_misc.c i_timer.c m_config.c tables.c \
    doom/x_events.c x_events_test.c
