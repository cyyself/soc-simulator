#include "verilated.h"
#include "verilated_vcd_c.h"
#include "svdpi.h"
// #include "Vmycpu_top__Dpi.h"
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
#include <atomic>

// CONFIG {
bool both_edge_commit = true; // hack for superscalar CPU, say false if you CPU does not support it.
#define HAS_DEBUG_EXT
#define OPEN_BOTH_EDGE_STATIC
// CONFIG }

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

// run time config {
enum {NOP, FUNC, PERF, SYS_TEST, UCORE, LINUX, UBOOT} run_mode = NOP;
uint64_t commit_timeout= 5000; // stop simulation if no instruction commit to GPR after xxxx ticks (cycle*2)
bool enable_axi_latency= true;  // ! -axifast
bool nodifftest = false;        // -nodiff
long sim_time = 1e3;            // -trace [time]
long trace_start_time = 0;      // -starttrace [time]
bool trace_pc = false;          // -pc
bool diff_memory_write = false; // -diffmem
// only available for perf {
bool confreg_simu_flag = false; // -perfonce
bool out_confreg_uart  = false; // -uart
bool diff_confreg_uart = false; // -diffuart
bool perf_stat = false;         // -stat
// -prog in main function
// only available for perf }
bool running = true;
std::atomic_bool trace_on;
// run time config }

unsigned int *pc;
VerilatedVcdC vcd;

void open_trace() {
    vcd.open("trace.vcd");
    trace_on.store(true, std::memory_order_seq_cst);
}

void uart_input(uart8250 &uart) {
    termios tmp;
    tcgetattr(STDIN_FILENO,&tmp);
    tmp.c_lflag &=(~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&tmp);
    while (running) {
        char c = getchar();
        if (c == 9) printf("pc = %x\n",*pc); // ctrl+i
        else if (c == 20) open_trace(); // ctrl+t
        else if (c == 10) c = 13; // convert lf to cr
        uart.putc(c);
    }
}

void func_run(Vmycpu_top *top, axi4_ref <32,32,4> &mmio_ref) {
    axi4     <32,32,4> mmio_sigs;
    axi4_ref <32,32,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<32,32,4> mmio(enable_axi_latency ? 23 : 0); // AXI latency

    // func mem at 0x1fc00000 and 0x0
    mmio_mem func_mem(262144*4, "../nscscc-group/func_test_v0.01/soft/func/obj/main.bin");
    func_mem.set_allow_warp(true);
    assert(mmio.add_dev(0x1fc00000, 0x100000  , &func_mem));
    assert(mmio.add_dev(0x00000000, 0x10000000, &func_mem));
    assert(mmio.add_dev(0x20000000, 0x20000000, &func_mem));
    assert(mmio.add_dev(0x40000000, 0x40000000, &func_mem));
    assert(mmio.add_dev(0x80000000, 0x80000000, &func_mem));

    // confreg at 0x1faf0000
    nscscc_confreg confreg(true);
    confreg.set_trace_file("../nscscc-group/func_test_v0.01/cpu132_gettrace/golden_trace.txt");
    assert(mmio.add_dev(0x1faf0000,0x10000,&confreg));

    // connect Vcd for trace
    top->trace(&vcd,0);

    // reset rtl for 100 ticks
    top->aresetn = 0;
    for (int i=0;i<100;i++) {
        top->aclk = !top->aclk;
        top->eval();
    }
    top->aresetn = 1;

    // start simulation
    uint64_t ticks = 0;
    uint64_t last_commit = 0;
    int test_point = 0;
    while (!Verilated::gotFinish() && sim_time > 0 && running) {
        // basic simulation {
        ticks ++;
        top->aclk = !top->aclk;
        // fetch axi signals from rtl at negedge
        if (top->aclk) mmio_sigs.update_input(mmio_ref);
        top->eval();
        // process axi signals at posedge
        if (top->aclk) {
            confreg.tick();
            mmio.beat(mmio_sigs_ref);
            mmio_sigs.update_output(mmio_ref);
            if (confreg.get_num() != test_point) {
                test_point = confreg.get_num();
                printf("Number %d Functional Test Point PASS!\n", test_point>>24);
            }
        }
        if (trace_on) { // print wave if trace_on
            top->eval(); // update rtl status changed by axi, this will only affect waveform
            vcd.dump(ticks);
            sim_time --;
        }
        // conditinal trace
        if (trace_start_time != 0 && ticks == trace_start_time) {
            open_trace();
        }
        // custom trace add to here
        // basic simulation }
        if (both_edge_commit || top->aclk)  {
            // deadlock detection {
            if (top->debug_wb_rf_wen) last_commit = ticks;
            if (ticks - last_commit >= commit_timeout) {
                printf("ERROR: There are %lu ticks since last commit\n", commit_timeout);
                running = false;
            }
            // deadlock detection }
            // trace pc {
            if (trace_pc && top->debug_wb_rf_wen) printf("pc = %lx\n", top->debug_wb_pc);
            // trace pc }
            // compare func trace
            if (!nodifftest) running = confreg.do_trace(top->debug_wb_pc,top->debug_wb_rf_wen,top->debug_wb_rf_wnum,top->debug_wb_rf_wdata);
            if (top->debug_wb_pc == 0xbfc00100u) {
                if ( (test_point >> 24) != 89) printf("Error! Hit function test end PC!\n");
                running = false;
            }
        }
        
    }
    if (trace_on) vcd.close();
    top->final();
}

