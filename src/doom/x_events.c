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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "x_events.h"

#include "i_system.h"
#include "m_misc.h"
#include "r_defs.h"
#include "r_main.h"

#include "cJSON.h"

#define MAX_FILENAME_LEN 128
#define JSON_BUFFER_LEN 4096

// Used to prevent constant malloc when printing json
char* jsonbuf = NULL;

// reference to event log file
int log_fd = -1;


// Convert an event type enum into a string representation
const char* eventTypeName(xeventtype_t ev)
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

// Convert a mobj type into an enemy name string
const char* enemyTypeName(mobj_t* enemy) {
    // printf("XXX enemy type: %d\n", enemy->type);
    switch (enemy->type)
    {
        case MT_POSSESSED:
            return "Soldier";
        case MT_SHOTGUY:
            return "ShotgunSoldier";
        case MT_VILE:
            return "Vile";
        case MT_TROOP:
            return "Imp";
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
    cJSON* json;

    json = cJSON_CreateObject();
    if (json == NULL)
    {
        I_Error("unable to create JSON object?!");
        return;
    }

    cJSON_AddStringToObject(json, "type", eventTypeName(ev->ev_type));
    cJSON_AddNumberToObject(json, "time_ms", I_GetTimeMS());

    if (ev->actor != NULL)
    {
        if (ev->actor->player)
        {
            cJSON_AddStringToObject(json, "actor", "player");
            cJSON_AddNumberToObject(json, "actorId", (uintptr_t) ev->actor);
        } else
        {
            cJSON_AddStringToObject(json, "actor", enemyTypeName(ev->actor));
            cJSON_AddNumberToObject(json, "actorId", (uintptr_t) ev->actor);
        }
    }

    if (ev->target != NULL)
    {
        if (ev->target->player)
        {
            cJSON_AddStringToObject(json, "target", "player");
            cJSON_AddNumberToObject(json, "targetId", (uintptr_t) ev->target);
        } else
        {
            cJSON_AddStringToObject(json, "target", enemyTypeName(ev->target));
            cJSON_AddNumberToObject(json, "targetId", (uintptr_t) ev->target);
        }
    }

    if (cJSON_PrintPreallocated(json, jsonbuf, JSON_BUFFER_LEN, false))
    {
        write(log_fd, jsonbuf, JSON_BUFFER_LEN);
        write(log_fd, "\n", 1);
        memset(jsonbuf, 0, JSON_BUFFER_LEN);
    } else
    {
        I_Error("failed to write event to log!");
    }
    cJSON_free(json);
}

////////

// Initialize event logging framework.
// Should only be called once!
int X_InitLog(int episode, int map)
{
    time_t t;
    char* filename;

    printf("XXX: X_InitLog(%d, %d) called\n", episode, map);

    // Don't init twice
    if (log_fd > -1)
    {
        printf("XXX: event logfile is already opened!\n");
        return -1;
    }

    // xxx: danger danger we gon cray cray
    t = time(NULL);
    filename = malloc(MAX_FILENAME_LEN);
    if (filename == NULL)
    {
        I_Error("failed to malloc room for filename");
    }

    // blind cast to int for now...who cares?
    M_snprintf(filename, MAX_FILENAME_LEN, "doom-e%dm%d-%d.log", episode, map, (int) t);
    log_fd = open(filename, O_WRONLY | O_APPEND | O_CREAT);
    free(filename);

#ifndef _WIN32
    if (fchmod(log_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != 0)
    {
        I_Error("couldn't chmod log file!");
    }
#endif

    if (log_fd < 0)
    {
        I_Error("failed to open logfile!");
    }

    // initialize json write buffer
    jsonbuf = calloc(JSON_BUFFER_LEN, sizeof(char));
    if (jsonbuf == NULL)
    {
        I_Error("failed to allocate space for json buffer");
    }

    return 0;
}

// Shutdown event logging.
int X_CloseLog()
{
    int r = 0;

    printf("XXX: X_CloseLog() called\n");

    if (close(log_fd) != 0)
    {
        I_Error("failed to close doom log file");
        r = -1;
    }

    log_fd = -1;

    if (jsonbuf != NULL) {
        free(jsonbuf);
	jsonbuf = NULL;
    } else {
        printf("XXX: json buffer not allocated?!\n");
        r = r - 2;
    }

    return r;
}


////////

void X_LogStart(int ep, int level, skill_t mode)
{
    xevent_t ev = { e_start_level, NULL, NULL };
    logEvent(&ev);
}

void X_LogExit()
{
    xevent_t ev = { e_end_level, NULL, NULL };
    logEvent(&ev);
}

void X_LogPosition(mobj_t *actor)
{

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
