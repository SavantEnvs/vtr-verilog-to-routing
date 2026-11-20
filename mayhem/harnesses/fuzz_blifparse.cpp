// Fuzz libblifparse (the flex/bison BLIF/EBLIF parser VPR reads circuits with) through its public
// blif_parse_file() entry, with the upstream BlifPrettyPrinter callback (as blifparse_test does) so the
// parsed nets/covers/latches/subckts/attributes are all consumed.
#include <cstdint>
#include <cstdio>

#include "blif_pretty_print.hpp"
#include "blifparse.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return -1;  // fmemopen() rejects a zero-length buffer
    FILE* f = fmemopen(const_cast<uint8_t*>(data), size, "r");
    if (!f) return 0;
    FILE* out = fopen("/dev/null", "w");
    {
        blifparse::BlifPrettyPrinter callback(/*print_file_line=*/true);
        // The pretty printer writes to stdout; parse errors are reported through the callback.
        FILE* saved = stdout;
        if (out) stdout = out;
        blifparse::blif_parse_file(f, callback, "fuzz.blif");
        stdout = saved;
    }
    if (out) fclose(out);
    fclose(f);
    return 0;
}
