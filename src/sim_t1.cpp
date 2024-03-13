#define ENABLE_TRACE
#include "verilated.h"
#ifdef ENABLE_TRACE
#include "verilated_fst_c.h"
#endif
#include "VT1Subsystem.h"


#include "tilelink_xbar.hpp"
#include "mmio_mem.hpp"
#include "uartlite.hpp"

#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <thread>

void connect_wire(tilelink_ptr<30, 8, 16, 3> &scalar_ptr, tilelink_ptr<29, 8, 2, 3> &mmio_ptr, tilelink_ptr<32, 8, 15, 3> (&vector_ptr)[12], VT1Subsystem *top) {
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
#define gen_vector_chan_conn(x) \
    vector_ptr[x].a_bits_opcode  = &(top->vectorChannel##x##_0_a_bits_opcode); \
    vector_ptr[x].a_bits_param   = &(top->vectorChannel##x##_0_a_bits_param); \
    vector_ptr[x].a_bits_size    = &(top->vectorChannel##x##_0_a_bits_size); \
    vector_ptr[x].a_bits_source  = &(top->vectorChannel##x##_0_a_bits_source); \
    vector_ptr[x].a_bits_address = &(top->vectorChannel##x##_0_a_bits_address); \
    vector_ptr[x].a_bits_mask    = &(top->vectorChannel##x##_0_a_bits_mask); \
    vector_ptr[x].a_bits_data    = &(top->vectorChannel##x##_0_a_bits_data); \
    vector_ptr[x].a_bits_corrupt = &(top->vectorChannel##x##_0_a_bits_corrupt); \
    vector_ptr[x].a_valid        = &(top->vectorChannel##x##_0_a_valid); \
    vector_ptr[x].a_ready        = &(top->vectorChannel##x##_0_a_ready); \
    vector_ptr[x].d_bits_opcode  = &(top->vectorChannel##x##_0_d_bits_opcode); \
    vector_ptr[x].d_bits_param   = &(top->vectorChannel##x##_0_d_bits_param); \
    vector_ptr[x].d_bits_size    = &(top->vectorChannel##x##_0_d_bits_size); \
    vector_ptr[x].d_bits_source  = &(top->vectorChannel##x##_0_d_bits_source); \
    vector_ptr[x].d_bits_denied  = &(top->vectorChannel##x##_0_d_bits_denied); \
    vector_ptr[x].d_bits_data    = &(top->vectorChannel##x##_0_d_bits_data); \
    vector_ptr[x].d_bits_corrupt = &(top->vectorChannel##x##_0_d_bits_corrupt); \
    vector_ptr[x].d_valid        = &(top->vectorChannel##x##_0_d_valid); \
    vector_ptr[x].d_ready        = &(top->vectorChannel##x##_0_d_ready);
    gen_vector_chan_conn(0);
    gen_vector_chan_conn(1);
    gen_vector_chan_conn(2);
    gen_vector_chan_conn(3);
    gen_vector_chan_conn(4);
    gen_vector_chan_conn(5);
    gen_vector_chan_conn(6);
    gen_vector_chan_conn(7);
    gen_vector_chan_conn(8);
    gen_vector_chan_conn(9);
    gen_vector_chan_conn(10);
    gen_vector_chan_conn(11);
#undef gen_vector_chan_conn
}

bool running = true;
uint64_t max_cycle = 0;
char *memory_path = NULL;
bool open_trace = false;

void parse_args(int argc, char** argv) {
    for (int i=1;i+1<argc;i++) {
        if (strcmp(argv[i], "-cycle") == 0) {
            max_cycle = atoll(argv[i+1]);
            i++;
        } else if (strcmp(argv[i], "-init") == 0) {
            memory_path = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "-trace") == 0) {
            open_trace = true;
        }
    }
}

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);
    parse_args(argc, argv);
    VT1Subsystem *top = new VT1Subsystem;
#ifdef ENABLE_TRACE
    VerilatedFstC fst;
    if (open_trace) {
        Verilated::traceEverOn(true);
        // connect fst for trace
        top->trace(&fst,0);
        fst.open("trace.fst");
    }
#endif
    tilelink_ptr <30, 8, 16, 3> mem_ptr;
    tilelink_ptr <29, 8, 2, 3> mmio_ptr;
    tilelink_ptr <32, 8, 15, 3> vector_ptr[12];

    connect_wire(mem_ptr, mmio_ptr, vector_ptr, top);
    assert(mem_ptr.check());
    assert(mmio_ptr.check());

    tilelink_ref <30, 8, 16, 3> mem_ref(mem_ptr);
    tilelink_ref <29, 8, 2, 3> mmio_ref(mmio_ptr);
    tilelink_ref <32, 8, 15, 3> vector_ptr_ref[12] = {
        vector_ptr[ 0], vector_ptr[ 1], vector_ptr[ 2], vector_ptr[ 3],
        vector_ptr[ 4], vector_ptr[ 5], vector_ptr[ 6], vector_ptr[ 7],
        vector_ptr[ 8], vector_ptr[ 9], vector_ptr[10], vector_ptr[11],
    };

    tilelink <30, 8, 16, 3> mem_sigs;
    tilelink <29, 8, 2, 3> mmio_sigs;
    tilelink <32, 8, 15, 3> vector_sigs[12];

    tilelink_ref <30, 8, 16, 3> mem_sigs_ref(mem_sigs);
    tilelink_ref <29, 8, 2, 3> mmio_sigs_ref(mmio_sigs);
    tilelink_ref <32, 8, 15, 3> vector_sigs_ref[12] = {
        vector_sigs[ 0], vector_sigs[ 1], vector_sigs[ 2], vector_sigs[ 3],
        vector_sigs[ 4], vector_sigs[ 5], vector_sigs[ 6], vector_sigs[ 7],
        vector_sigs[ 8], vector_sigs[ 9], vector_sigs[10], vector_sigs[11]
    };

    tilelink_xbar<30, 8, 16, 3> mem_xbar;
    tilelink_xbar<29, 8, 2, 3> mmio_xbar;
    tilelink_xbar<32, 8, 15, 3> vector_xbar[12];

    // scarlar memory
    mmio_mem scarlar_mem(512*1024*1024);
    mem_xbar.add_dev(0x20000000, 0x20000000, &scarlar_mem);
    if (memory_path) scarlar_mem.load_binary(0x0, memory_path);
    else fprintf(stderr, "Warning: no init memory\n");

    // uart
    uartlite uart;
    mmio_xbar.add_dev(0x10000000, 32, &uart);

    /*
        Vector Memory:
        [0-3] 4*512M DDR from 1G step +512M
        [4-11] 8*256K SRAM from 3G step 256K
     */
    mmio_mem vector_mem[12] = {
        mmio_mem(0x20000000), mmio_mem(0x20000000), mmio_mem(0x20000000),
        mmio_mem(0x20000000),
        mmio_mem(0x40000), mmio_mem(0x40000), mmio_mem(0x40000),
        mmio_mem(0x40000), mmio_mem(0x40000), mmio_mem(0x40000),
        mmio_mem(0x40000), mmio_mem(0x40000)
    };

    for (int i=0;i<4;i++) {
        vector_xbar[i].add_dev(0x40000000u+0x20000000*i, 0x20000000, &vector_mem[i]);
    }

    for (int i=0;i<8;i++) {
        vector_xbar[i+4].add_dev(0xC0000000u+0x40000*i, 0x40000, &vector_mem[i+4]);
    }

    // rtl simulation
    top->clock_0_reset = 1;
    top->clock_0_clock = 0;
    top->reset_vector_0 = 0x20000000;
    uint64_t ticks = 0;

    while (!Verilated::gotFinish() && (max_cycle == 0 || ticks < max_cycle) && running) {
        top->clock_0_clock = !top->clock_0_clock;
        if (ticks++ == 9) top->clock_0_reset = 0;
        if (top->clock_0_clock && !top->clock_0_reset) {
            // posedge
            mem_sigs.update_input(mem_ref);
            mmio_sigs.update_input(mmio_ref);
            for (int i=0;i<12;i++) vector_sigs[i].update_input(vector_ptr_ref[i]);
        }
        top->eval();
        if (top->clock_0_clock) {
            mem_xbar.tick(mem_sigs_ref);
            mmio_xbar.tick(mmio_sigs_ref);
            for (int i=0;i<12;i++) vector_xbar[i].tick(vector_sigs_ref[i]);
            mem_sigs.update_output(mem_ref);
            mmio_sigs.update_output(mmio_ref);
            for (int i=0;i<12;i++) vector_sigs[i].update_output(vector_ptr_ref[i]);
            // uart
            while (uart.exist_tx()) {
                char c = uart.getc();
                printf("%c",c);
                fflush(stdout);
            }
        }
        top->eval();
#ifdef ENABLE_TRACE
        if (open_trace) fst.dump(ticks);
#endif
    }
#ifdef ENABLE_TRACE
    if (open_trace) fst.close();
#endif
    top->final();
    return 0;
}