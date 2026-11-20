// Fuzz VTR's XML FPGA architecture reader (libarchfpga xml_read_arch), as libs/libarchfpga/src/main.cpp
// (the upstream read_arch tool) drives it: the reader takes a file path, so the input is written to a
// scratch file under $TMPDIR. Errors are reported with vtr::VtrError / std::exception.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

#include "arch_util.h"
#include "read_xml_arch_file.h"
#include "vtr_error.h"
#include "vtr_log.h"

static std::string g_path;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    const char* tmp = getenv("TMPDIR");
    std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/vtr_arch_XXXXXX.xml";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    int fd = mkstemps(buf.data(), 4);
    if (fd < 0) abort();
    close(fd);
    g_path = buf.data();
    // Silence the reader's (very chatty) logging; set once, identical for every input.
    vtr::set_log_file(nullptr);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FILE* f = fopen(g_path.c_str(), "wb");
    if (!f) abort();
    fwrite(data, 1, size, f);
    fclose(f);

    t_arch arch;
    std::vector<t_physical_tile_type> physical_tile_types;
    std::vector<t_logical_block_type> logical_block_types;
    try {
        xml_read_arch(g_path.c_str(), /*timing_enabled=*/true, &arch, physical_tile_types,
                      logical_block_types, /*device_model_warnings=*/true);
    } catch (vtr::VtrError&) {
    } catch (std::exception&) {
    }
    free_arch(&arch);
    free_type_descriptors(physical_tile_types);
    free_type_descriptors(logical_block_types);
    return 0;
}
