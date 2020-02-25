//
// Copyright(C) 2020 Dave Vouitla
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

#include "r_defs.h"
#include "r_main.h"
#include "x_events.h"

char* eventTypeName(xeventtype_t ev) 
{
    switch (ev) {
        case e_start_level:
            return "START_LEVEL";
        case e_end_level:
            return "END_LEVEL";
        case e_targeted:
            return "TARGETED";
        case e_killed:
            return "KILLED";
        case e_attack:
            return "ATTACKED";
        case e_counterattack:
            return "COUNTERATTACKED";
        case e_hit:
            return "HIT";
        case e_pickup_armor:
            return "PICKUP_ARMOR";
        case e_pickup_health:
            return "PICKUP_HEALTH";
        case e_pickup_weapon:
            return "PICKUP_WEAPON";
        case e_entered_sector:
            return "ENTER_SECTOR";
        case e_entered_subsector:
            return "ENTER_SUBSECTOR";
    }
    return "UNKNOWN_EVENT";
}

char* enemyTypeName(mobj_t* enemy) {
    switch (enemy->type)
    {
        case MT_POSSESSED:
            return "Soldier";
        case MT_SHOTGUY:
            return "ShotgunSoldier";
        case MT_VILE:
            return "Imp???";
        case MT_UNDEAD:
            return "Undead";
        case MT_SKULL:
            return "LostSoul";
        case MT_BARREL:
            return "Barrel";
        default:
            return "UNKNOWN_ENEMY";
    }
}

// Try to determine the location of an Actor
subsector_t* guessActorLocation(mobj_t *actor)
{
    return R_PointInSubsector(actor->x, actor->y);
}

// This should be the one call to rule them all
void logEvent(xevent_t *ev)
{
    char* actor_name;
    char* target_name;
    void* actor = NULL;
    void* target = NULL;

    if (ev->actor == NULL)
    {
        actor_name = "(none)";
    }
    else if (ev->actor->player)
    {
        actor_name = "player";
        actor = ev->actor;
    } else
    {
        actor_name = enemyTypeName(ev->actor);
        actor = ev->actor;
    }

    if (ev->target == NULL)
    {
        target_name = "(none)";
    }
    else if (ev->target->player)
    {
        target_name = "player";
        target = ev->target;
    } else
    {
        target_name = enemyTypeName(ev->target);
        target = ev->target;
    }
    
    printf(">>> [%s] actor: %s (%p)  target: %s (%p)\n", 
        eventTypeName(ev->ev_type), 
        actor_name, actor,
        target_name, target);
}

////////

void X_LogStart(int ep, int level, skill_t mode)
{
    xevent_t ev;
    ev.ev_type = e_start_level;
    logEvent(&ev);
}

void X_LogExit()
{
    xevent_t ev = { e_end_level, NULL, NULL };
    logEvent(&ev);
}

///////////////

void X_LogEnemyKilled(mobj_t *victim)
{
    xevent_t ev = { e_killed, victim, NULL };
    logEvent(&ev);
}

void X_LogPlayerDied(mobj_t *killer)
{
    xevent_t ev = { e_killed, killer, killer->target };
    logEvent(&ev);
}

///////////////

void X_LogTargeted(mobj_t *actor, mobj_t *target)
{
    xevent_t ev;
    ev.ev_type = e_targeted;
    ev.actor = actor;
    ev.target = target;
    logEvent(&ev);
}

void X_LogAttack(mobj_t *source, mobj_t *target)
{
    xevent_t ev;
    ev.ev_type = e_attack;
    ev.actor = source;
    ev.target = target;
    logEvent(&ev);
}

void X_LogCounterAttack(mobj_t *enemy, mobj_t *target)
{
    xevent_t ev;
    ev.ev_type = e_counterattack;
    ev.actor = enemy;
    ev.target = target;
    logEvent(&ev);
}

void X_LogHit(mobj_t *source, mobj_t *target, int damage)
{
    xevent_t ev;
    ev.ev_type = e_hit;
    ev.actor = source;
    ev.target = target;
    logEvent(&ev);
}

////

void X_LogSectorCrossing(mobj_t *actor)
{
    xevent_t ev = { e_entered_subsector, actor, NULL };    
    logEvent(&ev);
}

////

void X_LogArmorPickup(int armortype)
{
    xevent_t ev = { e_pickup_armor, NULL, NULL };
    logEvent(&ev);
}

void X_LogWeaponPickup(weapontype_t weapon)
{
    xevent_t ev = { e_pickup_weapon, NULL, NULL };
    logEvent(&ev);
}