/*
   Copyright (C) 2009 Her Majesty the Queen in Right of Canada (Communications
   Research Center Canada)

   Copyright (C) 2013, 2014 Matthias P. Braendli
    http://www.opendigitalradio.org

   ZeroMQ input. see www.zeromq.org for more info

   For the AAC+ input, each zeromq message must contain one superframe.

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

#include "dabInput.h"
#include "dabInputZmq.h"
#include "StatsServer.h"
#include "zmq.hpp"
#include "PcDebug.h"

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#ifdef HAVE_INPUT_ZEROMQ

#include <cstdio>
#include <cstdlib>
#include <list>
#include <exception>
#include <cstring>
#include <string>
#include <sstream>
#include <limits.h>

#ifdef __MINGW32__
#   define bzero(s, n) memset(s, 0, n)
#endif

using namespace std;

extern StatsServer* global_stats;

int readkey(string& keyfile, char* key)
{
    int fd = open(keyfile.c_str(), O_RDONLY);
    if (fd < 0)
        return fd;
    int ret = read(fd, key, CURVE_KEYLEN);
    if (ret < 0)
        return ret;
    close(fd);

    /* It needs to be zero-terminated */
    key[CURVE_KEYLEN] = '\0';

    return 0;
}

/***** Common functions (MPEG and AAC) ******/

/* If necessary, unbind the socket, then check the keys,
 * if they are ok and encryption is required, set the
 * keys to the socket, and finally bind the socket
 * to the new address
 */
void DabInputZmqBase::rebind()
{
    if (! m_zmq_sock_bound_to.empty()) {
        try {
            m_zmq_sock.unbind(m_zmq_sock_bound_to.c_str());
        }
        catch (zmq::error_t& err) {
            etiLog.level(warn) << "ZMQ unbind for input " << m_name << " failed";
        }
    }

    m_zmq_sock_bound_to = "";

    /* Load each key independently */
    if (! m_config.curve_public_keyfile.empty()) {
        int rc = readkey(m_config.curve_public_keyfile, m_curve_public_key);

        if (rc < 0) {
            etiLog.level(warn) << "Invalid public key for input " <<
                m_name;

            INVALIDATE_KEY(m_curve_public_key);
        }
    }

    if (! m_config.curve_secret_keyfile.empty()) {
        int rc = readkey(m_config.curve_secret_keyfile, m_curve_secret_key);

        if (rc < 0) {
            etiLog.level(warn) << "Invalid secret key for input " <<
                m_name;

            INVALIDATE_KEY(m_curve_secret_key);
        }
    }

    if (! m_config.curve_encoder_keyfile.empty()) {
        int rc = readkey(m_config.curve_encoder_keyfile, m_curve_encoder_key);

        if (rc < 0) {
            etiLog.level(warn) << "Invalid encoder key for input " <<
                m_name;

            INVALIDATE_KEY(m_curve_encoder_key);
        }
    }

    /* If you want encryption, you need to have defined all
     * key files
     */
    if ( m_config.enable_encryption &&
            ( ! (KEY_VALID(m_curve_public_key) &&
                 KEY_VALID(m_curve_secret_key) &&
                 KEY_VALID(m_curve_encoder_key) ) ) ) {
        throw std::runtime_error("When enabling encryption, all three "
                "keyfiles must be valid!");
    }

    if (m_config.enable_encryption) {
        try {
            /* We want to check that the encoder is the right one,
             * so the encoder is the CURVE server.
             */
            m_zmq_sock.setsockopt(ZMQ_CURVE_SERVERKEY,
                    m_curve_encoder_key, CURVE_KEYLEN);
        }
        catch (zmq::error_t& err) {
            std::ostringstream os;
            os << "ZMQ set encoder key for input " << m_name << " failed" <<
                err.what();
            throw std::runtime_error(os.str());
        }

        try {
            m_zmq_sock.setsockopt(ZMQ_CURVE_PUBLICKEY,
                    m_curve_public_key, CURVE_KEYLEN);
        }
        catch (zmq::error_t& err) {
            std::ostringstream os;
            os << "ZMQ set public key for input " << m_name << " failed" <<
                err.what();
            throw std::runtime_error(os.str());
        }

        try {
            m_zmq_sock.setsockopt(ZMQ_CURVE_SECRETKEY,
                    m_curve_secret_key, CURVE_KEYLEN);
        }
        catch (zmq::error_t& err) {
            std::ostringstream os;
            os << "ZMQ set secret key for input " << m_name << " failed" <<
                err.what();
            throw std::runtime_error(os.str());
        }
    }
    else {
        try {
            /* This forces the socket to go to the ZMQ_NULL auth
             * mechanism
             */
            const int no = 0;
            m_zmq_sock.setsockopt(ZMQ_CURVE_SERVER, &no, sizeof(no));
        }
        catch (zmq::error_t& err) {
            etiLog.level(warn) << "ZMQ disable encryption keys for input " <<
                m_name << " failed: " << err.what();
        }

    }

    // Prepare the ZMQ socket to accept connections
    try {
        m_zmq_sock.bind(m_inputUri.c_str());
    }
    catch (zmq::error_t& err) {
        std::ostringstream os;
        os << "ZMQ bind for input " << m_name << " failed" <<
            err.what();
        throw std::runtime_error(os.str());
    }

    m_zmq_sock_bound_to = m_inputUri;

    try {
        m_zmq_sock.setsockopt(ZMQ_SUBSCRIBE, NULL, 0);
    }
    catch (zmq::error_t& err) {
        std::ostringstream os;
        os << "ZMQ set socket options for input " << m_name << " failed" <<
            err.what();
        throw std::runtime_error(os.str());
    }
}