void perf_run(Vmycpu_top *top, axi4_ref <32,32,4> &rtl_mmio_ref, int test_start = 1, int test_end = 10) {
    memory_bus cemu_mmio;

    axi4     <32,32,4> rtl_mmio_sigs;
    axi4_ref <32,32,4> rtl_mmio_sigs_ref(rtl_mmio_sigs);
    axi4_xbar<32,32,4> rtl_mmio(enable_axi_latency ? 23 : 0); // AXI latency

    mmio_mem cemu_perf_mem(262144*4, "../nscscc-group/perf_test_v0.01/soft/perf_func/obj/allbench/inst_data.bin");
    mmio_mem  rtl_perf_mem(262144*4, "../nscscc-group/perf_test_v0.01/soft/perf_func/obj/allbench/inst_data.bin");
    cemu_perf_mem.set_allow_warp(true);
     rtl_perf_mem.set_allow_warp(true);
    assert(cemu_mmio.add_dev(0x1fc00000, 0x100000, &cemu_perf_mem));
    assert( rtl_mmio.add_dev(0x1fc00000, 0x100000, & rtl_perf_mem));

    nscscc_confreg cemu_confreg(confreg_simu_flag);
    nscscc_confreg  rtl_confreg(confreg_simu_flag);
    assert(cemu_mmio.add_dev(0x1faf0000,0x10000,&cemu_confreg));
    assert( rtl_mmio.add_dev(0x1faf0000,0x10000,& rtl_confreg));

    // setup diff memory
    if (diff_memory_write) rtl_perf_mem.set_diff_mem(cemu_perf_mem.get_mem_ptr());

    // connect Vcd for trace
    top->trace(&vcd,0);
    if (trace_on) open_trace();

    // init cemu_mips
    mips_core cemu_mips(cemu_mmio);

    // start simulation
    uint64_t last_commit = 0;
    uint64_t ticks = 0;

    
    for (int test=test_start;test<=test_end && running;test++) {
        cemu_confreg.reset();
         rtl_confreg.reset();
        cemu_confreg.set_switch(test);
         rtl_confreg.set_switch(test);
        
        // reset rtl for 100 ticks
        top->aresetn = 0;
        for (int i=0;i<100;i++) {
            top->aclk = !top->aclk;
            top->eval();
        }
        top->aresetn = 1;
        // reset cemu
        cemu_mips.reset();

        // setup statistic
        uint32_t last_wb_pc[2] = {0,0};
        uint32_t cur_wb_pc[2] = {0,0};
        uint64_t total_clk = 0;
        uint64_t stall_clk = 0;
        uint64_t dual_commit = 0;
        uint64_t has_commit = 0;

        bool cur_running = true;
        
        while (!Verilated::gotFinish() && sim_time > 0 && running && cur_running) {
            // basic simulation {
            ticks ++;
            top->aclk = !top->aclk;
            // fetch axi signals from rtl at negedge
            if (top->aclk) rtl_mmio_sigs.update_input(rtl_mmio_ref);
            top->eval();
            // process axi signals at posedge
            if (top->aclk) {
                rtl_confreg.tick();
                rtl_mmio.beat(rtl_mmio_sigs_ref);
                rtl_mmio_sigs.update_output(rtl_mmio_ref);
                while (out_confreg_uart && rtl_confreg.has_uart()) {
                    printf("%c", rtl_confreg.get_uart());
                    fflush(stdout);
                }
            }
            if (trace_on) { // print wave if trace_on
                top->eval(); // update rtl status changed by axi, this will only affect waveform
                vcd.dump(ticks);
                sim_time --;
            }
            // conditinal trace
            if (trace_start_time != 0 && ticks == trace_start_time) {
                open_trace();
            }
            // custom trace add to here
            // basic simulation }
            if (both_edge_commit || top->aclk)  {
                // deadlock detection {
                if (top->debug_wb_rf_wen) last_commit = ticks;
                if (ticks - last_commit >= commit_timeout) {
                    printf("ERROR: There are %lu cycles since last commit\n", commit_timeout);
                    running = false;
                }
                // deadlock detection }
                // trace pc {
                if (trace_pc && top->debug_wb_rf_wen) printf("pc = %lx\n", top->debug_wb_pc);
                // trace pc }
                // difftest with cemu {
                if (!nodifftest) {
                    if (top->debug_wb_rf_wen && top->debug_wb_rf_wnum) {
                        do {
                            cemu_mips.step();
                            cemu_confreg.tick();
                        } while(!(cemu_mips.debug_wb_wen && cemu_mips.debug_wb_wnum));
                        // sync confreg ticks
                        if (cemu_mips.debug_wb_is_timer && cemu_mips.debug_wb_wnum == top->debug_wb_rf_wnum)
                            cemu_mips.set_GPR(cemu_mips.debug_wb_wnum, top->debug_wb_rf_wdata);
                        if ( cemu_mips.debug_wb_pc    != top->debug_wb_pc      || 
                            cemu_mips.debug_wb_wnum  != top->debug_wb_rf_wnum ||
                            (cemu_mips.debug_wb_wdata != top->debug_wb_rf_wdata)
                        ) {
                            printf("Error!\n");
                            printf("reference: PC = 0x%08x, wb_rf_wnum = 0x%02x, wb_rf_wdata = 0x%08x\n", cemu_mips.debug_wb_pc, cemu_mips.debug_wb_wnum, cemu_mips.debug_wb_wdata);
                            printf("mycpu    : PC = 0x%08x, wb_rf_wnum = 0x%02x, wb_rf_wdata = 0x%08x\n", top->debug_wb_pc, top->debug_wb_rf_wnum, top->debug_wb_rf_wdata);
                            running = false;
                            printf("cemu pc history:\n");
                            while (!cemu_mips.pc_trace.empty()) {
                                printf("%x\n",cemu_mips.pc_trace.front());
                                cemu_mips.pc_trace.pop();
                            }
                        }
                        last_commit = ticks;
                    }
                    while (diff_confreg_uart && rtl_confreg.has_uart() && cemu_confreg.has_uart()) {
                        char mycpu_uart = rtl_confreg.get_uart();
                        char ref_uart = cemu_confreg.get_uart();
                        if (mycpu_uart != ref_uart) {
                            printf("ERROR!\n UART different at %lu ticks.\n", ticks);
                            printf("Expected: %08x, found: %08x\n", mycpu_uart, ref_uart);
                            running = false;
                        }
                    }
                }
                // difftest with cemu }
                // statistic {
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
                            if (cur_wb_pc[0] && cur_wb_pc[0] != cur_wb_pc[1]) dual_commit ++;
                        }
                    }
                    last_wb_pc[0] = cur_wb_pc[0];
                    last_wb_pc[1] = cur_wb_pc[1];
                }
                // statistic }
                if (top->debug_wb_pc == 0xbfc00100u) cur_running = false;
            }
        }
        if (trace_on) vcd.close();
        if (rtl_confreg.get_num()) printf("%x\n", rtl_confreg.get_num());
        else printf("Unfinished performance test %d!\n", test);
        if (perf_stat) {
            printf("total_clk = %lu, stall_clk = %lu, has_commit = %lu, dual_commit = %lu, dual_issue_rate = %0.5lf, IPC = %0.5lf\n", total_clk, stall_clk, has_commit, dual_commit, (double)dual_commit / has_commit, (double)(has_commit + dual_commit) / total_clk);
            printf("confreg_read: %d, confreg_write: %d\n", rtl_confreg.confreg_read, rtl_confreg.confreg_write);
            rtl_confreg.confreg_read = 0;
            rtl_confreg.confreg_write = 0;
        }
        if (sim_time <= 0) running = false;
    }
    top->final();
    if (trace_on) vcd.close();
    printf("total ticks = %lu\n", ticks);
}

