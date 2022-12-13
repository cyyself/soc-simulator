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

bool running = true;    // 运行
bool trace_on = false;  // 输出波形图
bool confreg_uart = false;// 开启confreg串口输出
long sim_time = 1e3;    // 开启trace时最大仿真时间
bool diff_uart = false; // 差分测试UART输出，检测MMIO访问是否正确
bool axi_fast = false;  // 关闭性能测试时AXI延迟，用于加速Debug，不可用于跑分
bool perf_once = false; // 性能测试只运行一次

unsigned int *pc;

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

void func_run(Vmycpu_top *top, axi4_ref <32,32,4> &mmio_ref) {
    axi4     <32,32,4> mmio_sigs;
    axi4_ref <32,32,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<32,32,4> mmio(23);

    // func mem at 0x1fc00000 and 0x0
    mmio_mem perf_mem(262144*4, "../../vivado/func_test_v0.01/soft/func/obj/main.bin");
    perf_mem.set_allow_warp(true);
    assert(mmio.add_dev(0x1fc00000,0x100000,&perf_mem));
    assert(mmio.add_dev(0x00000000,0x10000000,&perf_mem));
    assert(mmio.add_dev(0x20000000,0x20000000,&perf_mem));
    assert(mmio.add_dev(0x40000000,0x40000000,&perf_mem));
    assert(mmio.add_dev(0x80000000,0x80000000,&perf_mem));

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
    top->aresetn = 0;
    uint64_t ticks = 0;
    uint64_t rst_ticks = 1000;
    uint64_t last_commit = 0;
    uint64_t commit_timeout = 5000;
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
            top->eval();
            while (confreg_uart && confreg.has_uart()) printf("%c",confreg.get_uart());
            if (confreg.get_num() != test_point) {
                test_point = confreg.get_num();
                printf("Number %d Functional Test Point PASS!\n", test_point>>24);
            }
        }
        if (top->aclk) running = confreg.do_trace(top->debug_wb_pc,top->debug_wb_rf_wen,top->debug_wb_rf_wnum,top->debug_wb_rf_wdata);
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

