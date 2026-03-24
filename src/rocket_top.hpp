/*
 * rocket_top.hpp - Helper to drive ExampleRocketSystem clock/reset/interrupt ports
 *
 * The firtool-generated ExampleRocketSystem module exposes individual
 * io_aggregator_N_clock / io_aggregator_N_reset ports instead of a single
 * clock/reset.  This header provides convenience functions and a DMI controller.
 */

#ifndef ROCKET_TOP_HPP
#define ROCKET_TOP_HPP

#include "VExampleRocketSystem.h"
#include <cstdio>
#include <cstdint>

/* Drive all clock aggregator inputs with the given value. */
static inline void rocket_set_clock(VExampleRocketSystem *top, uint8_t clk) {
    top->io_aggregator_0_clock = clk;
    top->io_aggregator_1_clock = clk;
    top->io_aggregator_2_clock = clk;
    top->io_aggregator_3_clock = clk;
    top->io_aggregator_4_clock = clk;
    top->io_aggregator_5_clock = clk;
    top->debug_clock = clk;
    top->debug_clockeddmi_dmiClock = clk;
}

/* Drive all reset aggregator inputs with the given value. */
static inline void rocket_set_reset(VExampleRocketSystem *top, uint8_t rst) {
    top->io_aggregator_0_reset = rst;
    top->io_aggregator_1_reset = rst;
    top->io_aggregator_2_reset = rst;
    top->io_aggregator_3_reset = rst;
    top->io_aggregator_4_reset = rst;
    top->io_aggregator_5_reset = rst;
    top->debug_reset = rst;
    top->debug_clockeddmi_dmiReset = rst;
    top->resetctrl_hartIsInReset_0 = rst;
}

/* Tie off the debug DMI interface (no JTAG connected). */
static inline void rocket_tie_off_debug(VExampleRocketSystem *top) {
    top->debug_clockeddmi_dmi_req_valid = 0;
    top->debug_clockeddmi_dmi_req_bits_addr = 0;
    top->debug_clockeddmi_dmi_req_bits_data = 0;
    top->debug_clockeddmi_dmi_req_bits_op = 0;
    top->debug_clockeddmi_dmi_resp_ready = 0;
    top->debug_dmactiveAck = 0;
}

/* ===== DMI Controller ===== */

/* DMI operation codes */
enum {
    DMI_OP_NOP   = 0,
    DMI_OP_READ  = 1,
    DMI_OP_WRITE = 2,
};

/* DMI response codes */
enum {
    DMI_RESP_SUCCESS = 0,
    DMI_RESP_FAILURE = 1,
};

/* Debug module register addresses */
enum {
    DMI_DATA0       = 0x04,
    DMI_DATA1       = 0x05,
    DMI_DMCONTROL   = 0x10,
    DMI_DMSTATUS    = 0x11,
    DMI_ABSTRACTCS  = 0x16,
    DMI_COMMAND     = 0x17,
    DMI_PROGBUF0    = 0x20,
    DMI_PROGBUF1    = 0x21,
};

/* DMCONTROL bits */
enum {
    DMCONTROL_DMACTIVE   = (1U << 0),
    DMCONTROL_HALTREQ    = (1U << 31),
    DMCONTROL_RESUMEREQ  = (1U << 30),
};

/* DMSTATUS bits */
enum {
    DMSTATUS_ALLHALTED    = (1U << 9),
    DMSTATUS_ALLRESUMEACK = (1U << 17),
};

/*
 * Simple DMI controller that performs DMI read/write operations over
 * the debug_clockeddmi port of ExampleRocketSystem.
 * Each operation takes multiple clock cycles.
 */
class RocketDMI {
    VExampleRocketSystem *top_;
    /* Internal clock tick helper: drives one clock cycle (neg + pos edge) */
    void tick() {
        rocket_set_clock(top_, 0);
        top_->eval();
        rocket_set_clock(top_, 1);
        top_->eval();
        top_->debug_dmactiveAck = top_->debug_dmactive;
        top_->eval();  /* re-eval after updating dmactiveAck */
    }

public:
    RocketDMI(VExampleRocketSystem *top) : top_(top) {}

