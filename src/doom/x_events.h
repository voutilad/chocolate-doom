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

#ifndef __X_EVENTS__
#define __X_EVENTS__

#include "doomdef.h"
#include "p_mobj.h"
#include "d_player.h"
#include "i_system.h"
#include "m_misc.h"
#include "r_defs.h"
#include "r_main.h"

// Our logger types...maybe move to an enum later?
#define FILE_MODE       1
#define UDP_MODE        2
#define KAFKA_MODE      3

typedef enum
{
    e_start_level,
    e_end_level,
    e_targeted,
    e_killed,
    e_attack,
    e_counterattack,
    e_hit,
    e_move,
    e_pickup_weapon,
    e_pickup_health,
    e_pickup_armor,
    e_pickup_card,
    e_health_bonus,
    e_armor_bonus,
    e_entered_sector,
    e_entered_subsector
} xeventtype_t;

// A basic event data type, with optional actor and target references
typedef struct xevent_s
{
    // Type of event we're recording
    xeventtype_t     ev_type;

    // Moveable object that's the source of the event
    // (depending on type of event)
    mobj_t*         actor;

    // Moveable object that's the target
    // (optional, may be NULL)
    mobj_t*         target;
} xevent_t;

// Our Logger instance ties together a group of functions to keep it generic
typedef struct {
    // The type of Logger, e.g. FILE_MODE or UDP_MODE.
    int type;

    // Initialization function, should be called only once.
    int (*init)(void);

    // Closing/cleanup function, should be called only at shutdown.
    int (*close)(void);

    // Writes to the backing logger, should return bytes written successfully
    // or < 0 on error.
    int (*write)(char* buf, size_t len);
} Logger;

int X_InitTelemetry(void);
void X_StopTelemetry(void);

void X_LogStart(int ep, int level, skill_t mode);
void X_LogExit(mobj_t *actor);

void X_LogMove(mobj_t *actor);

void X_LogEnemyKilled(mobj_t *victim);
void X_LogPlayerDied(mobj_t *killer);

void X_LogTargeted(mobj_t *actor, mobj_t *target);
void X_LogPlayerAttack(mobj_t *player, weapontype_t weapon);
void X_LogAttack(mobj_t *source, mobj_t *target);
void X_LogCounterAttack(mobj_t *enemy, mobj_t *target);
void X_LogHit(mobj_t *source, mobj_t *target, int damage);

void X_LogSectorCrossing(mobj_t *actor);

void X_LogHealthBonus(player_t *player);
void X_LogArmorBonus(player_t *player);

void X_LogHealthPickup(player_t *player, int amount);
void X_LogArmorPickup(mobj_t *actor, int armortype);
void X_LogWeaponPickup(mobj_t *actor, weapontype_t weapon);
void X_LogCardPickup(player_t *player, card_t card);

void X_BindTelemetryVariables(void);

#endif
