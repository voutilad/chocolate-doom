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
#include "config.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include <SDL2/SDL_net.h>

#ifdef HAVE_LIBRDKAFKA
#include <librdkafka/rdkafka.h>
#endif

#include "cJSON.h"

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

static char *udp_host = "localhost";
static int udp_port = 10666;

static char *kafka_topic = "doom-telemetry";
static char *kafka_brokers = "localhost:9092";

#define ASSERT_TELEMETRY_ON(...) if (!telemetry_enabled) return __VA_ARGS__

// For now, we use a single global logger instance to keep things simple.
static Logger logger = { -1, NULL, NULL, NULL };

// Used to prevent constant malloc when printing json
static char* jsonbuf = NULL;

// Global ordered event counter, yes this will wrap, but the counter plus
// the frame tic should be enough info for a consuming system to properly
// order events we emit. Also, since it's assumed the code that accesses
// the counter is single-threaded, we don't attempt to deal with races.
static uint counter = 0;

///// FileSystem Logger
// reference to event log file
static int log_fd = -1;

///// UDP Logger
// UDP state
static UDPsocket sock;
// Target to broadcast packets to
static IPaddress addr;
// Re-useable packet instance since we only send 1 at a time
static UDPpacket *packet = NULL;

#ifdef HAVE_LIBRDKAFKA
static rd_kafka_t *kafka_producer;
static char kafka_errbuf[512];
#endif

// Convert an event type enum into a string representation
const char* eventTypeName(xeventtype_t ev)
{
    switch (ev)
    {
        case e_start_level:
            return "start_level";
        case e_end_level:
            return "end_level";
        case e_targeted:
            return "targeted";
        case e_killed:
            return "killed";
        case e_attack:
            return "attacked";
        case e_counterattack:
            return "counter_attacked";
        case e_hit:
            return "hit";
        case e_pickup_armor:
            return "pickup_armor";
        case e_pickup_health:
            return "pickup_health";
        case e_pickup_weapon:
            return "pickup_weapon";
        case e_pickup_card:
            return "pickup_card";
        case e_armor_bonus:
            return "armor_bonus";
        case e_health_bonus:
            return "health_bonus";
        case e_entered_sector:
            return "enter_sector";
        case e_entered_subsector:
            return "enter_subsector";
        case e_move:
            return "move";
    }

    printf("XXX: Unknown event type: %d\n", ev);
    return "unknown_event";
}

