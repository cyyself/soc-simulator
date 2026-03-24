/*
 * sim_qemu.cpp - SoC simulator with QEMU bridge
 *
 * Connects a Rocket-Chip CPU (RTL via Verilator) to QEMU system emulator.
 * soc-simulator listens on a TCP port, QEMU connects to it.
 * The CPU's MMIO accesses are forwarded to QEMU over the TCP socket.
 *
 * Two memory modes:
 *   local (default): Memory resides in soc-simulator. QEMU DMAs via protocol.
 *   qemu:            Memory resides in QEMU. CPU accesses forwarded via protocol.
 *
 * Usage: ./sim_qemu [options]
 *   -s <[host:]port>    TCP listen address (default: 0.0.0.0:2345)
 *   -b <binary>         Binary to load at 0x80000000 (local mode only)
 *   -m <size_mb>        Memory size in MB (default: 256)
 *   -M <local|qemu>     Memory mode (default: local)
 */

#include "verilated.h"
#include "verilated_threads.h"
#include "VExampleRocketSystem.h"

#include "axi4.hpp"
#include "axi4_mem.hpp"
#include "axi4_xbar.hpp"
#include "axi4_master.hpp"
#include "mmio_mem.hpp"
#include "qemu_bridge.hpp"
#include "rocket_top.hpp"

#include <iostream>
#include <memory>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <getopt.h>
#include <unistd.h>

static bool running = true;

static void sigint_handler(int sig) {
    (void)sig;
    running = false;
}

void connect_l2_wire(axi4_ptr<32,64,8> &l2_ptr,
                     VExampleRocketSystem *top) {
    /* L2 frontend AXI4 bus (soc-sim is master, Rocket L2 is slave) */
    l2_ptr.awaddr  = &(top->l2_frontend_bus_axi4_0_aw_bits_addr);
    l2_ptr.awburst = &(top->l2_frontend_bus_axi4_0_aw_bits_burst);
    l2_ptr.awid    = &(top->l2_frontend_bus_axi4_0_aw_bits_id);
    l2_ptr.awlen   = &(top->l2_frontend_bus_axi4_0_aw_bits_len);
    l2_ptr.awready = &(top->l2_frontend_bus_axi4_0_aw_ready);
    l2_ptr.awsize  = &(top->l2_frontend_bus_axi4_0_aw_bits_size);
    l2_ptr.awvalid = &(top->l2_frontend_bus_axi4_0_aw_valid);
    l2_ptr.wdata   = &(top->l2_frontend_bus_axi4_0_w_bits_data);
    l2_ptr.wlast   = &(top->l2_frontend_bus_axi4_0_w_bits_last);
    l2_ptr.wready  = &(top->l2_frontend_bus_axi4_0_w_ready);
    l2_ptr.wstrb   = &(top->l2_frontend_bus_axi4_0_w_bits_strb);
    l2_ptr.wvalid  = &(top->l2_frontend_bus_axi4_0_w_valid);
    l2_ptr.bid     = &(top->l2_frontend_bus_axi4_0_b_bits_id);
    l2_ptr.bready  = &(top->l2_frontend_bus_axi4_0_b_ready);
    l2_ptr.bresp   = &(top->l2_frontend_bus_axi4_0_b_bits_resp);
    l2_ptr.bvalid  = &(top->l2_frontend_bus_axi4_0_b_valid);
    l2_ptr.araddr  = &(top->l2_frontend_bus_axi4_0_ar_bits_addr);
    l2_ptr.arburst = &(top->l2_frontend_bus_axi4_0_ar_bits_burst);
    l2_ptr.arid    = &(top->l2_frontend_bus_axi4_0_ar_bits_id);
    l2_ptr.arlen   = &(top->l2_frontend_bus_axi4_0_ar_bits_len);
    l2_ptr.arready = &(top->l2_frontend_bus_axi4_0_ar_ready);
    l2_ptr.arsize  = &(top->l2_frontend_bus_axi4_0_ar_bits_size);
    l2_ptr.arvalid = &(top->l2_frontend_bus_axi4_0_ar_valid);
    l2_ptr.rdata   = &(top->l2_frontend_bus_axi4_0_r_bits_data);
    l2_ptr.rid     = &(top->l2_frontend_bus_axi4_0_r_bits_id);
    l2_ptr.rlast   = &(top->l2_frontend_bus_axi4_0_r_bits_last);
    l2_ptr.rready  = &(top->l2_frontend_bus_axi4_0_r_ready);
    l2_ptr.rresp   = &(top->l2_frontend_bus_axi4_0_r_bits_resp);
    l2_ptr.rvalid  = &(top->l2_frontend_bus_axi4_0_r_valid);
}