void rtl_cemu_diff_generic(Vmycpu_top *top, axi4_ref <32,32,4> &rtl_mmio_ref) {
    memory_bus cemu_mmio;

    axi4     <32,32,4> rtl_mmio_sigs;
    axi4_ref <32,32,4> rtl_mmio_sigs_ref(rtl_mmio_sigs);
    axi4_xbar<32,32,4> rtl_mmio(enable_axi_latency ? 23 : 0); // AXI latency

    std::vector<mmio_dev*> devices;

    int uart_irq_number = 1; // default 1 for ucore and linux, the same as soc_run_os

    // set up dram and uart, we use it in sys, ucore, linux.
    mmio_mem cemu_dram(128*1024*1024);
    mmio_mem  rtl_dram(128*1024*1024);
    uart8250 cemu_uart;
    uart8250  rtl_uart;
    assert(cemu_mmio.add_dev(0x0       , 128*1024*1024, &cemu_dram));
    assert( rtl_mmio.add_dev(0x0       , 128*1024*1024, & rtl_dram));
    assert(cemu_mmio.add_dev(0x1fe40000, 0x10000      , &cemu_uart));
    assert( rtl_mmio.add_dev(0x1fe40000, 0x10000      , & rtl_uart));

    switch(run_mode) {
        case SYS_TEST: {
            mmio_mem *spi_flash = new mmio_mem(0x100000, 
                                        "../supervisor-mips32/kernel/kernel.bin");
            // since the spi flash is read only, shares for both rtl and cemu
            assert(cemu_mmio.add_dev(0x1fc00000, 0x100000, spi_flash));
            assert( rtl_mmio.add_dev(0x1fc00000, 0x100000, spi_flash));
            devices.push_back(spi_flash);
            uart_irq_number = 2; // for system_test, uart irq number is 2
            break;
        }
        case UBOOT: {
            mmio_mem *spi_flash = new mmio_mem(2048*1024, 
                                        "../u-boot/u-boot.bin");
            // since the spi flash is read only, shares for both rtl and cemu
            assert(cemu_mmio.add_dev(0x1fc00000, 2048*1024, spi_flash));
            assert( rtl_mmio.add_dev(0x1fc00000, 2048*1024, spi_flash));
            devices.push_back(spi_flash);
            break;
        }
        case UCORE: {
            const uint32_t text_start = 0x80000000;
            uint32_t loader_instr[4] = {
                (0x3c1f0000u) | (text_start >> 16),     // lui  ra,%HI(text_start)
                (0x37ff0000u) | (text_start & 0xffff),  // ori  ra,ra,%LO(text_start)
                0x03e00008u,   // jr   ra
                0x00000000u    // nop
            };
            mmio_mem *bl_mem = new mmio_mem(4096);
            bl_mem->do_write(0,sizeof(loader_instr),(uint8_t*)&loader_instr);
            // since the bl_mem is read only, shares for both rtl and cemu
            assert(cemu_mmio.add_dev(0x1fc00000, 4096, bl_mem));
            assert( rtl_mmio.add_dev(0x1fc00000, 4096, bl_mem));
            cemu_dram.load_binary(0x0, "../ucore-thumips/obj/ucore-kernel-initrd.bin");
             rtl_dram.load_binary(0x0, "../ucore-thumips/obj/ucore-kernel-initrd.bin");
            break;
        }
        case LINUX: {
            const uint32_t text_start = 0x80100000; // linux .text starts at 0x801000000
            uint32_t loader_instr[4] = {
                (0x3c1f0000u) | (text_start >> 16),     // lui  ra,%HI(text_start)
                (0x37ff0000u) | (text_start & 0xffff),  // ori  ra,ra,%LO(text_start)
                0x03e00008u,   // jr   ra
                0x00000000u    // nop
            };
            mmio_mem *bl_mem = new mmio_mem(4096);
            bl_mem->do_write(0,sizeof(loader_instr),(uint8_t*)&loader_instr);
            // since the bl_mem is read only, shares for both rtl and cemu
            assert(cemu_mmio.add_dev(0x1fc00000, 4096, bl_mem));
            assert( rtl_mmio.add_dev(0x1fc00000, 4096, bl_mem));
            cemu_dram.load_binary(0x100000, "../linux/vmlinux.bin");
             rtl_dram.load_binary(0x100000, "../linux/vmlinux.bin");
            break;
        }
        default: {
            printf("Unknown running mode.\n");
            exit(-ENOENT);
        }
    }

    // setup uart input thread
    std::thread *uart_input_thread = new std::thread(uart_input,std::ref(rtl_uart));

    // setup diff memory
    if (diff_memory_write) rtl_dram.set_diff_mem(cemu_dram.get_mem_ptr());

    // connect Vcd for trace
    top->trace(&vcd,0);

    // reset rtl for 100 ticks
    top->aresetn = 0;
    for (int i=0;i<100;i++) {
        top->aclk = !top->aclk;
        top->eval();
    }
    top->aresetn = 1;

    // init cemu_mips
    mips_core cemu_mips(cemu_mmio);
    cemu_mips.set_difftest_mode(!nodifftest);

    // start simulation
    uint64_t last_commit = 0;
    uint64_t ticks = 0;
    
    while (!Verilated::gotFinish() && sim_time > 0 && running) {
        // basic simulation {
        ticks ++;
        top->aclk = !top->aclk;
        top->ext_int = rtl_uart.irq() << uart_irq_number; // level irq
        // fetch axi signals from rtl at negedge
        if (top->aclk) rtl_mmio_sigs.update_input(rtl_mmio_ref);
        top->eval();
        // process axi signals at posedge
        if (top->aclk) {
            rtl_mmio.beat(rtl_mmio_sigs_ref);
            rtl_mmio_sigs.update_output(rtl_mmio_ref);
            while (rtl_uart.exist_tx()) {
                char c = rtl_uart.getc();
                printf("%c",c);
                fflush(stdout);
            }
        }
        if (trace_on) { // print wave if trace_on
            top->eval(); // update rtl status changed by axi, this will only affect waveform
            vcd.dump(ticks);
            sim_time --;
        }
        // conditinal trace
        if (trace_start_time != 0 && ticks == trace_start_time) {
            open_trace();
        }
        // custom trace add to here
        // basic simulation }
        if (both_edge_commit || top->aclk)  {
            // deadlock detection {
            if (top->debug_wb_rf_wen) last_commit = ticks;
            if (ticks - last_commit >= commit_timeout) {
                printf("ERROR: There are %lu cycles since last commit\n", commit_timeout);
                running = false;
            }
            // deadlock detection }
            // trace pc {
            if (trace_pc && top->debug_wb_rf_wen) printf("pc = %lx\n", top->debug_wb_pc);
            // trace pc }
#ifdef HAS_DEBUG_EXT
            if (!nodifftest && top->debug_commit && top->debug_wb_pc) { // cemu difftest
                cemu_mips.import_diff_test_info(top->debug_cp0_count, top->debug_cp0_random, top->debug_cp0_cause, top->debug_int);
                cemu_uart.clear_access_flag();
                cemu_mips.step();
                if (cemu_uart.has_access_flag()) {
                    cemu_uart.clear_access_flag();
                    cemu_mips.set_GPR(cemu_mips.debug_wb_wnum, top->debug_wb_rf_wdata);
                }
                if (
                    cemu_mips.debug_wb_pc    != top->debug_wb_pc      || (
                    cemu_mips.debug_wb_wen  && (
                    cemu_mips.debug_wb_wnum  != top->debug_wb_rf_wnum ||
                    (cemu_mips.debug_wb_wdata != top->debug_wb_rf_wdata) ) )
                ) {
                    printf("Error at %ld ps!\n",ticks);
                    printf("reference: PC = 0x%08x, wb_rf_wnum = 0x%02x, wb_rf_wdata = 0x%08x\n", cemu_mips.debug_wb_pc, cemu_mips.debug_wb_wnum, cemu_mips.debug_wb_wdata);
                    printf("mycpu    : PC = 0x%08x, wb_rf_wnum = 0x%02x, wb_rf_wdata = 0x%08x\n", top->debug_wb_pc, top->debug_wb_rf_wnum, top->debug_wb_rf_wdata);
                    running = false;
                    cemu_dram.save_binary("cemu_memory.dump");
                    printf("cemu memory dumped!\n");
                    printf("cemu pc history:\n");
                    while (!cemu_mips.pc_trace.empty()) {
                        printf("%x\n",cemu_mips.pc_trace.front());
                        cemu_mips.pc_trace.pop();
                    }
                }
            } // cemu difftest }
#else
            if (!nodifftest) {
                printf("Error: Your CPU should have debug extension to use this function.\n");
                printf("If you have already add these signals, you can add \"#define HAS_DEBUG_EXT\" to the config section of sim_mycpu.cpp.\n");
                printf("If you only want to run linux/ucore/uboot without difftest, please add \"-nodiff\" to the arguments.\n");
                exit(-EINVAL);
            }
#endif
        }
    }
    top->final();
    if (trace_on) vcd.close();
    pthread_kill(uart_input_thread->native_handle(),SIGKILL);
    for (auto x : devices) delete x;
}

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);

    std::signal(SIGINT, [](int) {
        running = false;
    });

    int perf_start = 1;
    int perf_end = 10;

    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i],"-axifast") == 0) {
            enable_axi_latency = false;
        }
        else if (strcmp(argv[i], "-nodiff") == 0) {
            nodifftest = true;
        }
        else if (strcmp(argv[i],"-trace") == 0) {
            trace_on = true;
            if (i+1 < argc) {
                sscanf(argv[++i],"%lu",&sim_time);
            }
        }
        else if (strcmp(argv[i],"-starttrace") == 0) {
            if (i+1 < argc) {
                sscanf(argv[++i],"%lu",&trace_start_time);
            }
        }
        else if (strcmp(argv[i],"-pc") == 0) {
            trace_pc = true;
        }
        else if (strcmp(argv[i],"-diffmem") == 0) {
            diff_memory_write = true;
        }
        else if (strcmp(argv[i],"-perfonce") == 0) {
            confreg_simu_flag = true;
        }
        else if (strcmp(argv[i],"-uart") == 0) {
            out_confreg_uart = true;
        }
        else if (strcmp(argv[i],"-diffuart") == 0) {
            diff_confreg_uart = true;
        }
        else if (strcmp(argv[i],"-stat") == 0) {
            perf_stat = true;
        }
        else if (strcmp(argv[i],"-prog") == 0) {
            if (i+1 < argc) {
                sscanf(argv[++i],"%lu",&perf_start);
                perf_end = perf_start;
                printf("set performance test program to %d\n",perf_start);
            }
        }
        else if (strcmp(argv[i],"-func") == 0) {
            run_mode = FUNC;
        }
        else if (strcmp(argv[i],"-perf") == 0) {
            run_mode = PERF;
        }
        else if (strcmp(argv[i],"-sys") == 0) {
            run_mode = SYS_TEST;
        }
        else if (strcmp(argv[i],"-ucore") == 0) {
            run_mode = UCORE;
        }
        else if (strcmp(argv[i],"-linux") == 0) {
            run_mode = LINUX;
        }
        else if (strcmp(argv[i],"-uboot") == 0) {
            run_mode = UBOOT;
        }
    }
    Verilated::traceEverOn(true);
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
        default:
            rtl_cemu_diff_generic(top, mmio_ref);
            break;
    }
    return 0;
}
