#include "edi_ts.h"
#include "AFPacket.h"
#include <thread>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>

class InputHandler : public ts::PluginEventHandlerInterface
{
public:
    // Constructors.
    InputHandler() = delete;
    InputHandler(ts::Report &report, PacketBuffer &buffer);

    // Event handling (from ts::PluginEventHandlerInterface).
    virtual void handlePluginEvent(const ts::PluginEventContext &context) override;

private:
    ts::Report &_report;
    PacketBuffer &_buffer;
};

// Constructor.
InputHandler::InputHandler(ts::Report &report, PacketBuffer &buffer) : _report(report),
                                                                         _buffer(buffer)
{
}

// This event handler is called each time the memory plugin needs input packets.
void InputHandler::handlePluginEvent(const ts::PluginEventContext &context)
{
    ts::PluginEventData *data = dynamic_cast<ts::PluginEventData *>(context.pluginData());
    
    if (data != nullptr)
    {
        if (data->maxSize() >= ts::PKT_SIZE)
        {
            std::vector<uint8_t> _data = _buffer.pop();
            if (!data->append(_data.data(), ts::PKT_SIZE))
            {
                printf("******** PACKETS NOT ACCEPTED INTO MUX\n");
            } 
        }
    }
}

void edi_ts::Open(const std::string &test)
{
    std::thread ts_thread(&edi_ts::SetupMux, this);
    ts_thread.detach();
}

void edi_ts::SetupMux()
{
    ts::AsyncReport report(ts::Severity::Info);

    InputHandler meminput(report, buffer);

    // Create and start a background system monitor.
    ts::SystemMonitor monitor(report);
    monitor.start();

    // Build tsp options. Accept most default values, except a few ones.
    ts::TSProcessorArgs opt;
    opt.app_name = u"odr-dabmux"; // for error messages only.
    opt.instuff_nullpkt = 50; //Stuff more packets into the input, we'll trim to get CBR later.
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
        printf("SRT ");
        if (output_srt_passphrase != "")
        {   printf("pass: %s\n", output_srt_passphrase.c_str());
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

void edi_ts::send(const std::vector<uint8_t> &data)
{
    int offset = 12;
    int pid = 1051;

    offset += 4;

    for (size_t i = 0; i < data.size(); i += (ts::PKT_SIZE - offset))
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
            p_ts.setPID(pid);
            p_ts.setCC(++i_last_cc);
        }

        size_t read_size = std::min(static_cast<size_t>(ts::PKT_SIZE - offset), data.size() - i);
        std::copy_n(data.begin() + i, read_size, p_ts.b + offset);

        std::vector<uint8_t> packet_data(sizeof(p_ts));
        std::memcpy(packet_data.data(), &p_ts, sizeof(p_ts));
        buffer.push(packet_data);
    }
}