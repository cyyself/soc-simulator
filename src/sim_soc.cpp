#include "verilated_vcd_c.h"
#include "verilated.h"
#include "VChipTop.h"

#include "axi4.hpp"
#include "axi4_mem.hpp"
#include "axi4_xbar.hpp"
#include "mmio_mem.hpp"

#include <iostream>

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

int main(int argc, char** argv, char** env) {
    Verilated::commandArgs(argc, argv);
    VChipTop *top = new VChipTop;
    axi4_ptr <31,64,4> mmio_ptr;
    axi4_ptr <33,64,4> mem_ptr;

    connect_wire(mmio_ptr,mem_ptr,top);
    
    axi4_ref <31,64,4> mmio_ref(mmio_ptr);
    axi4     <31,64,4> mmio_sigs;
    axi4_ref <31,64,4> mmio_sigs_ref(mmio_sigs);
    axi4_xbar<31,64,4> mmio;

    mmio_mem           bootram(1024*1024,"../u-boot/u-boot.bin");
    mmio.add_dev(0x60000000,1024*1024,&bootram);
    
    axi4_ref <33,64,4> mem_ref(mem_ptr);
    axi4     <33,64,4> mem_sigs;
    axi4_ref <33,64,4> mem_sigs_ref(mem_sigs);
    axi4_mem <33,64,4> mem(4096l*1024*1024);
    
    top->reset = 1;
    unsigned long ticks = 0;
    while (!Verilated::gotFinish()) {
        top->eval();
        if (ticks == 10) top->reset = 0;
        top->clock_clock = 1;
        // posedge
        mmio_sigs.update_input(mmio_ref);
        mem_sigs.update_input(mem_ref);
        top->eval();
        if (!top->reset) {
            mem.beat(mem_sigs_ref);
            mmio.beat(mmio_sigs_ref);
        }
        mmio_sigs.update_output(mmio_ref);
        mem_sigs.update_output(mem_ref);
        top->clock_clock = 0;
        ticks ++;
    }
    return 0;
}