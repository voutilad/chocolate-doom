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

#include "telemetry.h"

#define HELP_URL "https://github.com/voutilad/chocolate-doom/blob/personal/TELEMETRY.md"

static int telemetry_enabled = 1;

void ConfigTelemetry(TXT_UNCAST_ARG(widget), void *user_data)
{
    txt_window_t *window;

    window = TXT_NewWindow("Telemetry");

    TXT_SetWindowHelpURL(window, HELP_URL);

    TXT_AddWidgets(window, TXT_NewCheckBox("Enabled Telemetry",
                                           &telemetry_enabled),
                   NULL);
}

void BindTelemetryVariables(void)
{
    M_BindIntVariable("telemetry_enabled", &telemetry_enabled);
}
