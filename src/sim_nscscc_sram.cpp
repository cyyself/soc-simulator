#include "verilated.h"
#include "verilated_vcd_c.h"
#include "Vmycpu_top.h"

#include "nscscc_sram.hpp"
#include "nscscc_sram_slave.hpp"
#include "nscscc_sram_xbar.hpp"
#include "nscscc_confreg.hpp"

#include "mmio_mem.hpp"

#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <thread>
#include <csignal>
#include <sstream>


bool running = true;
bool trace_on = true;
long sim_time = 1e7;

void connect_wire(nscscc_sram_ptr &sram_ptr, Vmycpu_top *top) {
    sram_ptr.inst_sram_en       = &(top->inst_sram_en);
    sram_ptr.inst_sram_addr     = &(top->inst_sram_addr);
    sram_ptr.inst_sram_wen      = &(top->inst_sram_wen);
    sram_ptr.inst_sram_rdata    = &(top->inst_sram_rdata);
    sram_ptr.inst_sram_wdata    = &(top->inst_sram_wdata);
    sram_ptr.data_sram_en       = &(top->data_sram_en);
    sram_ptr.data_sram_addr     = &(top->data_sram_addr);
    sram_ptr.data_sram_wen      = &(top->data_sram_wen);
    sram_ptr.data_sram_rdata    = &(top->data_sram_rdata);
    sram_ptr.data_sram_wdata    = &(top->data_sram_wdata);
}

void func_test(Vmycpu_top *top, nscscc_sram_ref &mmio_ref) {
    nscscc_sram         mmio_sigs;
    nscscc_sram_ref     mmio_sigs_ref(mmio_sigs);
    nscscc_sram_xbar    mmio;
    mmio.set_address_mask(0x1fffffff); // for some CPU have no MMU.

    // func mem at 0x1fc00000 and 0x0
    mmio_mem func_mem(262144*4, "../../vivado/func_test_v0.01/soft/func/obj/main.bin");
    func_mem.set_allow_warp(true);
    assert(mmio.add_dev(0x1fc00000,0x100000,&func_mem));
    assert(mmio.add_dev(0x00000000,0x10000000,&func_mem));

    // confreg at 0x1faf0000
    nscscc_confreg confreg(true);
    confreg.set_trace_file("../../vivado/func_test_v0.01/cpu132_gettrace/golden_trace.txt");
    assert(mmio.add_dev(0x1faf0000,0x10000,&confreg));

    // connect Vcd for trace
    VerilatedVcdC vcd;
    if (trace_on) {
        top->trace(&vcd,0);
        vcd.open("trace.vcd");
    }

    // init and run
    top->resetn = 0;
    uint64_t ticks = 0;
    uint64_t rst_ticks = 1000;
    uint64_t last_commit = 0;
    uint64_t commit_timeout = 5000;
    int test_point = 0;
    while (!Verilated::gotFinish() && sim_time > 0 && running) {
        if (rst_ticks  > 0) {
            top->resetn = 0;
            rst_ticks --;
        }
        else top->resetn = 1;
        top->clk = !top->clk;
        if (top->clk && top->resetn) mmio_sigs.update_input(mmio_ref);
        top->eval();
        if (top->clk && top->resetn) {
            confreg.tick();
            mmio.beat(mmio_sigs_ref);
            mmio_sigs.update_output(mmio_ref);
            top->eval();
            if (confreg.get_num() != test_point) {
                test_point = confreg.get_num();
                printf("Number %d Functional Test Point PASS!\n", test_point>>24);
            }
            running = confreg.do_trace(top->debug_wb_pc,top->debug_wb_rf_wen,top->debug_wb_rf_wnum,top->debug_wb_rf_wdata);
        }
        if (top->debug_wb_pc == 0xbfc00100u) running = false;
        if (trace_on) {
            vcd.dump(ticks);
            sim_time --;
        }
        if (ticks - last_commit >= commit_timeout) {
            printf("ERROR: There are %lu cycles since last commit\n", commit_timeout);
            running = false;
        }
        else last_commit = ticks;
        ticks ++;
    }
    if (trace_on) vcd.close();
    top->final();
}

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    std::signal(SIGINT, [](int) {
        running = false;
    });

    // setup soc
    Vmycpu_top *top = new Vmycpu_top;
    nscscc_sram_ptr sram_ptr;
    connect_wire(sram_ptr, top);
    nscscc_sram_ref sram_ref(sram_ptr);
    func_test(top, sram_ref);

    return 0;
}