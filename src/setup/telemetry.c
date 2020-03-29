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
static char *telemetry_host = "localhost";
static int telemetry_port = 10666;

void ConfigTelemetry(TXT_UNCAST_ARG(widget), void *user_data)
{
    txt_window_t *window;

    window = TXT_NewWindow("Telemetry");

    TXT_SetWindowHelpURL(window, HELP_URL);

    TXT_AddWidgets(window,
                   TXT_NewCheckBox("Enabled Telemetry", &telemetry_enabled),
                   TXT_NewSeparator("Telemetry Mode"),
                   TXT_NewRadioButton("File system", &telemetry_mode, FILE_MODE),
                   TXT_NewRadioButton("UDP", &telemetry_mode, UDP_MODE),
                   TXT_NewSeparator("UDP (IPv4 Only)"),
                   TXT_NewHorizBox(TXT_NewLabel(" Host/IP:  "),
                                   TXT_NewInputBox(&telemetry_host, 50),
                                   NULL),
                   TXT_NewHorizBox(TXT_NewLabel("UDP port:  "),
                                   TXT_NewIntInputBox(&telemetry_port, 6),
                                   NULL),
                   NULL);
}

void BindTelemetryVariables(void)
{
    M_BindIntVariable("telemetry_enabled", &telemetry_enabled);
    M_BindIntVariable("telemetry_mode", &telemetry_mode);
    M_BindStringVariable("telemetry_host", &telemetry_host);
    M_BindIntVariable("telemetry_port", &telemetry_port);
}
