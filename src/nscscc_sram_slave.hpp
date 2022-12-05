#ifndef NSCSCC_SRAM_SLAVE
#define NSCSCC_SRAM_SLAVE

#include "nscscc_sram.hpp"
#include <utility>
#include <vector>

class nscscc_sram_slave {
public:
    nscscc_sram_slave() {

    }
    void beat(nscscc_sram_ref &pin) {
        inst_channel(pin);
        data_channel(pin);
    }
    void inst_channel(nscscc_sram_ref &pin) {
        if (pin.inst_sram_en) {
            assert(!pin.inst_sram_wen);
            do_read(pin.inst_sram_addr & address_mask,4,(uint8_t*)&(pin.inst_sram_rdata));
        }
    }
    void data_channel(nscscc_sram_ref &pin) {
        if (pin.data_sram_en) {
            if (pin.data_sram_wen) {
                std::vector<std::pair<int,int> > write_range = strb_to_range(pin.data_sram_wen);
                for (std::pair<int,int> sub_range : write_range) {
                    int &addr = sub_range.first;
                    int &len  = sub_range.second;
                    do_write(((pin.data_sram_addr & address_mask)&0xfffffffc)+addr, len, ((uint8_t*)&(pin.data_sram_wdata)) + addr);
                }
            }
            do_read(pin.data_sram_addr & address_mask,4,(uint8_t*)&(pin.data_sram_rdata));
        }
    }
    void set_address_mask(uint32_t new_mask) {
        address_mask = new_mask;
    }
protected:
    virtual void do_read (uint64_t start_addr, uint64_t size, uint8_t* buffer) = 0;
    virtual void do_write(uint64_t start_addr, uint64_t size, const uint8_t* buffer) = 0;
private:
    uint32_t address_mask = 0xffffffff;
    std::vector<std::pair<int,int> > strb_to_range(AUTO_IN (wen, 3, 0)) {
        std::vector<std::pair<int,int> > res;
        int l = 0;
        while (l < 4) {
            if ((wen >> l) & 1) {
                int r = l;
                while ((wen >> r) & 1 && r < 4) r ++;
                res.emplace_back(l,r-l);
                l = r + 1;
            }
            else l ++;
        }
        return res;
    }
};

#endif