#ifndef AXI4_SLAVE
#define AXI4_SLAVE

#include "axi4.hpp"
#include "memory_timing_model.hpp"
#include "simple_delay_model.hpp"

#include <queue>
#include <algorithm>
#include <utility>
#include <vector>
#include <queue>
#include <climits>
#include <bit>

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64,
          unsigned int ID_WIDTH = 4>
class axi4_slave {
public:
    axi4_slave() {
    }
    axi4_slave(int delay):axi4_slave() {
        delay_model = new simple_delay_model(delay);
        assert(insert_memory_timing_model(0, ULONG_MAX, delay_model));
    }
    void reset() {
        while (!ar.empty()) ar.pop();
        while (!r.empty()) r.pop();
        while (!aw.empty()) aw.pop();
        while (!w.empty()) w.pop();
        while (!b.empty()) b.pop();
        r_index = -1;
        cur_w.data.clear();
        cur_w.strb.clear();
        for (auto &each_model : timing_constrain) {
            each_model.second->reset();
        }
    }
    void beat(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
        input_transaction(pin);
        if (delay_model) delay_model->tick();
        transaction_process();
        do_timing_constrain();
        output_transaction(pin);
    }
    bool insert_memory_timing_model(uint64_t start_addr, uint64_t length,
                                    memory_timing_model* model) {
        std::pair<uint64_t, uint64_t> addr_range(start_addr,
                                                 start_addr+length);
        if (start_addr % length) return false;
        // check range
        auto it = timing_constrain.upper_bound(addr_range);
        if (it != timing_constrain.end()) {
            uint64_t l_max = std::max(it->first.first,addr_range.first);
            uint64_t r_min = std::min(it->first.second,addr_range.second);
            if (l_max < r_min) return false; // overleap
        }
        if (it != timing_constrain.begin()) {
            it = std::prev(it);
            uint64_t l_max = std::max(it->first.first,addr_range.first);
            uint64_t r_min = std::min(it->first.second,addr_range.second);
            if (l_max < r_min) return false; // overleap
        }
        // overleap check pass
        timing_constrain[addr_range] = model;
        return true;
    }
protected:
    virtual axi_resp do_read (uint64_t start_addr, uint64_t size,
                              char* buffer) = 0;
    virtual axi_resp do_write(uint64_t start_addr, uint64_t size,
                              const char* buffer) = 0;
private:
    // for memory timing constrain {
    simple_delay_model* delay_model = NULL;
    //                 addr_start,addr_end
    std::map < std::pair<uint64_t,uint64_t>,
               memory_timing_model* > timing_constrain;
    std::map < int64_t, ar_packet > pending_ar;
    std::map < int64_t, aw_w_packet > pending_aw_w;
    int64_t req_id_gen = 0; // +2 everytime, lsb[0]: write else read
    memory_timing_model* find_timing_model(uint64_t start_addr, 
                                           uint64_t size) {
        auto it = timing_constrain.upper_bound(std::make_pair(start_addr,
                                                              ULONG_MAX));
        if (it == timing_constrain.begin()) return nullptr;
        it = std::prev(it);
        if (it->first.first <= start_addr && 
            start_addr + size <= it->first.second) {
            return it->second;
        }
        else return nullptr;
    }
    void do_timing_constrain() {
        if (pending_ar.empty() && pending_aw_w.empty()) return;
        for (auto &each_model : timing_constrain) {
            while (each_model.second->has_finished_req()) {
                int64_t req_id = each_model.second->get_finished_req_id();
                if (req_id & 1) {
                    // write
                    if (pending_aw_w.count(req_id)) {
                        aw_w_packet cur_aw_w = pending_aw_w[req_id];
                        pending_aw_w.erase(req_id);
                        aw_process(cur_aw_w.aw, cur_aw_w.w);
                    }
                }
                else {
                    if (pending_ar.count(req_id)) {
                        ar_packet cur_ar = pending_ar[req_id];
                        pending_ar.erase(req_id);
                        ar_process(cur_ar);
                    }
                }
            }
        }
    }
    // for memory timing constrain }
    std::queue <ar_packet> ar;
    std::queue <r_packet> r;
    std::queue <aw_packet> aw;
    std::queue <w_packet> w;
    std::queue <b_packet> b;
    r_packet cur_r;
    w_packet cur_w;
    int r_index = -1;
    uint8_t d_width_log2 = std::bit_width(D_WIDTH) - 1;
    void input_transaction(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
        // input ar
        if (pin.arvalid && pin.arready) { // ar.fire
            ar_packet tmp;
            tmp.id = pin.arid;
            tmp.addr = pin.araddr;
            tmp.len = pin.arlen;
            tmp.size = pin.arsize;
            tmp.d_width = d_width_log2;
            tmp.burst = static_cast<axi_burst_type>(pin.arburst);
            ar.push(tmp);
        }
        // input aw
        if (pin.awvalid && pin.awready) { // aw.fire
            aw_packet tmp;
            tmp.id = pin.awid;
            tmp.addr = pin.awaddr;
            tmp.len = pin.awlen;
            tmp.size = pin.awsize;
            tmp.d_width = d_width_log2;
            tmp.burst = static_cast<axi_burst_type>(pin.awburst);
            aw.push(tmp);
        }
        // input w
        if (pin.wvalid && pin.wready) { // w.fire
            cur_w.d_width = d_width_log2;
            for (int i = 0; i < D_WIDTH / 8; i++) {
                cur_w.data.push_back(((char*)(&pin.wdata))[i]);
                bool cur_strb = ((((char*)(&pin.wstrb))[i/8])&(1<<(i%8))) ?
                                true : false;
                cur_w.strb.push_back(cur_strb);
            }
            if (pin.wlast) {
                w.push(cur_w);
                cur_w.data.clear();
                cur_w.strb.clear();
            }
        }
        // output control signal
        pin.arready = 1;
        pin.awready = 1;
        pin.wready = 1;
    }
    void output_transaction(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
        // output r {
        if (r_index != -1) { // check pending transaction first
            if (pin.rready) {
                // next
                if (pin.rlast) {
                    // here is the last transation, stop
                    // if there is a new transaction, it will be solved in
                    // the next if
                    r_index = -1;
                }
                else {
                    for (int i = 0; i < (1<<cur_r.d_width) / 8; i++) {
                        ((char*)&(pin.rdata))[i] = cur_r.data[r_index++];
                    }
                    pin.rlast = (r_index == cur_r.data.size());
                }
            }
        }
        if (r_index == -1 && !r.empty()) { // start new transation
            cur_r = r.front();
            r.pop();
            r_index = 0;
            pin.rid = cur_r.id;
            for (int i = 0; i < (1<<cur_r.d_width) / 8; i++) {
                ((char*)&(pin.rdata))[i] = cur_r.data[r_index++];
            }
            pin.rlast = (r_index == cur_r.data.size());
            pin.rresp = cur_r.resp;
        }
        pin.rvalid = (r_index != -1);
        // output r }
        // output b
        if (pin.bvalid && pin.bready) pin.bvalid = false;
        if (!pin.bvalid && !b.empty()) {
            b_packet tmp = b.front();
            b.pop();
            pin.bid = tmp.id;
            pin.bresp = tmp.resp;
            pin.bvalid = true;
        }
    }
    void transaction_process() {
        // TODO: AXI address range decode
        while (!ar.empty()) {
            ar_packet cur_ar = ar.front();
            ar.pop();
            int64_t req_size = (1<<cur_ar.size) * (cur_ar.len + 1);
            memory_timing_model* model = find_timing_model(cur_ar.addr,
                                                           req_size);
            if (model) {
                // has timing constrain
                int64_t req_id = req_id_gen;
                req_id_gen += 2;
                if (req_id_gen < 0) req_id_gen = 0;
                pending_ar[req_id] = cur_ar;
                model->add_req(cur_ar.addr, req_size, false, req_id);
            }
            else {
                // no timing constrain
                ar_process(cur_ar);
            }
        }
        while (!aw.empty() && !w.empty()) {
            aw_packet cur_aw = aw.front();
            aw.pop();
            w_packet cur_w = w.front();
            w.pop();
            int64_t req_size = (1<<cur_aw.size) * (cur_aw.len + 1);
            memory_timing_model* model = find_timing_model(cur_aw.addr,
                                                           req_size);
            if (model) {
                // has timing constrain
                int64_t req_id = req_id_gen + 1;
                req_id_gen += 2;
                if (req_id_gen < 0) req_id_gen = 0;
                pending_aw_w[req_id] = aw_w_packet(cur_aw, cur_w);
                model->add_req(cur_aw.addr, req_size, true, req_id);
            }
            else {
                // no timing constrain
                aw_process(cur_aw, cur_w);
            }
        }
    }
    void ar_process(ar_packet &cur_ar) {
        uint64_t size_burst = (1 << cur_ar.size);
        int d_width_bytes = (1<<cur_ar.d_width) / 8;
        int burst_length = cur_ar.len + 1;
        std::vector<char> tmp(d_width_bytes * burst_length, 0);
        switch (cur_ar.burst) {
            case BURST_FIXED: {
                uint64_t cur_addr_l = cur_ar.addr;
                uint64_t cur_addr_r = cur_addr_l + size_burst - 
                                      cur_addr_l % size_burst;
                int res = RESP_OKEY;
                for (int i=0;i<burst_length;i++) {
                    int data_pos = d_width_bytes * i + 
                                   cur_addr_l % d_width_bytes;
                    res |= do_read(cur_addr_l, cur_addr_r - cur_addr_l,
                                   (char*)&tmp.data()[data_pos]);
                }
                r.push(r_packet(static_cast<axi_resp>(res), cur_ar.id, tmp,
                                cur_ar.d_width));
                break;
            }
            case BURST_INCR: {
                uint64_t cur_addr_l = cur_ar.addr;
                uint64_t cur_addr_r = cur_addr_l + size_burst - 
                                      cur_addr_l % size_burst;
                int res = RESP_OKEY;
                for (int i=0;i<burst_length;i++) {
                    int data_pos = d_width_bytes * i +
                                   cur_addr_l % d_width_bytes;
                    res |= do_read(cur_addr_l, cur_addr_r - cur_addr_l,
                                   (char*)&tmp.data()[data_pos]);
                    cur_addr_l = cur_addr_r;
                    cur_addr_r += size_burst;
                }
                r.push(r_packet(static_cast<axi_resp>(res), cur_ar.id, tmp,
                                cur_ar.d_width));
                break;
            }
            case BURST_WRAP: {
                uint64_t req_size = size_burst * burst_length;
                if (burst_length == 2 || burst_length == 4 ||
                    burst_length == 8 || burst_length == 16) {
                    if (cur_ar.addr % size_burst == 0) {
                        uint64_t start_addr = cur_ar.addr - 
                                              cur_ar.addr % req_size;
                        uint64_t end_addr = start_addr + req_size;
                        uint64_t cur_addr = cur_ar.addr;
                        int res = RESP_OKEY;
                        for (int i=0;i<cur_ar.len+1;i++) {
                            int data_pos = d_width_bytes * i +
                                           cur_addr % d_width_bytes;
                            res |= do_read(cur_addr, size_burst,
                                           (char*)&tmp.data()[data_pos]);
                            cur_addr += size_burst;
                            if (cur_addr == end_addr)
                                cur_addr = start_addr;
                        }
                        r.push(r_packet(static_cast<axi_resp>(res),
                                        cur_ar.id, tmp, cur_ar.d_width));
                    }
                    else {
                        r.push(r_packet(RESP_DECERR, cur_ar.id, tmp,
                                        cur_ar.d_width));
                    }
                }
                else {
                    r.push(r_packet(RESP_DECERR, cur_ar.id, tmp,
                                    cur_ar.d_width));
                }
                break;
            }
            default: {
                r.push(r_packet(RESP_DECERR, cur_ar.id, tmp,
                                cur_ar.d_width));
                break;
            }
        }
    }
    void aw_process(aw_packet &cur_aw, w_packet &cur_w) {
        // check the length first
        assert(cur_aw.d_width == cur_w.d_width);
        uint64_t size_burst = (1 << cur_aw.size);
        int d_width_bytes = (1<<cur_aw.d_width) / 8;
        int burst_length = cur_aw.len + 1;
        if ( burst_length * d_width_bytes == cur_w.data.size() &&
             (1 << cur_aw.size) <= d_width_bytes ) {
            switch (cur_aw.burst) {
                case BURST_FIXED: {
                    uint64_t cur_addr_l = cur_aw.addr;
                    uint64_t cur_addr_r = cur_addr_l + size_burst -
                                          cur_addr_l % size_burst;
                    int res = RESP_OKEY;
                    for (int i=0;i<burst_length;i++) {
                        uint64_t data_pos = (cur_addr_l % d_width_bytes) +
                                            d_width_bytes * i;
                        res |= do_write_with_strobe(cur_addr_l,
                                                    data_pos, 
                                                    cur_addr_r-cur_addr_l,
                                                    cur_w.data,
                                                    cur_w.strb);
                    }
                    b.push(b_packet(static_cast<axi_resp>(res), cur_aw.id));
                    break;
                }
                case BURST_INCR: {
                    uint64_t cur_addr_l = cur_aw.addr;
                    uint64_t cur_addr_r = cur_addr_l + size_burst -
                                          cur_addr_l % size_burst;
                    int res = RESP_OKEY;
                    for (int i=0;i<burst_length;i++) {
                        uint64_t data_pos = (cur_addr_l % d_width_bytes) +
                                            d_width_bytes * i;
                        res |= do_write_with_strobe(cur_addr_l,
                                                    data_pos, 
                                                    cur_addr_r-cur_addr_l,
                                                    cur_w.data,
                                                    cur_w.strb);
                        cur_addr_l = cur_addr_r;
                        cur_addr_r += size_burst;
                    }
                    b.push(b_packet(static_cast<axi_resp>(res), cur_aw.id));
                    break;
                }
                case BURST_WRAP: {
                    if (burst_length == 2 || burst_length == 4 ||
                        burst_length == 8 || burst_length == 16) {
                        uint64_t req_size = size_burst * burst_length;
                        if (cur_aw.addr % size_burst == 0) {
                            uint64_t start_addr = cur_aw.addr -
                                                  cur_aw.addr % req_size;
                            uint64_t end_addr = start_addr + req_size;
                            uint64_t cur_addr = cur_aw.addr;
                            int res = RESP_OKEY;
                            for (int i=0;i<burst_length;i++) {
                                uint64_t data_pos = (cur_addr % d_width_bytes) +
                                                    d_width_bytes * i;
                                res |= do_write_with_strobe(cur_addr,
                                                            data_pos, 
                                                            size_burst,
                                                            cur_w.data,
                                                            cur_w.strb);
                                cur_addr += size_burst;
                                if (cur_addr == end_addr)
                                    cur_addr = start_addr;
                            }
                            b.push(b_packet(static_cast<axi_resp>(res),
                                            cur_aw.id));
                        }
                        else {
                            b.push(b_packet(RESP_DECERR, cur_aw.id));
                        }
                    }
                    else {
                        b.push(b_packet(RESP_DECERR, cur_aw.id));
                    }
                    break;
                }
                default: {
                    b.push(b_packet(RESP_DECERR, cur_aw.id));
                    break;
                }
            }
        }
        else {
            b.push(b_packet(RESP_DECERR, cur_aw.id));
        }
    }
    axi_resp do_write_with_strobe(uint64_t start_addr, int64_t data_pos,
                                  int64_t data_len,
                                  std::vector<char> &data,
                                  std::vector<bool> &strb) {
        // As the type of std::vector<bool> is very special, we don't use
        // it .data() to pass the bool* pointer.
        // we use the data_pos and data_len instead and pass the reference
        // of the vector<char> and vector<bool> as arguments.
        int64_t l = data_pos;
        int64_t r = data_pos;
        int res = RESP_OKEY;
        for (int64_t i = data_pos; i < data_pos + data_len; i++) {
            if (strb[i]) {
                r = i + 1;
            }
            else {
                if (l < r) {
                    res |= do_write(start_addr + l - data_pos, r - l,
                                    (char*)&data.data()[l]);
                }
                l = i + 1;
            }
        }
        if (l < r) {
            res |= do_write(start_addr + l - data_pos, r - l,
                            (char*)&data.data()[l]);
        }
        return static_cast<axi_resp>(res);
    }
};

#endif
