/****************************************************************************
 *
 * Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 * Copyright (C) 2013-2013 Sourcefire, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.  You may not use, modify or
 * distribute this program under any other version of the GNU General
 * Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 ****************************************************************************/

#ifndef FLOW_H
#define FLOW_H

#include <assert.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "utils/bitop.h"
#include "sfip/sfip_t.h"
#include "flow/flow_key.h"
#include "framework/inspector.h"
#include "framework/plug_data.h"
#include "framework/codec.h"

#define SSNFLAG_SEEN_CLIENT         0x00000001
#define SSNFLAG_SEEN_SENDER         0x00000001
#define SSNFLAG_SEEN_SERVER         0x00000002
#define SSNFLAG_SEEN_RESPONDER      0x00000002
#define SSNFLAG_ESTABLISHED         0x00000004
#define SSNFLAG_MIDSTREAM           0x00000008 /* picked up midstream */

#define SSNFLAG_ECN_CLIENT_QUERY    0x00000010
#define SSNFLAG_ECN_SERVER_REPLY    0x00000020
#define SSNFLAG_CLIENT_FIN          0x00000040 /* server sent fin */
#define SSNFLAG_SERVER_FIN          0x00000080 /* client sent fin */

#define SSNFLAG_COUNTED_INITIALIZE  0x00000100
#define SSNFLAG_COUNTED_ESTABLISH   0x00000200
#define SSNFLAG_COUNTED_CLOSING     0x00000400

#define SSNFLAG_TIMEDOUT            0x00001000
#define SSNFLAG_PRUNED              0x00002000
#define SSNFLAG_RESET               0x00004000

#define SSNFLAG_DROP_CLIENT         0x00010000
#define SSNFLAG_DROP_SERVER         0x00020000
#define SSNFLAG_FORCE_BLOCK         0x00040000

#define SSNFLAG_STREAM_ORDER_BAD    0x00100000
#define SSNFLAG_CLIENT_SWAP         0x00200000
#define SSNFLAG_CLIENT_SWAPPED      0x00400000

#define SSNFLAG_ALL                 0xFFFFFFFF /* all that and a bag of chips */
#define SSNFLAG_NONE                0x00000000 /* nothing, an MT bag of chips */

#define SSNFLAG_BLOCK (SSNFLAG_DROP_CLIENT|SSNFLAG_DROP_SERVER)

#define STREAM5_STATE_NONE              0x0000
#define STREAM5_STATE_SYN               0x0001
#define STREAM5_STATE_SYN_ACK           0x0002
#define STREAM5_STATE_ACK               0x0004
#define STREAM5_STATE_ESTABLISHED       0x0008
#define STREAM5_STATE_DROP_CLIENT       0x0010
#define STREAM5_STATE_DROP_SERVER       0x0020
#define STREAM5_STATE_MIDSTREAM         0x0040
#define STREAM5_STATE_TIMEDOUT          0x0080
#define STREAM5_STATE_UNREACH           0x0100
#define STREAM5_STATE_CLOSED            0x0800
#define STREAM5_STATE_IGNORE            0x1000
#define STREAM5_STATE_NO_PICKUP         0x2000

struct Packet;

typedef void (*StreamAppDataFree)(void*);

struct StreamFlowData
{
    BITOP boFlowbits;
    unsigned char flowb[1];
};

class SO_PUBLIC FlowData
{
public:
    FlowData(unsigned u, Inspector* = nullptr);
    virtual ~FlowData();

    unsigned get_id()
    { return id; };

    static unsigned get_flow_id()
    { return ++flow_id; };

public:
    FlowData* next;
    FlowData* prev;

private:
    static unsigned flow_id;
    Inspector* handler;
    unsigned id;
};

struct LwState
{
    uint32_t   session_flags;

    int16_t    ipprotocol;
    int16_t    application_protocol;

    char       direction;
    char       ignore_direction; /* flag to ignore traffic on this session */
};

enum Stream_Event
{
    SE_REXMIT,
    SE_EOF,
    SE_MAX
};

// this struct is organized by member size for compactness
class Flow
{
public:
    enum FlowState
    {
        SETUP,
        INSPECT,
        BLOCK,
        ALLOW
    };
    Flow();
    ~Flow();

    void init(PktType);
    void term();

    void reset();
    void restart(bool freeAppData = true);
    void clear(bool freeAppData = true);

    int set_application_data(FlowData*);
    FlowData* get_application_data(uint32_t proto);
    void free_application_data(uint32_t proto);
    void free_application_data(FlowData*);
    void free_application_data();

    void markup_packet_flags(Packet*);
    void set_direction(Packet*);

    void set_expire(Packet*, uint32_t timeout);
    int get_expire(Packet*);
    bool expired(Packet*);

    void set_ttl(Packet*, bool client);

    void block()
    { ssn_state.session_flags |= SSNFLAG_BLOCK; };

    bool was_blocked() const
    { return (ssn_state.session_flags & SSNFLAG_BLOCK) != 0; };

    bool full_inspection() const
    { return flow_state <= INSPECT; };

    void set_state(FlowState fs)
    { flow_state = fs; };

    void set_client(Inspector* ins)
    {
        ssn_client = ins;
        ssn_client->add_ref();
    };
    void set_server(Inspector* ins)
    {
        ssn_server = ins;
        ssn_server->add_ref();
    };
    void set_clouseau(Inspector* ins)
    {
        clouseau = ins;
        clouseau->add_ref();
    };
    void clear_clouseau()
    {
        clouseau->rem_ref();
        clouseau = nullptr;
    };
    void set_gadget(Inspector* ins)
    {
        gadget = ins;
        gadget->add_ref();
    };
    void clear_gadget()
    {
        gadget->rem_ref();
        gadget = nullptr;
    };
    void set_data(PlugData* pd)
    {
        data = pd;
        data->add_ref();
    };
    void clear_data()
    {
        data->rem_ref();
        data = nullptr;
    };

public:  // FIXIT-M privatize if possible
    // these fields are const after initialization
    const FlowKey* key;
    class Session* session;
    StreamFlowData* flowdata;
    uint8_t ip_proto; // FIXIT-M  -- do we need both of these?
    PktType protocol; // ^^

    // these fields are always set; not zeroed
    Flow* prev, * next;
    Inspector* ssn_client;
    Inspector* ssn_server;
    long last_data_seen;

    // everything from here down is zeroed
    FlowData* appDataList;
    Inspector* clouseau;
    Inspector* gadget;
    PlugData* data;
    const char* service;

    unsigned policy_id;

    FlowState flow_state;
    LwState ssn_state;

    // FIXIT-L can client and server ip and port be removed from flow?
    sfip_t client_ip; // FIXIT-L family and bits should be changed to uint16_t
    sfip_t server_ip; // or uint8_t to reduce sizeof from 24 to 20

    uint64_t expire_time;

    int32_t iface_in;
    int32_t iface_out;

    uint16_t client_port;
    uint16_t server_port;

    uint16_t ssn_policy;
    uint16_t session_state;

    uint8_t  handler[SE_MAX];
    uint8_t  response_count;

    uint8_t  inner_client_ttl, inner_server_ttl;
    uint8_t  outer_client_ttl, outer_server_ttl;
};

#endif

