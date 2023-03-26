/*
   Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Matthias P. Braendli, matthias.braendli@mpb.li
   Andy Mace, andy.mace@mediauk.net

    http://www.opendigitalradio.org

   TS output
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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

//#if HAVE_OUTPUT_TS
#include <cstring>
#include <cstdio>
#include <signal.h>
#include <limits.h>
#include "dabOutput.h"
#include <unistd.h>
#include <sys/time.h>
#include <list>
#include <vector>
#include <atomic>
#include <thread>
#include "ThreadsafeQueue.h"
#include <tsduck.h>

using namespace std;

using vec_u8 = std::vector<uint8_t>;

// In ETI one element would be an ETI frame of 6144 bytes.
// 250 frames correspond to 6 seconds. This is mostly here
// to ensure we do not accumulate data for faulty sockets, delay
// management has to be done on the receiver end.
const size_t MAX_QUEUED_ETI_FRAMES = 250;

class ETIInputHandler : public ts::PluginEventHandlerInterface
{
public:
    // Constructors.
    ETIInputHandler() = delete;
    ETIInputHandler(ts::Report &report, PacketBuffer &buffer);

    // Event handling (from ts::PluginEventHandlerInterface).
    virtual void handlePluginEvent(const ts::PluginEventContext &context) override;

private:
    ts::Report &_report;
    PacketBuffer &_buffer;
};

// Constructor.
ETIInputHandler::ETIInputHandler(ts::Report &report, PacketBuffer &buffer) : _report(report),
                                                                         _buffer(buffer)
{
}

// This event handler is called each time the memory plugin needs input packets.
void ETIInputHandler::handlePluginEvent(const ts::PluginEventContext &context)
{
    ts::PluginEventData *data = dynamic_cast<ts::PluginEventData *>(context.pluginData());
    
    if (data != nullptr)
    {   
        int data_size = data->maxSize();

        while (data_size >= static_cast<int>(ts::PKT_SIZE))
        {
            std::vector<uint8_t> _data = _buffer.pop();
        
            if (!data->append(_data.data(), ts::PKT_SIZE))
            {
                printf("******** PACKETS NOT ACCEPTED INTO MUX\n");
            }
            data_size -= ts::PKT_SIZE; 
        }
    }
}

std::string DabOutput::get_info() const
{
    return "TODO";
}

int DabOutputTS::Open(const char* name)
{
    std::thread ts_thread(&DabOutputTS::SetupMux, this);
    ts_thread.detach();
    return 0;
}

void DabOutputTS::SetupMux()
{
    ts::AsyncReport report(ts::Severity::Info);

    ETIInputHandler meminput(report, pbuffer);

    // Create and start a background system monitor.
    ts::SystemMonitor monitor(report);
    monitor.start();

    // Build tsp options. Accept most default values, except a few ones.
    ts::TSProcessorArgs opt;
    opt.app_name = u"odr-dabmux"; // for error messages only.
    opt.instuff_nullpkt = 25; //Stuff more packets into the input, we'll trim to get CBR later.
    opt.instuff_inpkt = 2;

    opt.input = {u"memory", {}};

    ts::UString pat = u"<?xml version=\"1.0\" encoding=\"UTF-8\"?><tsduck><PAT version=\"0\" transport_stream_id=\"" + ts::UString::Decimal(ts_id) + u"\" network_PID=\"0x0010\"><service service_id=\"" + ts::UString::Decimal(service_id) + u"\" program_map_PID=\"" + ts::UString::Decimal(pmt_pid) + u"\"/></PAT></tsduck>";
    ts::UString pmt = u"<?xml version=\"1.0\" encoding=\"UTF-8\"?><tsduck><PMT version=\"0\" service_id=\"" + ts::UString::Decimal(service_id) + u"\"><component elementary_PID=\"" + ts::UString::Decimal(payload_pid) + u"\" stream_type=\"" + ts::UString::Decimal(service_type) + u"\"/></PMT></tsduck>";
    ts::UString sdt = u"<?xml version=\"1.0\" encoding=\"UTF-8\"?><tsduck><SDT version=\"0\" current=\"true\" transport_stream_id=\"" + ts::UString::Decimal(service_id) + u"\" original_network_id=\"2\" actual=\"true\"> <service service_id=\"" + ts::UString::Decimal(service_id) + u"\" EIT_schedule=\"false\" EIT_present_following=\"false\" running_status=\"running\" CA_mode=\"false\"><service_descriptor service_type=\"0x0C\" service_name=\"" + ts::UString::FromUTF8(service_name) + u"\" service_provider_name=\"" + ts::UString::FromUTF8(service_provider_name) + u"\"/> </service></SDT></tsduck>";

    opt.plugins = {
        {u"inject", {pat, u"--pid", u"0", u"-s", u"--inter-packet", u"100"}},
        {u"inject", {pmt, u"--pid", u"256", u"-s", u"--inter-packet", u"100"}},
        {u"inject", {sdt, u"--pid", u"17", u"-s", u"--inter-packet", u"1000"}},
        {u"limit", {u"--bitrate", u"2500000", u"-p", u"8191", u"-w"}},
    };

    ts::UString port = ts::UString::Decimal(output_port, 0, true, u"", false, ts::SPACE);
    if (output == "srt")
    {
        if (output_srt_passphrase != "")
        {   
            opt.output = {u"srt", {u"-c", ts::UString::FromUTF8(output_host) + u":" + port, u"-e", u"--local-interface", ts::UString::FromUTF8(output_source_address), u"--ipttl", ts::UString::Decimal(output_ttl), u"--passphrase", ts::UString::FromUTF8(output_srt_passphrase) }};
        }
        else
        {   
            opt.output = {u"srt", {u"-c", ts::UString::FromUTF8(output_host) + u":" + port, u"-e", u"--local-interface", ts::UString::FromUTF8(output_source_address), u"--ipttl", ts::UString::Decimal(output_ttl)}};
        }
    }
    else if (output == "udp")
    {
        opt.output = {u"ip", {u"-e", u"-f", u"-l", ts::UString::FromUTF8(output_source_address), ts::UString::FromUTF8(output_host) + u":" + port}};
    }
    else
    {
        printf("No ts output type\n");
        exit(-1);
    }

    ts::TSProcessor tsproc(report);
    tsproc.registerEventHandler(&meminput, ts::PluginType::INPUT);

    // Start the TS processing.
    if (!tsproc.start(opt))
    {
        exit(EXIT_FAILURE);
    }

    // And wait for TS processing termination.
    tsproc.waitForTermination();
}



int DabOutputTS::Write(void* buffer, int size)
{
    int offset = 12;
    offset += 4;

    vec_u8 data(6144);
    uint8_t* buffer_u8 = (uint8_t*)buffer;

    std::copy(buffer_u8, buffer_u8 + size, data.begin());

    // Pad to 6144 bytes
    std::fill(data.begin() + size, data.end(), 0x55);
    
    for (size_t i = 0; i < data.size(); i += (static_cast<int>(ts::PKT_SIZE) - offset))
    {
        ts::TSPacket p_ts;
        if (offset == 1)
        {
            p_ts.b[0] = 0x47;
        }
        else
        {
            p_ts.init();
        }

        if (offset >= 16)
        {
            p_ts.setPID(payload_pid);
            p_ts.setCC(++i_last_cc);
        }

        size_t read_size = std::min(static_cast<size_t>(ts::PKT_SIZE - offset), data.size() - i);
        std::copy_n(data.begin() + i, read_size, p_ts.b + offset);

        std::vector<uint8_t> packet_data(sizeof(p_ts));
        std::memcpy(packet_data.data(), &p_ts, sizeof(p_ts));
        
        pbuffer.push(packet_data);
    }

    return size;
}


int DabOutputTS::Close()
{
    return 0;
}

void DabOutputTS::setMetadata(std::shared_ptr<OutputMetadata> &md)
{
    if (m_allow_metadata) {
        meta_.push_back(md);
    }
}

//#endif