void perf_run(Vmycpu_top *top, axi4_ref <32,32,4> &mmio_ref, int test_start = 1, int test_end = 10) {
    axi4     <32,32,4> mmio_sigs;
    axi4_ref <32,32,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<32,32,4> mmio(axi_fast ? 0 : 23);

    // perf mem at 0x1fc00000
    mmio_mem perf_mem(262144*4, "../../vivado/perf_test_v0.01/soft/perf_func/obj/allbench/inst_data.bin");
    assert(mmio.add_dev(0x1fc00000,0x100000,&perf_mem));

    // confreg at 0x1faf0000
    nscscc_confreg confreg(perf_once);
    assert(mmio.add_dev(0x1faf0000,0x10000,&confreg));
    
    // connect Vcd for trace
    VerilatedVcdC vcd;
    if (trace_on) {
        top->trace(&vcd,0);
    }
    // init and run
    uint64_t ticks = 0;

    for (int test=test_start;test<=test_end && running;test++) {
        bool test_end = false;
        confreg.set_switch(test);
        top->aresetn = 0;
        std::stringstream ss;
        ss << "trace-perf-" << test << ".vcd";
        if (trace_on) vcd.open(ss.str().c_str());
        uint64_t rst_ticks = 1000;
        uint64_t last_commit = ticks;
        uint64_t commit_timeout = 5000;
        while (!Verilated::gotFinish() && sim_time > 0 && running && !test_end) {
            if (rst_ticks  > 0) {
                top->aresetn = 0;
                rst_ticks --;
                mmio.reset();
            }
            else top->aresetn = 1;
            top->aclk = !top->aclk;
            if (top->aclk && top->aresetn) mmio_sigs.update_input(mmio_ref);
            top->eval();
            if (top->aclk && top->aresetn) {
                confreg.tick();
                mmio.beat(mmio_sigs_ref);
                mmio_sigs.update_output(mmio_ref);
                top->eval();
                while (confreg_uart && confreg.has_uart()) printf("%c",confreg.get_uart());
            }
            if (top->aresetn && top->debug_wb_pc == 0xbfc00100u) test_end = true;
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
        printf("%x\n",confreg.get_num());
    }
    top->final();
    printf("total ticks = %lu\n", ticks);
}

void cemu_perf_diff(Vmycpu_top *top, axi4_ref <32,32,4> &mmio_ref, int test_start = 1, int test_end = 10) {
    // cemu {
    memory_bus cemu_mmio;
    
    mmio_mem cemu_func_mem(262144*4, "../../vivado/perf_test_v0.01/soft/perf_func/obj/allbench/inst_data.bin");
    cemu_func_mem.set_allow_warp(true);
    assert(cemu_mmio.add_dev(0x1fc00000,0x100000  ,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x00000000,0x10000000,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x20000000,0x20000000,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x40000000,0x40000000,&cemu_func_mem));
    assert(cemu_mmio.add_dev(0x80000000,0x80000000,&cemu_func_mem));

    nscscc_confreg cemu_confreg(perf_once);
    assert(cemu_mmio.add_dev(0x1faf0000,0x10000,&cemu_confreg));

    mips_core cemu_mips(cemu_mmio);
    // cemu }

    // rtl soc-simulator {
    axi4     <32,32,4> mmio_sigs;
    axi4_ref <32,32,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<32,32,4> mmio(axi_fast ? 0 : 23);

    // perf mem at 0x1fc00000
    mmio_mem perf_mem(262144*4, "../../vivado/perf_test_v0.01/soft/perf_func/obj/allbench/inst_data.bin");
    assert(mmio.add_dev(0x1fc00000,0x100000,&perf_mem));

    // confreg at 0x1faf0000
    nscscc_confreg confreg(perf_once);
    assert(mmio.add_dev(0x1faf0000,0x10000,&confreg));
    
    // connect Vcd for trace
    VerilatedVcdC vcd;
    if (trace_on) {
        top->trace(&vcd,0);
    }
    uint64_t ticks = 0;
    // rtl soc-simulator }

    for (int test=test_start;test<=test_end && running;test++) {
        bool test_end = false;
        confreg.set_switch(test);
        cemu_confreg.set_switch(test);
        top->aresetn = 0;
        std::stringstream ss;
        ss << "trace-perf-" << test << ".vcd";
        if (trace_on) vcd.open(ss.str().c_str());
        uint64_t rst_ticks = 1000;
        uint64_t last_commit = ticks;
        uint64_t commit_timeout = 5000;
        cemu_mips.reset();
        while (!Verilated::gotFinish() && sim_time > 0 && running && !test_end) {
            if (rst_ticks  > 0) {
                top->aresetn = 0;
                rst_ticks --;
                mmio.reset();
            }
            else top->aresetn = 1;
            top->aclk = !top->aclk;
            if (top->aclk && top->aresetn) mmio_sigs.update_input(mmio_ref);
            top->eval();
            if (top->aclk && top->aresetn) {
                confreg.tick();
                mmio.beat(mmio_sigs_ref);
                mmio_sigs.update_output(mmio_ref);
                top->eval();
                while (confreg_uart && confreg.has_uart()) printf("%c",confreg.get_uart());
            }
            if (top->aresetn && top->debug_wb_pc == 0xbfc00100u) test_end = true;
            if (trace_on) {
                vcd.dump(ticks);
                sim_time --;
            }
            // trace with cemu {
            if (top->aclk && top->debug_wb_rf_wen && top->debug_wb_rf_wnum) {
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
                    running = false;
                }
                else {
                    if (cemu_mips.debug_wb_is_timer) {
                        cemu_mips.set_GPR(cemu_mips.debug_wb_wnum, top->debug_wb_rf_wdata);
                    }
                }
                last_commit = ticks;
            }
            if (ticks - last_commit >= commit_timeout) {
                printf("ERROR: There are %lu cycles since last commit\n", commit_timeout);
                running = false;
            }
            while (diff_uart && confreg.has_uart() && cemu_confreg.has_uart()) {
                char mycpu_uart = confreg.get_uart();
                char ref_uart = cemu_confreg.get_uart();
                if (mycpu_uart != ref_uart) {
                    printf("ERROR!\n UART different at %lu ticks.\n", ticks);
                    printf("Expected: %08x, found: %08x\n", mycpu_uart, ref_uart);
                    running = false;
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

    enum {NOP, FUNC, PERF, CEMU_PERF_DIFF} run_mode = NOP;

    int perf_start = 1;
    int perf_end = 10;

    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i],"-trace") == 0) {
            trace_on = true;
            if (i+1 < argc) {
                sscanf(argv[++i],"%lu",&sim_time);
            }
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
        else if (strcmp(argv[i],"-perfdiff") == 0) {
            run_mode = CEMU_PERF_DIFF;
        }
        else if (strcmp(argv[i],"-diffuart") == 0) {
            diff_uart = true;
        }
        else if (strcmp(argv[i],"-axifast") == 0) {
            axi_fast = true;
        }
        else if (strcmp(argv[i],"-perfonce") == 0) {
            perf_once = true;
        }
    }

    Verilated::traceEverOn(trace_on);
    // setup soc
    Vmycpu_top *top = new Vmycpu_top;
    pc = &(top->debug_wb_pc);
    axi4_ptr <32,32,4> mmio_ptr;

    connect_wire(mmio_ptr,top);
    assert(mmio_ptr.check());
    
    axi4_ref <32,32,4> mmio_ref(mmio_ptr);

    switch (run_mode) {
        case FUNC:
            func_run(top, mmio_ref);
            break;
        case PERF:
            if (trace_on && perf_start != perf_end) {
                printf("Warning: You should better set perf program.\n");
            }
            perf_run(top, mmio_ref, perf_start, perf_end);
            break;
        case CEMU_PERF_DIFF:
            if (trace_on && perf_start != perf_end) {
                printf("Warning: You should better set perf program.\n");
            }
            if (diff_uart) {
                printf("diff uart is on\n");
            }
            cemu_perf_diff(top, mmio_ref, perf_start, perf_end);
            break;
        default:
            printf("Unknown running mode. Please read readme.md!\n");
            exit(-ENOENT);
    }
    return 0;
}
