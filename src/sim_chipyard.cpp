#include "verilated.h"
#include "verilated_fst_c.h"
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
#include <csignal>


// run time config {
VerilatedFstC fst;
std::atomic_bool trace_on;
bool running = true;
long sim_time = 1e3;            // -trace [time]
// run time config }

void open_trace() {
    fst.open("trace.fst");
    trace_on.store(true, std::memory_order_seq_cst);
}

void connect_wire(axi4_ptr <31,64,4> &mmio_ptr, axi4_ptr <32,64,4> &mem_ptr, VChipTop *dut) {
    // connect
    // mmio
    // aw   
    mmio_ptr.awaddr     = &(dut->axi4_mmio_0_bits_aw_bits_addr);
    mmio_ptr.awburst    = &(dut->axi4_mmio_0_bits_aw_bits_burst);
    mmio_ptr.awid       = &(dut->axi4_mmio_0_bits_aw_bits_id);
    mmio_ptr.awlen      = &(dut->axi4_mmio_0_bits_aw_bits_len);
    mmio_ptr.awready    = &(dut->axi4_mmio_0_bits_aw_ready);
    mmio_ptr.awsize     = &(dut->axi4_mmio_0_bits_aw_bits_size);
    mmio_ptr.awvalid    = &(dut->axi4_mmio_0_bits_aw_valid);
    // w
    mmio_ptr.wdata      = &(dut->axi4_mmio_0_bits_w_bits_data);
    mmio_ptr.wlast      = &(dut->axi4_mmio_0_bits_w_bits_last);
    mmio_ptr.wready     = &(dut->axi4_mmio_0_bits_w_ready);
    mmio_ptr.wstrb      = &(dut->axi4_mmio_0_bits_w_bits_strb);
    mmio_ptr.wvalid     = &(dut->axi4_mmio_0_bits_w_valid);
    // b
    mmio_ptr.bid        = &(dut->axi4_mmio_0_bits_b_bits_id);
    mmio_ptr.bready     = &(dut->axi4_mmio_0_bits_b_ready);
    mmio_ptr.bresp      = &(dut->axi4_mmio_0_bits_b_bits_resp);
    mmio_ptr.bvalid     = &(dut->axi4_mmio_0_bits_b_valid);
    // ar
    mmio_ptr.araddr     = &(dut->axi4_mmio_0_bits_ar_bits_addr);
    mmio_ptr.arburst    = &(dut->axi4_mmio_0_bits_ar_bits_burst);
    mmio_ptr.arid       = &(dut->axi4_mmio_0_bits_ar_bits_id);
    mmio_ptr.arlen      = &(dut->axi4_mmio_0_bits_ar_bits_len);
    mmio_ptr.arready    = &(dut->axi4_mmio_0_bits_ar_ready);
    mmio_ptr.arsize     = &(dut->axi4_mmio_0_bits_ar_bits_size);
    mmio_ptr.arvalid    = &(dut->axi4_mmio_0_bits_ar_valid);
    // r
    mmio_ptr.rdata      = &(dut->axi4_mmio_0_bits_r_bits_data);
    mmio_ptr.rid        = &(dut->axi4_mmio_0_bits_r_bits_id);
    mmio_ptr.rlast      = &(dut->axi4_mmio_0_bits_r_bits_last);
    mmio_ptr.rready     = &(dut->axi4_mmio_0_bits_r_ready);
    mmio_ptr.rresp      = &(dut->axi4_mmio_0_bits_r_bits_resp);
    mmio_ptr.rvalid     = &(dut->axi4_mmio_0_bits_r_valid);
    // mem
    // aw
    mem_ptr.awaddr  = &(dut->axi4_mem_0_bits_aw_bits_addr);
    mem_ptr.awburst = &(dut->axi4_mem_0_bits_aw_bits_burst);
    mem_ptr.awid    = &(dut->axi4_mem_0_bits_aw_bits_id);
    mem_ptr.awlen   = &(dut->axi4_mem_0_bits_aw_bits_len);
    mem_ptr.awready = &(dut->axi4_mem_0_bits_aw_ready);
    mem_ptr.awsize  = &(dut->axi4_mem_0_bits_aw_bits_size);
    mem_ptr.awvalid = &(dut->axi4_mem_0_bits_aw_valid);
    // w
    mem_ptr.wdata   = &(dut->axi4_mem_0_bits_w_bits_data);
    mem_ptr.wlast   = &(dut->axi4_mem_0_bits_w_bits_last);
    mem_ptr.wready  = &(dut->axi4_mem_0_bits_w_ready);
    mem_ptr.wstrb   = &(dut->axi4_mem_0_bits_w_bits_strb);
    mem_ptr.wvalid  = &(dut->axi4_mem_0_bits_w_valid);
    // b
    mem_ptr.bid     = &(dut->axi4_mem_0_bits_b_bits_id);
    mem_ptr.bready  = &(dut->axi4_mem_0_bits_b_ready);
    mem_ptr.bresp   = &(dut->axi4_mem_0_bits_b_bits_resp);
    mem_ptr.bvalid  = &(dut->axi4_mem_0_bits_b_valid);
    // ar
    mem_ptr.araddr  = &(dut->axi4_mem_0_bits_ar_bits_addr);
    mem_ptr.arburst = &(dut->axi4_mem_0_bits_ar_bits_burst);
    mem_ptr.arid    = &(dut->axi4_mem_0_bits_ar_bits_id);
    mem_ptr.arlen   = &(dut->axi4_mem_0_bits_ar_bits_len);
    mem_ptr.arready = &(dut->axi4_mem_0_bits_ar_ready);
    mem_ptr.arsize  = &(dut->axi4_mem_0_bits_ar_bits_size);
    mem_ptr.arvalid = &(dut->axi4_mem_0_bits_ar_valid);
    // r
    mem_ptr.rdata   = &(dut->axi4_mem_0_bits_r_bits_data);
    mem_ptr.rid     = &(dut->axi4_mem_0_bits_r_bits_id);
    mem_ptr.rlast   = &(dut->axi4_mem_0_bits_r_bits_last);
    mem_ptr.rready  = &(dut->axi4_mem_0_bits_r_ready);
    mem_ptr.rresp   = &(dut->axi4_mem_0_bits_r_bits_resp);
    mem_ptr.rvalid  = &(dut->axi4_mem_0_bits_r_valid);
}

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

