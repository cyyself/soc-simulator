/*
 * AXI4 Master: drives an AXI4 slave port for DMA transactions.
 *
 * Used to drive Rocket-Chip's L2 frontend bus from the soc-simulator side.
 * Receives DMA requests from a thread-safe queue (fed by qemu_bridge's
 * recv_loop) and converts them to AXI4 bus transactions.
 *
 * Each beat() call processes one clock cycle of the state machine.
 * The master works directly with axi4_ref pointing to Verilator signals:
 * it drives the master-side signals (AW, AR, W, bready, rready) and reads
 * the slave-side responses (awready, arready, wready, B, R channels).
 *
 * Usage:
 *   axi4_master<32,64,8> dma_master;
 *   dma_master.on_read_complete = [](uint32_t id, uint32_t st,
 *                                     const uint8_t *d, uint32_t sz) { ... };
 *   dma_master.on_write_complete = [](uint32_t id, uint32_t st) { ... };
 *   // In sim loop after top->eval():
 *   dma_master.beat(l2_ref);
 */

#ifndef AXI4_MASTER_HPP
#define AXI4_MASTER_HPP

#include "axi4.hpp"

#include <queue>
#include <mutex>
#include <functional>
#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <bit>

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64,
          unsigned int ID_WIDTH = 4>
class axi4_master {
public:
    static constexpr unsigned BUS_BYTES = D_WIDTH / 8;
    static constexpr unsigned SIZE_LOG2 = std::bit_width(BUS_BYTES) - 1;
    static constexpr unsigned AXI_MAX_BURST = 256;

    struct dma_read_request {
        uint64_t addr;
        uint32_t size;
        uint32_t req_id;
    };

    struct dma_write_request {
        uint64_t addr;
        uint32_t size;
        uint32_t req_id;
        std::vector<uint8_t> data;
    };

    /* Callbacks invoked from beat() when a DMA operation completes. */
    std::function<void(uint32_t req_id, uint32_t status,
                       const uint8_t *data, uint32_t size)> on_read_complete;
    std::function<void(uint32_t req_id, uint32_t status)> on_write_complete;

    axi4_master() = default;

    /* Thread-safe: queue a DMA read request (called from recv_loop). */
    void queue_read(dma_read_request req) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        read_queue_.push(req);
    }

    /* Thread-safe: queue a DMA write request (called from recv_loop). */
    void queue_write(dma_write_request req) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        write_queue_.push(std::move(req));
    }

    /* Call once per clock cycle from the main simulation thread. */
    void beat(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        /* Always ready to accept responses */
        pin.bready = 1;
        pin.rready = 1;

        switch (state_) {
        case State::IDLE:       do_idle(pin);       break;
        case State::WRITE_ADDR: do_write_addr(pin); break;
        case State::WRITE_DATA: do_write_data(pin); break;
        case State::WRITE_RESP: do_write_resp(pin); break;
        case State::READ_ADDR:  do_read_addr(pin);  break;
        case State::READ_DATA:  do_read_data(pin);  break;
        }
    }

