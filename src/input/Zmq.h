/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2017 Matthias P. Braendli
    http://www.opendigitalradio.org

   ZeroMQ input. see www.zeromq.org for more info

   For the AAC+ input, each zeromq message must contain one superframe,
   or one zmq_frame_header_t followed by a superframe.

   For the MPEG input, each zeromq message must contain one frame.

   Encryption is provided by zmq_curve, see the corresponding manpage.

   From the ZeroMQ manpage 'zmq':

       The 0MQ lightweight messaging kernel is a library which extends the standard
       socket interfaces with features traditionally provided by specialised
       messaging middleware products. 0MQ sockets provide an abstraction of
       asynchronous message queues, multiple messaging patterns, message filtering
       (subscriptions), seamless access to multiple transport protocols and more.
   */
/*
   This file is part of ODR-DabMux.

   It defines a ZeroMQ input for audio and dabplus data.

   ODR-DabMux is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMux is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMux.  If not, see <http://www.gnu.org/licenses/>.
   */

#pragma once

#include <list>
#include <string>
#include <stdint.h>
#include "zmq.hpp"
#include "input/inputs.h"
#include "ManagementServer.h"

namespace Inputs {

/* The frame_buffer contains DAB logical frames as defined in
 * TS 102 563, section 6.
 * Five elements of this buffer make one AAC superframe (120ms audio)
 */

// Minimum frame_buffer size in number of elements
// This is one AAC superframe, but you probably don't want to
// go that low anyway.
const size_t INPUT_ZMQ_MIN_BUFFER_SIZE = 5*1; // 120ms

// Maximum frame_buffer size in number of elements
// One minute is clearly way over what everybody would
// want.
const size_t INPUT_ZMQ_MAX_BUFFER_SIZE = 5*500; // 60s

/* The ZeroMQ Curve key is 40 bytes long in Z85 representation
 *
 * But we need to store it as zero-terminated string.
 */
const size_t CURVE_KEYLEN = 40;

/* helper to invalidate a key */
#define INVALIDATE_KEY(k) memset(k, 0, CURVE_KEYLEN+1)

/* Verification for key validity */
#define KEY_VALID(k) (k[0] != '\0')

/* Read a key from file into key
 *
 * Returns 0 on success, negative value on failure
 */
int readkey(std::string& keyfile, char* key);

struct dab_input_zmq_config_t
{
    /* The size of the internal buffer, measured in number
     * of elements.
     *
     * Each element corresponds to five frames,
     * or one AAC superframe.
     */
    size_t buffer_size;

    /* The amount of prebuffering to do before we start streaming
     *
     * Same units as buffer_size
     */
    size_t prebuffering;

    /* Whether to enforce encryption or not
     */
    bool enable_encryption;

    /* Full path to file containing public key.
     */
    std::string curve_public_keyfile;

    /* Full path to file containing secret key.
     */
    std::string curve_secret_keyfile;

    /* Full path to file containing encoder public key.
     */
    std::string curve_encoder_keyfile;
};

#define ZMQ_ENCODER_FDK 1
#define ZMQ_ENCODER_TOOLAME 2

/* This defines the on-wire representation of a ZMQ message header.
 *
 * The data follows right after this header */
struct zmq_frame_header_t
{
    uint16_t version; // we support version=1 now
    uint16_t encoder; // see ZMQ_ENCODER_XYZ

    /* length of the 'data' field */
    uint32_t datasize;

    /* Audio level, peak, linear PCM */
    int16_t audiolevel_left;
    int16_t audiolevel_right;

    /* Data follows this header */
} __attribute__ ((packed));

/* The expected frame size incl data of the given frame */
#define ZMQ_FRAME_SIZE(f) (sizeof(zmq_frame_header_t) + f->datasize)

#define ZMQ_FRAME_DATA(f) ( ((uint8_t*)f)+sizeof(zmq_frame_header_t) )


class ZmqBase : public InputBase, public RemoteControllable {
    public:
        ZmqBase(const std::string& name,
                dab_input_zmq_config_t config)
            : RemoteControllable(name),
            m_zmq_context(1),
            m_zmq_sock(m_zmq_context, ZMQ_SUB),
            m_zmq_sock_bound_to(""),
            m_bitrate(0),
            m_enable_input(true),
            m_config(config),
            m_stats(m_name),
            m_prebuf_current(config.prebuffering) {
                RC_ADD_PARAMETER(enable,
                        "If the input is enabled. Set to zero to empty the buffer.");

                RC_ADD_PARAMETER(encryption,
                        "If encryption is enabled or disabled [1 or 0]."
                        " If 1 is written, the keys are reloaded.");

                RC_ADD_PARAMETER(publickey,
                        "The multiplexer's public key file.");

                RC_ADD_PARAMETER(secretkey,
                        "The multiplexer's secret key file.");

                RC_ADD_PARAMETER(encoderkey,
                        "The encoder's public key file.");

                /* Set all keys to zero */
                INVALIDATE_KEY(m_curve_public_key);
                INVALIDATE_KEY(m_curve_secret_key);
                INVALIDATE_KEY(m_curve_encoder_key);
            }

        virtual int open(const std::string& inputUri);
        virtual int readFrame(uint8_t* buffer, size_t size);
        virtual int setBitrate(int bitrate);
        virtual int close();

        /* Remote control */
        virtual void set_parameter(const std::string& parameter,
               const std::string& value);

        /* Getting a parameter always returns a string. */
        virtual const std::string get_parameter(const std::string& parameter) const;

    protected:
        virtual int readFromSocket(size_t framesize) = 0;

        virtual void rebind();

        zmq::context_t m_zmq_context;
        zmq::socket_t m_zmq_sock; // handle for the zmq socket

        /* If the socket is bound, this saves the endpoint,
         * otherwise, it's an empty string
         */
        std::string m_zmq_sock_bound_to;
        int m_bitrate;

        /* set this to zero to empty the input buffer */
        bool m_enable_input;

        /* stores elements of type char[<superframesize>] */
        std::list<uint8_t*> m_frame_buffer;

        dab_input_zmq_config_t m_config;

        /* Key management, keys need to be zero-terminated */
        char m_curve_public_key[CURVE_KEYLEN+1];
        char m_curve_secret_key[CURVE_KEYLEN+1];
        char m_curve_encoder_key[CURVE_KEYLEN+1];

        std::string m_inputUri;

        InputStat m_stats;

    private:
        size_t m_prebuf_current;
};

class ZmqMPEG : public ZmqBase {
    public:
        ZmqMPEG(const std::string& name,
                dab_input_zmq_config_t config)
            : ZmqBase(name, config) {
                RC_ADD_PARAMETER(buffer,
                        "Size of the input buffer [mpeg frames]");

                RC_ADD_PARAMETER(prebuffering,
                        "Min buffer level before streaming starts [mpeg frames]");
            }

    private:
        virtual int readFromSocket(size_t framesize);
};

class ZmqAAC : public ZmqBase {
    public:
        ZmqAAC(const std::string& name,
                dab_input_zmq_config_t config)
            : ZmqBase(name, config) {
                RC_ADD_PARAMETER(buffer,
                        "Size of the input buffer [aac superframes]");

                RC_ADD_PARAMETER(prebuffering,
                        "Min buffer level before streaming starts [aac superframes]");
            }

    private:
        virtual int readFromSocket(size_t framesize);
};

};


