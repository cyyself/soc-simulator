#ifndef NSCSCC_SRAM_XBAR
#define NSCSCC_SRAM_XBAR

#include "nscscc_sram_slave.hpp"
#include "mmio_dev.hpp"

#include <map>
#include <utility>
#include <algorithm>
#include <climits>

void debug() {

}

class nscscc_sram_xbar : public nscscc_sram_slave {
public:
    nscscc_sram_xbar() {

    }
    bool add_dev(uint64_t start_addr, uint64_t length, mmio_dev *dev, bool byte_mode = false) {
        std::tuple<uint64_t,uint64_t,bool> addr_range = std::make_tuple(start_addr, start_addr+length, byte_mode);
        if (start_addr % length) return false;
        // check range
        auto it = devices.upper_bound(addr_range);
        if (it != devices.end()) {
            uint64_t l_max = std::max(std::get<0>(it->first),std::get<0>(addr_range));
            uint64_t r_min = std::min(std::get<1>(it->first),std::get<1>(addr_range));
            if (l_max < r_min) return false; // overleap
        }
        if (it != devices.begin()) {
            it = std::prev(it);
            uint64_t l_max = std::max(std::get<0>(it->first),std::get<0>(addr_range));
            uint64_t r_min = std::min(std::get<1>(it->first),std::get<1>(addr_range));
            if (l_max < r_min) return false; // overleap
        }
        // overleap check pass
        devices[addr_range] = dev;
        return true;
    }
protected:
    void do_read(uint64_t start_addr, uint64_t size, unsigned char* buffer) {
        auto it = devices.upper_bound(std::make_tuple(start_addr,ULONG_MAX,true));
        if (it == devices.begin()) {
            *((uint32_t*)buffer) = 0xdec0dee3;
            return;
        }
        it = std::prev(it);
        if (std::get<0>(it->first) <= start_addr && start_addr + 1 <= std::get<1>(it->first) ) {
            uint64_t dev_size = std::get<1>(it->first) - std::get<0>(it->first);
            if (std::get<2>(it->first)) size = 1; // treat as byte device
            else start_addr -= start_addr & 3;
            it->second->do_read(start_addr % dev_size, size, buffer + (start_addr % size));
        }
        else *((uint32_t*)buffer) = 0xdec0dee3;
    }
    void do_write(uint64_t start_addr, uint64_t size, const unsigned char* buffer) {
        auto it = devices.upper_bound(std::make_tuple(start_addr,ULONG_MAX,true));
        if (it == devices.begin()) {
            *((uint32_t*)buffer) = 0xdec0dee3;
            return;
        }
        it = std::prev(it);
        uint64_t end_addr = start_addr + size;
        if (std::get<0>(it->first) <= start_addr && end_addr <= std::get<1>(it->first) ) {
            uint64_t dev_size = std::get<1>(it->first) - std::get<0>(it->first);
            it->second->do_write(start_addr % dev_size, size, buffer);
        }
        else *((uint32_t*)buffer) = 0xdec0dee3;
    }
private:
    std::map < std::tuple<uint64_t,uint64_t,bool>, mmio_dev* > devices;
};

#endif