#include <string>
#include <memory>
#include <tsduck.h>
#include "CircularBuffer.h"
#include "ThreadsafeQueue.h"

class edi_ts {
public:
    unsigned int payload_pid;
    unsigned int pmt_pid;
    unsigned int ts_id;
    unsigned int service_type;
    unsigned int service_id;
    std::string service_name;
    std::string service_provider_name;
    std::string output;
    std::string output_host;
    unsigned int output_port;
    unsigned int output_ttl;
    std::string output_srt_passphrase;
    std::string output_source_address;
    uint8_t i_last_cc = -1;
    CircularBuffer buffer;

    void Open(const std::string& test);
    void Close();
    void send(const std::vector<uint8_t>& data);
  private:
    void SetupMux();
    ts::TSPacketVector packetise_payload(const std::vector<uint8_t>& data, int offset, int pid);
    ThreadsafeQueue<std::vector<uint8_t> > m_queue;  
};