int DabInputZmqBase::open(const std::string inputUri)
{
    m_inputUri = inputUri;

    /* Let caller handle exceptions when we open() */
    rebind();

    // We want to appear in the statistics !
    global_stats->registerInput(m_name);

    return 0;
}

int DabInputZmqBase::close()
{
    m_zmq_sock.close();
    return 0;
}

int DabInputZmqBase::setBitrate(int bitrate)
{
    m_bitrate = bitrate;
    return bitrate; // TODO do a nice check here
}

// size corresponds to a frame size. It is constant for a given bitrate
int DabInputZmqBase::readFrame(void* buffer, int size)
{
    int rc;

    /* We must *always* read data from the ZMQ socket,
     * to make sure that ZMQ internal buffers are emptied
     * quickly. It's the only way to control the buffers
     * of the whole path from encoder to our frame_buffer.
     */
    rc = readFromSocket(size);

    /* Notify of a buffer overrun, and drop some frames */
    if (m_frame_buffer.size() >= m_config.buffer_size) {
        global_stats->notifyOverrun(m_name);

        /* If the buffer is really too full, we drop as many frames as needed
         * to get down to the prebuffering size. We would like to have our buffer
         * filled to the prebuffering length.
         */
        if (m_frame_buffer.size() >= 1.5*m_config.buffer_size) {
            size_t over_max = m_frame_buffer.size() - m_config.prebuffering;

            while (over_max--) {
                m_frame_buffer.pop_front();
            }
        }
        else {
            /* Our frame_buffer contains DAB logical frames. Five of these make one
             * AAC superframe.
             *
             * Dropping this superframe amounts to dropping 120ms of audio.
             *
             * We're actually not sure to drop five DAB logical frames
             * beloning to the same AAC superframe. It is assumed that no
             * receiver will crash because of this. At least, the DAB logical frame
             * vs. AAC superframe alignment is preserved.
             *
             * TODO: of course this assumption probably doesn't hold. Fix this !
             * TODO: also, with MPEG, the above doesn't hold, so we drop five
             *       frames even though we could drop less.
             * */
            m_frame_buffer.pop_front();
            m_frame_buffer.pop_front();
            m_frame_buffer.pop_front();
            m_frame_buffer.pop_front();
            m_frame_buffer.pop_front();
        }
    }

    if (m_prebuf_current > 0) {
        if (rc > 0)
            m_prebuf_current--;
        if (m_prebuf_current == 0)
            etiLog.log(info, "inputZMQ %s input pre-buffering complete\n",
                m_name.c_str());

        /* During prebuffering, give a zeroed frame to the mux */
        global_stats->notifyUnderrun(m_name);
        memset(buffer, 0, size);
        return size;
    }

    // Save stats data in bytes, not in frames
    global_stats->notifyBuffer(m_name, m_frame_buffer.size() * size);

    if (m_frame_buffer.empty()) {
        etiLog.log(warn, "inputZMQ %s input empty, re-enabling pre-buffering\n",
                m_name.c_str());
        // reset prebuffering
        m_prebuf_current = m_config.prebuffering;

        /* We have no data to give, we give a zeroed frame */
        global_stats->notifyUnderrun(m_name);
        memset(buffer, 0, size);
        return size;
    }
    else
    {
        /* Normal situation, give a frame from the frame_buffer */
        uint8_t* newframe = m_frame_buffer.front();
        memcpy(buffer, newframe, size);
        delete[] newframe;
        m_frame_buffer.pop_front();
        return size;
    }
}


