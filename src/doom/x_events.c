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

#include "cJSON.h"

#include "m_config.h"
#include "m_fixed.h"
#include "x_events.h"

#define MAX_FILENAME_LEN 128

// Maximal size of our serialized JSON, picked to be < the typical MTU setting
#define JSON_BUFFER_LEN 1024

// Config Variables and related Macros
static int telemetry_enabled = 0;
static int telemetry_mode = FILE_MODE;
static char *telemetry_host = "localhost";
static int telemetry_port = 10666;

#define ASSERT_TELEMETRY_ON(...) if (!telemetry_enabled) return __VA_ARGS__

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
    case e_pickup_card:
        return "PICKUP_CARD";
    case e_armor_bonus:
        return "ARMOR_BONUS";
    case e_health_bonus:
        return "HEALTH_BONUS";
    case e_entered_sector:
        return "ENTER_SECTOR";
    case e_entered_subsector:
        return "ENTER_SUBSECTOR";
    case e_move:
        return "MOVE";
    }

    printf("XXX: Unknown event type: %d\n", ev);
    return "UNKNOWN_EVENT";
}

// Convert a mobj type into an enemy name string
const char* enemyTypeName(mobj_t* enemy) {
    switch (enemy->type)
    {
    case MT_POSSESSED:
        return "Soldier";
    case MT_SHOTGUY:
        return "Shotgun Soldier";
    case MT_VILE:
        return "Vile";
    case MT_SERGEANT:
        return "Demon";
    case MT_SHADOWS:
        return "Spectre";
    case MT_TROOP:
        return "Imp";
    case MT_TROOPSHOT:
        return "Imp Fireball";
    case MT_UNDEAD:
        return "Undead";
    case MT_SKULL:
        return "LostSoul";
    case MT_HEAD:
        return "Cacodemon";
    case MT_HEADSHOT:
        return "Cacodemon Fireball";
    case MT_BRUISER:
        return "Baron of Hell";
    case MT_BRUISERSHOT:
        return "Baron Fireball";
    case MT_BARREL:
        return "Barrel";
    case MT_ROCKET:
        return "Rocket";
    case MT_PLASMA:
        return "Plasma";
    default:
        printf("XXX: Unknown enemy type: %d\n", enemy->type);
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
    cJSON* actor = NULL;
    cJSON* frame = NULL;
    cJSON* target = NULL;
    cJSON* pos = NULL;
    size_t buflen = 0;

    ASSERT_TELEMETRY_ON();

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

    frame = cJSON_CreateObject();
    if (frame == NULL)
    {
        I_Error("unable to create frame JSON object?!");
    }
    cJSON_AddNumberToObject(frame, "millis", I_GetTimeMS());
    cJSON_AddNumberToObject(frame, "tic", I_GetTime());
    cJSON_AddItemToObject(json, "frame", frame);

    if (ev->actor != NULL)
    {
        actor = cJSON_CreateObject();
        pos = cJSON_CreateObject();
        if (actor == NULL || pos == NULL)
        {
            I_Error("unable to create actor/position JSON object(s)?!");
        }

        cJSON_AddNumberToObject(pos, "x", ev->actor->x);
        cJSON_AddNumberToObject(pos, "y", ev->actor->y);
        cJSON_AddNumberToObject(pos, "z", ev->actor->z);
        cJSON_AddNumberToObject(pos, "subsector",
                                (uintptr_t) guessActorLocation(ev->actor));
        cJSON_AddItemToObject(actor, "position", pos);

        if (ev->actor->player)
        {
            cJSON_AddStringToObject(actor, "type", "player");
            cJSON_AddNumberToObject(actor, "id", (uintptr_t) ev->actor);
        } else
        {
            cJSON_AddStringToObject(actor, "type", enemyTypeName(ev->actor));
            cJSON_AddNumberToObject(actor, "id", (uintptr_t) ev->actor);
        }
        cJSON_AddItemToObject(json, "actor", actor);
    }

    if (ev->target != NULL)
    {
        target = cJSON_CreateObject();
        if (target == NULL)
        {
            I_Error("unable to create target JSON object?!");
        }

        if (ev->target->player)
        {
            cJSON_AddStringToObject(target, "type", "player");
            cJSON_AddNumberToObject(target, "id", (uintptr_t) ev->target);
        } else
        {
            cJSON_AddStringToObject(target, "type", enemyTypeName(ev->target));
            cJSON_AddNumberToObject(target, "id", (uintptr_t) ev->target);
        }
        cJSON_AddItemToObject(json, "target", target);
    }

    if (cJSON_PrintPreallocated(json, jsonbuf, JSON_BUFFER_LEN, false))
    {
        // TODO: refactor out into log writer vs. udp network writer
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

void logEventWithExtraNumber(xevent_t *ev, const char* key, int value)
{
    cJSON *num = cJSON_CreateNumber(value);
    if (num == NULL)
    {
        I_Error("Failed to create JSON number!?!");
    }
    logEventWithExtra(ev, key, num);
    cJSON_Delete(num);
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

    ASSERT_TELEMETRY_ON(0);

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

    ASSERT_TELEMETRY_ON(0);

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

void X_LogExit(mobj_t *actor)
{
    xevent_t ev = { e_end_level, actor, NULL };
    logEvent(&ev);
}

void X_LogPlayerMove(mobj_t *player)
{
    xevent_t ev = { e_move, player, NULL };
    logEventWithExtraNumber(&ev, "angle", player->angle);
}

void X_LogEnemyMove(mobj_t *enemy)
{
    xevent_t ev = { e_move, enemy, NULL };
    logEventWithExtraNumber(&ev, "angle", enemy->angle);
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
    logEventWithExtraNumber(&ev, "weaponType", weapon);
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
    logEventWithExtraNumber(&ev, "damage", damage);
}

////

void X_LogSectorCrossing(mobj_t *actor)
{
    xevent_t ev = { e_entered_subsector, actor, NULL };
    logEvent(&ev);
}

////

void X_LogArmorBonus(player_t *player)
{
    xevent_t ev = { e_armor_bonus, player->mo, NULL };
    logEventWithExtraNumber(&ev, "armor", player->armorpoints);
}

void X_LogHealthBonus(player_t *player)
{
    xevent_t ev = { e_health_bonus, player->mo, NULL };
    logEventWithExtraNumber(&ev, "health", player->health);
}

void X_LogHealthPickup(player_t *player, int amount)
{
    xevent_t ev = { e_pickup_health, player->mo, NULL };
    logEventWithExtraNumber(&ev, "health", amount);
}

void X_LogArmorPickup(mobj_t *actor, int armortype)
{
    xevent_t ev = { e_pickup_armor, actor, NULL };
    logEventWithExtraNumber(&ev, "armorType", armortype);
}

void X_LogWeaponPickup(mobj_t *actor, weapontype_t weapon)
{
    xevent_t ev = { e_pickup_weapon, actor, NULL };
    logEventWithExtraNumber(&ev, "weaponType", weapon);
}

void X_LogCardPickup(player_t *player, card_t card)
{
    // xxx: card_t is an enum, we should resolve it in the future
    xevent_t ev = { e_pickup_card, player->mo, NULL };
    logEventWithExtraNumber(&ev, "card", card);
}

////

void X_BindTelemetryVariables()
{
    M_BindIntVariable("telemetry_enabled", &telemetry_enabled);
    M_BindIntVariable("telemetry_mode", &telemetry_mode);
    M_BindStringVariable("telemetry_host", &telemetry_host);
    M_BindIntVariable("telemetry_port", &telemetry_port);
}
