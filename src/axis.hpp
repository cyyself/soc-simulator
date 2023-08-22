#ifndef AXIS_HPP
#define AXIS_HPP

#include <cstdint>
#include <set>
#include <cstring>
#include <queue>
#include "auto_sig.hpp"


// TODO: test axis
template <unsigned int D_WIDTH = 64>
struct axis_ptr {
    static_assert(__builtin_popcount(D_WIDTH) == 1,"D_WIDTH should be the power of 2.");
    static_assert(D_WIDTH >= 8,"D_WIDTH should be larger or equal to 8.");
    AUTO_SIG(*tlast     , 0, 0)             = NULL;
    AUTO_SIG(*tdata     , D_WIDTH-1, 0)     = NULL;
    AUTO_SIG(*tkeep     , (D_WIDTH/8)-1, 0) = NULL;
    AUTO_SIG(*tvalid    , 0, 0)             = NULL;
    AUTO_SIG(*tready    , 0, 0)             = NULL;
    bool check() {
        std::set <void*> s;
        s.insert((void*)tvalid);
        s.insert((void*)tready);
        s.insert((void*)tlast);
        s.insert((void*)tdata);
        s.insert((void*)tkeep);
        return s.size() == 5 && s.count(NULL) == 0;
    }
};

template <unsigned int D_WIDTH = 64>
struct axis_rx {
    axis_rx(axis_ptr<D_WIDTH> port):port(port) {
        *(port.tready) = true; // always tready
    }
    bool has_valid_data() {
        return !rx_queue.empty();
    }
    ssize_t recv(int max_size, char *dst_buf) {
        if (rx_queue.empty()) return -1;
        else {
            std::vector<char> data = rx_queue.front();
            rx_queue.pop();
            // TODO: check max size
            for (auto x : data) *(dst_buf++) = x;
        }
    }
    void tick() {
        if (*(port.tvalid)) {
            for (int i=0;i<D_WIDTH;i++) {
                bool should_preserve = (((unsigned char*)port.tkeep)[i/8] >> (i % 8)) & 1;
                if (should_preserve) {
                    recv_buf.push_back(((char*)port.tdata)[i]);
                }
            }
            if (*(port.tlast)) {
                rx_queue.push(recv_buf);
                recv_buf.clear();
            }
        }
    }
    std::vector <char> recv_buf;
    std::queue <std::vector<char>> rx_queue;
    axis_ptr<D_WIDTH> port;
};

template <unsigned int D_WIDTH = 64>
struct axis_tx {
    axis_tx(axis_ptr<D_WIDTH> port):port(port) {
        regnext_tready = false;
        byte_width = D_WIDTH / 8;
    }
    void send(const char *src_buf, ssize_t size) {
        std::queue<char> cur_buf;
        for (int i=0;i<size;i++) cur_buf.push(*(src_buf++));
        tx_queue.push(cur_buf);
    }
    void tick() {
        if (regnext_tready) {
            // last transaction have been accepted
            *(port.tvalid) = false;
            // start new transaction
            if (tx_buf.empty() && !tx_queue.empty()) {
                tx_buf = tx_queue.front();
                tx_queue.pop();
            }
            if (!tx_buf.empty()) {
                int byte_pos = 0;
                while (!tx_queue.empty() && byte_pos < byte_width) {
                    ((char*)port.tdata)[byte_pos++] = tx_buf.front();
                    tx_buf.pop();
                }
                // fill tkeep
                for (int i=0;i<D_WIDTH/8;i++) {
                    unsigned char cur_fill = 0;
                    for (int j=0;j<8;j++) {
                        if (i * 8 + j < byte_pos) cur_fill |= 1 << j;
                    }
                    ((unsigned char*)port.tkeep)[i] = cur_fill;
                }
                // set tlast
                *(port.tlast) = tx_buf.empty();
                *(port.tvalid) = true;
            }
        }
        regnext_tready = *(port.tready);
    }
    std::queue <char> tx_buf;
    std::queue <std::queue<char> > tx_queue;
    axis_ptr<D_WIDTH> port;
    bool regnext_tready;
    int byte_width;
};

#endif