/******** MPEG input *******/

// Read a MPEG frame from the socket, and push to list
int DabInputZmqMPEG::readFromSocket(size_t framesize)
{
    bool messageReceived;
    zmq::message_t msg;

    try {
        messageReceived = m_zmq_sock.recv(&msg, ZMQ_DONTWAIT);
        if (!messageReceived) {
            return 0;
        }

    }
    catch (zmq::error_t& err)
    {
        etiLog.level(error) << "Failed to receive MPEG frame from zmq socket " <<
                m_name << ": " << err.what();
    }

    char* data = (char*)msg.data();

    if (msg.size() == framesize)
    {
        if (m_frame_buffer.size() > m_config.buffer_size) {
            etiLog.level(warn) <<
                "inputZMQ " << m_name <<
                " buffer full (" << m_frame_buffer.size() << "),"
                " dropping incoming frame !";
            messageReceived = 0;
        }
        else if (m_enable_input) {
            // copy the input frame blockwise into the frame_buffer
            uint8_t* frame = new uint8_t[framesize];
            memcpy(frame, data, framesize);
            m_frame_buffer.push_back(frame);
        }
        else {
            return 0;
        }
    }
    else {
        etiLog.level(error) <<
            "inputZMQ " << m_name <<
            " wrong data size: recv'd " << msg.size() <<
            ", need " << framesize << ".";
    }

    return msg.size();
}

/******** AAC+ input *******/

// Read a AAC+ superframe from the socket, cut it into five frames,
// and push to list
int DabInputZmqAAC::readFromSocket(size_t framesize)
{
    bool messageReceived;
    zmq::message_t msg;

    try {
        messageReceived = m_zmq_sock.recv(&msg, ZMQ_DONTWAIT);
        if (!messageReceived) {
            return 0;
        }

    }
    catch (zmq::error_t& err)
    {
        etiLog.level(error) <<
            "Failed to receive AAC superframe from zmq socket " <<
            m_name << ": " << err.what();
    }

    /* This is the old 'one superframe per ZMQ message' format */
    uint8_t* data  = (uint8_t*)msg.data();
    size_t datalen = msg.size();

    /* Look for the new zmq_frame_header_t format */
    zmq_frame_header_t* frame = (zmq_frame_header_t*)msg.data();

    if (msg.size() == ZMQ_FRAME_SIZE(frame) &&
            frame->version == 1 &&
            frame->encoder == ZMQ_ENCODER_FDK) {
        datalen = frame->datasize;
        data = ZMQ_FRAME_DATA(frame);
    }


    /* TS 102 563, Section 6:
     * Audio super frames are transported in five successive DAB logical frames
     * with additional error protection.
     */
    if (datalen)
    {
        if (datalen == 5*framesize)
        {
            if (m_frame_buffer.size() > m_config.buffer_size) {
                etiLog.level(warn) <<
                    "inputZMQ " << m_name <<
                    " buffer full (" << m_frame_buffer.size() << "),"
                    " dropping incoming superframe !";
                messageReceived = 0;
            }
            else if (m_enable_input) {
                // copy the input frame blockwise into the frame_buffer
                for (uint8_t* framestart = data;
                        framestart < &data[5*framesize];
                        framestart += framesize) {
                    uint8_t* audioframe = new uint8_t[framesize];
                    memcpy(audioframe, framestart, framesize);
                    m_frame_buffer.push_back(audioframe);
                }
            }
            else {
                datalen = 0;
            }
        }
        else {
            etiLog.level(error) <<
                "inputZMQ " << m_name <<
                " wrong data size: recv'd " << msg.size() <<
                ", need " << 5*framesize << ".";

            datalen = 0;
        }
    }
    else {
        etiLog.level(error) <<
            "inputZMQ " << m_name <<
            " invalid frame received";
    }

    return datalen;
}