    /* Perform a DMI write. Returns 0 on success. */
    int dmi_write(uint8_t addr, uint32_t data) {
        /* Drive request */
        top_->debug_clockeddmi_dmi_req_valid = 1;
        top_->debug_clockeddmi_dmi_req_bits_addr = addr;
        top_->debug_clockeddmi_dmi_req_bits_data = data;
        top_->debug_clockeddmi_dmi_req_bits_op = DMI_OP_WRITE;
        top_->debug_clockeddmi_dmi_resp_ready = 1;

        /* Wait for req_ready handshake */
        bool got_ready = false;
        for (int i = 0; i < 2000; i++) {
            tick();
            if (top_->debug_clockeddmi_dmi_req_ready) {
                got_ready = true;
                break;
            }
        }
        if (!got_ready) {
            fprintf(stderr, "DMI write(0x%02x): timeout waiting for req_ready\n", addr);
            top_->debug_clockeddmi_dmi_req_valid = 0;
            return -1;
        }
        /* Handshake occurred on this edge; deassert after one more cycle */
        top_->debug_clockeddmi_dmi_req_valid = 0;

        /* Wait for response - may take many cycles due to async crossing */
        for (int i = 0; i < 5000; i++) {
            tick();
            if (top_->debug_clockeddmi_dmi_resp_valid) {
                int resp = top_->debug_clockeddmi_dmi_resp_bits_resp;
                return resp;
            }
        }
        fprintf(stderr, "DMI write(0x%02x): timeout waiting for resp\n", addr);
        return -1;
    }

    /* Perform a DMI read. Returns 0 on success, stores data in *out. */
    int dmi_read(uint8_t addr, uint32_t *out) {
        top_->debug_clockeddmi_dmi_req_valid = 1;
        top_->debug_clockeddmi_dmi_req_bits_addr = addr;
        top_->debug_clockeddmi_dmi_req_bits_data = 0;
        top_->debug_clockeddmi_dmi_req_bits_op = DMI_OP_READ;
        top_->debug_clockeddmi_dmi_resp_ready = 1;

        bool got_ready = false;
        for (int i = 0; i < 2000; i++) {
            tick();
            if (top_->debug_clockeddmi_dmi_req_ready) {
                got_ready = true;
                break;
            }
        }
        if (!got_ready) {
            fprintf(stderr, "DMI read(0x%02x): timeout waiting for req_ready\n", addr);
            top_->debug_clockeddmi_dmi_req_valid = 0;
            return -1;
        }
        top_->debug_clockeddmi_dmi_req_valid = 0;

        for (int i = 0; i < 5000; i++) {
            tick();
            if (top_->debug_clockeddmi_dmi_resp_valid) {
                *out = top_->debug_clockeddmi_dmi_resp_bits_data;
                int resp = top_->debug_clockeddmi_dmi_resp_bits_resp;
                return resp;
            }
        }
        fprintf(stderr, "DMI read(0x%02x): timeout waiting for resp\n", addr);
        return -1;
    }

