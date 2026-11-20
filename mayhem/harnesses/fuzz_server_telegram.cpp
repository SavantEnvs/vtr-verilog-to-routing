// Fuzz VPR's server-mode (--server) TCP telegram protocol, the bytes a client sends: the stream is
// fed to comm::TelegramBuffer (framing: "IPA\0" signature, length, checksum, compressor id) the way
// server::GateIO::handle_receiving_data does (vpr/src/server/gateio.cpp), every complete frame body is
// parsed with comm::TelegramParser (job_id / cmd / options), and the options string is decoded with
// server::TelegramOptions for the option keys TaskResolver reads (vpr/src/server/taskresolver.cpp).
// The input is appended in chunks whose sizes come from the input itself, as TCP reads would split it.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "commconstants.h"
#include "telegrambuffer.h"
#include "telegramheader.h"
#include "telegramoptions.h"
#include "telegramparser.h"
#include "vtr_error.h"

static void process_options(const std::string& opts) {
    using namespace comm;
    {  // TaskResolver::process_get_path_list_task
        server::TelegramOptions options{opts, {OPTION_PATH_NUM, OPTION_PATH_TYPE, OPTION_DETAILS_LEVEL,
                                               OPTION_IS_FLAT_ROUTING}};
        if (!options.has_errors()) {
            (void)options.get_int(OPTION_PATH_NUM, 1);
            (void)options.get_string(OPTION_PATH_TYPE);
            (void)options.get_int(OPTION_DETAILS_LEVEL, 0);
            (void)options.get_bool(OPTION_IS_FLAT_ROUTING, false);
        }
    }
    {  // TaskResolver::process_draw_critical_path_task
        server::TelegramOptions options{opts, {OPTION_PATH_ELEMENTS, OPTION_HIGHLIGHT_MODE,
                                               OPTION_DRAW_PATH_CONTOUR}};
        if (!options.has_errors()) {
            (void)options.get_map_of_sets(OPTION_PATH_ELEMENTS);
            (void)options.get_string(OPTION_HIGHLIGHT_MODE);
            (void)options.get_bool(OPTION_DRAW_PATH_CONTOUR, false);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return -1;
    // byte 0: TCP read chunk size (1..256)
    size_t chunk = (size_t)data[0] + 1;
    const char* p = reinterpret_cast<const char*>(data + 1);
    size_t n = size - 1;
    comm::TelegramBuffer buff;
    std::vector<comm::TelegramFramePtr> frames;
    std::vector<std::string> errors;
    try {
        for (size_t off = 0; off < n || off == 0; off += chunk) {
            size_t len = std::min(chunk, n - off);
            buff.append(comm::ByteArray(p + off, len));
            buff.take_errors(errors);
            buff.take_telegram_frames(frames);
            for (const comm::TelegramFramePtr& frame : frames) {
                std::string message{frame->body};
                if (message == comm::ECHO_TELEGRAM_BODY) continue;
                auto job_id = comm::TelegramParser::try_extract_field_job_id(message);
                auto cmd = comm::TelegramParser::try_extract_field_cmd(message);
                auto opts = comm::TelegramParser::try_extract_field_options(message);
                (void)comm::TelegramParser::try_extract_field_data(message);
                (void)comm::TelegramParser::try_extract_field_status(message);
                if (job_id && cmd && opts) process_options(*opts);
            }
            frames.clear();
            errors.clear();
            if (n == 0) break;
        }
    } catch (vtr::VtrError&) {
    }
    return 0;
}

#ifdef MAYHEM_TELEGRAM_SEED_GEN
// Writes one seed: chunk byte + header + body, using upstream's own header construction.
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    std::string body = argv[2];
    comm::TelegramHeader h = comm::TelegramHeader::construct_from_body(body);
    FILE* f = fopen(argv[1], "wb");
    fputc(63, f);
    fwrite(h.buffer().data(), 1, h.buffer().size(), f);
    fwrite(body.data(), 1, body.size(), f);
    fclose(f);
    return 0;
}
#endif