/********* REMOTE CONTROL ***********/

void DabInputZmqBase::set_parameter(const string& parameter,
        const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "buffer") {
        size_t new_limit = atol(value.c_str());

        if (new_limit < INPUT_ZMQ_MIN_BUFFER_SIZE) {
            throw ParameterError("Desired buffer size too small."
                   " Minimum " STRINGIFY(INPUT_ZMQ_MIN_BUFFER_SIZE) );
        }
        else if (new_limit > INPUT_ZMQ_MAX_BUFFER_SIZE) {
            throw ParameterError("Desired buffer size too large."
                   " Maximum " STRINGIFY(INPUT_ZMQ_MAX_BUFFER_SIZE) );
        }

        m_config.buffer_size = new_limit;
    }
    else if (parameter == "prebuffering") {
        size_t new_prebuf = atol(value.c_str());

        if (new_prebuf < INPUT_ZMQ_MIN_BUFFER_SIZE) {
            throw ParameterError("Desired prebuffering too small."
                   " Minimum " STRINGIFY(INPUT_ZMQ_MIN_BUFFER_SIZE) );
        }
        else if (new_prebuf > INPUT_ZMQ_MAX_BUFFER_SIZE) {
            throw ParameterError("Desired prebuffering too large."
                   " Maximum " STRINGIFY(INPUT_ZMQ_MAX_BUFFER_SIZE) );
        }

        m_config.prebuffering = new_prebuf;
    }
    else if (parameter == "enable") {
        if (value == "1") {
            m_enable_input = true;
        }
        else if (value == "0") {
            m_enable_input = false;
        }
        else {
            throw ParameterError("Value not understood, specify 0 or 1.");
        }
    }
    else if (parameter == "encryption") {
        if (value == "1") {
            m_config.enable_encryption = true;
        }
        else if (value == "0") {
            m_config.enable_encryption = false;
        }
        else {
            throw ParameterError("Value not understood, specify 0 or 1.");
        }

        try {
            rebind();
        }
        catch (std::runtime_error &e) {
            stringstream ss;
            ss << "Could not bind socket again with new keys." <<
                e.what();
            throw ParameterError(ss.str());
        }
    }
    else if (parameter == "secretkey") {
        m_config.curve_secret_keyfile = value;
    }
    else if (parameter == "publickey") {
        m_config.curve_public_keyfile = value;
    }
    else if (parameter == "encoderkey") {
        m_config.curve_encoder_keyfile = value;
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string DabInputZmqBase::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "buffer") {
        ss << m_config.buffer_size;
    }
    else if (parameter == "prebuffering") {
        ss << m_config.prebuffering;
    }
    else if (parameter == "enable") {
        if (m_enable_input)
            ss << "true";
        else
            ss << "false";
    }
    else if (parameter == "encryption") {
        if (m_config.enable_encryption)
            ss << "true";
        else
            ss << "false";
    }
    else if (parameter == "secretkey") {
        ss << m_config.curve_secret_keyfile;
    }
    else if (parameter == "publickey") {
        ss << m_config.curve_public_keyfile;
    }
    else if (parameter == "encoderkey") {
        ss << m_config.curve_encoder_keyfile;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();

}

#endif

