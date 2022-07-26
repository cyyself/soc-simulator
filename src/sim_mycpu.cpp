#include "verilated.h"
#include "verilated_vcd_c.h"
#include "Vmycpu_top.h"

#include "axi4.hpp"
#include "axi4_mem.hpp"
#include "axi4_xbar.hpp"
#include "mmio_mem.hpp"
#include "uart8250.hpp"
#include "nscscc_confreg.hpp"
#include "memory_bus.hpp"
#include "mips_core.hpp"

#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <thread>
#include <csignal>
#include <sstream>

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

bool running = true;
bool trace_on = false;
bool trace_pc = false;
bool confreg_uart = false;
bool perf_stat = false;
long sim_time = 1e3;

void uart_input(uart8250 &uart) {
    termios tmp;
    tcgetattr(STDIN_FILENO,&tmp);
    tmp.c_lflag &=(~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&tmp);
    while (running) {
        char c = getchar();
        if (c == 10) c = 13; // convert lf to cr
        uart.putc(c);
    }
}

void system_test(Vmycpu_top *top, axi4_ref <32,32,4> &mmio_ref) {
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
    pthread_kill(uart_input_thread->native_handle(),SIGKILL);
}

void func_run(Vmycpu_top *top, axi4_ref <32,32,4> &mmio_ref) {
    axi4     <32,32,4> mmio_sigs;
    axi4_ref <32,32,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<32,32,4> mmio(23);

    // func mem at 0x1fc00000 and 0x0
    mmio_mem perf_mem(262144*4, "../nscscc-group/func_test_v0.01/soft/func/obj/main.bin");
    perf_mem.set_allow_warp(true);
    assert(mmio.add_dev(0x1fc00000,0x100000,&perf_mem));
    assert(mmio.add_dev(0x00000000,0x10000000,&perf_mem));
    assert(mmio.add_dev(0x20000000,0x20000000,&perf_mem));
    assert(mmio.add_dev(0x40000000,0x40000000,&perf_mem));
    assert(mmio.add_dev(0x80000000,0x80000000,&perf_mem));

    // confreg at 0x1faf0000
    nscscc_confreg confreg(true);
    confreg.set_trace_file("../nscscc-group/func_test_v0.01/cpu132_gettrace/golden_trace.txt");
    assert(mmio.add_dev(0x1faf0000,0x10000,&confreg));

    // connect Vcd for trace
    VerilatedVcdC vcd;
    if (trace_on) {
        top->trace(&vcd,0);
        vcd.open("trace.vcd");
    }

    // init and run
    top->aresetn = 0;
    unsigned long ticks = 0;
    unsigned long rst_ticks = 1000;
    int test_point = 0;
    while (!Verilated::gotFinish() && sim_time > 0 && running) {
        if (rst_ticks  > 0) {
            top->aresetn = 0;
            rst_ticks --;
        }
        else top->aresetn = 1;
        top->aclk = !top->aclk;
        if (top->aclk && top->aresetn) mmio_sigs.update_input(mmio_ref);
        top->eval();
        if (top->aclk && top->aresetn) {
            confreg.tick();
            mmio.beat(mmio_sigs_ref);
            mmio_sigs.update_output(mmio_ref);
            while (confreg_uart && confreg.has_uart()) printf("%c",confreg.get_uart());
            if (confreg.get_num() != test_point) {
                test_point = confreg.get_num();
                printf("Number %d Functional Test Point PASS!\n", test_point>>24);
            }
        }
        running = confreg.do_trace(top->debug_wb_pc,top->debug_wb_rf_wen,top->debug_wb_rf_wnum,top->debug_wb_rf_wdata);
        if (top->debug_wb_pc == 0xbfc00100u) running = false;
        if (trace_pc && top->debug_wb_rf_wen) printf("pc = %lx\n", top->debug_wb_pc);
        if (trace_on) {
            vcd.dump(ticks);
            sim_time --;
        }
        ticks ++;
    }
    if (trace_on) vcd.close();
    top->final();
}

