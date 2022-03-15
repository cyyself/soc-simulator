#include "verilated_vcd_c.h"
#include "verilated.h"
#include "VChipTop.h"

#include "axi4.hpp"
#include "axi4_mem.hpp"
#include "axi4_xbar.hpp"
#include "mmio_mem.hpp"
#include "uartlite.hpp"

#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <thread>

void connect_wire(axi4_ptr <31,64,4> &mmio_ptr, axi4_ptr <33,64,4> &mem_ptr, VChipTop *top) {
    // connect
    // mmio
    // aw   
    mmio_ptr.awaddr     = &(top->axi4_mmio_0_bits_aw_bits_addr);
    mmio_ptr.awburst    = &(top->axi4_mmio_0_bits_aw_bits_burst);
    mmio_ptr.awid       = &(top->axi4_mmio_0_bits_aw_bits_id);
    mmio_ptr.awlen      = &(top->axi4_mmio_0_bits_aw_bits_len);
    mmio_ptr.awready    = &(top->axi4_mmio_0_bits_aw_ready);
    mmio_ptr.awsize     = &(top->axi4_mmio_0_bits_aw_bits_size);
    mmio_ptr.awvalid    = &(top->axi4_mmio_0_bits_aw_valid);
    // w
    mmio_ptr.wdata      = &(top->axi4_mmio_0_bits_w_bits_data);
    mmio_ptr.wlast      = &(top->axi4_mmio_0_bits_w_bits_last);
    mmio_ptr.wready     = &(top->axi4_mmio_0_bits_w_ready);
    mmio_ptr.wstrb      = &(top->axi4_mmio_0_bits_w_bits_strb);
    mmio_ptr.wvalid     = &(top->axi4_mmio_0_bits_w_valid);
    // b
    mmio_ptr.bid        = &(top->axi4_mmio_0_bits_b_bits_id);
    mmio_ptr.bready     = &(top->axi4_mmio_0_bits_b_ready);
    mmio_ptr.bresp      = &(top->axi4_mmio_0_bits_b_bits_resp);
    mmio_ptr.bvalid     = &(top->axi4_mmio_0_bits_b_valid);
    // ar
    mmio_ptr.araddr     = &(top->axi4_mmio_0_bits_ar_bits_addr);
    mmio_ptr.arburst    = &(top->axi4_mmio_0_bits_ar_bits_burst);
    mmio_ptr.arid       = &(top->axi4_mmio_0_bits_ar_bits_id);
    mmio_ptr.arlen      = &(top->axi4_mmio_0_bits_ar_bits_len);
    mmio_ptr.arready    = &(top->axi4_mmio_0_bits_ar_ready);
    mmio_ptr.arsize     = &(top->axi4_mmio_0_bits_ar_bits_size);
    mmio_ptr.arvalid    = &(top->axi4_mmio_0_bits_ar_valid);
    // r
    mmio_ptr.rdata      = &(top->axi4_mmio_0_bits_r_bits_data);
    mmio_ptr.rid        = &(top->axi4_mmio_0_bits_r_bits_id);
    mmio_ptr.rlast      = &(top->axi4_mmio_0_bits_r_bits_last);
    mmio_ptr.rready     = &(top->axi4_mmio_0_bits_r_ready);
    mmio_ptr.rresp      = &(top->axi4_mmio_0_bits_r_bits_resp);
    mmio_ptr.rvalid     = &(top->axi4_mmio_0_bits_r_valid);
    // mem
    // aw
    mem_ptr.awaddr  = &(top->axi4_mem_0_bits_aw_bits_addr);
    mem_ptr.awburst = &(top->axi4_mem_0_bits_aw_bits_burst);
    mem_ptr.awid    = &(top->axi4_mem_0_bits_aw_bits_id);
    mem_ptr.awlen   = &(top->axi4_mem_0_bits_aw_bits_len);
    mem_ptr.awready = &(top->axi4_mem_0_bits_aw_ready);
    mem_ptr.awsize  = &(top->axi4_mem_0_bits_aw_bits_size);
    mem_ptr.awvalid = &(top->axi4_mem_0_bits_aw_valid);
    // w
    mem_ptr.wdata   = &(top->axi4_mem_0_bits_w_bits_data);
    mem_ptr.wlast   = &(top->axi4_mem_0_bits_w_bits_last);
    mem_ptr.wready  = &(top->axi4_mem_0_bits_w_ready);
    mem_ptr.wstrb   = &(top->axi4_mem_0_bits_w_bits_strb);
    mem_ptr.wvalid  = &(top->axi4_mem_0_bits_w_valid);
    // b
    mem_ptr.bid     = &(top->axi4_mem_0_bits_b_bits_id);
    mem_ptr.bready  = &(top->axi4_mem_0_bits_b_ready);
    mem_ptr.bresp   = &(top->axi4_mem_0_bits_b_bits_resp);
    mem_ptr.bvalid  = &(top->axi4_mem_0_bits_b_valid);
    // ar
    mem_ptr.araddr  = &(top->axi4_mem_0_bits_ar_bits_addr);
    mem_ptr.arburst = &(top->axi4_mem_0_bits_ar_bits_burst);
    mem_ptr.arid    = &(top->axi4_mem_0_bits_ar_bits_id);
    mem_ptr.arlen   = &(top->axi4_mem_0_bits_ar_bits_len);
    mem_ptr.arready = &(top->axi4_mem_0_bits_ar_ready);
    mem_ptr.arsize  = &(top->axi4_mem_0_bits_ar_bits_size);
    mem_ptr.arvalid = &(top->axi4_mem_0_bits_ar_valid);
    // r
    mem_ptr.rdata   = &(top->axi4_mem_0_bits_r_bits_data);
    mem_ptr.rid     = &(top->axi4_mem_0_bits_r_bits_id);
    mem_ptr.rlast   = &(top->axi4_mem_0_bits_r_bits_last);
    mem_ptr.rready  = &(top->axi4_mem_0_bits_r_ready);
    mem_ptr.rresp   = &(top->axi4_mem_0_bits_r_bits_resp);
    mem_ptr.rvalid  = &(top->axi4_mem_0_bits_r_valid);
}

