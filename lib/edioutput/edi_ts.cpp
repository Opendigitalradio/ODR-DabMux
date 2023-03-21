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

#define TS_SIZE 188

class InputHandler : public ts::PluginEventHandlerInterface
{
public:
    // Constructors.
    InputHandler() = delete;
    InputHandler(ts::Report &report, CircularBuffer &buffer);

    // Event handling (from ts::PluginEventHandlerInterface).
    virtual void handlePluginEvent(const ts::PluginEventContext &context) override;

private:
    ts::Report &_report;
    CircularBuffer &_buffer;
};

// Constructor.
InputHandler::InputHandler(ts::Report &report, CircularBuffer &buffer) : _report(report),
                                                                         _buffer(buffer)
{
}

// This event handler is called each time the memory plugin needs input packets.
void InputHandler::handlePluginEvent(const ts::PluginEventContext &context)
{
    ts::PluginEventData *data = dynamic_cast<ts::PluginEventData *>(context.pluginData());
    if (data != nullptr)
    {
        ts::TSPacketVector _data = _buffer.pop();
        for (ts::TSPacketVector::const_iterator it = _data.begin(); it != _data.end(); ++it)
            {
                if (data->maxSize() >= ts::PKT_SIZE)
                {
                    if (data->append(&(*it), ts::PKT_SIZE))
                    {
//                        printf("Data accepted\n");
                    }
                    else
                    {
                        printf("******** NOT ACCEPTED\n");
                    }
                }
            }
        }
    }

void edi_ts::Open(const std::string &test)
{
    std::thread ts_thread(&edi_ts::SetupMux, this);
    ts_thread.detach();
}

void edi_ts::Close()
{
    // implementation for closing the connection
}

void edi_ts::SetupMux()
{

    // Use an asynchronous logger to report errors, logs, debug, etc.
    // Make it display all messages up to debug level (default is info level).
    // ts::AsyncReport report(ts::Severity::Debug);
    ts::AsyncReport report(ts::Severity::Debug);

    InputHandler meminput(report, buffer);

    // Create and start a background system monitor.
    ts::SystemMonitor monitor(report);
    monitor.start();

    // Build tsp options. Accept most default values, except a few ones.
    ts::TSProcessorArgs opt;
    opt.app_name = u"odr-dabmux"; // for error messages only.
    opt.instuff_start = 10;       // insert 10 null packets at start of stream.
    opt.instuff_stop = 5;         // insert 5 null packets at end of stream.
    opt.instuff_nullpkt = 7;
    opt.instuff_inpkt = 24;

    opt.input = {u"memory", {}};

    opt.plugins = {
        {u"inject", {u"<?xml version=\"1.0\" encoding=\"UTF-8\"?><tsduck><PAT version=\"0\" transport_stream_id=\"0x0001\" network_PID=\"0x0010\"><service service_id=\"0x0001\" program_map_PID=\"256\"/></PAT></tsduck>", u"--pid", u"0", u"-s", u"--inter-packet", u"100"}},
        {u"inject", {u"<?xml version=\"1.0\" encoding=\"UTF-8\"?><tsduck><PMT version=\"4\" service_id=\"0x0001\"><component elementary_PID=\"1051\" stream_type=\"0x12\"/></PMT></tsduck>", u"--pid", u"256", u"-s", u"--inter-packet", u"100"}},
        {u"inject", {u"<?xml version=\"1.0\" encoding=\"UTF-8\"?><tsduck><SDT version=\"0\" current=\"true\" transport_stream_id=\"1\" original_network_id=\"2\" actual=\"true\"> <service service_id=\"1\" EIT_schedule=\"false\" EIT_present_following=\"false\" running_status=\"running\" CA_mode=\"false\"><service_descriptor service_type=\"0x0C\" service_name=\"SCambs DAB\" service_provider_name=\"Andy Test\"/> </service></SDT></tsduck>", u"--pid", u"17", u"-s", u"--inter-packet", u"1000"}},
    };

    opt.output = {u"srt", {u"-c", u"192.168.105.2:5001", u"--passphrase", u"testtesttest"}};

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
    ts::TSPacketVector packets;
    int offset = 12;
    int pid = 1051;

    packets.reserve(data.size() / (TS_SIZE - offset) + 1);

    offset += 4;
    unsigned long int packet_count = 0;
    for (size_t i = 0; i < data.size(); i += TS_SIZE - offset)
    {
        ts::TSPacket p_ts;

        p_ts.init();
        if (offset >= 16)
        {
            p_ts.setPID(pid);
            p_ts.setCC(++i_last_cc);
            // printf("CC: %d\n", i_last_cc);
        }

        size_t read_size = std::min(static_cast<size_t>(TS_SIZE - offset), data.size() - i);
        std::copy_n(data.begin() + i, read_size, p_ts.b + offset);

        packets.push_back(p_ts);
        packet_count++;
    }

        for (ts::TSPacketVector::const_iterator it = packets.begin(); it != packets.end(); ++it)
        {
        std::cout << "Packet data in:\n" << ts::UString::Dump(&(*it), ts::PKT_SIZE, ts::UString::SINGLE_LINE) << std::endl;
        }


    buffer.push(packets);
    // printf("Packets Q Size: %d\n", static_cast<int>(packets.size()));
}