void perf_run(Vmycpu_top *top, axi4_ref <32,32,4> &mmio_ref, int test_start = 1, int test_end = 10) {
    axi4     <32,32,4> mmio_sigs;
    axi4_ref <32,32,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<32,32,4> mmio(23);

    // perf mem at 0x1fc00000
    mmio_mem perf_mem(262144*4, "../nscscc-group/perf_test_v0.01/soft/perf_func/obj/allbench/inst_data.bin");
    assert(mmio.add_dev(0x1fc00000,0x100000,&perf_mem));

    // confreg at 0x1faf0000
    nscscc_confreg confreg;
    assert(mmio.add_dev(0x1faf0000,0x10000,&confreg));
    
    // connect Vcd for trace
    VerilatedVcdC vcd;
    if (trace_on) {
        top->trace(&vcd,0);
    }
    unsigned long ticks = 0;
    uint32_t last_wb_pc[2] = {0,0};
    uint32_t cur_wb_pc[2] = {0,0};
    // init and run
    for (int test=test_start;test<=test_end;test++) {
        running = true;
        confreg.set_switch(test);
        top->aresetn = 0;
        std::stringstream ss;
        ss << "trace-perf-" << test << ".vcd";
        if (trace_on) vcd.open(ss.str().c_str());
        unsigned long rst_ticks = 1000;
        // dual-issue-statistic {
        unsigned long total_clk = 0;
        unsigned long stall_clk = 0;
        unsigned long dual_commit = 0;
        unsigned long has_commit = 0;
        // dual-issue-statistic }
        while (!Verilated::gotFinish() && sim_time > 0 && running) {
            if (rst_ticks  > 0) {
                top->aresetn = 0;
                rst_ticks --;
            }
            else top->aresetn = 1;
            top->aclk = !top->aclk;
            if (top->aclk && top->aresetn) mmio_sigs.update_input(mmio_ref);
            top->eval();
            if (top->aclk && top->aresetn) {
                confreg.tick();
                mmio.beat(mmio_sigs_ref);
                mmio_sigs.update_output(mmio_ref);
                while (confreg_uart && confreg.has_uart()) printf("%c",confreg.get_uart());
            }
            if (top->debug_wb_pc == 0xbfc00100u) running = false;
            if (trace_pc && top->debug_wb_rf_wen) printf("pc = %lx\n", top->debug_wb_pc);
            if (trace_on) {
                vcd.dump(ticks);
                sim_time --;
            }
            // dual-issue-statistic {
            cur_wb_pc[top->aclk] = top->debug_wb_pc;
            if (!top->aclk) {
                total_clk ++;
                bool is_stall = false;
                if (last_wb_pc[0] == cur_wb_pc[0] && last_wb_pc[1] == cur_wb_pc[1]) {
                    // pipeline stall
                    is_stall = true;
                }
                if (is_stall) stall_clk ++;
                else {
                    if (cur_wb_pc[1]) { // master
                        has_commit ++;
                        if (cur_wb_pc[0]) dual_commit ++;
                    }
                }
                last_wb_pc[0] = cur_wb_pc[0];
                last_wb_pc[1] = cur_wb_pc[1];
            }
            // dual-issue-statistic }
            ticks ++;
        }
        if (trace_on) vcd.close();
        printf("%x\n",confreg.get_num());
        if (perf_stat) printf("total_clk = %lu, stall_clk = %lu, has_commit = %lu, dual_commit = %lu, dual_issue_rate = %0.5lf, IPC = %0.5lf\n", total_clk, stall_clk, has_commit, dual_commit, (double)dual_commit / has_commit, (double)(has_commit + dual_commit) / total_clk);
    }
    top->final();
    printf("total ticks = %lu\n", ticks);
}

void cemu_func() {
    memory_bus cemu_mmio;
    
    mmio_mem cemu_func_mem(262144*4, "../nscscc-group/func_test_v0.01/soft/func/obj/main.bin");
    cemu_func_mem.set_allow_warp(true);
    assert(cemu_mmio.add_dev(0x1fc00000,0x100000  ,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x00000000,0x10000000,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x20000000,0x20000000,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x40000000,0x40000000,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x80000000,0x80000000,&cemu_func_mem));

    nscscc_confreg cemu_confreg(true);
    cemu_confreg.set_trace_file("../nscscc-group/func_test_v0.01/cpu132_gettrace/golden_trace.txt");
    assert(cemu_mmio.add_dev(0x1faf0000,0x10000,&cemu_confreg));

    mips_core cemu_mips(cemu_mmio);

    int test_point = 0;
    bool running = true;
    while (running) {
        cemu_mips.step();
        cemu_confreg.tick();
        running = cemu_confreg.do_trace(cemu_mips.debug_wb_pc, cemu_mips.debug_wb_wen, cemu_mips.debug_wb_wnum, cemu_mips.debug_wb_wdata);
        while (cemu_confreg.has_uart()) printf("%c", cemu_confreg.get_uart());
        if (cemu_confreg.get_num() != test_point) {
            test_point = cemu_confreg.get_num();
            printf("Number %d Functional Test Point PASS!\n", test_point>>24);
            if ( (test_point >> 24) == 89) return;
        }
    }
}

