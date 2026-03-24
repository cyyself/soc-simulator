/*
 * QEMU Bridge for soc-simulator
 *
 * This module provides two components:
 * 1. qemu_mmio_bridge - an mmio_dev that forwards MMIO reads/writes to QEMU
 * 2. qemu_bridge - manages the socket connection, DMA, and interrupts
 *
 * The bridge runs a background thread to handle incoming messages from QEMU
 * (DMA requests, interrupt updates) while the main simulation thread makes
 * synchronous MMIO calls.
 *
 * Connection model: soc-simulator listens on a TCP port, QEMU connects to it.
 */

#ifndef QEMU_BRIDGE_HPP
#define QEMU_BRIDGE_HPP

#include "mmio_dev.hpp"
#include "axi4_mem.hpp"
#include "rtl_protocol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <unordered_map>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

/*
 * Helper: reliable send/recv over socket
 */
static inline bool sock_send_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return false;
        }
        p += n;
        len -= n;
    }
    return true;
}

static inline bool sock_recv_all(int fd, void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return false;
        }
        p += n;
        len -= n;
    }
    return true;
}

/*
 * qemu_mmio_bridge: An mmio_dev that forwards reads/writes to QEMU.
 *
 * This is registered in the AXI crossbar for the MMIO address range
 * that should be handled by QEMU peripherals.
 */
class qemu_bridge;

class qemu_mmio_bridge : public mmio_dev {
public:
    qemu_mmio_bridge(qemu_bridge *bridge, uint64_t base_addr)
        : bridge_(bridge), base_addr_(base_addr) {}

    bool do_read(uint64_t start_addr, uint64_t size, char *buffer) override;
    bool do_write(uint64_t start_addr, uint64_t size, const char *buffer) override;

private:
    qemu_bridge *bridge_;
    uint64_t base_addr_;
};

/*
 * qemu_mem_bridge: An mmio_dev that forwards memory reads/writes to QEMU.
 *
 * Used in RTL_MEMMODE_QEMU mode: registered in the AXI crossbar for
 * the DRAM address range so CPU AXI memory transactions are forwarded
 * to QEMU's memory via CPU_MEM_READ / CPU_MEM_WRITE messages.
 */
class qemu_mem_bridge : public mmio_dev {
public:
    qemu_mem_bridge(qemu_bridge *bridge, uint64_t base_addr)
        : bridge_(bridge), base_addr_(base_addr) {}

    bool do_read(uint64_t start_addr, uint64_t size, char *buffer) override;
    bool do_write(uint64_t start_addr, uint64_t size, const char *buffer) override;

private:
    qemu_bridge *bridge_;
    uint64_t base_addr_;
};

/*
 * qemu_bridge: Main bridge managing socket communication with QEMU.
 */
class qemu_bridge {
public:
    qemu_bridge() : listen_fd_(-1), sock_fd_(-1), connected_(false),
                    running_(false), cpu_running_(false), irq_level_(0),
                    req_id_counter_(0), tick_count_(0),
                    mem_ptr_(nullptr), mem_size_(0),
                    mem_base_addr_(0x80000000ULL),
                    memory_mode_(RTL_MEMMODE_LOCAL) {}

    ~qemu_bridge() {
        shutdown();
    }

    /*
     * Listen on a TCP port and wait for QEMU to connect, then send HELLO.
     * listen_addr is "host:port" or just "port".
     * Returns true on success.
     */
    bool listen_and_accept(const char *listen_addr,
                           uint32_t num_cores,
                           uint32_t xlen,
                           const char *isa_string,
                           uint32_t num_irq_lines,
                           const struct rtl_addr_range *addr_ranges,
                           uint32_t num_addr_ranges,
                           uint32_t flags = 0,
                           uint32_t memory_mode = RTL_MEMMODE_LOCAL) {
        /* Parse host:port */
        std::string addr_str(listen_addr);
        std::string host = "0.0.0.0";
        uint16_t port = 0;
        auto colon = addr_str.rfind(':');
        if (colon != std::string::npos) {
            host = addr_str.substr(0, colon);
            port = (uint16_t)atoi(addr_str.substr(colon + 1).c_str());
        } else {
            port = (uint16_t)atoi(addr_str.c_str());
        }

        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            perror("qemu_bridge: socket() failed");
            return false;
        }