void run_system(VChipTop *dut, const char* load_path, uint64_t jump_addr = 0x80000000) {
    // loader {
    uint32_t loader_instr[3] = {
        0x600010b7u,// lui	ra,0x60001
        0x0000b083u,// ld	ra,0(ra) # 60001000
        0x000080e7u // jalr	ra
    };
    // loader }

    axi4_ptr <31,64,4> mmio_ptr;
    axi4_ptr <32,64,4> mem_ptr;

    connect_wire(mmio_ptr,mem_ptr,dut);
    assert(mmio_ptr.check());
    assert(mem_ptr.check());

    // connect fst for trace
    dut->trace(&fst,0);
    if (trace_on) open_trace();
    
    axi4_ref <31,64,4> mmio_ref(mmio_ptr);
    axi4     <31,64,4> mmio_sigs;
    axi4_ref <31,64,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<31,64,4> mmio;

    mmio_mem           boot_ram(262144*4);
    boot_ram.do_write(0,sizeof(loader_instr),(uint8_t*)&loader_instr);
    boot_ram.do_write(0x1000,8,(uint8_t*)&jump_addr);
    uartlite           uart;
    std::thread        uart_input_thread(uart_input,std::ref(uart));
    assert(mmio.add_dev(0x60000000,1024*1024,&boot_ram));
    assert(mmio.add_dev(0x60100000,1024*1024,&uart));

    axi4_ref <32,64,4> mem_ref(mem_ptr);
    axi4     <32,64,4> mem_sigs;
    axi4_ref <32,64,4> mem_sigs_ref(mem_sigs);
    axi4_mem <32,64,4> mem(4096l*1024*1024);
    if (load_path != NULL) mem.load_binary(load_path, 0x80000000);
    dut->reset_io = 1;
    uint64_t ticks = 0;
    uint64_t uart_tx_bytes = 0;
    while (!Verilated::gotFinish() && sim_time > 0 && running) {
        dut->eval();
        ticks ++;
        if (trace_on) { // print wave if trace_on
            dut->eval(); // update rtl status changed by axi, this will only affect waveform
            fst.dump(ticks);
            sim_time --;
        }
        if (ticks == 9) dut->reset_io = 0;
        dut->clock_uncore = 1;
        // posedge
        mmio_sigs.update_input(mmio_ref);
        mem_sigs.update_input(mem_ref);
        dut->eval();
        ticks ++;
        if (!dut->reset_io) {
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
        if (trace_on) { // print wave if trace_on
            dut->eval(); // update rtl status changed by axi, this will only affect waveform
            fst.dump(ticks);
            sim_time --;
        }
        dut->ext_interrupts = uart.irq();
        dut->clock_uncore = 0;
    }
    if (trace_on) fst.close();
    dut->final();
    pthread_kill(uart_input_thread.native_handle(),SIGKILL);
}

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);

    std::signal(SIGINT, [](int) {
        running = false;
    });

    char *load_path = NULL;
    uint64_t start_addr = 0x80000000L;

    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i], "-trace") == 0) {
            trace_on = true;
            if (i+1 < argc) {
                sscanf(argv[++i], "%lu", &sim_time);
            }
        }
        else if (strcmp(argv[i], "-init") == 0) {
            if (i+1 < argc) {
                load_path = argv[++i];
            }
        }
        else if (strcmp(argv[i], "-start") == 0) {
            if (i+1 < argc) {
                sscanf(argv[++i], "%lx", &start_addr);
            }
        }
    }
    Verilated::traceEverOn(trace_on);

    VChipTop *dut = new VChipTop;

    run_system(dut, load_path, start_addr);

    return 0;
}