// Convert a mobj type into an enemy name string
const char* enemyTypeName(mobj_t* enemy) {
    switch (enemy->type)
    {
        case MT_POSSESSED:
            return "soldier";
        case MT_SHOTGUY:
            return "shotgun_soldier";
        case MT_VILE:
            return "vile";
        case MT_SERGEANT:
            return "demon";
        case MT_SHADOWS:
            return "spectre";
        case MT_TROOP:
            return "imp";
        case MT_TROOPSHOT:
            return "imp_fireball";
        case MT_UNDEAD:
            return "undead";
        case MT_SKULL:
            return "lost_soul";
        case MT_HEAD:
            return "cacodemon";
        case MT_HEADSHOT:
            return "cacodemon_fireball";
        case MT_BRUISER:
            return "baron_of_hell";
        case MT_BRUISERSHOT:
            return "baron_fireball";
        case MT_BARREL:
            return "barrel";
        case MT_ROCKET:
            return "rocket";
        case MT_PLASMA:
            return "plasma";
        default:
            printf("XXX: Unknown enemy type: %d\n", enemy->type);
            return "unknown_enemy";
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

    cJSON_AddNumberToObject(json, "counter", counter++);
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
        cJSON_AddNumberToObject(pos, "angle", ev->actor->angle);
        cJSON_AddNumberToObject(pos, "subsector",
                                (uintptr_t) guessActorLocation(ev->actor));
        cJSON_AddItemToObject(actor, "position", pos);

        if (ev->actor->player)
        {
            cJSON_AddStringToObject(actor, "type", "player");
            cJSON_AddNumberToObject(actor, "health", ev->actor->player->health);
            cJSON_AddNumberToObject(actor, "armor", ev->actor->player->armorpoints);
            cJSON_AddNumberToObject(actor, "id", (uintptr_t) ev->actor);
        } else
        {
            cJSON_AddStringToObject(actor, "type", enemyTypeName(ev->actor));
            cJSON_AddNumberToObject(actor, "health", ev->actor->health);
            cJSON_AddNumberToObject(actor, "id", (uintptr_t) ev->actor);
        }
        cJSON_AddItemToObject(json, "actor", actor);
    }

    // Compose what we know about any possible Target to the action
    if (ev->target != NULL)
    {
        target = cJSON_CreateObject();
        pos = cJSON_CreateObject();
        if (target == NULL || pos == NULL)
        {
            I_Error("unable to create target/position JSON object(s)?!");
        }

        cJSON_AddNumberToObject(pos, "x", ev->target->x);
        cJSON_AddNumberToObject(pos, "y", ev->target->y);
        cJSON_AddNumberToObject(pos, "z", ev->target->z);
        cJSON_AddNumberToObject(pos, "angle", ev->target->angle);
        cJSON_AddNumberToObject(pos, "subsector",
                                (uintptr_t) guessActorLocation(ev->target));
        cJSON_AddItemToObject(target, "position", pos);

        if (ev->target->player)
        {
            cJSON_AddStringToObject(target, "type", "player");
            cJSON_AddNumberToObject(target, "health", ev->target->player->health);
            cJSON_AddNumberToObject(target, "armor", ev->target->player->armorpoints);
            cJSON_AddNumberToObject(target, "id", (uintptr_t) ev->target);
        } else
        {
            cJSON_AddStringToObject(target, "type", enemyTypeName(ev->target));
            cJSON_AddNumberToObject(target, "health", ev->target->health);
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
    log_fd = open(filename, O_WRONLY | O_APPEND | O_CREAT, 0666);

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

    printf("X_InitTelemetry: initialized filesystem logger writing to '%s'\n",
        filename);
    free(filename);

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

    sock = SDLNet_UDP_Open(0);
    if (sock == NULL)
    {
        I_Error("X_InitTelemetry: could not bind a port for outbound UDP!?");
    }

    if (SDLNet_ResolveHost(&addr, udp_host, udp_port) != 0)
    {
        I_Error("X_InitTelemetry: unable to resolve %s:%d",
                udp_host, udp_port);
    }

    packet = SDLNet_AllocPacket(JSON_BUFFER_LEN + 1);
    if (packet == NULL)
    {
        I_Error("X_InitTelemetry: could not allocate udp packet?!");
    }
    packet->address = addr;

    printf("X_InitTelemetry: initialized udp logger to %s:%d\n",
           udp_host, udp_port);

    return 0;
}

// TODO: cleanup any bufs?
int closeUdpLog(void)
{
    if (packet != NULL)
    {
        SDLNet_FreePacket(packet);
    }

    if (sock != NULL)
    {
        SDLNet_UDP_Close(sock);
    }

    return 0;
}

// Try to create and broadcast a UDPpacket;
int writeUdpLog(char *msg, size_t len)
{
    // We mutate the same UDPpacket, updating the payload in .data and the .len
    if (!M_StringCopy((char*)packet->data, msg, len + 1))
    {
        I_Error("X_Telemetry: udp packet buf truncated...something wrong!?");
    }
    packet->len = len;

    if (SDLNet_UDP_Send(sock, -1, packet) != 1)
    {
        printf("XXX: something wrong with sending udp packet?");
    }

    // contains total bytes sent
    return packet->status;
}

//////////////////////////////////////////////////////////////////////////////
//////// KAFKA PUBLISHER FUNCTIONS
#ifdef HAVE_LIBRDKAFKA

static void dr_msg_cb(rd_kafka_t *rk,
                      const rd_kafka_message_t *rkmessage,
                      void *opaque)
{
    if (rkmessage->err)
    {
        printf("X_Telemetry: kafka message dlivery failed, %s\n",
               rd_kafka_err2str(rkmessage->err));
    }
}

int initKafkaPublisher(void)
{
    rd_kafka_conf_t *kafka_conf;
    printf("X_InitTelemetry: starting Kafka producer using librdkafka v%s\n",
           rd_kafka_version_str());

    memset(kafka_errbuf, 0, sizeof(kafka_errbuf));

    kafka_conf = rd_kafka_conf_new();
    if (rd_kafka_conf_set(kafka_conf, "bootstrap.servers",
                          kafka_brokers, kafka_errbuf,
                          sizeof(kafka_errbuf)) != RD_KAFKA_CONF_OK)
    {
        I_Error("X_InitTelemetry: could not set Kafka brokers, %s",
                kafka_errbuf);
    }

    rd_kafka_conf_set_dr_msg_cb(kafka_conf, dr_msg_cb);

    kafka_producer = rd_kafka_new(RD_KAFKA_PRODUCER, kafka_conf, kafka_errbuf,
                                  sizeof(kafka_errbuf));
    if (!kafka_producer)
    {
        I_Error("X_InitTelemetry: could not create kafka producer, %s",
                kafka_errbuf);
    }
    return 0;
}

int closeKafkaPublisher(void)
{
    int flush_timeout_s = 15;
    int unflushed = 0;

    printf("X_StopTelemetry: shutting down Kafka producer");

    if (!kafka_producer)
    {
        I_Error("X_StopTelemetry: Kafka producer does not appear initialized!");
    }

    // be polite and flush
    printf("X_StopTelemetry: waiting %ds for Kafka output queue to empty...\n",
           flush_timeout_s);
    rd_kafka_flush(kafka_producer, flush_timeout_s * 1000);

    unflushed = rd_kafka_outq_len(kafka_producer);
    if (unflushed > 0)
    {
        printf("X_StopTelemetry: could not deliver %d message(s)\n",
               unflushed);
    }

    // blow it up!
    rd_kafka_destroy(kafka_producer);
    kafka_producer = NULL;

    return 0;
}

int writeKafkaLog(char *msg, size_t len)
{
    rd_kafka_resp_err_t err;
    err = rd_kafka_producev(kafka_producer,
                            RD_KAFKA_V_TOPIC(kafka_topic),
                            RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                            RD_KAFKA_V_VALUE(msg, len),
                            RD_KAFKA_V_OPAQUE(NULL),
                            RD_KAFKA_V_END);
    if (err)
    {
        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL)
        {
            // queue full...for now we just shrug
            printf("X_Telemetry: internal Kafka oubound queue is full :-(\n");
        }
        else
        {
            printf("X_Telemetry: unknown kafka issue... %s\n",
                   rd_kafka_err2str(err));
        }

        return 0;
    }

    // XXX: this is 100% a lie as technically can't guarantee anything was
    // transmitted since rdkafka does so asynchronously.
    return len;
}

#else // no kafka

int initKafkaPublisher(void)
{
    printf("X_InitTelemetry: kafka mode enabled, but not compiled in!\n");
    telemetry_enabled = 0;

    return 0;
}

int closeKafkaPublisher(void)
{
    return 0;
}

int writeKafkaLog(char *msg, size_t len)
{
    return 1;
}
#endif

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
            case KAFKA_MODE:
                logger.type = KAFKA_MODE;
                logger.init = initKafkaPublisher;
                logger.close = closeKafkaPublisher;
                logger.write = writeKafkaLog;
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

        // reset counter
        counter = 0;
    }

    return logger.type;
}