bool trace_on = false;

void uart_input(uartlite &uart) {
    termios tmp;
    tcgetattr(STDIN_FILENO,&tmp);
    tmp.c_lflag &=(~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&tmp);
    while (1) {
        char c = getchar();
        if (c == 10) c = 13; // convert lf to cr
        uart.putc(c);
    }
}

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    VerilatedVcdC vcd;
    VChipTop *top = new VChipTop;
    top->trace(&vcd,0);
    vcd.open("trace.vcd");
    axi4_ptr <31,64,4> mmio_ptr;
    axi4_ptr <33,64,4> mem_ptr;

    connect_wire(mmio_ptr,mem_ptr,top);
    assert(mmio_ptr.check());
    assert(mem_ptr.check());
    
    axi4_ref <31,64,4> mmio_ref(mmio_ptr);
    axi4     <31,64,4> mmio_sigs;
    axi4_ref <31,64,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<31,64,4> mmio;

    mmio_mem           bootram(1024*1024,"../u-boot/u-boot.bin");
    uartlite           uart;
    std::thread        uart_input_thread(uart_input,std::ref(uart));
    assert(mmio.add_dev(0x60000000,1024*1024,&bootram));
    assert(mmio.add_dev(0x60100000,1024*1024,&uart));

    axi4_ref <33,64,4> mem_ref(mem_ptr);
    axi4     <33,64,4> mem_sigs;
    axi4_ref <33,64,4> mem_sigs_ref(mem_sigs);
    axi4_mem <33,64,4> mem(4096l*1024*1024);
    
    top->reset = 1;
    unsigned long ticks = 0;
    long max_trace_ticks = 1000;
    unsigned long uart_tx_bytes = 0;
    while (!Verilated::gotFinish() && max_trace_ticks > 0) {
        top->eval();
        ticks ++;
        if (trace_on) {
            vcd.dump(ticks);
            max_trace_ticks --;
        }
        if (ticks == 9) top->reset = 0;
        top->clock_clock = 1;
        // posedge
        mmio_sigs.update_input(mmio_ref);
        mem_sigs.update_input(mem_ref);
        top->eval();
        ticks ++;
        if (!top->reset) {
            mem.beat(mem_sigs_ref);
            mmio.beat(mmio_sigs_ref);
            while (uart.exist_tx()) {
                char c = uart.getc();
                printf("%c",c);
                fflush(stdout);
            }
        }
        mmio_sigs.update_output(mmio_ref);
        mem_sigs.update_output(mem_ref);
        top->ext_interrupts = uart.irq();
        if (trace_on) {
            vcd.dump(ticks);
            max_trace_ticks --;
        }
        top->clock_clock = 0;
    }
    if (trace_on) vcd.close();
    top->final();
    return 0;
}