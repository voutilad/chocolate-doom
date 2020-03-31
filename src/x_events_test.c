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
// DESCRIPTION:
//	Event logging framework and utils
//

#include <stdio.h>
#include "doom/p_mobj.h"
#include "doom/x_events.h"
#include "m_config.h"

int main()
{
    player_t p;
    mobj_t m1, m2, mp;
    m1.x = 10;
    m1.y = 20;
    m1.z = 0;
    m1.type = MT_SHOTGUY;
    m2.x = 11;
    m2.y = 22;
    m2.z = 1;
    m2.type = MT_BARREL;

    mp.x = 12;
    mp.y = 13;
    mp.z = 0;
    mp.player = &p;
    p.mo = &mp;

    // udp mode
    X_BindTelemetryVariables();
    M_SetVariable("telemetry_enabled", "1");
    M_SetVariable("telemetry_mode", "2");
    M_SetVariable("telemetry_host", "localhost");
    M_SetVariable("telemetry_port", "10666");

    if (X_InitTelemetry() < 1) {
        printf("failed to init log\n");
        return -1;
    }
    X_LogStart(69, 69, 1);
    X_LogArmorPickup(p.mo, 69);
    X_LogWeaponPickup(p.mo, wp_shotgun);
    X_LogEnemyMove(&m1);
    X_LogTargeted(&m1, &m2);
    X_LogEnemyKilled(&m1);

    X_LogPlayerMove(p.mo);

    X_LogPlayerDied(&m1);

    X_StopTelemetry();
    return 0;
}