        int optval = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &saddr.sin_addr) <= 0) {
            fprintf(stderr, "qemu_bridge: invalid address '%s'\n", host.c_str());
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        if (bind(listen_fd_, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
            perror("qemu_bridge: bind() failed");
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        if (listen(listen_fd_, 1) < 0) {
            perror("qemu_bridge: listen() failed");
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        fprintf(stderr, "qemu_bridge: listening on %s:%u, waiting for QEMU...\n",
                host.c_str(), port);

        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);
        sock_fd_ = accept(listen_fd_, (struct sockaddr *)&peer_addr, &peer_len);
        if (sock_fd_ < 0) {
            perror("qemu_bridge: accept() failed");
            close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        /* Disable Nagle's algorithm for low latency */
        int flag = 1;
        setsockopt(sock_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        char peer_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_str, sizeof(peer_str));
        fprintf(stderr, "qemu_bridge: QEMU connected from %s:%u\n",
                peer_str, ntohs(peer_addr.sin_port));

        /* Send HELLO */
        struct rtl_msg_hello hello;
        memset(&hello, 0, sizeof(hello));
        hello.hdr.type = RTL_MSG_HELLO;
        hello.hdr.length = sizeof(hello);
        hello.magic = RTL_PROTOCOL_MAGIC;
        hello.version = RTL_PROTOCOL_VERSION;
        hello.num_cores = num_cores;
        hello.xlen = xlen;
        hello.num_irq_lines = num_irq_lines;
        strncpy(hello.isa_string, isa_string, sizeof(hello.isa_string) - 1);
        hello.num_addr_ranges = num_addr_ranges;
        hello.flags = flags;
        hello.memory_mode = memory_mode;
        if (num_addr_ranges > RTL_MAX_ADDR_RANGES)
            num_addr_ranges = RTL_MAX_ADDR_RANGES;
        memcpy(hello.addr_ranges, addr_ranges,
               num_addr_ranges * sizeof(struct rtl_addr_range));

        if (!sock_send_all(sock_fd_, &hello, sizeof(hello))) {
            fprintf(stderr, "qemu_bridge: failed to send HELLO\n");
            close(sock_fd_);
            sock_fd_ = -1;
            return false;
        }

        /* Wait for HELLO_ACK */
        struct rtl_msg_hello_ack ack;
        if (!sock_recv_all(sock_fd_, &ack, sizeof(ack))) {
            fprintf(stderr, "qemu_bridge: failed to receive HELLO_ACK\n");
            close(sock_fd_);
            sock_fd_ = -1;
            return false;
        }

        if (ack.hdr.type != RTL_MSG_HELLO_ACK || ack.status != 0) {
            fprintf(stderr, "qemu_bridge: HELLO_ACK failed (status=%u)\n",
                    ack.status);
            close(sock_fd_);
            sock_fd_ = -1;
            return false;
        }

        memory_mode_ = ack.memory_mode;
        fprintf(stderr, "qemu_bridge: memory_mode=%s\n",
                memory_mode_ == RTL_MEMMODE_QEMU ? "qemu" : "local");

        connected_ = true;
        running_ = true;

        /* Start background thread for incoming QEMU messages */
        recv_thread_ = std::thread(&qemu_bridge::recv_loop, this);

        fprintf(stderr, "qemu_bridge: handshake complete.\n");
        return true;
    }

    /*
     * Perform an MMIO read via QEMU.
     * Called from the simulation thread (via qemu_mmio_bridge).
     */
    bool mmio_read(uint64_t addr, uint32_t size, uint64_t *data) {
        if (!connected_) return false;

        struct rtl_msg_mmio_read req;
        memset(&req, 0, sizeof(req));
        req.hdr.type = RTL_MSG_MMIO_READ;
        req.hdr.length = sizeof(req);
        req.addr = addr;
        req.size = size;
        req.req_id = alloc_req_id();

        /* Register pending response */
        {
            std::unique_lock<std::mutex> lock(resp_mutex_);
            pending_mmio_read_[req.req_id] = {};
        }

        {
            std::unique_lock<std::mutex> lock(send_mutex_);
            if (!sock_send_all(sock_fd_, &req, sizeof(req))) {
                fprintf(stderr, "qemu_bridge: mmio_read send failed\n");
                return false;
            }
        }

        /* Wait for response */
        struct rtl_msg_mmio_read_resp resp;
        {
            std::unique_lock<std::mutex> lock(resp_mutex_);
            resp_cv_.wait(lock, [&]() {
                return pending_mmio_read_.count(req.req_id) &&
                       pending_mmio_read_[req.req_id].ready;
            });
            resp = pending_mmio_read_[req.req_id].resp;
            pending_mmio_read_.erase(req.req_id);
        }

        *data = resp.data;
        return (resp.status == 0);
    }

    /*
     * Perform an MMIO write via QEMU.
     * Called from the simulation thread (via qemu_mmio_bridge).
     */
    bool mmio_write(uint64_t addr, uint32_t size, uint64_t data) {
        if (!connected_) return false;

        struct rtl_msg_mmio_write req;
        memset(&req, 0, sizeof(req));
        req.hdr.type = RTL_MSG_MMIO_WRITE;
        req.hdr.length = sizeof(req);
        req.addr = addr;
        req.data = data;
        req.size = size;
        req.req_id = alloc_req_id();

        /* Register pending response */
        {
            std::unique_lock<std::mutex> lock(resp_mutex_);
            pending_mmio_write_[req.req_id] = {};
        }

        {
            std::unique_lock<std::mutex> lock(send_mutex_);
            if (!sock_send_all(sock_fd_, &req, sizeof(req))) {
                fprintf(stderr, "qemu_bridge: mmio_write send failed\n");
                return false;
            }
        }

        /* Wait for response */
        struct rtl_msg_mmio_write_resp resp;
        {
            std::unique_lock<std::mutex> lock(resp_mutex_);
            resp_cv_.wait(lock, [&]() {
                return pending_mmio_write_.count(req.req_id) &&
                       pending_mmio_write_[req.req_id].ready;
            });
            resp = pending_mmio_write_[req.req_id].resp;
            pending_mmio_write_.erase(req.req_id);
        }

        return (resp.status == 0);
    }

    /*
     * Callbacks for DMA requests from QEMU.
     * Set these before calling listen_and_accept().
     * Called from the recv_loop background thread.
     */
    std::function<void(uint64_t addr, uint32_t size, uint32_t req_id)>
        on_dma_read;
    std::function<void(uint64_t addr, uint32_t size, uint32_t req_id,
                       const uint8_t *data)>
        on_dma_write;

    /*
     * DMI (Debug Module Interface) request queue.
     * QEMU sends DEBUG_DMI_REQ messages; the main simulation loop
     * picks them up and drives the DMI port on the Rocket-Chip.
     */
    struct dmi_request {
        uint32_t req_id;
        uint8_t  addr;
        uint8_t  op;     /* 1=read, 2=write */
        uint32_t data;
    };

    bool pop_dmi_request(dmi_request &req) {
        std::unique_lock<std::mutex> lock(dmi_mutex_);
        if (dmi_queue_.empty()) return false;
        req = dmi_queue_.front();
        dmi_queue_.pop_front();
        return true;
    }

    void send_dmi_response(uint32_t req_id, uint32_t data, uint32_t status) {
        if (!connected_) return;
        struct rtl_msg_debug_dmi_resp resp;
        memset(&resp, 0, sizeof(resp));
        resp.hdr.type = RTL_MSG_DEBUG_DMI_RESP;
        resp.hdr.length = sizeof(resp);
        resp.req_id = req_id;
        resp.data = data;
        resp.status = status;
        std::unique_lock<std::mutex> lock(send_mutex_);
        sock_send_all(sock_fd_, &resp, sizeof(resp));
    }

    /*
     * Send DMA read response to QEMU (called from main sim thread
     * via axi4_master completion callback).
     */
    void send_dma_read_resp(uint32_t req_id, uint32_t status,
                            const uint8_t *data, uint32_t size) {
        if (!connected_) return;
        uint32_t msg_size = (uint32_t)(sizeof(struct rtl_msg_dma_read_resp) + size);
        std::vector<uint8_t> buf(msg_size, 0);
        struct rtl_msg_dma_read_resp *resp =
            (struct rtl_msg_dma_read_resp *)buf.data();
        resp->hdr.type = RTL_MSG_DMA_READ_RESP;
        resp->hdr.length = msg_size;
        resp->req_id = req_id;
        resp->status = status;
        if (data && size > 0)
            memcpy(resp->data, data, size);
        std::unique_lock<std::mutex> lock(send_mutex_);
        sock_send_all(sock_fd_, buf.data(), msg_size);
    }

    /*
     * Send DMA write response to QEMU.
     */
    void send_dma_write_resp(uint32_t req_id, uint32_t status) {
        if (!connected_) return;
        struct rtl_msg_dma_write_resp resp;
        memset(&resp, 0, sizeof(resp));
        resp.hdr.type = RTL_MSG_DMA_WRITE_RESP;
        resp.hdr.length = sizeof(resp);
        resp.req_id = req_id;
        resp.status = status;
        std::unique_lock<std::mutex> lock(send_mutex_);
        sock_send_all(sock_fd_, &resp, sizeof(resp));
    }

    /*
     * Get current interrupt level bitmask from QEMU.
     */
    uint64_t get_irq_level() const {
        return irq_level_.load(std::memory_order_relaxed);
    }

    /*
     * Check if CPU has been started by QEMU.
     */
    bool is_cpu_running() const {
        return cpu_running_.load(std::memory_order_acquire);
    }

    /*
     * Get current tick count.
     */
    uint64_t get_tick_count() const {
        return tick_count_;
    }

    /*
     * Set tick count (called by simulation loop).
     */
    void set_tick_count(uint64_t tick) {
        tick_count_ = tick;
    }

    /*
     * Send sync message to QEMU (call periodically from sim loop).
     */
    void sync(uint64_t tick) {
        tick_count_ = tick;
        if (!connected_) return;

        struct rtl_msg_sync msg;
        memset(&msg, 0, sizeof(msg));
        msg.hdr.type = RTL_MSG_SYNC;
        msg.hdr.length = sizeof(msg);
        msg.tick_count = tick;

        std::unique_lock<std::mutex> lock(send_mutex_);
        sock_send_all(sock_fd_, &msg, sizeof(msg));
    }

    /*
     * Set the system memory backing store for pre-boot memory access.
     * QEMU will use MEM_READ/MEM_WRITE to load firmware before CPU starts.
     * This is safe because the CPU is not running (no cache to bypass).
     */
    void set_system_memory(char *mem, size_t size, uint64_t base_addr) {
        mem_ptr_ = mem;
        mem_size_ = size;
        mem_base_addr_ = base_addr;
    }

    /*
     * Get the negotiated memory mode.
     */
    uint32_t get_memory_mode() const { return memory_mode_; }

    /*
     * CPU memory read via QEMU (RTL_MEMMODE_QEMU only).
     * Called from the simulation thread (via qemu_mem_bridge).
     */
    bool cpu_mem_read(uint64_t addr, uint32_t size, void *buffer) {
        if (!connected_) return false;

        struct rtl_msg_cpu_mem_read req;
        memset(&req, 0, sizeof(req));
        req.hdr.type = RTL_MSG_CPU_MEM_READ;
        req.hdr.length = sizeof(req);
        req.addr = addr;
        req.size = size;
        req.req_id = alloc_req_id();

        {
            std::unique_lock<std::mutex> lock(resp_mutex_);
            pending_cpu_mem_read_[req.req_id] = {};
        }

        {
            std::unique_lock<std::mutex> lock(send_mutex_);
            if (!sock_send_all(sock_fd_, &req, sizeof(req))) {
                fprintf(stderr, "qemu_bridge: cpu_mem_read send failed\n");
                return false;
            }
        }

        /* Wait for response */
        {
            std::unique_lock<std::mutex> lock(resp_mutex_);
            resp_cv_.wait(lock, [&]() {
                return pending_cpu_mem_read_.count(req.req_id) &&
                       pending_cpu_mem_read_[req.req_id].ready;
            });
            auto &p = pending_cpu_mem_read_[req.req_id];
            if (p.status == 0 && p.data.size() >= size) {
                memcpy(buffer, p.data.data(), size);
            }
            bool ok = (p.status == 0);
            pending_cpu_mem_read_.erase(req.req_id);
            return ok;
        }
    }

    /*
     * CPU memory write via QEMU (RTL_MEMMODE_QEMU only).
     * Called from the simulation thread (via qemu_mem_bridge).
     */
    bool cpu_mem_write(uint64_t addr, uint32_t size, const void *buffer) {
        if (!connected_) return false;

        uint32_t msg_size = rtl_cpu_mem_write_size(size);
        std::vector<uint8_t> msg_buf(msg_size, 0);
        struct rtl_msg_cpu_mem_write *req =
            (struct rtl_msg_cpu_mem_write *)msg_buf.data();

        req->hdr.type = RTL_MSG_CPU_MEM_WRITE;
        req->hdr.length = msg_size;
        req->addr = addr;
        req->size = size;
        req->req_id = alloc_req_id();
        memcpy(req->data, buffer, size);

        {
            std::unique_lock<std::mutex> lock(resp_mutex_);
            pending_cpu_mem_write_[req->req_id] = {};
        }

        {
            std::unique_lock<std::mutex> lock(send_mutex_);
            if (!sock_send_all(sock_fd_, msg_buf.data(), msg_size)) {
                fprintf(stderr, "qemu_bridge: cpu_mem_write send failed\n");
                return false;
            }
        }

        /* Wait for response */
        {
            std::unique_lock<std::mutex> lock(resp_mutex_);
            resp_cv_.wait(lock, [&]() {
                return pending_cpu_mem_write_.count(req->req_id) &&
                       pending_cpu_mem_write_[req->req_id].ready;
            });
            bool ok = (pending_cpu_mem_write_[req->req_id].status == 0);
            pending_cpu_mem_write_.erase(req->req_id);
            return ok;
        }
    }

    /*
     * Shutdown the bridge.
     */
    void shutdown() {
        if (!running_) return;
        running_ = false;

        if (connected_ && sock_fd_ >= 0) {
            struct rtl_msg_shutdown msg;
            memset(&msg, 0, sizeof(msg));
            msg.hdr.type = RTL_MSG_SHUTDOWN;
            msg.hdr.length = sizeof(msg);
            msg.reason = 0;
            std::unique_lock<std::mutex> lock(send_mutex_);
            sock_send_all(sock_fd_, &msg, sizeof(msg));
        }

        if (sock_fd_ >= 0) {
            ::shutdown(sock_fd_, SHUT_RDWR);
            close(sock_fd_);
            sock_fd_ = -1;
        }

        if (listen_fd_ >= 0) {
            close(listen_fd_);
            listen_fd_ = -1;
        }

        connected_ = false;

        /* Wake up any waiters */
        resp_cv_.notify_all();

        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }
    }

    bool is_connected() const { return connected_; }

private:
    int listen_fd_;
    int sock_fd_;
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    std::atomic<bool> cpu_running_;
    std::atomic<uint64_t> irq_level_;
    std::atomic<uint32_t> req_id_counter_;
    uint64_t tick_count_;

    /* System memory backing store (for pre-boot MEM_READ/MEM_WRITE) */
    char *mem_ptr_;
    size_t mem_size_;
    uint64_t mem_base_addr_;

    /* Negotiated memory mode */
    uint32_t memory_mode_;

    /* Thread management */
    std::thread recv_thread_;
    std::mutex send_mutex_;
    std::mutex resp_mutex_;
    std::condition_variable resp_cv_;

    /* DMI request queue (filled by recv_loop, consumed by main sim loop) */
    std::mutex dmi_mutex_;
    std::deque<dmi_request> dmi_queue_;

    /* Pending response tracking */
    struct mmio_read_pending {
        bool ready = false;
        struct rtl_msg_mmio_read_resp resp;
    };
    struct mmio_write_pending {
        bool ready = false;
        struct rtl_msg_mmio_write_resp resp;
    };
    struct cpu_mem_read_pending {
        bool ready = false;
        uint32_t status = 0;
        std::vector<uint8_t> data;
    };
    struct cpu_mem_write_pending {
        bool ready = false;
        uint32_t status = 0;
    };

    std::unordered_map<uint32_t, mmio_read_pending> pending_mmio_read_;
    std::unordered_map<uint32_t, mmio_write_pending> pending_mmio_write_;
    std::unordered_map<uint32_t, cpu_mem_read_pending> pending_cpu_mem_read_;
    std::unordered_map<uint32_t, cpu_mem_write_pending> pending_cpu_mem_write_;

    uint32_t alloc_req_id() {
        return req_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }

    /*
     * Background receive loop: handles incoming messages from QEMU.
     */
    void recv_loop() {
        while (running_) {
            struct rtl_msg_header hdr;
            if (!sock_recv_all(sock_fd_, &hdr, sizeof(hdr))) {
                if (running_) {
                    fprintf(stderr, "qemu_bridge: connection lost\n");
                    connected_ = false;
                    resp_cv_.notify_all();
                }
                return;
            }

            switch (hdr.type) {
            case RTL_MSG_MMIO_READ_RESP:
                handle_mmio_read_resp(hdr);
                break;
            case RTL_MSG_MMIO_WRITE_RESP:
                handle_mmio_write_resp(hdr);
                break;
            case RTL_MSG_MEM_READ:
                handle_mem_read(hdr);
                break;
            case RTL_MSG_MEM_WRITE:
                handle_mem_write(hdr);
                break;
            case RTL_MSG_CPU_START:
                handle_cpu_start(hdr);
                break;
            case RTL_MSG_CPU_STOP:
                handle_cpu_stop(hdr);
                break;
            case RTL_MSG_CPU_STATUS:
                handle_cpu_status(hdr);
                break;
            case RTL_MSG_DMA_READ:
                handle_dma_read(hdr);
                break;
            case RTL_MSG_DMA_WRITE:
                handle_dma_write(hdr);
                break;
            case RTL_MSG_IRQ_UPDATE:
                handle_irq_update(hdr);
                break;
            case RTL_MSG_SYNC_ACK:
                handle_sync_ack(hdr);
                break;
            case RTL_MSG_CPU_MEM_READ_RESP:
                handle_cpu_mem_read_resp(hdr);
                break;
            case RTL_MSG_CPU_MEM_WRITE_RESP:
                handle_cpu_mem_write_resp(hdr);
                break;
            case RTL_MSG_DEBUG_DMI_REQ:
                handle_debug_dmi_req(hdr);
                break;
            case RTL_MSG_SHUTDOWN:
                fprintf(stderr, "qemu_bridge: received shutdown\n");
                running_ = false;
                connected_ = false;
                resp_cv_.notify_all();
                return;
            default:
                fprintf(stderr, "qemu_bridge: unknown message type 0x%x\n",
                        hdr.type);
                /* Skip remaining bytes */
                if (hdr.length > sizeof(hdr)) {
                    size_t remaining = hdr.length - sizeof(hdr);
                    char skip[256];
                    while (remaining > 0) {
                        size_t chunk = remaining < sizeof(skip) ?
                                       remaining : sizeof(skip);
                        if (!sock_recv_all(sock_fd_, skip, chunk)) return;
                        remaining -= chunk;
                    }
                }
                break;
            }
        }
    }

    void handle_mmio_read_resp(const struct rtl_msg_header &hdr) {
        struct rtl_msg_mmio_read_resp resp;
        resp.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&resp) + sizeof(hdr),
                           sizeof(resp) - sizeof(hdr))) {
            return;
        }

        std::unique_lock<std::mutex> lock(resp_mutex_);
        if (pending_mmio_read_.count(resp.req_id)) {
            pending_mmio_read_[resp.req_id].resp = resp;
            pending_mmio_read_[resp.req_id].ready = true;
            resp_cv_.notify_all();
        }
    }

    void handle_mmio_write_resp(const struct rtl_msg_header &hdr) {
        struct rtl_msg_mmio_write_resp resp;
        resp.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&resp) + sizeof(hdr),
                           sizeof(resp) - sizeof(hdr))) {
            return;
        }

        std::unique_lock<std::mutex> lock(resp_mutex_);
        if (pending_mmio_write_.count(resp.req_id)) {
            pending_mmio_write_[resp.req_id].resp = resp;
            pending_mmio_write_[resp.req_id].ready = true;
            resp_cv_.notify_all();
        }
    }

    void handle_mem_read(const struct rtl_msg_header &hdr) {
        struct rtl_msg_mem_read req;
        req.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&req) + sizeof(hdr),
                           sizeof(req) - sizeof(hdr))) {
            return;
        }

        uint32_t resp_size = rtl_mem_read_resp_size(req.size);
        std::vector<uint8_t> resp_buf(resp_size, 0);
        struct rtl_msg_mem_read_resp *resp =
            (struct rtl_msg_mem_read_resp *)resp_buf.data();

        resp->hdr.type = RTL_MSG_MEM_READ_RESP;
        resp->hdr.length = resp_size;
        resp->req_id = req.req_id;

        uint64_t offset = req.addr - mem_base_addr_;
        if (mem_ptr_ && offset + req.size <= mem_size_) {
            memcpy(resp->data, mem_ptr_ + offset, req.size);
            resp->status = 0;
        } else {
            resp->status = 1;
            fprintf(stderr, "qemu_bridge: MEM_READ out of range: "
                    "addr=0x%lx size=%u\n",
                    (unsigned long)req.addr, req.size);
        }

        std::unique_lock<std::mutex> lock(send_mutex_);
        sock_send_all(sock_fd_, resp_buf.data(), resp_size);
    }

    void handle_mem_write(const struct rtl_msg_header &hdr) {
        struct rtl_msg_mem_write req_hdr;
        req_hdr.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&req_hdr) + sizeof(hdr),
                           sizeof(req_hdr) - sizeof(hdr))) {
            return;
        }

        std::vector<uint8_t> data(req_hdr.size);
        if (req_hdr.size > 0 &&
            !sock_recv_all(sock_fd_, data.data(), req_hdr.size)) {
            return;
        }

        struct rtl_msg_mem_write_resp resp;
        memset(&resp, 0, sizeof(resp));
        resp.hdr.type = RTL_MSG_MEM_WRITE_RESP;
        resp.hdr.length = sizeof(resp);
        resp.req_id = req_hdr.req_id;

        uint64_t offset = req_hdr.addr - mem_base_addr_;
        if (mem_ptr_ && offset + req_hdr.size <= mem_size_) {
            memcpy(mem_ptr_ + offset, data.data(), req_hdr.size);
            resp.status = 0;
        } else {
            resp.status = 1;
            fprintf(stderr, "qemu_bridge: MEM_WRITE out of range: "
                    "addr=0x%lx size=%u\n",
                    (unsigned long)req_hdr.addr, req_hdr.size);
        }

        std::unique_lock<std::mutex> lock(send_mutex_);
        sock_send_all(sock_fd_, &resp, sizeof(resp));
    }

    void handle_cpu_start(const struct rtl_msg_header &hdr) {
        struct rtl_msg_cpu_start req;
        req.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&req) + sizeof(hdr),
                           sizeof(req) - sizeof(hdr))) {
            return;
        }

        cpu_running_.store(true, std::memory_order_release);
        fprintf(stderr, "qemu_bridge: CPU_START received (addr=0x%lx)\n",
                (unsigned long)req.start_addr);

        struct rtl_msg_cpu_start_ack ack;
        memset(&ack, 0, sizeof(ack));
        ack.hdr.type = RTL_MSG_CPU_START_ACK;
        ack.hdr.length = sizeof(ack);
        ack.status = 0;

        std::unique_lock<std::mutex> lock(send_mutex_);
        sock_send_all(sock_fd_, &ack, sizeof(ack));
    }

    void handle_cpu_stop(const struct rtl_msg_header &hdr) {
        struct rtl_msg_cpu_stop req;
        req.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&req) + sizeof(hdr),
                           sizeof(req) - sizeof(hdr))) {
            return;
        }

        cpu_running_.store(false, std::memory_order_release);
        fprintf(stderr, "qemu_bridge: CPU_STOP received (reason=%u)\n",
                req.reason);

        struct rtl_msg_cpu_stop_ack ack;
        memset(&ack, 0, sizeof(ack));
        ack.hdr.type = RTL_MSG_CPU_STOP_ACK;
        ack.hdr.length = sizeof(ack);
        ack.status = 0;

        std::unique_lock<std::mutex> lock(send_mutex_);
        sock_send_all(sock_fd_, &ack, sizeof(ack));
    }

    void handle_cpu_status(const struct rtl_msg_header &hdr) {
        /* CPU_STATUS has no body beyond the header */
        (void)hdr;

        struct rtl_msg_cpu_status_resp resp;
        memset(&resp, 0, sizeof(resp));
        resp.hdr.type = RTL_MSG_CPU_STATUS_RESP;
        resp.hdr.length = sizeof(resp);
        resp.status = cpu_running_.load(std::memory_order_relaxed)
                      ? RTL_CPU_STATUS_RUNNING
                      : RTL_CPU_STATUS_STOPPED;
        resp.tick_count = tick_count_;

        std::unique_lock<std::mutex> lock(send_mutex_);
        sock_send_all(sock_fd_, &resp, sizeof(resp));
    }

    void handle_dma_read(const struct rtl_msg_header &hdr) {
        struct rtl_msg_dma_read req;
        req.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&req) + sizeof(hdr),
                           sizeof(req) - sizeof(hdr))) {
            return;
        }
        fprintf(stderr, "qemu_bridge: DMA_READ addr=0x%lx size=%u\n",
                (unsigned long)req.addr, req.size);
        if (on_dma_read) {
            on_dma_read(req.addr, req.size, req.req_id);
        } else {
            /* No handler — send error response */
            send_dma_read_resp(req.req_id, 1, nullptr, 0);
        }
    }

    void handle_dma_write(const struct rtl_msg_header &hdr) {
        struct rtl_msg_dma_write req_hdr;
        req_hdr.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&req_hdr) + sizeof(hdr),
                           sizeof(req_hdr) - sizeof(hdr))) {
            return;
        }
        std::vector<uint8_t> data(req_hdr.size);
        if (req_hdr.size > 0 &&
            !sock_recv_all(sock_fd_, data.data(), req_hdr.size)) {
            return;
        }
        fprintf(stderr, "qemu_bridge: DMA_WRITE addr=0x%lx size=%u\n",
                (unsigned long)req_hdr.addr, req_hdr.size);
        if (on_dma_write) {
            on_dma_write(req_hdr.addr, req_hdr.size, req_hdr.req_id,
                         data.data());
        } else {
            /* No handler — send error response */
            send_dma_write_resp(req_hdr.req_id, 1);
        }
    }

    void handle_irq_update(const struct rtl_msg_header &hdr) {
        struct rtl_msg_irq_update msg;
        msg.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&msg) + sizeof(hdr),
                           sizeof(msg) - sizeof(hdr))) {
            return;
        }
        irq_level_.store(msg.irq_levels, std::memory_order_relaxed);
    }

    void handle_sync_ack(const struct rtl_msg_header &hdr) {
        struct rtl_msg_sync_ack msg;
        msg.hdr = hdr;
        sock_recv_all(sock_fd_, ((uint8_t *)&msg) + sizeof(hdr),
                      sizeof(msg) - sizeof(hdr));
    }

    void handle_cpu_mem_read_resp(const struct rtl_msg_header &hdr) {
        struct rtl_msg_cpu_mem_read_resp resp_hdr;
        resp_hdr.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&resp_hdr) + sizeof(hdr),
                           sizeof(resp_hdr) - sizeof(hdr))) {
            return;
        }

        /* Read variable-length data */
        uint32_t data_size = hdr.length - (uint32_t)sizeof(resp_hdr);
        std::vector<uint8_t> data(data_size);
        if (data_size > 0 && !sock_recv_all(sock_fd_, data.data(), data_size)) {
            return;
        }

        std::unique_lock<std::mutex> lock(resp_mutex_);
        if (pending_cpu_mem_read_.count(resp_hdr.req_id)) {
            pending_cpu_mem_read_[resp_hdr.req_id].status = resp_hdr.status;
            pending_cpu_mem_read_[resp_hdr.req_id].data = std::move(data);
            pending_cpu_mem_read_[resp_hdr.req_id].ready = true;
            resp_cv_.notify_all();
        }
    }

    void handle_cpu_mem_write_resp(const struct rtl_msg_header &hdr) {
        struct rtl_msg_cpu_mem_write_resp resp;
        resp.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&resp) + sizeof(hdr),
                           sizeof(resp) - sizeof(hdr))) {
            return;
        }

        std::unique_lock<std::mutex> lock(resp_mutex_);
        if (pending_cpu_mem_write_.count(resp.req_id)) {
            pending_cpu_mem_write_[resp.req_id].status = resp.status;
            pending_cpu_mem_write_[resp.req_id].ready = true;
            resp_cv_.notify_all();
        }
    }

    void handle_debug_dmi_req(const struct rtl_msg_header &hdr) {
        struct rtl_msg_debug_dmi_req req;
        req.hdr = hdr;
        if (!sock_recv_all(sock_fd_, ((uint8_t *)&req) + sizeof(hdr),
                           sizeof(req) - sizeof(hdr))) {
            return;
        }
        dmi_request r;
        r.req_id = req.req_id;
        r.addr   = req.addr;
        r.op     = req.op;
        r.data   = req.data;
        {
            std::unique_lock<std::mutex> lock(dmi_mutex_);
            dmi_queue_.push_back(r);
        }
    }
};

