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

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "doom/p_mobj.h"
#include "doom/x_events.h"
#include "m_config.h"

#ifdef _WIN32
// Stub out this function
boolean D_IsIWADName(const char *name)
{
    return true;
}
#endif

int main()
{
    char* modes[] = { "1", "2", "3", NULL };
    int i;
    player_t p;
    mobj_t m1, m2, mp;
    m1.x = 10;
    m1.y = 20;
    m1.z = 0;
    m1.angle = 180;
    m1.type = MT_SHOTGUY;
    m1.health = 25;
    m1.player = NULL;

    m2.x = 11;
    m2.y = 22;
    m2.z = 1;
    m2.angle = 180;
    m2.type = MT_BARREL;
    m2.health = 10;
    m2.player = NULL;

    mp.x = 12;
    mp.y = 13;
    mp.z = 0;
    mp.angle = 180;
    mp.player = &p;
    p.mo = &mp;
    p.health = 10;
    p.armorpoints = 20;

#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
                            BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    AttachConsole(-1);
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    // udp mode
    X_BindTelemetryVariables();
    M_SetVariable("telemetry_enabled", "1");
    M_SetVariable("telemetry_udp_host", "localhost");
    M_SetVariable("telemetry_udp_port", "10666");

    for (i = 0; modes[i] != NULL; i++)
    {
        printf("----- TESTING MODE %s -----\n", modes[i]);
        M_SetVariable("telemetry_mode", modes[i]);
        if (X_InitTelemetry() < 1) {
            printf("failed to init log\n");
            return -1;
        }
        printf("...log start\n");
        X_LogStart(&p, 69, 69, 1);

        printf("...log armor\n");
        X_LogArmorPickup(p.mo, 69);

        printf("...log weapon\n");
        X_LogWeaponPickup(p.mo, wp_shotgun);

        printf("...log enemy move\n");
        X_LogMove(&m1);

        printf("...log targeted\n");
        m1.target = &m2;
        X_LogTargeted(&m1, &m2);

        printf("...log enemy killed\n");
        X_LogEnemyKilled(&p, &m1);

        printf("...log player move\n");
        X_LogMove(p.mo);

        printf("...log player died\n");
        X_LogPlayerDied(&p, &m1);

        printf("...stop\n");
        X_StopTelemetry();
    }
    return 0;
}
