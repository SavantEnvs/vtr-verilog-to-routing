# fuzz_expr_eval known findings

All three live in `libs/libvtrutil/src/vtr_expr_eval.cpp` and are reachable from an architecture
XML file: switchblock/wireconn formulas go through `get_sb_formula_raw_result()`
(`libs/libarchfpga/src/parse_switchblocks.cpp`), and the same parser evaluates grid-location
expressions (`vpr/src/base/setup_grid.cpp`). Reproduce with `/mayhem/fuzz_expr_eval-standalone <file>`.

| dir | input | crash | cause | one-line fix |
| --- | --- | --- | --- | --- |
| `div-by-zero/` | `W/0`, `W%0` | UBSan `division by zero` in `apply_rpn_op` (E_OP_DIV / E_OP_MOD) | the divisor is never checked | throw `vtr::VtrError` when `arg2.data.num == 0` for `/` and `%` |
| `empty-formula/` | ` ` (whitespace only) | UBSan `reference binding to null pointer` in `parse_rpn_vector` (`rpn_vec[0]` on an empty vector) | `get_sb_formula_raw_result` rejects `""` but not a formula that tokenizes to nothing | in `parse_rpn_vector`, throw when `rpn_vec.empty()` |
| `missing-operand/` | `1+`, a malformed piecewise formula | UBSan pointer-overflow / out-of-bounds `rpn_vec[ivec - 2]` in `parse_rpn_vector` | an operator with fewer than two preceding operands indexes before the start of the vector | throw when `ivec < 2` before applying the operator |

Impact: a malformed architecture file makes `vpr`/`read_arch` read out of bounds or trap
(UB) instead of reporting a parse error.

The harness does not fuzz `parse_formula(..., is_breakpoint=true)`: breakpoint expressions are
typed into the interactive graphics debugger (`vpr/src/draw`), not read from a file. An empty
breakpoint expression hits the same `empty-formula` bug.
