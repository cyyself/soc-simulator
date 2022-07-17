#include "verilated.h"
#include "verilated_vcd_c.h"
#include "Vmycpu_top.h"

#include "axi4.hpp"
#include "axi4_mem.hpp"
#include "axi4_xbar.hpp"
#include "mmio_mem.hpp"
#include "uart8250.hpp"

#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <thread>

void connect_wire(axi4_ptr <32,32,4> &mmio_ptr, Vmycpu_top *top) {
    // connect
    // mmio
    // aw   
    mmio_ptr.awaddr     = &(top->awaddr);
    mmio_ptr.awburst    = &(top->awburst);
    mmio_ptr.awid       = &(top->awid);
    mmio_ptr.awlen      = &(top->awlen);
    mmio_ptr.awready    = &(top->awready);
    mmio_ptr.awsize     = &(top->awsize);
    mmio_ptr.awvalid    = &(top->awvalid);
    // w
    mmio_ptr.wdata      = &(top->wdata);
    mmio_ptr.wlast      = &(top->wlast);
    mmio_ptr.wready     = &(top->wready);
    mmio_ptr.wstrb      = &(top->wstrb);
    mmio_ptr.wvalid     = &(top->wvalid);
    // b
    mmio_ptr.bid        = &(top->bid);
    mmio_ptr.bready     = &(top->bready);
    mmio_ptr.bresp      = &(top->bresp);
    mmio_ptr.bvalid     = &(top->bvalid);
    // ar
    mmio_ptr.araddr     = &(top->araddr);
    mmio_ptr.arburst    = &(top->arburst);
    mmio_ptr.arid       = &(top->arid);
    mmio_ptr.arlen      = &(top->arlen);
    mmio_ptr.arready    = &(top->arready);
    mmio_ptr.arsize     = &(top->arsize);
    mmio_ptr.arvalid    = &(top->arvalid);
    // r
    mmio_ptr.rdata      = &(top->rdata);
    mmio_ptr.rid        = &(top->rid);
    mmio_ptr.rlast      = &(top->rlast);
    mmio_ptr.rready     = &(top->rready);
    mmio_ptr.rresp      = &(top->rresp);
    mmio_ptr.rvalid     = &(top->rvalid);
}

void uart_input(uart8250 &uart) {
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

bool running = true;

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);
    // setup trace
    bool trace_on = false;
    bool trace_pc = false;
    long sim_time = 1e3;
    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i],"-trace") == 0) {
            trace_on = true;
            if (i+1 < argc) {
                sscanf(argv[i+1],"%lu",&sim_time);
            }
        }
        else if (strcmp(argv[i],"-pc") == 0) {
            trace_pc = true;
        }
    }
    Verilated::traceEverOn(trace_on);
    // setup soc
    Vmycpu_top *top = new Vmycpu_top;
    axi4_ptr <32,32,4> mmio_ptr;

    connect_wire(mmio_ptr,top);
    assert(mmio_ptr.check());
    
    axi4_ref <32,32,4> mmio_ref(mmio_ptr);
    axi4     <32,32,4> mmio_sigs;
    axi4_ref <32,32,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<32,32,4> mmio;


    // uart8250 at 0x1fe40000 (APB)
    uart8250 uart;
    std::thread *uart_input_thread = new std::thread(uart_input,std::ref(uart));
    assert(mmio.add_dev(0x1fe40000,0x10000,&uart));

    // SPI Flash as memory at 0x1fc00000
    mmio_mem spi_flash(0x100000, "../supervisor-mips32/kernel/kernel.bin");
    assert(mmio.add_dev(0x1fc00000,0x100000,&spi_flash));

    // DRAM(128M) at 0x00000000
    mmio_mem dram(0x8000000);
    assert(mmio.add_dev(0x0,0x8000000,&dram));

    // connect Vcd for trace
    VerilatedVcdC vcd;
    if (trace_on) {
        top->trace(&vcd,0);
        vcd.open("trace.vcd");
    }

    // init and run
    top->aresetn = 0;
    unsigned long ticks = 0;
    while (!Verilated::gotFinish() && sim_time > 0 && running) {
        top->eval();
        ticks ++;
        if (trace_pc && top->debug_wb_rf_wen) printf("pc = %lx\n", top->debug_wb_pc);
        if (trace_on) {
            vcd.dump(ticks);
            sim_time --;
        }
        if (ticks == 9) top->aresetn = 1;
        top->aclk = 1;
        // posedge
        mmio_sigs.update_input(mmio_ref);
        top->eval();
        ticks ++;
        if (top->aresetn) {
            mmio.beat(mmio_sigs_ref);
            while (uart.exist_tx()) {
                char c = uart.getc();
                printf("%c",c);
                fflush(stdout);
            }
        }
        mmio_sigs.update_output(mmio_ref);
        if (trace_pc && top->debug_wb_rf_wen) printf("pc = %lx\n", top->debug_wb_pc);
        if (trace_on) {
            vcd.dump(ticks);
            sim_time --;
        }
        top->ext_int = uart.irq() << 2;
        top->aclk = 0;
    }
    if (trace_on) vcd.close();
    top->final();
    uart_input_thread->~thread();
    return 0;
}