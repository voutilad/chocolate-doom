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

// toggle to turn on very very very verbose movement logging
#define X_LOG_MOVEMENT 0

#include "doomdef.h"
#include "m_fixed.h"
#include "p_mobj.h"
#include "d_player.h"

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
    e_entered_sector,
    e_entered_subsector
} xeventtype_t;

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

int X_InitLog(int episode, int map);
int X_CloseLog();

void X_LogStart(int ep, int level, skill_t mode);
void X_LogExit();

void X_LogMove(mobj_t *actor);

void X_LogEnemyKilled(mobj_t *victim);
void X_LogPlayerDied(mobj_t *killer);

void X_LogTargeted(mobj_t *actor, mobj_t *target);
void X_LogPlayerAttack(mobj_t *player, weapontype_t weapon);
void X_LogAttack(mobj_t *source, mobj_t *target);
void X_LogCounterAttack(mobj_t *enemy, mobj_t *target);
void X_LogHit(mobj_t *source, mobj_t *target, int damage);

void X_LogSectorCrossing(mobj_t *actor);

void X_LogArmorPickup(int armortype);
void X_LogWeaponPickup(weapontype_t weapon);

#endif
