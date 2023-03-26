#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#if HAVE_OUTPUT_TS

#include <string>
#include <memory>
#include <tsduck.h>
#include "../../src/PacketBuffer.h"
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
    PacketBuffer pbuffer;

    void Open(const std::string& test);
    void send(const std::vector<uint8_t>& data);
  private:
    void SetupMux();
    
};
#endif