void cemu_perf_diff(Vmycpu_top *top, axi4_ref <32,32,4> &mmio_ref, int test_start = 1, int test_end = 10) {
    // cemu {
    memory_bus cemu_mmio;
    
    mmio_mem cemu_func_mem(262144*4, "../nscscc-group/perf_test_v0.01/soft/perf_func/obj/allbench/inst_data.bin");
    cemu_func_mem.set_allow_warp(true);
    assert(cemu_mmio.add_dev(0x1fc00000,0x100000  ,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x00000000,0x10000000,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x20000000,0x20000000,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x40000000,0x40000000,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x80000000,0x80000000,&cemu_func_mem));

    nscscc_confreg cemu_confreg(true);
    assert(cemu_mmio.add_dev(0x1faf0000,0x10000,&cemu_confreg));

    mips_core cemu_mips(cemu_mmio);
    // cemu }

    // rtl soc-simulator {
    axi4     <32,32,4> mmio_sigs;
    axi4_ref <32,32,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<32,32,4> mmio(23);

    // perf mem at 0x1fc00000
    mmio_mem perf_mem(262144*4, "../nscscc-group/perf_test_v0.01/soft/perf_func/obj/allbench/inst_data.bin");
    assert(mmio.add_dev(0x1fc00000,0x100000,&perf_mem));

    // confreg at 0x1faf0000
    nscscc_confreg confreg(true);
    assert(mmio.add_dev(0x1faf0000,0x10000,&confreg));
    
    // connect Vcd for trace
    VerilatedVcdC vcd;
    if (trace_on) {
        top->trace(&vcd,0);
    }
    unsigned long ticks = 0;
    // rtl soc-simulator }

    for (int test=test_start;test<=test_end;test++) {
        running = true;
        confreg.set_switch(test);
        cemu_confreg.set_switch(test);
        top->aresetn = 0;
        std::stringstream ss;
        ss << "trace-perf-" << test << ".vcd";
        if (trace_on) vcd.open(ss.str().c_str());
        unsigned long rst_ticks = 1000;
        cemu_mips.reset();
        while (!Verilated::gotFinish() && sim_time > 0 && running) {
            if (rst_ticks  > 0) {
                top->aresetn = 0;
                rst_ticks --;
            }
            else top->aresetn = 1;
            top->aclk = !top->aclk;
            if (top->aclk && top->aresetn) mmio_sigs.update_input(mmio_ref);
            top->eval();
            if (top->aclk && top->aresetn) {
                confreg.tick();
                mmio.beat(mmio_sigs_ref);
                mmio_sigs.update_output(mmio_ref);
                while (confreg_uart && confreg.has_uart()) printf("%c",confreg.get_uart());
            }
            if (top->debug_wb_pc == 0xbfc00100u) running = false;
            if (trace_pc && top->debug_wb_rf_wen) printf("pc = %lx\n", top->debug_wb_pc);
            if (trace_on) {
                vcd.dump(ticks);
                sim_time --;
            }
            // trace with cemu {
            if (top->debug_wb_rf_wen && top->debug_wb_rf_wnum) {
                do {
                    cemu_mips.step();
                    cemu_confreg.tick();
                } while(!(cemu_mips.debug_wb_wen && cemu_mips.debug_wb_wnum));
                if ( cemu_mips.debug_wb_pc    != top->debug_wb_pc      || 
                     cemu_mips.debug_wb_wnum  != top->debug_wb_rf_wnum ||
                    (cemu_mips.debug_wb_wdata != top->debug_wb_rf_wdata && !cemu_mips.debug_wb_is_timer)
                ) {
                    printf("Error!\n");
                    printf("reference: PC = 0x%08x, wb_rf_wnum = 0x%02x, wb_rf_wdata = 0x%08x\n", cemu_mips.debug_wb_pc, cemu_mips.debug_wb_wnum, cemu_mips.debug_wb_wdata);
                    printf("mycpu    : PC = 0x%08x, wb_rf_wnum = 0x%02x, wb_rf_wdata = 0x%08x\n", top->debug_wb_pc, top->debug_wb_rf_wnum, top->debug_wb_rf_wdata);
                    exit(0);
                }
                else {
                    if (cemu_mips.debug_wb_is_timer) {
                        cemu_mips.set_GPR(cemu_mips.debug_wb_wnum, top->debug_wb_rf_wdata);
                    }
                    // printf("OK!\n");
                    // printf("reference: PC = 0x%08x, wb_rf_wnum = 0x%02x, wb_rf_wdata = 0x%08x\n", cemu_mips.debug_wb_pc, cemu_mips.debug_wb_wnum, cemu_mips.debug_wb_wdata);
                    // printf("mycpu    : PC = 0x%08x, wb_rf_wnum = 0x%02x, wb_rf_wdata = 0x%08x\n", top->debug_wb_pc, top->debug_wb_rf_wnum, top->debug_wb_rf_wdata);
                }
            }
            // trace with cemu }
            ticks ++;
        }
        if (trace_on) vcd.close();
        printf("%x\n",confreg.get_num());
    }
    top->final();
    printf("total ticks = %lu\n", ticks);
}

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);

    std::signal(SIGINT, [](int) {
        running = false;
    });

    enum {NOP, FUNC, PERF, SYS_TEST, RUN_OS, CEMU_FUNC, CEMU_PERF_DIFF} run_mode = NOP;

    int perf_start = 1;
    int perf_end = 10;

    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i],"-trace") == 0) {
            trace_on = true;
            if (i+1 < argc) {
                sscanf(argv[++i],"%lu",&sim_time);
            }
        }
        else if (strcmp(argv[i],"-pc") == 0) {
            trace_pc = true;
        }
        else if (strcmp(argv[i],"-sys") == 0) {
            run_mode = SYS_TEST;
        }
        else if (strcmp(argv[i],"-func") == 0) {
            run_mode = FUNC;
        }
        else if (strcmp(argv[i],"-perf") == 0) {
            run_mode = PERF;
        }
        else if (strcmp(argv[i],"-uart") == 0) {
            confreg_uart = true;
        }
        else if (strcmp(argv[i],"-prog") == 0) {
            if (i+1 < argc) {
                sscanf(argv[++i],"%lu",&perf_start);
                perf_end = perf_start;
                printf("set performance test program to %d\n",perf_start);
            }
        }
        else if (strcmp(argv[i],"-stat") == 0) {
            perf_stat = true;
        }
        else if (strcmp(argv[i],"-cemufunc") == 0) {
            run_mode = CEMU_FUNC;
        }
        else if (strcmp(argv[i],"-perfdiff") == 0) {
            run_mode = CEMU_PERF_DIFF;
        }
    }
    Verilated::traceEverOn(trace_on);
    // setup soc
    Vmycpu_top *top = new Vmycpu_top;
    axi4_ptr <32,32,4> mmio_ptr;

    connect_wire(mmio_ptr,top);
    assert(mmio_ptr.check());
    
    axi4_ref <32,32,4> mmio_ref(mmio_ptr);

    switch (run_mode) {
        case SYS_TEST:
            system_test(top, mmio_ref);
            break;
        case FUNC:
            func_run(top, mmio_ref);
            break;
        case PERF:
            if (trace_on && perf_start != perf_end) {
                printf("Warning: You should better set perf program.\n");
            }
            perf_run(top, mmio_ref, perf_start, perf_end);
            break;
        case CEMU_FUNC:
            cemu_func();
            break;
        case CEMU_PERF_DIFF:
            cemu_perf_diff(top, mmio_ref, perf_start, perf_end);
            break;
        default:
            printf("Unknown running mode.\n");
            exit(-ENOENT);
    }
    return 0;
}