    /*
     * Halt hart 0, set DPC to target_pc, and resume.
     * This is needed because the boot ROM puts harts into WFI.
     * Returns 0 on success.
     */
    int halt_set_pc_resume(uint64_t target_pc) {
        uint32_t val;
        int rc;

        /* 1. Activate debug module */
        rc = dmi_write(DMI_DMCONTROL, DMCONTROL_DMACTIVE);
        if (rc) { fprintf(stderr, "DMI: dmcontrol write returned %d\n", rc); return -1; }

        /* Let dmactive propagate and dmactiveAck loop back */
        for (int i = 0; i < 200; i++) tick();

        /* 2. Halt hart 0 */
        rc = dmi_write(DMI_DMCONTROL, DMCONTROL_DMACTIVE | DMCONTROL_HALTREQ);
        if (rc) { fprintf(stderr, "DMI: failed to halt\n"); return -1; }

        /* 3. Poll until halted */
        for (int i = 0; i < 5000; i++) {
            rc = dmi_read(DMI_DMSTATUS, &val);
            if (rc) continue;
            if (val & DMSTATUS_ALLHALTED)
                break;
            if (i == 4999) {
                fprintf(stderr, "DMI: timeout waiting for halt (dmstatus=0x%08x)\n", val);
                return -1;
            }
        }
        fprintf(stderr, "DMI: hart 0 halted (dmstatus=0x%08x)\n", val);

        /* 4. Write low 32 bits of DPC via abstract command */
        /* DATA0 = lower 32 bits of target PC */
        rc = dmi_write(DMI_DATA0, (uint32_t)(target_pc & 0xFFFFFFFF));
        if (rc) { fprintf(stderr, "DMI: failed to write DATA0\n"); return -1; }

        /* DATA1 = upper 32 bits of target PC */
        rc = dmi_write(DMI_DATA1, (uint32_t)(target_pc >> 32));
        if (rc) { fprintf(stderr, "DMI: failed to write DATA1\n"); return -1; }

        /* ACCESS_REGISTER command: write=1, transfer=1, aarsize=3 (64-bit),
         * regno = 0x7b1 (DPC)
         * Command word: cmdtype=0 (access register), aarsize=3, write=1,
         *               transfer=1, regno=0x7b1
         * Bits: [31:24]=0x00, [23]=0 (aarpostincr), [22:20]=3 (aarsize),
         *       [19]=0, [18]=0, [17]=1 (transfer), [16]=1 (write),
         *       [15:0]=0x07b1 (DPC csr = 0x7b1)
         */
        uint32_t cmd = (0U << 24)        /* cmdtype = 0 (access register) */
                     | (3U << 20)         /* aarsize = 3 (64-bit) */
                     | (1U << 17)         /* transfer = 1 */
                     | (1U << 16)         /* write = 1 (write to register) */
                     | 0x07b1;            /* regno = 0x07b1 (DPC) */
        rc = dmi_write(DMI_COMMAND, cmd);
        if (rc) { fprintf(stderr, "DMI: failed to write command\n"); return -1; }

        /* Wait for abstract command to complete */
        for (int i = 0; i < 2000; i++) {
            rc = dmi_read(DMI_ABSTRACTCS, &val);
            if (rc) continue;
            if ((val & 0x700) == 0 && !(val & (1 << 12)))
                break; /* busy=0, cmderr=0 */
            if (val & 0x700) {
                fprintf(stderr, "DMI: abstract command error (abstractcs=0x%08x)\n", val);
                /* Clear error */
                dmi_write(DMI_ABSTRACTCS, 0x700);
                return -1;
            }
        }
        fprintf(stderr, "DMI: DPC set to 0x%lx\n", (unsigned long)target_pc);

        /* 5. Clear haltreq and set resumereq */
        rc = dmi_write(DMI_DMCONTROL, DMCONTROL_DMACTIVE | DMCONTROL_RESUMEREQ);
        if (rc) { fprintf(stderr, "DMI: failed to resume\n"); return -1; }

        /* 6. Poll until resumed */
        for (int i = 0; i < 5000; i++) {
            rc = dmi_read(DMI_DMSTATUS, &val);
            if (rc) continue;
            if (val & DMSTATUS_ALLRESUMEACK)
                break;
        }
        fprintf(stderr, "DMI: hart 0 resumed (dmstatus=0x%08x)\n", val);

        /* Clear resumereq */
        dmi_write(DMI_DMCONTROL, DMCONTROL_DMACTIVE);

        return 0;
    }
};

/* Tie off the L2 frontend bus (AXI slave, no DMA connected yet). */
static inline void rocket_tie_off_l2_frontend(VExampleRocketSystem *top) {
    top->l2_frontend_bus_axi4_0_aw_valid = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_id = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_addr = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_len = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_size = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_burst = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_lock = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_cache = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_prot = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_qos = 0;
    top->l2_frontend_bus_axi4_0_w_valid = 0;
    top->l2_frontend_bus_axi4_0_w_bits_data = 0;
    top->l2_frontend_bus_axi4_0_w_bits_strb = 0;
    top->l2_frontend_bus_axi4_0_w_bits_last = 0;
    top->l2_frontend_bus_axi4_0_b_ready = 0;
    top->l2_frontend_bus_axi4_0_ar_valid = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_id = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_addr = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_len = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_size = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_burst = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_lock = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_cache = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_prot = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_qos = 0;
    top->l2_frontend_bus_axi4_0_r_ready = 0;
}

#endif /* ROCKET_TOP_HPP */