/*
 * qemu_mmio_bridge implementation
 */
inline bool qemu_mmio_bridge::do_read(uint64_t start_addr, uint64_t size,
                                       char *buffer) {
    uint64_t data = 0;
    bool ok = bridge_->mmio_read(base_addr_ + start_addr, (uint32_t)size,
                                  &data);
    if (ok) {
        memcpy(buffer, &data, size);
    }
    return ok;
}

inline bool qemu_mmio_bridge::do_write(uint64_t start_addr, uint64_t size,
                                        const char *buffer) {
    uint64_t data = 0;
    memcpy(&data, buffer, size);
    return bridge_->mmio_write(base_addr_ + start_addr, (uint32_t)size, data);
}

/*
 * qemu_mem_bridge implementation
 */
inline bool qemu_mem_bridge::do_read(uint64_t start_addr, uint64_t size,
                                      char *buffer) {
    return bridge_->cpu_mem_read(base_addr_ + start_addr, (uint32_t)size,
                                 buffer);
}

inline bool qemu_mem_bridge::do_write(uint64_t start_addr, uint64_t size,
                                       const char *buffer) {
    return bridge_->cpu_mem_write(base_addr_ + start_addr, (uint32_t)size,
                                  buffer);
}

#endif /* QEMU_BRIDGE_HPP */