/* Set L2 frontend signals not in axi4_ptr (lock/cache/prot/qos) to defaults */
static inline void l2_set_defaults(VExampleRocketSystem *top) {
    top->l2_frontend_bus_axi4_0_aw_bits_lock  = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_cache = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_prot  = 0;
    top->l2_frontend_bus_axi4_0_aw_bits_qos   = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_lock  = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_cache = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_prot  = 0;
    top->l2_frontend_bus_axi4_0_ar_bits_qos   = 0;
}

void connect_wire(axi4_ptr<31,64,4> &mmio_ptr, axi4_ptr<32,64,4> &mem_ptr,
                  VExampleRocketSystem *top) {
    /* MMIO AXI4 bus */
    mmio_ptr.awaddr     = &(top->mmio_axi4_0_aw_bits_addr);
    mmio_ptr.awburst    = &(top->mmio_axi4_0_aw_bits_burst);
    mmio_ptr.awid       = &(top->mmio_axi4_0_aw_bits_id);
    mmio_ptr.awlen      = &(top->mmio_axi4_0_aw_bits_len);
    mmio_ptr.awready    = &(top->mmio_axi4_0_aw_ready);
    mmio_ptr.awsize     = &(top->mmio_axi4_0_aw_bits_size);
    mmio_ptr.awvalid    = &(top->mmio_axi4_0_aw_valid);
    mmio_ptr.wdata      = &(top->mmio_axi4_0_w_bits_data);
    mmio_ptr.wlast      = &(top->mmio_axi4_0_w_bits_last);
    mmio_ptr.wready     = &(top->mmio_axi4_0_w_ready);
    mmio_ptr.wstrb      = &(top->mmio_axi4_0_w_bits_strb);
    mmio_ptr.wvalid     = &(top->mmio_axi4_0_w_valid);
    mmio_ptr.bid        = &(top->mmio_axi4_0_b_bits_id);
    mmio_ptr.bready     = &(top->mmio_axi4_0_b_ready);
    mmio_ptr.bresp      = &(top->mmio_axi4_0_b_bits_resp);
    mmio_ptr.bvalid     = &(top->mmio_axi4_0_b_valid);
    mmio_ptr.araddr     = &(top->mmio_axi4_0_ar_bits_addr);
    mmio_ptr.arburst    = &(top->mmio_axi4_0_ar_bits_burst);
    mmio_ptr.arid       = &(top->mmio_axi4_0_ar_bits_id);
    mmio_ptr.arlen      = &(top->mmio_axi4_0_ar_bits_len);
    mmio_ptr.arready    = &(top->mmio_axi4_0_ar_ready);
    mmio_ptr.arsize     = &(top->mmio_axi4_0_ar_bits_size);
    mmio_ptr.arvalid    = &(top->mmio_axi4_0_ar_valid);
    mmio_ptr.rdata      = &(top->mmio_axi4_0_r_bits_data);
    mmio_ptr.rid        = &(top->mmio_axi4_0_r_bits_id);
    mmio_ptr.rlast      = &(top->mmio_axi4_0_r_bits_last);
    mmio_ptr.rready     = &(top->mmio_axi4_0_r_ready);
    mmio_ptr.rresp      = &(top->mmio_axi4_0_r_bits_resp);
    mmio_ptr.rvalid     = &(top->mmio_axi4_0_r_valid);

    /* Memory AXI4 bus */
    mem_ptr.awaddr  = &(top->mem_axi4_0_aw_bits_addr);
    mem_ptr.awburst = &(top->mem_axi4_0_aw_bits_burst);
    mem_ptr.awid    = &(top->mem_axi4_0_aw_bits_id);
    mem_ptr.awlen   = &(top->mem_axi4_0_aw_bits_len);
    mem_ptr.awready = &(top->mem_axi4_0_aw_ready);
    mem_ptr.awsize  = &(top->mem_axi4_0_aw_bits_size);
    mem_ptr.awvalid = &(top->mem_axi4_0_aw_valid);
    mem_ptr.wdata   = &(top->mem_axi4_0_w_bits_data);
    mem_ptr.wlast   = &(top->mem_axi4_0_w_bits_last);
    mem_ptr.wready  = &(top->mem_axi4_0_w_ready);
    mem_ptr.wstrb   = &(top->mem_axi4_0_w_bits_strb);
    mem_ptr.wvalid  = &(top->mem_axi4_0_w_valid);
    mem_ptr.bid     = &(top->mem_axi4_0_b_bits_id);
    mem_ptr.bready  = &(top->mem_axi4_0_b_ready);
    mem_ptr.bresp   = &(top->mem_axi4_0_b_bits_resp);
    mem_ptr.bvalid  = &(top->mem_axi4_0_b_valid);
    mem_ptr.araddr  = &(top->mem_axi4_0_ar_bits_addr);
    mem_ptr.arburst = &(top->mem_axi4_0_ar_bits_burst);
    mem_ptr.arid    = &(top->mem_axi4_0_ar_bits_id);
    mem_ptr.arlen   = &(top->mem_axi4_0_ar_bits_len);
    mem_ptr.arready = &(top->mem_axi4_0_ar_ready);
    mem_ptr.arsize  = &(top->mem_axi4_0_ar_bits_size);
    mem_ptr.arvalid = &(top->mem_axi4_0_ar_valid);
    mem_ptr.rdata   = &(top->mem_axi4_0_r_bits_data);
    mem_ptr.rid     = &(top->mem_axi4_0_r_bits_id);
    mem_ptr.rlast   = &(top->mem_axi4_0_r_bits_last);
    mem_ptr.rready  = &(top->mem_axi4_0_r_ready);
    mem_ptr.rresp   = &(top->mem_axi4_0_r_bits_resp);
    mem_ptr.rvalid  = &(top->mem_axi4_0_r_valid);
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -s <[host:]port>  TCP listen address (default: 0.0.0.0:2345)\n"
        "  -b <file>        Binary to load at 0x80000000 (local mode only)\n"
        "  -m <MB>          Memory size in MB (default: 256)\n"
        "  -M <local|qemu>  Memory mode (default: local)\n"
        "  -h               Show this help\n",
        prog);
}

