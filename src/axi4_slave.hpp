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

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64, unsigned int ID_WIDTH = 4>
class axi4_slave {
public:
    axi4_slave() {
    }
    axi4_slave(int delay) {
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
    bool insert_memory_timing_model(uint64_t start_addr, uint64_t length, memory_timing_model* model) {
        std::pair<uint64_t, uint64_t> addr_range = std::make_pair(start_addr,start_addr+length);
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
    virtual axi_resp do_read (uint64_t start_addr, uint64_t size, uint8_t* buffer) = 0;
    virtual axi_resp do_write(uint64_t start_addr, uint64_t size, const uint8_t* buffer) = 0;
private:
    // for memory timing constrain {
    //                 addr_start,addr_end
    simple_delay_model* delay_model = NULL;
    std::map < std::pair<uint64_t,uint64_t>, memory_timing_model* > timing_constrain;
    std::map < int64_t, ar_packet > pending_ar;
    std::map < int64_t, aw_w_packet > pending_aw_w;
    int64_t req_id_gen = 0; // +2 everytime, lsb[0]: write else read
    memory_timing_model* find_timing_model(uint64_t start_addr, uint64_t size) {
        auto it = timing_constrain.upper_bound(std::make_pair(start_addr, ULONG_MAX));
        if (it == timing_constrain.begin()) return nullptr;
        it = std::prev(it);
        if (it->first.first <= start_addr && start_addr + size <= it->first.second) {
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
    void input_transaction(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
        // TODO: add memory timing model and support bit endian host ISA
        // input ar
        if (pin.arvalid && pin.arready) { // ar.fire
            ar_packet tmp;
            tmp.id = pin.arid;
            tmp.addr = pin.araddr;
            tmp.len = pin.arlen;
            tmp.size = pin.arsize;
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
            tmp.burst = static_cast<axi_burst_type>(pin.awburst);
            aw.push(tmp);
        }
        // input w
        if (pin.wvalid && pin.wready) { // w.fire
            for (int i = 0; i < D_WIDTH / 8; i++) {
                cur_w.data.push_back(((char*)(&pin.wdata))[i]);
                cur_w.strb.push_back( ( (((char*)(&pin.wstrb))[i/8]) & (1 << (i % 8)) ) ? true : false);
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
                    // if there is a new transaction, it will be solved in the next if
                    r_index = -1;
                }
                else {
                    for (int i = 0; i < D_WIDTH / 8; i++) {
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
            for (int i = 0; i < D_WIDTH / 8; i++) {
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
            memory_timing_model* model = find_timing_model(cur_ar.addr, (1<<cur_ar.size));
            if (model) {
                // has timing constrain
                int64_t req_id = req_id_gen;
                req_id_gen += 2;
                if (req_id_gen < 0) req_id_gen = 0;
                pending_ar[req_id] = cur_ar;
                model->add_req(cur_ar.addr, (1<<cur_ar.size), false, req_id);
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
            memory_timing_model* model = find_timing_model(cur_aw.addr, (1<<cur_aw.size));
            if (model) {
                // has timing constrain
                int64_t req_id = req_id_gen + 1;
                req_id_gen += 2;
                if (req_id_gen < 0) req_id_gen = 0;
                pending_aw_w[req_id] = aw_w_packet(cur_aw, cur_w);
                model->add_req(cur_aw.addr, (1<<cur_aw.size), true, req_id);
            }
            else {
                // no timing constrain
                aw_process(cur_aw, cur_w);
            }
        }
    }
    void ar_process(ar_packet &cur_ar) {
        std::vector<char> tmp((D_WIDTH/8)*(cur_ar.len+1), 0);
        switch (cur_ar.burst) {
            case BURST_FIXED: {
                uint64_t cur_addr_l = cur_ar.addr;
                uint64_t cur_addr_r = cur_addr_l + (1 << cur_ar.size) - cur_addr_l % (1 << cur_ar.size);
                int res = RESP_OKEY;
                for (int i=0;i<cur_ar.len+1;i++) {
                    res |= do_read(cur_addr_l, cur_addr_r - cur_addr_l, (unsigned char*)&tmp.data()[(D_WIDTH / 8) * i + cur_addr_l % (D_WIDTH / 8)]);
                }
                r.push(r_packet(static_cast<axi_resp>(res), cur_ar.id, tmp));
                break;
            }
            case BURST_INCR: {
                uint64_t cur_addr_l = cur_ar.addr;
                int res = RESP_OKEY;
                for (int i=0;i<cur_ar.len+1;i++) {
                    uint64_t cur_addr_r = cur_addr_l + (1 << cur_ar.size) - cur_addr_l % (1 << cur_ar.size);
                    res |= do_read(cur_addr_l, cur_addr_r - cur_addr_l, (unsigned char*)&tmp.data()[(D_WIDTH / 8) * i + cur_addr_l % (D_WIDTH / 8)]);
                    cur_addr_l = cur_addr_r;
                }
                r.push(r_packet(static_cast<axi_resp>(res), cur_ar.id, tmp));
                break;
            }
            case BURST_WRAP: {
                if (cur_ar.len + 1 == 2 || cur_ar.len + 1 == 4 || cur_ar.len + 1 == 8 || cur_ar.len + 1 == 16) {
                    if (cur_ar.addr % (1 << cur_ar.size) == 0) {
                        uint64_t start_addr = cur_ar.addr - cur_ar.addr % ((1 << cur_ar.size) * (cur_ar.len + 1));
                        uint64_t end_addr = start_addr + ((1 << cur_ar.size) * (cur_ar.len + 1));
                        uint64_t cur_addr = cur_ar.addr;
                        int res = RESP_OKEY;
                        for (int i=0;i<cur_ar.len+1;i++) {
                            res |= do_read(cur_addr, (1 << cur_ar.size), (unsigned char*)&tmp.data()[(D_WIDTH / 8) * i + cur_addr % (D_WIDTH / 8)]);
                            cur_addr += (1 << cur_ar.size);
                            if (cur_addr == end_addr) cur_addr = start_addr;
                        }
                        r.push(r_packet(static_cast<axi_resp>(res), cur_ar.id, tmp));
                    }
                    else {
                        r.push(r_packet(RESP_DECERR, cur_ar.id, tmp));
                    }
                }
                else {
                    r.push(r_packet(RESP_DECERR, cur_ar.id, tmp));
                }
                break;
            }
            default: {
                r.push(r_packet(RESP_DECERR, cur_ar.id, tmp));
                break;
            }
        }
    }
    void aw_process(aw_packet &cur_aw, w_packet &cur_w) {
        // check the length first
        if ( (cur_aw.len + 1) * (D_WIDTH / 8) == cur_w.data.size() && (1 << cur_aw.size) <= (D_WIDTH / 8) ) {
            switch (cur_aw.burst) {
                case BURST_FIXED: {
                    uint64_t cur_addr_l = cur_aw.addr;
                    uint64_t cur_addr_r = cur_addr_l + (1 << cur_aw.size) - cur_addr_l % (1 << cur_aw.size);
                    int res = RESP_OKEY;
                    for (int i=0;i<cur_aw.len+1;i++) {
                        res |= do_write_with_strobe(cur_addr_l, (cur_addr_l % (D_WIDTH / 8)) + (D_WIDTH / 8) * i, 
                                                    cur_addr_r - cur_addr_l, cur_w.data, cur_w.strb);
                    }
                    b.push(b_packet(static_cast<axi_resp>(res), cur_aw.id));
                    break;
                }
                case BURST_INCR: {
                    uint64_t cur_addr_l = cur_aw.addr;
                    int res = RESP_OKEY;
                    for (int i=0;i<cur_aw.len+1;i++) {
                        uint64_t cur_addr_r = cur_addr_l + (1 << cur_aw.size) - cur_addr_l % (1 << cur_aw.size);
                        res |= do_write_with_strobe(cur_addr_l, (cur_addr_l % (D_WIDTH / 8)) + (D_WIDTH / 8) * i, 
                                                    cur_addr_r - cur_addr_l, cur_w.data, cur_w.strb);
                        cur_addr_l = cur_addr_r;
                    }
                    b.push(b_packet(static_cast<axi_resp>(res), cur_aw.id));
                    break;
                }
                case BURST_WRAP: {
                    if (cur_aw.len + 1 == 2 || cur_aw.len + 1 == 4 || cur_aw.len + 1 == 8 || cur_aw.len + 1 == 16) {
                        if (cur_aw.addr % (1 << cur_aw.size) == 0) {
                            uint64_t start_addr = cur_aw.addr - cur_aw.addr % ((1 << cur_aw.size) * (cur_aw.len + 1));
                            uint64_t end_addr = start_addr + ((1 << cur_aw.size) * (cur_aw.len + 1));
                            uint64_t cur_addr = cur_aw.addr;
                            int res = RESP_OKEY;
                            for (int i=0;i<cur_aw.len+1;i++) {
                                res |= do_write_with_strobe(cur_addr, (cur_addr % (D_WIDTH / 8)) + (D_WIDTH / 8) * i, 
                                                            (1 << cur_aw.size), cur_w.data, cur_w.strb);
                                cur_addr += (1 << cur_aw.size);
                                if (cur_addr == end_addr) cur_addr = start_addr;
                            }
                            b.push(b_packet(static_cast<axi_resp>(res), cur_aw.id));
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
    axi_resp do_write_with_strobe(uint64_t start_addr, int64_t data_pos, int64_t data_len, std::vector<char> &data, std::vector<bool> &strb) {
        // As the type of std::vector<bool> is very special, we don't use it .data() to pass the bool* pointer
        // we use the data_pos and data_len instead and pass the reference of the vector<char> and vector<bool> as arguments
        int64_t l = data_pos;
        int64_t r = data_pos;
        int res = RESP_OKEY;
        for (int64_t i = data_pos; i < data_pos + data_len; i++) {
            if (strb[i]) {
                r = i + 1;
            }
            else {
                if (l < r) {
                    res |= do_write(start_addr + l - data_pos, r - l, (unsigned char*)&data.data()[l]);
                }
                l = i + 1;
            }
        }
        if (l < r) {
            res |= do_write(start_addr + l - data_pos, r - l, (unsigned char*)&data.data()[l]);
        }
        return static_cast<axi_resp>(res);
    }
};

#endif