private:
    enum class State {
        IDLE,
        WRITE_ADDR,   /* Asserting AW, waiting for awready */
        WRITE_DATA,   /* Sending W beats, waiting for wready each */
        WRITE_RESP,   /* Waiting for B response */
        READ_ADDR,    /* Asserting AR, waiting for arready */
        READ_DATA,    /* Waiting for R beats */
    };

    State state_ = State::IDLE;

    std::mutex queue_mutex_;
    std::queue<dma_read_request> read_queue_;
    std::queue<dma_write_request> write_queue_;

    /* Current DMA operation */
    uint64_t dma_addr_ = 0;
    uint32_t dma_size_ = 0;
    uint32_t dma_req_id_ = 0;
    uint32_t dma_offset_ = 0;          /* bytes transferred so far */
    std::vector<uint8_t> dma_data_;    /* write: source; read: accumulated */
    uint32_t dma_status_ = 0;

    /* Current AXI burst */
    uint64_t burst_addr_ = 0;
    uint32_t burst_data_bytes_ = 0;    /* actual payload bytes in this burst */
    uint32_t burst_first_lane_ = 0;    /* first-beat byte lane offset */
    uint8_t  burst_len_ = 0;           /* AXI len field (num_beats - 1) */
    uint8_t  burst_beat_ = 0;          /* current beat index within burst */

    uint8_t  next_id_ = 1;

    void deassert_requests(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        pin.awvalid = 0;
        pin.wvalid  = 0;
        pin.arvalid = 0;
    }

    static constexpr uint64_t align_up(uint64_t value, uint64_t align) {
        return (value + align - 1) & ~(align - 1);
    }

    void current_beat_layout(uint32_t &lane, uint32_t &count,
                             uint32_t &buf_offset) const {
        uint32_t first_count = std::min<uint32_t>(burst_data_bytes_,
                                                  BUS_BYTES - burst_first_lane_);

        if (burst_beat_ == 0) {
            lane = burst_first_lane_;
            count = first_count;
            buf_offset = 0;
            return;
        }

        buf_offset = first_count +
                     ((uint32_t)burst_beat_ - 1U) * BUS_BYTES;
        lane = 0;
        count = std::min<uint32_t>(burst_data_bytes_ - buf_offset,
                                   BUS_BYTES);
    }

    /*
     * Calculate next AXI burst parameters for the current DMA operation.
     * Respects 4KB boundary crossing and AXI max burst length.
     */
    void calc_next_burst() {
        uint64_t addr = dma_addr_ + dma_offset_;
        uint32_t remaining = dma_size_ - dma_offset_;

        /* Bytes until next 4KB boundary */
        uint64_t off_in_4k = addr & 0xFFFULL;
        uint32_t to_boundary = (uint32_t)(0x1000ULL - off_in_4k);
        if (off_in_4k == 0) to_boundary = 0x1000;

        burst_first_lane_ = (uint32_t)(addr % BUS_BYTES);

        uint32_t burst_bytes = std::min(remaining, to_boundary);
        burst_bytes = std::min<uint32_t>(
            burst_bytes,
            (uint32_t)(AXI_MAX_BURST * BUS_BYTES - burst_first_lane_));

        uint32_t num_beats =
            (burst_first_lane_ + burst_bytes + BUS_BYTES - 1) / BUS_BYTES;
        if (num_beats == 0) num_beats = 1;

        burst_addr_ = addr;
        burst_data_bytes_ = burst_bytes;
        burst_len_  = (uint8_t)(num_beats - 1);
        burst_beat_ = 0;
    }

    /* ------ IDLE state: check for queued DMA requests ------ */

    void do_idle(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        deassert_requests(pin);

        dma_write_request wreq;
        dma_read_request  rreq;
        bool have_write = false, have_read = false;

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (!write_queue_.empty()) {
                wreq = std::move(write_queue_.front());
                write_queue_.pop();
                have_write = true;
            } else if (!read_queue_.empty()) {
                rreq = read_queue_.front();
                read_queue_.pop();
                have_read = true;
            }
        }

        if (have_write) {
            dma_addr_   = wreq.addr;
            dma_size_   = wreq.size;
            dma_req_id_ = wreq.req_id;
            dma_offset_ = 0;
            dma_data_   = std::move(wreq.data);
            dma_status_ = 0;
            calc_next_burst();
            begin_write_addr(pin);
        } else if (have_read) {
            dma_addr_   = rreq.addr;
            dma_size_   = rreq.size;
            dma_req_id_ = rreq.req_id;
            dma_offset_ = 0;
            dma_data_.assign(rreq.size, 0);
            dma_status_ = 0;
            calc_next_burst();
            begin_read_addr(pin);
        }
    }

    /* ------ Write path ------ */

    void begin_write_addr(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        state_ = State::WRITE_ADDR;

        pin.awvalid = 1;
        pin.awaddr  = (decltype(pin.awaddr))burst_addr_;
        pin.awid    = next_id_;
        pin.awlen   = burst_len_;
        pin.awsize  = SIZE_LOG2;
        pin.awburst = BURST_INCR;

        pin.arvalid = 0;
        pin.wvalid  = 0;
    }

    void do_write_addr(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        if (pin.awvalid && pin.awready) {
            /* AW handshake complete, begin W beats */
            pin.awvalid = 0;
            state_ = State::WRITE_DATA;
            drive_write_beat(pin);
        }
    }

    void drive_write_beat(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        pin.wvalid = 1;

        uint8_t *wd = (uint8_t *)&pin.wdata;
        uint8_t *ws = (uint8_t *)&pin.wstrb;
        memset(wd, 0, BUS_BYTES);
        memset(ws, 0, (BUS_BYTES + 7) / 8);

        uint32_t lane, count, buf_offset;
        current_beat_layout(lane, count, buf_offset);

        for (uint32_t index = 0; index < count; index++) {
            uint32_t pos = dma_offset_ + buf_offset + index;
            uint32_t beat_lane = lane + index;
            if (pos < dma_size_ && pos < (uint32_t)dma_data_.size() &&
                beat_lane < BUS_BYTES) {
                wd[beat_lane] = dma_data_[pos];
                ws[beat_lane / 8] |= (uint8_t)(1 << (beat_lane % 8));
            }
        }

        pin.wlast = (burst_beat_ == burst_len_) ? 1 : 0;
    }

    void do_write_data(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        pin.awvalid = 0;

        if (pin.wvalid && pin.wready) {
            burst_beat_++;
            if (burst_beat_ > burst_len_) {
                /* All W beats sent, wait for B */
                pin.wvalid = 0;
                state_ = State::WRITE_RESP;
            } else {
                drive_write_beat(pin);
            }
        }
    }

    void do_write_resp(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        pin.awvalid = 0;
        pin.wvalid  = 0;

        if (pin.bvalid && pin.bready) {
            if (pin.bresp != RESP_OKEY) dma_status_ = 1;

            dma_offset_ += burst_data_bytes_;

            if (dma_offset_ < dma_size_) {
                /* More bursts needed */
                calc_next_burst();
                begin_write_addr(pin);
            } else {
                /* DMA write complete */
                if (on_write_complete)
                    on_write_complete(dma_req_id_, dma_status_);
                next_id_++;
                state_ = State::IDLE;
                deassert_requests(pin);
            }
        }
    }

    /* ------ Read path ------ */

    void begin_read_addr(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        state_ = State::READ_ADDR;

        pin.arvalid = 1;
        pin.araddr  = (decltype(pin.araddr))burst_addr_;
        pin.arid    = next_id_;
        pin.arlen   = burst_len_;
        pin.arsize  = SIZE_LOG2;
        pin.arburst = BURST_INCR;

        pin.awvalid = 0;
        pin.wvalid  = 0;
    }

    void do_read_addr(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        if (pin.arvalid && pin.arready) {
            pin.arvalid = 0;
            state_ = State::READ_DATA;
        }
    }

    void do_read_data(axi4_ref<A_WIDTH, D_WIDTH, ID_WIDTH> &pin) {
        pin.arvalid = 0;
        pin.awvalid = 0;
        pin.wvalid  = 0;

        if (pin.rvalid && pin.rready) {
            /* Store read data */
            const uint8_t *rd = (const uint8_t *)&pin.rdata;
            uint32_t lane, count, buf_offset;
            current_beat_layout(lane, count, buf_offset);
            for (uint32_t index = 0; index < count; index++) {
                uint32_t pos = dma_offset_ + buf_offset + index;
                uint32_t beat_lane = lane + index;
                if (pos < dma_size_ && beat_lane < BUS_BYTES) {
                    dma_data_[pos] = rd[beat_lane];
                }
            }
            burst_beat_++;

            if (pin.rlast) {
                if (pin.rresp != RESP_OKEY) dma_status_ = 1;

                dma_offset_ += burst_data_bytes_;

                if (dma_offset_ < dma_size_) {
                    /* More bursts needed */
                    calc_next_burst();
                    begin_read_addr(pin);
                } else {
                    /* DMA read complete */
                    if (on_read_complete)
                        on_read_complete(dma_req_id_, dma_status_,
                                         dma_data_.data(), dma_size_);
                    next_id_++;
                    state_ = State::IDLE;
                    deassert_requests(pin);
                }
            }
        }
    }
};

#endif /* AXI4_MASTER_HPP */
