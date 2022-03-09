#include "verilated_vcd_c.h"
#include "verilated.h"
#include "VChipTop.h"
#include "axi4.hpp"

#include <iostream>

int main(int argc, char** argv, char** env) {
    VChipTop *chiptop = new VChipTop;
    axi4_ptr <64,32,4> mmio;
    std::cout << mmio.check() << std::endl;
    return 0;
}