// Shutdown telemetry service, cleaning up any global buffers.
void X_StopTelemetry(void)
{
    ASSERT_TELEMETRY_ON();

    if (logger.type > 0)
    {
        if (logger.close != NULL)
        {
            if (logger.close() != 0)
            {
                printf("XXX: problem closing logger (type=%d)?!\n",
                       logger.type);
            }
        }

        // Cleanup JSON byte buffer
        if (jsonbuf != NULL) {
            free(jsonbuf);
            jsonbuf = NULL;
        } else {
            printf("XXX: json buffer not allocated?!\n");
        }

        // Cleanup Logger
        logger.type = -1;
        logger.init = NULL;
        logger.close = NULL;
        logger.write = NULL;

        printf("X_StopTelemetry: total events sent is %d\n", counter);
        printf("X_StopTelemetry: shut down telemetry service\n");
    }
}

// Called to bind our local variables into the configuration framework so they
// can be set by the config file or command line
void X_BindTelemetryVariables(void)
{
    M_BindIntVariable("telemetry_enabled", &telemetry_enabled);
    M_BindIntVariable("telemetry_mode", &telemetry_mode);
    M_BindStringVariable("telemetry_udp_host", &udp_host);
    M_BindIntVariable("telemetry_udp_port", &udp_port);
#ifdef HAVE_LIBRDKAFKA
    M_BindStringVariable("telemetry_kafka_topic", &kafka_topic);
    M_BindStringVariable("telemetry_kafka_brokers", &kafka_brokers);
#endif
}

//////////////////////////////////////////////////////////////////////////////
//////// Public Logging Calls (these should be inserted throughoug Doom code)

///////// Basic start/stop/movement

void X_LogStart(player_t *player, int ep, int level, skill_t mode)
{
    cJSON *json = cJSON_CreateObject();
    if (!json)
    {
        I_Error("failed to instantiate level json metadata!");
    }
    cJSON_AddNumberToObject(json, "episode", ep);
    cJSON_AddNumberToObject(json, "level", level);
    cJSON_AddNumberToObject(json, "difficulty", mode);

    xevent_t ev = { e_start_level, player->mo, NULL };
    logEventWithExtra(&ev, "level", json);
    cJSON_Delete(json);
}

void X_LogExit(player_t *player)
{
    xevent_t ev = { e_end_level, player->mo, NULL };
    logEvent(&ev);
}

void X_LogMove(mobj_t *actor)
{
    xevent_t ev = { e_move, actor, NULL };
    logEvent(&ev);
}

void X_LogSectorCrossing(mobj_t *actor)
{
    xevent_t ev = { e_entered_subsector, actor, NULL };
    logEvent(&ev);
}

////////// Death :-(

void X_LogEnemyKilled(player_t *player, mobj_t *victim)
{
    xevent_t ev = { e_killed, player->mo, victim };
    logEvent(&ev);
}

void X_LogPlayerDied(player_t *player, mobj_t *killer)
{
    xevent_t ev = { e_killed, killer, player->mo };
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
    logEventWithExtraNumber(&ev, "weapon_type", weapon);
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
    logEventWithExtraNumber(&ev, "armor_type", armortype);
}

void X_LogWeaponPickup(mobj_t *actor, weapontype_t weapon)
{
    xevent_t ev = { e_pickup_weapon, actor, NULL };
    logEventWithExtraNumber(&ev, "weapon_type", weapon);
}

void X_LogCardPickup(player_t *player, card_t card)
{
    // xxx: card_t is an enum, we should resolve it in the future
    xevent_t ev = { e_pickup_card, player->mo, NULL };
    logEventWithExtraNumber(&ev, "card", card);
}