int main(int argc, char **argv, char **env) {
    auto contextp = std::make_unique<VerilatedContext>();
    contextp->commandArgs(argc, argv);
    contextp->threads(1);

    const char *listen_addr = "0.0.0.0:2345";
    const char *binary_file = nullptr;
    size_t mem_size_mb = 256;
    uint32_t memory_mode = RTL_MEMMODE_LOCAL;

    int opt;
    while ((opt = getopt(argc, argv, "s:b:m:M:h")) != -1) {
        switch (opt) {
        case 's': listen_addr = optarg; break;
        case 'b': binary_file = optarg; break;
        case 'm': mem_size_mb = atoi(optarg); break;
        case 'M':
            if (strcmp(optarg, "qemu") == 0) {
                memory_mode = RTL_MEMMODE_QEMU;
            } else if (strcmp(optarg, "local") == 0) {
                memory_mode = RTL_MEMMODE_LOCAL;
            } else {
                fprintf(stderr, "Unknown memory mode: %s\n", optarg);
                usage(argv[0]);
                return 1;
            }
            break;
        case 'h':
        default:
            usage(argv[0]);
            return (opt == 'h') ? 0 : 1;
        }
    }

    signal(SIGINT, sigint_handler);

    /* Create the RTL CPU model */
    VExampleRocketSystem *top = new VExampleRocketSystem{contextp.get()};
    axi4_ptr<31,64,4> mmio_ptr;
    axi4_ptr<32,64,4> mem_ptr;
    axi4_ptr<32,64,8> l2_ptr;
    connect_wire(mmio_ptr, mem_ptr, top);
    connect_l2_wire(l2_ptr, top);
    assert(mmio_ptr.check());
    assert(mem_ptr.check());
    assert(l2_ptr.check());

    /* AXI signal intermediaries */
    axi4_ref<31,64,4>  mmio_ref(mmio_ptr);
    axi4    <31,64,4>  mmio_sigs;
    axi4_ref<31,64,4>  mmio_sigs_ref(mmio_sigs);
    axi4_xbar<31,64,4> mmio_xbar;

    axi4_ref<32,64,4>  mem_ref(mem_ptr);
    axi4    <32,64,4>  mem_sigs;
    axi4_ref<32,64,4>  mem_sigs_ref(mem_sigs);

    axi4_ref<32,64,8>  l2_ref(l2_ptr);
    axi4    <32,64,8>  l2_sigs;
    axi4_ref<32,64,8>  l2_sigs_ref(l2_sigs);

    /* Main memory setup depends on memory mode.
     *
     * LOCAL mode:  axi4_mem serves CPU's memory AXI port directly.
     *              QEMU uses MEM_READ/MEM_WRITE to pre-load firmware.
     * QEMU  mode:  CPU memory requests are forwarded to QEMU via
     *              CPU_MEM_READ/WRITE.  Memory lives in QEMU's address space.
     */
    static constexpr uint64_t DRAM_BASE = 0x80000000ULL;
    size_t dram_size = mem_size_mb * 1024UL * 1024;
    size_t total_mem = DRAM_BASE + dram_size;

    /* QEMU bridge (declared before memory setup because qemu_mem_bridge needs it) */
    qemu_bridge bridge;

    /* DMA master drives the L2 frontend bus for coherent DMA */
    axi4_master<32,64,8> dma_master;

    /* Wire DMA callbacks: recv_loop queues requests, master sends responses */
    bridge.on_dma_read = [&dma_master](uint64_t addr, uint32_t size,
                                        uint32_t req_id) {
        dma_master.queue_read({addr, size, req_id});
    };
    bridge.on_dma_write = [&dma_master](uint64_t addr, uint32_t size,
                                         uint32_t req_id,
                                         const uint8_t *data) {
        std::vector<uint8_t> v(data, data + size);
        dma_master.queue_write({addr, size, req_id, std::move(v)});
    };
    dma_master.on_write_complete = [&bridge](uint32_t req_id,
                                             uint32_t status) {
        bridge.send_dma_write_resp(req_id, status);
    };
    dma_master.on_read_complete = [&bridge](uint32_t req_id, uint32_t status,
                                            const uint8_t *data,
                                            uint32_t size) {
        bridge.send_dma_read_resp(req_id, status, data, size);
    };

    /* These are only used in local mode but declared here for scope */
    axi4_mem<32,64,4> *mem_local = nullptr;

    /* These are only used in QEMU mode */
    axi4_xbar<32,64,4> *mem_xbar = nullptr;
    qemu_mem_bridge *qemu_mem = nullptr;

    if (memory_mode == RTL_MEMMODE_LOCAL) {
        mem_local = new axi4_mem<32,64,4>(total_mem);
        if (binary_file) {
            mem_local->load_binary(binary_file, DRAM_BASE);
            fprintf(stderr, "Loaded binary: %s at 0x%lx\n", binary_file,
                    (unsigned long)DRAM_BASE);
        }
        bridge.set_system_memory(mem_local->get_mem_ptr() + DRAM_BASE,
                                 dram_size, DRAM_BASE);
    } else {
        /* QEMU memory mode: route CPU's memory AXI port to QEMU */
        mem_xbar = new axi4_xbar<32,64,4>();
        qemu_mem = new qemu_mem_bridge(&bridge, DRAM_BASE);
        mem_xbar->add_dev(DRAM_BASE, dram_size, qemu_mem);
        fprintf(stderr, "Memory mode: QEMU (DRAM 0x%lx - 0x%lx via QEMU)\n",
                (unsigned long)DRAM_BASE,
                (unsigned long)(DRAM_BASE + dram_size));
        if (binary_file) {
            fprintf(stderr, "Warning: -b ignored in QEMU memory mode "
                    "(QEMU loads firmware)\n");
        }
    }

    /* QEMU MMIO forwarder: forward CPU's MMIO range to QEMU.
     * Rocket-Chip's MMIO port maps to 0x60000000 - 0x7FFFFFFF.
     * QEMU will place UART, VirtIO, etc. in this range. */
    qemu_mmio_bridge qemu_mmio(&bridge, 0x60000000ULL);
    assert(mmio_xbar.add_dev(0x60000000ULL, 0x20000000ULL, &qemu_mmio));

    /* Connect to QEMU and send CPU capabilities */
    struct rtl_addr_range ranges[2];

    ranges[0].base = 0x80000000ULL;
    ranges[0].size = dram_size;
    ranges[0].type = 0; /* memory */
    ranges[0].flags = 0;

    ranges[1].base = 0x60000000ULL;
    ranges[1].size = 0x20000000ULL;
    ranges[1].type = 1; /* MMIO */
    ranges[1].flags = 0;

    /* Capability flags: L2 frontend bus is connected for coherent DMA */
    uint32_t cap_flags = RTL_CAP_COHERENT_DMA;

    if (!bridge.listen_and_accept(listen_addr,
                        1,             /* num_cores */
                        64,            /* xlen */
                        "rv64imafdc",  /* isa_string */
                        64,            /* num_irq_lines */
                        ranges, 2,
                        cap_flags,
                        memory_mode)) {
        fprintf(stderr, "Failed to listen/accept on %s\n", listen_addr);
        fprintf(stderr, "Make sure the port is available and QEMU will connect with:\n"
                "  qemu-system-riscv64 -machine rtl,rtl-sock=%s\n",
                listen_addr);
        delete top;
        return 1;
    }

    /* Wait for QEMU to send CPU_START (it may load firmware first) */
    fprintf(stderr, "Waiting for QEMU to start CPU...\n");
    while (!bridge.is_cpu_running() && running && bridge.is_connected()) {
        usleep(1000); /* 1ms poll */
    }

    if (!bridge.is_cpu_running()) {
        fprintf(stderr, "CPU not started, exiting.\n");
        bridge.shutdown();
        delete top;
        return 1;
    }

    /* Main simulation loop */
    fprintf(stderr, "CPU started, beginning simulation...\n");

    bool in_reset = true;
    rocket_set_reset(top, 1);
    uint64_t ticks = 0;
    const uint64_t sync_interval = 10000;

    /* Non-blocking DMI state machine */
    enum dmi_state_t { DMI_IDLE, DMI_REQ, DMI_WAIT_RESP };
    dmi_state_t dmi_state = DMI_IDLE;
    qemu_bridge::dmi_request dmi_cur{};
    int dmi_timeout = 0;

    /* Activate debug module at startup */
    top->debug_clockeddmi_dmi_req_valid = 0;
    top->debug_clockeddmi_dmi_req_bits_addr = 0;
    top->debug_clockeddmi_dmi_req_bits_data = 0;
    top->debug_clockeddmi_dmi_req_bits_op = 0;
    top->debug_clockeddmi_dmi_resp_ready = 1;
    top->debug_dmactiveAck = 0;

    l2_set_defaults(top);  /* L2 frontend driven by dma_master */

    while (!contextp->gotFinish() && running && bridge.is_connected()
           && bridge.is_cpu_running()) {
        top->eval();
        ticks++;

        if (ticks == 9) {
            in_reset = false;
            rocket_set_reset(top, 0);
            fprintf(stderr, "Reset deasserted at tick %lu\n",
                    (unsigned long)ticks);
        }


        rocket_set_clock(top, 1);

        /* Posedge: sample bus signals BEFORE eval */
        mmio_sigs.update_input(mmio_ref);
        mem_sigs.update_input(mem_ref);

        /* Snapshot L2 bus (both master-driven and slave-driven signals)
         * before eval, so the DMA master sees pre-edge signal values
         * for correct AXI handshake detection. */
        l2_sigs.update_input(l2_ref);    /* master signals: awvalid, wvalid, etc. */
        l2_sigs.awready = l2_ref.awready;
        l2_sigs.wready  = l2_ref.wready;
        l2_sigs.bvalid  = l2_ref.bvalid;
        l2_sigs.bid     = l2_ref.bid;
        l2_sigs.bresp   = l2_ref.bresp;
        l2_sigs.arready = l2_ref.arready;
        l2_sigs.rvalid  = l2_ref.rvalid;
        l2_sigs.rdata   = l2_ref.rdata;
        l2_sigs.rid     = l2_ref.rid;
        l2_sigs.rlast   = l2_ref.rlast;
        l2_sigs.rresp   = l2_ref.rresp;

        top->eval();

        if (!in_reset) {
            /* Process AXI transactions */
            if (memory_mode == RTL_MEMMODE_LOCAL) {
                mem_local->beat(mem_sigs_ref);
            } else {
                mem_xbar->beat(mem_sigs_ref);
            }
            mmio_xbar.beat(mmio_sigs_ref);

            /* Process DMA via L2 frontend bus (using snapshot) */
            dma_master.beat(l2_sigs_ref);

            /* Write back master-driven signals to Verilator */
            l2_ref.awvalid = l2_sigs.awvalid;
            l2_ref.awaddr  = l2_sigs.awaddr;
            l2_ref.awid    = l2_sigs.awid;
            l2_ref.awlen   = l2_sigs.awlen;
            l2_ref.awsize  = l2_sigs.awsize;
            l2_ref.awburst = l2_sigs.awburst;
            l2_ref.wvalid  = l2_sigs.wvalid;
            l2_ref.wdata   = l2_sigs.wdata;
            l2_ref.wstrb   = l2_sigs.wstrb;
            l2_ref.wlast   = l2_sigs.wlast;
            l2_ref.bready  = l2_sigs.bready;
            l2_ref.arvalid = l2_sigs.arvalid;
            l2_ref.araddr  = l2_sigs.araddr;
            l2_ref.arid    = l2_sigs.arid;
            l2_ref.arlen   = l2_sigs.arlen;
            l2_ref.arsize  = l2_sigs.arsize;
            l2_ref.arburst = l2_sigs.arburst;
            l2_ref.rready  = l2_sigs.rready;
        }

        mmio_sigs.update_output(mmio_ref);
        mem_sigs.update_output(mem_ref);

        top->interrupts = (uint8_t)(bridge.get_irq_level() & 0x3);
        top->debug_dmactiveAck = top->debug_dmactive;

        /* Re-evaluate after updating dmactiveAck and other inputs.
         * This is critical for the debug module's combinational paths
         * (e.g., resp_valid from async queue) to settle properly,
         * matching what RocketDMI::tick() does with its 3rd eval(). */
        top->eval();

        /* Non-blocking DMI state machine: process one step per clock */
        switch (dmi_state) {
        case DMI_IDLE:
            if (bridge.pop_dmi_request(dmi_cur)) {
                top->debug_clockeddmi_dmi_req_valid = 1;
                top->debug_clockeddmi_dmi_req_bits_addr = dmi_cur.addr;
                top->debug_clockeddmi_dmi_req_bits_data = dmi_cur.data;
                top->debug_clockeddmi_dmi_req_bits_op = dmi_cur.op;
                dmi_state = DMI_REQ;
                dmi_timeout = 5000;
            }
            break;
        case DMI_REQ:
            if (top->debug_clockeddmi_dmi_req_ready) {
                /* Handshake completed, deassert request */
                top->debug_clockeddmi_dmi_req_valid = 0;
                /* DMIToTL is purely combinational: the TL D channel response
                 * often arrives in the SAME cycle as the A channel handshake.
                 * Check resp_valid immediately to avoid missing it. */
                if (top->debug_clockeddmi_dmi_resp_valid) {
                    uint32_t rdata = top->debug_clockeddmi_dmi_resp_bits_data;
                    uint32_t rresp = top->debug_clockeddmi_dmi_resp_bits_resp;
                    bridge.send_dmi_response(dmi_cur.req_id, rdata, rresp);
                    dmi_state = DMI_IDLE;
                } else {
                    dmi_state = DMI_WAIT_RESP;
                    dmi_timeout = 50000;
                }
            } else if (--dmi_timeout <= 0) {
                fprintf(stderr, "DMI: req timeout (tick=%lu)\n",
                        (unsigned long)ticks);
                top->debug_clockeddmi_dmi_req_valid = 0;
                bridge.send_dmi_response(dmi_cur.req_id, 0, 1);
                dmi_state = DMI_IDLE;
            }
            break;
        case DMI_WAIT_RESP:
            if (top->debug_clockeddmi_dmi_resp_valid) {
                uint32_t rdata = top->debug_clockeddmi_dmi_resp_bits_data;
                uint32_t rresp = top->debug_clockeddmi_dmi_resp_bits_resp;
                bridge.send_dmi_response(dmi_cur.req_id, rdata, rresp);
                dmi_state = DMI_IDLE;
            } else if (--dmi_timeout <= 0) {
                fprintf(stderr, "DMI: resp timeout (tick=%lu)\n",
                        (unsigned long)ticks);
                bridge.send_dmi_response(dmi_cur.req_id, 0, 1);
                dmi_state = DMI_IDLE;
            }
            break;
        }

        rocket_set_clock(top, 0);

        /* Debug: print progress every 1M ticks */
        if ((ticks % 1000000) == 0) {
            fprintf(stderr, "tick=%lu\n", (unsigned long)ticks);
        }

        /* Periodic sync with QEMU */
        if ((ticks % sync_interval) == 0) {
            bridge.set_tick_count(ticks);
            bridge.sync(ticks);
        }
    }

    fprintf(stderr, "\nSimulation ended after %lu ticks.\n",
            (unsigned long)ticks);

    bridge.shutdown();
    top->final();
    delete top;
    delete mem_local;
    delete mem_xbar;
    delete qemu_mem;

    return 0;
}
