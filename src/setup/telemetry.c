//
// Copyright(C) 2020 Dave Voutila
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include "m_config.h"
#include "textscreen.h"

#include "doom/x_events.h"

#include "telemetry.h"

#define HELP_URL "https://github.com/voutilad/chocolate-doom/blob/personal/TELEMETRY.md"

static int telemetry_enabled = 1;
static int telemetry_mode = FILE_MODE;

static char *udp_host = "localhost";
static int udp_port = 10666;

#ifdef HAVE_LIBRDKAFKA
static char *kafka_topic = "doom-telemetry";
static char *kafka_brokers = "localhost:9092";
#endif

#ifdef HAVE_TLS
static char *ws_host = "localhost";
static int ws_port = 8000;
static int ws_tls_enabled = 1;
#endif

void ConfigTelemetry(TXT_UNCAST_ARG(widget), void *user_data)
{
    txt_window_t *window;

    window = TXT_NewWindow("Telemetry");

    TXT_SetWindowHelpURL(window, HELP_URL);

    TXT_AddWidgets(window,
                   TXT_NewCheckBox("Enable Telemetry", &telemetry_enabled),
                   TXT_NewSeparator("Telemetry Mode"),
                   TXT_NewRadioButton("File system", &telemetry_mode, FILE_MODE),
                   TXT_NewRadioButton("UDP", &telemetry_mode, UDP_MODE),
#ifdef HAVE_LIBRDKAFKA
                   TXT_NewRadioButton("Kafka", &telemetry_mode, KAFKA_MODE),
#endif
#ifdef HAVE_TLS
                   TXT_NewRadioButton("Websockets", &telemetry_mode, WEBSOCKET_MODE);
#endif
                   TXT_NewSeparator("UDP (IPv4 Only)"),
                   TXT_NewHorizBox(TXT_NewLabel("Host/IP:  "),
                                   TXT_NewInputBox(&udp_host, 50),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel("   Port:  "),
                                   TXT_NewIntInputBox(&udp_port, 6),
                                   NULL),
#ifdef HAVE_LIBRDKAFKA
                   TXT_NewSeparator("Kafka"),
                   TXT_NewHorizBox(TXT_NewLabel("  Topic:  "),
                                   TXT_NewInputBox(&kafka_topic, 50),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel("Brokers:  "),
                                   TXT_NewInputBox(&kafka_brokers, 50),
                                   NULL),
#endif
                   NULL);
}

void BindTelemetryVariables(void)
{
    M_BindIntVariable("telemetry_enabled", &telemetry_enabled);
    M_BindIntVariable("telemetry_mode", &telemetry_mode);
    M_BindStringVariable("telemetry_udp_host", &udp_host);
    M_BindIntVariable("telemetry_udp_port", &udp_port);
#ifdef HAVE_LIBRDKAFKA
    M_BindStringVariable("telemetry_kafka_topic", &kafka_topic);
    M_BindStringVariable("telemetry_kafka_brokers", &kafka_brokers);
#endif
#ifdef HAVE_TLS
    M_BindStringVariable("telemetry_ws_host", &ws_host);
    M_BindIntVariable("telemetry_ws_port", &ws_port);
    M_BindIntVariable("telementry_ws_tls_enabled", &ws_tls_enabled);
#endif
}
