#!/bin/sh

cc -o test_events -DTEST -I doom/ -I . -I .. -lSDL2 \
    m_argv.o z_zone.o cJSON.o i_system.o m_misc.o i_timer.o m_config.o tables.o \
    doom/x_events.c x_events_test.c