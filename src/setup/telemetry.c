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

#include "textscreen.h"
#include "m_config.h"

#include "doom/x_events.h"

#include "telemetry.h"

#define HELP_URL "https://github.com/voutilad/chocolate-doom/blob/personal/TELEMETRY.md"

static int telemetry_enabled = 0;
static int telemetry_mode = FILE_MODE;

static char *udp_host = NULL;
static int udp_port = 10666;

#ifdef HAVE_LIBRDKAFKA
static char *kafka_topic = NULL;
static char *kafka_brokers = NULL;
static int kafka_ssl = 0;
#ifdef HAVE_LIBSASL2
static char *kafka_sasl_username = NULL;
static char *kafka_sasl_password = NULL;
static int kafka_sasl_mechanism = SASL_PLAIN;
#endif
#endif

#ifdef HAVE_LIBTLS
static char *ws_host = NULL;
static int ws_port = 8000;
static char *ws_path = NULL;
static int ws_tls_enabled = 1;
#ifdef HAVE_MQTT
// TODO: do we need a param here?
#endif /* HAVE_MQTT */
#endif /* HAVE_LIBTLS */

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
#ifdef HAVE_LIBTLS
                   TXT_NewRadioButton("WebSockets", &telemetry_mode, WEBSOCKET_MODE),
#ifdef HAVE_MQTT
                   TXT_NewRadioButton("MQTTv3 over WebSockets", &telemetry_mode,
                                      MQTT_MODE),
                   TXT_NewSeparator("MQTT"),
                   TXT_NewHorizBox(TXT_NewLabel("TBD"), NULL),
#endif /* HAVE_MQTT */
                   TXT_NewSeparator("WebSockets"),
                   TXT_NewHorizBox(TXT_NewLabel(" Host/IP: "),
                                   TXT_NewInputBox(&ws_host, 60),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel("    Port: "),
                                   TXT_NewIntInputBox(&ws_port, 6),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel("    Path: "),
                                   TXT_NewInputBox(&ws_path, 44),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewCheckBox("Uses TLS?", &ws_tls_enabled),
                                   NULL),
// UDP
                   TXT_NewSeparator("UDP (IPv4 Only)"),
                   TXT_NewHorizBox(TXT_NewLabel("Host/IP:  "),
                                   TXT_NewInputBox(&udp_host, 50),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel("   Port:  "),
                                   TXT_NewIntInputBox(&udp_port, 6),
                                   NULL),
#endif /* HAVE_LIBTLS */
#ifdef HAVE_LIBRDKAFKA
                   TXT_NewSeparator("Kafka"),
                   TXT_NewHorizBox(TXT_NewLabel("    Topic:  "),
                                   TXT_NewInputBox(&kafka_topic, 50),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel("  Brokers:  "),
                                   TXT_NewInputBox(&kafka_brokers, 70),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel("  Use SSL:  "),
                                   TXT_NewRadioButton("No", &kafka_ssl, 0),
                                   TXT_NewRadioButton("Yes", &kafka_ssl, 1),
                                   NULL),
#ifdef HAVE_LIBSASL2
                   TXT_NewHorizBox(TXT_NewLabel("     User:  "),
                                   TXT_NewInputBox(&kafka_sasl_username, 50),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel(" Password:  "),
                                   TXT_NewInputBox(&kafka_sasl_password, 50),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel("Mechanism:  "),
                                   TXT_NewRadioButton("PLAIN", &kafka_sasl_mechanism, SASL_PLAIN),
                                   TXT_NewRadioButton("SCRAM-SHA-256", &kafka_sasl_mechanism, SCRAM_SHA_256),
                                   TXT_NewRadioButton("SCRAM-SHA-512", &kafka_sasl_mechanism, SCRAM_SHA_512),
                                   NULL),
#endif /* HAVE_LIBSASL2 */
#endif /* HAVE_LIBRDKAFKA */
                   NULL);
}

void BindTelemetryVariables(void)
{
    M_BindIntVariable("telemetry_enabled",              &telemetry_enabled);
    M_BindIntVariable("telemetry_mode",                 &telemetry_mode);
    M_BindStringVariable("telemetry_udp_host",          &udp_host);
    M_BindIntVariable("telemetry_udp_port",             &udp_port);

#ifdef HAVE_LIBRDKAFKA
    M_BindStringVariable("telemetry_kafka_topic",       &kafka_topic);
    M_BindStringVariable("telemetry_kafka_brokers",     &kafka_brokers);
    M_BindIntVariable("telemetry_kafka_ssl",            &kafka_ssl);
#ifdef HAVE_LIBSASL2
    M_BindStringVariable("telemetry_kafka_username",    &kafka_sasl_username);
    M_BindStringVariable("telemetry_kafka_password",    &kafka_sasl_password);
    M_BindIntVariable("telemetry_kafka_sasl_mechanism", &kafka_sasl_mechanism);
#endif // HAVE_LIBSASL2
#endif // HAVE_LIBRDKAFKA

#ifdef HAVE_LIBTLS
    M_BindStringVariable("telemetry_ws_host",           &ws_host);
    M_BindIntVariable("telemetry_ws_port",              &ws_port);
    M_BindStringVariable("telemetry_ws_path",           &ws_path);
    M_BindIntVariable("telemetry_ws_tls_enabled",       &ws_tls_enabled);
#endif
}
