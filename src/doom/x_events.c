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
#include "SDL2/SDL_net.h"

#include "m_config.h"
#include "m_fixed.h"
#include "x_events.h"

#define MAX_FILENAME_LEN 128

// Maximal size of our serialized JSON, picked to be < the typical MTU setting
// minus 1 byte to reserve for '\n'
#define JSON_BUFFER_LEN 1023


// Local config variables and related Macros. These are bound and set via the
// Doom config framework calls during startup, but to be safe we set defaults.
static int telemetry_enabled = 0;
static int telemetry_mode = FILE_MODE;
static char *telemetry_host = "localhost";
static int telemetry_port = 10666;

#define ASSERT_TELEMETRY_ON(...) if (!telemetry_enabled) return __VA_ARGS__

// For now, we use a single global logger instance to keep things simple.
static Logger logger = { -1, NULL, NULL, NULL };

// Used to prevent constant malloc when printing json
static char* jsonbuf = NULL;

///// FileSystem Logger
// reference to event log file
static int log_fd = -1;

///// UDP Logger
// UDP state
static UDPsocket socket;
// Target to broadcast packets to
static IPaddress addr;

// Convert an event type enum into a string representation
const char* eventTypeName(xeventtype_t ev)
{
    switch (ev)
    {
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

//////////////////////////////////////////////////////////////////////////////
//// JSON wranglin' and wrastlin'

// The primary logging logic, composes a buffer to send to the Logger.
// Optionally takes a keyword and additional JSON type to add into the
// resulting JSON Object serialized into the buffer.
void logEventWithExtra(xevent_t *ev, const char* key, cJSON* extra)
{
    int bytes = 0;
    cJSON* json = NULL;
    cJSON* actor = NULL;
    cJSON* frame = NULL;
    cJSON* target = NULL;
    cJSON* pos = NULL;
    size_t buflen = 0;

    // XXX: short circuit here for now as it catches all code paths, so most
    // JSON manipulation is avoided if we're not using telemetry.
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

    // Doom calls frames "tics". We'll track both time and tics.
    // See also: TICRATE in i_timer.h (it's 35)
    //           TruRunTics() in d_loop.c
    frame = cJSON_CreateObject();
    if (frame == NULL)
    {
        I_Error("unable to create frame JSON object?!");
    }
    cJSON_AddNumberToObject(frame, "millis", I_GetTimeMS());
    cJSON_AddNumberToObject(frame, "tic", I_GetTime());
    cJSON_AddItemToObject(json, "frame", frame);

    // Compose what we know about any given Actor including their position.
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

    // Compose what we know about any possible Target to the action
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

    // Serialize the JSON out into a buffer that we then hand off to the Logger
    if (cJSON_PrintPreallocated(json, jsonbuf, JSON_BUFFER_LEN, false))
    {
        buflen = strlen(jsonbuf);

        if (buflen > JSON_BUFFER_LEN)
        {
            I_Error("XXX: something horribly wrong with jsonbuf!");
        }

        // the world is prettier when we end a message with a newline ;-)
        jsonbuf[buflen] = '\n';

        bytes = logger.write(jsonbuf, buflen + 1);
        if (bytes < 1)
        {
            // TODO: how do we want to handle possible errors?
            printf("XXX: ??? wrote zero bytes to logger?\n");
        }
        memset(jsonbuf, 0, JSON_BUFFER_LEN + 1);
    }
    else
    {
        I_Error("failed to write JSON to json buffer?!?");
    }

    if (json != NULL)
    {
        cJSON_Delete(json);
    }
}

// Helper function for adding a single Number entry into the JSON Object
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

// Simplest logging routine to be called by the exposed log functions
void logEvent(xevent_t *ev)
{
    logEventWithExtra(ev, NULL, NULL);
}

//////////////////////////////////////////////////////////////////////////////
//////// FILESYSTEM LOGGER FUNCTIONS

// Initialize a doom-<timestamp-in-millis>.log file to write events into,
// setting a globally tracked file descriptor
int initFileLog(void)
{
    time_t t;
    char* filename;

    // Don't init twice
    if (log_fd > -1)
    {
        printf("X_InitTelemetry: telemetry logfile is already opened!\n");
        return -1;
    }

    // xxx: danger danger we gon cray cray
    t = time(NULL);
    filename = malloc(MAX_FILENAME_LEN);
    if (filename == NULL)
    {
        I_Error("X_InitTelemetry: failed to malloc room for filename");
    }

    // blind cast to int for now...who cares?
    M_snprintf(filename, MAX_FILENAME_LEN, "doom-%d.log", (int) t);
    log_fd = open(filename, O_WRONLY | O_APPEND | O_CREAT);
    free(filename);

#ifndef _WIN32
    // On Win32, this makes sure we end up with proper file permissions
    if (fchmod(log_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != 0)
    {
        I_Error("X_InitTelemetry: couldn't chmod log file!");
    }
#endif

    if (log_fd < 0)
    {
        I_Error("X_InitTelemetry: failed to open logfile!");
    }

    printf("X_InitTelemetry: initialized filesystem logger\n");

    return 0;
}

// Try to close an open file descriptor, making sure all data is flushed out
int closeFileLog(void)
{
    if (close(log_fd) != 0)
    {
        printf("X_StopTelemetry: failed to close doom log file!!");
        return -1;
    }

    log_fd = -1;

    return 0;
}

// A naive write implementation for our filesystem logger
int writeFileLog(char* msg, size_t len)
{
    return write(log_fd, msg, len);
}

//////////////////////////////////////////////////////////////////////////////
//////// UDP LOGGER FUNCTIONS

// Initialize SDL_Net even though it may be initialized elsewhere in Doom.
int initUdpLog(void)
{
    const SDL_version *link_version = SDLNet_Linked_Version();

    printf("X_InitTelemetry: starting udp logger using SDL_Net v%d.%d.%d\n",
           link_version->major, link_version->minor, link_version->patch);

    if (SDLNet_Init() != 0)
    {
        I_Error("X_InitTelemetry: failed to initialize SDL_Net?!");
    }

    socket = SDLNet_UDP_Open(0);
    if (!socket)
    {
        I_Error("X_InitTelemetry: could not bind a port for outbound UDP!?");
    }

    if (SDLNet_ResolveHost(&addr, telemetry_host, telemetry_port) != 0)
    {
        I_Error("X_InitTelemetry: unable to resolve %s:%d",
                telemetry_host, telemetry_port);
    }

    // TODO: initialize bufs??

    return 0;
}

// TODO: cleanup any bufs?
int closeUdpLog(void)
{
    // TODO: should we call SDLNet_Quit()?
    return 0;
}

// Try to create and broadcast a UDPpacket;
int writeUdpLog(char *msg, size_t len)
{
    // XXX: naive approach for now, constantly mallocs etc.
    UDPpacket *packet = NULL;
    uint8_t *buf = NULL;
    int r = 0;

    packet = SDLNet_AllocPacket(len);
    if (packet == NULL)
    {
        I_Error("X_Telemetry: couldn't allocate a UDPpacket!?");
    }

    buf = malloc(packet->maxlen);
    if (buf == NULL)
    {
        I_Error("X_Telemetry: failed to allocate buffer for UDPpacket?!");
    }

    strlcpy((char*)buf, msg, len + 1);
    packet->data = buf;
    packet->len = len;
    packet->address = addr;

    if (SDLNet_UDP_Send(socket, -1, packet) != 1)
    {
        printf("XXX: something wrong with sending udp? (r = %d)", r);
    }

    r = packet->status;
    SDLNet_FreePacket(packet);

    return r;
}

//////////////////////////////////////////////////////////////////////////////
//////// Basic framework housekeeping

// Initialize telemetry service based on config, initialize global buffers.
int X_InitTelemetry(void)
{
    ASSERT_TELEMETRY_ON(0);

    if (logger.type < 1)
    {
        switch (telemetry_mode)
        {
            case FILE_MODE:
                logger.type = FILE_MODE;
                logger.init = initFileLog;
                logger.close = closeFileLog;
                logger.write = writeFileLog;
                break;
            case UDP_MODE:
                logger.type = UDP_MODE;
                logger.init = initUdpLog;
                logger.close = closeUdpLog;
                logger.write = writeUdpLog;
                break;
            default:
                I_Error("X_InitTelemetry: Unsupported telemetry mode (%d)", telemetry_mode);
        }

        // initialize json write buffer
        jsonbuf = calloc(JSON_BUFFER_LEN + 1, sizeof(char));
        if (jsonbuf == NULL)
        {
            I_Error("failed to allocate space for json buffer");
        }

        // initialize chosen telemetry service
        if (logger.init() == 0)
        {
            printf("X_InitTelemetry: enabled telemetry mode (%d)\n", logger.type);
        } else
        {
            I_Error("X_InitTelemetry: failed to initialize telemetry mode!?");
        }
    }

    return logger.type;
}

// Shutdown telemetry service, cleaning up any global buffers.
void X_StopTelemetry(void)
{
    ASSERT_TELEMETRY_ON();

    if (logger.type > 0)
    {
        // Cleanup JSON byte buffer
        if (jsonbuf != NULL) {
            free(jsonbuf);
            jsonbuf = NULL;
        } else {
            printf("XXX: json buffer not allocated?!\n");
        }
    }
}

// Called to bind our local variables into the configuration framework so they
// can be set by the config file or command line
void X_BindTelemetryVariables(void)
{
    M_BindIntVariable("telemetry_enabled", &telemetry_enabled);
    M_BindIntVariable("telemetry_mode", &telemetry_mode);
    M_BindStringVariable("telemetry_host", &telemetry_host);
    M_BindIntVariable("telemetry_port", &telemetry_port);
}

//////////////////////////////////////////////////////////////////////////////
//////// Public Logging Calls (these should be inserted throughoug Doom code)

///////// Basic start/stop/movement

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

void X_LogSectorCrossing(mobj_t *actor)
{
    xevent_t ev = { e_entered_subsector, actor, NULL };
    logEvent(&ev);
}

////////// Death :-(

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

////////// Fighting!

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

////////// Pickups!

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
