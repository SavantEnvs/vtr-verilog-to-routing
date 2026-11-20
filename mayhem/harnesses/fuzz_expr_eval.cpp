// Fuzz vtr::FormulaParser, the expression evaluator VTR uses for architecture switchblock /
// wire-connection formulas, through libarchfpga's get_sb_formula_raw_result() — the entry the
// architecture reader uses: it rejects an empty formula and dispatches to parse_piecewise_formula()
// or parse_formula() via is_piecewise_formula(). The input is the formula text; like an XML
// attribute value it ends at the first NUL. Variables are set as in
// libs/libvtrutil/test/test_expr_eval.cpp.
//
// The is_breakpoint=true mode is not fuzzed: breakpoint expressions are typed into the interactive
// graphics debugger (vpr/src/draw), not read from a file (see mayhem/fuzz_expr_eval/known-findings/).
#include <cstdint>
#include <cstring>
#include <string>

#include "parse_switchblocks.h"
#include "vtr_error.h"
#include "vtr_expr_eval.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const char* p = reinterpret_cast<const char*>(data);
    std::string formula(p, strnlen(p, size));
    vtr::t_formula_data vars;
    vars.set_var_value("W", 300);
    vars.set_var_value("x", 3);
    vars.set_var_value("y", 7);
    vars.set_var_value("t", 2);
    vars.set_var_value("Fs", 3);
    vars.set_var_value("Fc_in", 6);
    vars.set_var_value("Fc_out", 10);
    vtr::FormulaParser fp;
    try {
        get_sb_formula_raw_result(fp, formula, vars);
    } catch (vtr::VtrError&) {
    }
    return 0;
}
