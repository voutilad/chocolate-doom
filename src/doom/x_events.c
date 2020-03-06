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

#define MAX_FILENAME_LEN 128

// Maximal size of our serialized JSON, picked to be < the typical MTU setting
#define JSON_BUFFER_LEN 1024

// Used to prevent constant malloc when printing json
char* jsonbuf = NULL;

// reference to event log file
static int log_fd = -1;


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
        case e_move:
            return "MOVE";
    }
    return "UNKNOWN_EVENT";
}

// Convert a mobj type into an enemy name string
const char* enemyTypeName(mobj_t* enemy) {
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

#ifdef TEST
sector_t test_sector = {};
subsector_t test_subsector = { &test_sector, 0, 0 };
#endif

// Try to determine the location of an Actor
subsector_t* guessActorLocation(mobj_t *actor)
{
#ifdef TEST
    return &test_subsector;
#else
    return R_PointInSubsector(actor->x, actor->y);
#endif
}

void logEventWithExtra(xevent_t *ev, const char* key, cJSON* extra)
{
    cJSON* json = NULL;
    cJSON* pos = NULL;
    size_t buflen = 0;

    json = cJSON_CreateObject();
    if (json == NULL)
    {
        I_Error("unable to create JSON object?!");
    }

    if (key != NULL && extra != NULL)
    {
        // XXX: The "extra" metadata is added as a reference so we don't
        // transfer ownership, avoiding possible double-free's in functions
        // calling logEventWithExtra()
        cJSON_AddItemReferenceToObject(json, key, extra);
    }

    cJSON_AddStringToObject(json, "type", eventTypeName(ev->ev_type));
    cJSON_AddNumberToObject(json, "time_ms", I_GetTimeMS());

    if (ev->actor != NULL)
    {
        pos = cJSON_CreateObject();
        if (pos == NULL)
        {
            I_Error("unable to create position JSON object?!");
        }
        cJSON_AddNumberToObject(pos, "x", ev->actor->x);
        cJSON_AddNumberToObject(pos, "y", ev->actor->y);
        cJSON_AddNumberToObject(pos, "z", ev->actor->z);
        cJSON_AddNumberToObject(pos, "subsector",
                                (uintptr_t) guessActorLocation(ev->actor));
        cJSON_AddItemToObject(json, "position", pos);

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
        buflen = strlen(jsonbuf);
        write(log_fd, jsonbuf, buflen);
        write(log_fd, "\n", 1);
        memset(jsonbuf, 0, JSON_BUFFER_LEN);
    }
    else
    {
        I_Error("failed to write event to log!");
    }

    if (json != NULL)
    {
        cJSON_Delete(json);
    }
}

void logEvent(xevent_t *ev)
{
    logEventWithExtra(ev, NULL, NULL);
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
    M_snprintf(filename, MAX_FILENAME_LEN, "doom-e%dm%d-%d.log",
               episode, map, (int) t);
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
    cJSON *json = cJSON_CreateObject();
    if (!json)
    {
        I_Error("failed to instantiate level json metadata!");
    }
    cJSON_AddNumberToObject(json, "episode", ep);
    cJSON_AddNumberToObject(json, "level", level);
    cJSON_AddNumberToObject(json, "difficulty", mode);

    xevent_t ev = { e_start_level, NULL, NULL };
    logEventWithExtra(&ev, "level", json);
    cJSON_Delete(json);
}

void X_LogExit()
{
    xevent_t ev = { e_end_level, NULL, NULL };
    logEvent(&ev);
}

void X_LogMove(mobj_t *actor)
{
#if X_LOG_MOVEMENT
    xevent_t ev = { e_move, actor, NULL };
    logEvent(&ev);
#endif
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
    xevent_t ev = { e_targeted, actor, target };
    logEvent(&ev);
}

void X_LogPlayerAttack(mobj_t *player, weapontype_t weapon)
{
    xevent_t ev = { e_attack, player, NULL };
    logEventWithExtra(&ev, "weaponType", cJSON_CreateNumber(weapon));
}

void X_LogAttack(mobj_t *source, mobj_t *target)
{
    xevent_t ev = { e_attack, source, target };
    logEvent(&ev);
}

void X_LogCounterAttack(mobj_t *enemy, mobj_t *target)
{
    xevent_t ev = { e_counterattack, enemy, target };
    logEvent(&ev);
}

void X_LogHit(mobj_t *source, mobj_t *target, int damage)
{
    xevent_t ev = { e_hit, source, target };
    logEventWithExtra(&ev, "damage", cJSON_CreateNumber(damage));
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
    cJSON *type = cJSON_CreateNumber(armortype);
    xevent_t ev = { e_pickup_armor, NULL, NULL };
    logEventWithExtra(&ev, "armorType", type);
    cJSON_Delete(type);
}

void X_LogWeaponPickup(weapontype_t weapon)
{
    cJSON *w = cJSON_CreateNumber(weapon);
    xevent_t ev = { e_pickup_weapon, NULL, NULL };
    logEventWithExtra(&ev, "weaponType", w);
    cJSON_Delete(w);
}
