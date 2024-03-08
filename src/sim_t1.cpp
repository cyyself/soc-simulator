#define ENABLE_TRACE
#include "verilated.h"
#ifdef ENABLE_TRACE
#include "verilated_fst_c.h"
#endif
#include "VT1Subsystem.h"


#include "tilelink_xbar.hpp"

#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <thread>

void connect_wire(tilelink_ptr<30, 8, 16, 3> &scalar_ptr, tilelink_ptr<29, 8, 2, 3> &mmio_ptr, VT1Subsystem *top) {
    // connect
    // a
    scalar_ptr.a_bits_opcode  = &(top->scalarPort_0_a_bits_opcode);
    scalar_ptr.a_bits_param   = &(top->scalarPort_0_a_bits_param);
    scalar_ptr.a_bits_size    = &(top->scalarPort_0_a_bits_size);
    scalar_ptr.a_bits_source  = &(top->scalarPort_0_a_bits_source);
    scalar_ptr.a_bits_address = &(top->scalarPort_0_a_bits_address);
    scalar_ptr.a_bits_mask    = &(top->scalarPort_0_a_bits_mask);
    scalar_ptr.a_bits_data    = &(top->scalarPort_0_a_bits_data);
    scalar_ptr.a_bits_corrupt = &(top->scalarPort_0_a_bits_corrupt);
    scalar_ptr.a_valid        = &(top->scalarPort_0_a_valid);
    scalar_ptr.a_ready        = &(top->scalarPort_0_a_ready);
    // d
    scalar_ptr.d_bits_opcode  = &(top->scalarPort_0_d_bits_opcode);
    scalar_ptr.d_bits_param   = &(top->scalarPort_0_d_bits_param);
    scalar_ptr.d_bits_size    = &(top->scalarPort_0_d_bits_size);
    scalar_ptr.d_bits_source  = &(top->scalarPort_0_d_bits_source);
    scalar_ptr.d_bits_denied  = &(top->scalarPort_0_d_bits_denied);
    scalar_ptr.d_bits_data    = &(top->scalarPort_0_d_bits_data);
    scalar_ptr.d_bits_corrupt = &(top->scalarPort_0_d_bits_corrupt);
    scalar_ptr.d_valid        = &(top->scalarPort_0_d_valid);
    scalar_ptr.d_ready        = &(top->scalarPort_0_d_ready);

    // mmio
    // a
    mmio_ptr.a_bits_opcode  = &(top->mmioPort_0_a_bits_opcode);
    mmio_ptr.a_bits_param   = &(top->mmioPort_0_a_bits_param);
    mmio_ptr.a_bits_size    = &(top->mmioPort_0_a_bits_size);
    mmio_ptr.a_bits_source  = &(top->mmioPort_0_a_bits_source);
    mmio_ptr.a_bits_address = &(top->mmioPort_0_a_bits_address);
    mmio_ptr.a_bits_mask    = &(top->mmioPort_0_a_bits_mask);
    mmio_ptr.a_bits_data    = &(top->mmioPort_0_a_bits_data);
    mmio_ptr.a_bits_corrupt = &(top->mmioPort_0_a_bits_corrupt);
    mmio_ptr.a_valid        = &(top->mmioPort_0_a_valid);
    mmio_ptr.a_ready        = &(top->mmioPort_0_a_ready);
    // d
    mmio_ptr.d_bits_opcode  = &(top->mmioPort_0_d_bits_opcode);
    mmio_ptr.d_bits_param   = &(top->mmioPort_0_d_bits_param);
    mmio_ptr.d_bits_size    = &(top->mmioPort_0_d_bits_size);
    mmio_ptr.d_bits_source  = &(top->mmioPort_0_d_bits_source);
    mmio_ptr.d_bits_denied  = &(top->mmioPort_0_d_bits_denied);
    mmio_ptr.d_bits_data    = &(top->mmioPort_0_d_bits_data);
    mmio_ptr.d_bits_corrupt = &(top->mmioPort_0_d_bits_corrupt);
    mmio_ptr.d_valid        = &(top->mmioPort_0_d_valid);
    mmio_ptr.d_ready        = &(top->mmioPort_0_d_ready);
}

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);
    VT1Subsystem *top = new VT1Subsystem;
#ifdef ENABLE_TRACE
    Verilated::traceEverOn(true);
    // fst
    VerilatedFstC fst;
    // connect fst for trace
    top->trace(&fst,0);
    fst.open("trace.fst");
#endif
    tilelink_ptr <30, 8, 16, 3> mem_ptr;
    tilelink_ptr <29, 8, 2, 3> mmio_ptr;

    connect_wire(mem_ptr, mmio_ptr, top);
    assert(mem_ptr.check());
    assert(mmio_ptr.check());

    tilelink_ref <30, 8, 16, 3> mem_ref(mem_ptr);
    tilelink_ref <29, 8, 2, 3> mmio_ref(mmio_ptr);

    tilelink <30, 8, 16, 3> mem_sigs;
    tilelink <29, 8, 2, 3> mmio_sigs;

    tilelink_ref <30, 8, 16, 3> mem_sigs_ref(mem_sigs);
    tilelink_ref <29, 8, 2, 3> mmio_sigs_ref(mmio_sigs);

    tilelink_xbar<30, 8, 16, 3> mem_xbar;
    tilelink_xbar<29, 8, 2, 3> mmio_xbar;

    top->io_localT1Subsystem_0_reset = 1;
    top->io_localT1Subsystem_0_clock = 0;
    top->reset_vector_0 = 0x20000000;
    uint64_t ticks = 0;

    while (!Verilated::gotFinish() && ticks < 10000) {
        top->io_localT1Subsystem_0_clock = !top->io_localT1Subsystem_0_clock;
        if (ticks++ == 9) top->io_localT1Subsystem_0_reset = 0;
        if (top->io_localT1Subsystem_0_clock && !top->io_localT1Subsystem_0_reset) {
            // posedge
            mem_sigs.update_input(mem_ref);
            mmio_sigs.update_input(mmio_ref);
        }
        top->eval();
        if (top->io_localT1Subsystem_0_clock) {
            mem_xbar.tick(mem_sigs_ref);
            mmio_xbar.tick(mmio_sigs_ref);
            mem_sigs.update_output(mem_ref);
            mmio_sigs.update_output(mmio_ref);
        }
        top->eval();
#ifdef ENABLE_TRACE
        fst.dump(ticks);
#endif
    }
#ifdef ENABLE_TRACE
    fst.close();
#endif
    top->final();
    return 0;
}