#ifndef AXI4_CTRL_H
#define AXI4_CTRL_H

#include "axi4.hpp"
#include "axi4_slv.hpp"
#include "mmio_dev.hpp"

#include <map>
#include <utility>
#include <algorithm>
#include <climits>

class axi4_ctrl: public axi4_slv {
public:
    axi4_ctrl():axi4_slv() {

    }
    bool add_dev(uint64_t start_addr, uint64_t length, mmio_dev *dev) {
        std::pair<uint64_t, uint64_t> addr_range = std::make_pair(start_addr,start_addr+length);
        if (start_addr % length) return false;
        // check range
        auto it = devices.upper_bound(addr_range);
        if (it != devices.end()) {
            uint64_t l_max = std::max(it->first.first,addr_range.first);
            uint64_t r_min = std::min(it->first.second,addr_range.second);
            if (l_max < r_min) return false; // overleap
        }
        if (it != devices.begin()) {
            it = std::prev(it);
            uint64_t l_max = std::max(it->first.first,addr_range.first);
            uint64_t r_min = std::min(it->first.second,addr_range.second);
            if (l_max < r_min) return false; // overleap
        }
        // overleap check pass
        devices[addr_range] = dev;
        return true;
    }
protected:
    axi_resp do_read(uint64_t start_addr, uint64_t size, char* buffer) {
        auto it = devices.upper_bound(std::make_pair(start_addr,ULONG_MAX));
        if (it == devices.begin()) return RESP_DECERR;
        it = std::prev(it);
        uint64_t end_addr = start_addr + size;
        if (it->first.first <= start_addr && end_addr <= it->first.second) {
            uint64_t dev_size = it->first.second - it->first.first;
            return it->second->do_read(start_addr % dev_size, size, buffer) ? RESP_OKEY : RESP_SLVERR;
        }
        else return RESP_DECERR;
    }
    axi_resp do_write(uint64_t start_addr, uint64_t size, const char* buffer) {
        auto it = devices.upper_bound(std::make_pair(start_addr,ULONG_MAX));
        if (it == devices.begin()) return RESP_DECERR;
        it = std::prev(it);
        uint64_t end_addr = start_addr + size;
        if (it->first.first <= start_addr && end_addr <= it->first.second) {
            uint64_t dev_size = it->first.second - it->first.first;
            return it->second->do_write(start_addr % dev_size, size, buffer) ? RESP_OKEY : RESP_SLVERR;
        }
        else return RESP_DECERR;
    }
private:
    std::map < std::pair<uint64_t,uint64_t>, mmio_dev* > devices;
};

#endif