#ifndef NSCSCC_SRAM_HPP
#define NSCSCC_SRAM_HPP

#include <verilated.h>
#include <condition_variable>
#include <cstdint>
#include <set>

#define AUTO_SIG(name, msb, lsb) \
    typename std::conditional <(msb-lsb+1) <=  8, CData, \
    typename std::conditional <(msb-lsb+1) <= 16, SData, \
    typename std::conditional <(msb-lsb+1) <= 32, IData, QData >::type >::type >::type name

#define AUTO_IN(name, msb, lsb)  AUTO_SIG(name, msb, lsb)
#define AUTO_OUT(name, msb, lsb) AUTO_SIG(name, msb, lsb)

struct nscscc_sram;

struct nscscc_sram_ptr {
    // inst channel
    AUTO_IN (*inst_sram_en, 0, 0)       = NULL;
    AUTO_IN (*inst_sram_wen, 3, 0)      = NULL;
    AUTO_IN (*inst_sram_addr, 31, 0)    = NULL;
    AUTO_IN (*inst_sram_wdata, 31, 0)   = NULL;
    AUTO_OUT(*inst_sram_rdata, 31, 0)   = NULL;
    // data channel
    AUTO_IN (*data_sram_en, 0, 0)       = NULL;
    AUTO_IN (*data_sram_wen, 3, 0)      = NULL;
    AUTO_IN (*data_sram_addr, 31, 0)    = NULL;
    AUTO_IN (*data_sram_wdata, 31, 0)   = NULL;
    AUTO_OUT(*data_sram_rdata, 31, 0)   = NULL;
    bool check() {
        std::set <void*> s;
        s.insert((void*)inst_sram_en);
        s.insert((void*)inst_sram_wen);
        s.insert((void*)inst_sram_addr);
        s.insert((void*)inst_sram_wdata);
        s.insert((void*)inst_sram_rdata);
        s.insert((void*)data_sram_en);
        s.insert((void*)data_sram_wen);
        s.insert((void*)data_sram_addr);
        s.insert((void*)data_sram_wdata);
        s.insert((void*)data_sram_rdata);
        return s.size() == 10 && s.count(NULL) == 0;
    }
};

struct nscscc_sram_ref {
    // inst channel
    AUTO_IN (&inst_sram_en, 0, 0);
    AUTO_IN (&inst_sram_wen, 3, 0);
    AUTO_IN (&inst_sram_addr, 31, 0);
    AUTO_IN (&inst_sram_wdata, 31, 0);
    AUTO_OUT(&inst_sram_rdata, 31, 0);
    // data channel
    AUTO_IN (&data_sram_en, 0, 0);
    AUTO_IN (&data_sram_wen, 3, 0);
    AUTO_IN (&data_sram_addr, 31, 0);
    AUTO_IN (&data_sram_wdata, 31, 0);
    AUTO_OUT(&data_sram_rdata, 31, 0);
    nscscc_sram_ref(nscscc_sram_ptr &ptr):
        inst_sram_en    (*(ptr.inst_sram_en)),
        inst_sram_wen   (*(ptr.inst_sram_wen)),
        inst_sram_addr  (*(ptr.inst_sram_addr)),
        inst_sram_wdata (*(ptr.inst_sram_wdata)),
        inst_sram_rdata (*(ptr.inst_sram_rdata)),
        data_sram_en    (*(ptr.data_sram_en)),
        data_sram_wen   (*(ptr.data_sram_wen)),
        data_sram_addr  (*(ptr.data_sram_addr)),
        data_sram_wdata (*(ptr.data_sram_wdata)),
        data_sram_rdata (*(ptr.data_sram_rdata))
    {}
    nscscc_sram_ref(nscscc_sram &sram);
};

struct nscscc_sram {
    // inst channel
    AUTO_IN (inst_sram_en, 0, 0);
    AUTO_IN (inst_sram_wen, 3, 0);
    AUTO_IN (inst_sram_addr, 31, 0);
    AUTO_IN (inst_sram_wdata, 31, 0);
    AUTO_OUT(inst_sram_rdata, 31, 0);
    // data channel
    AUTO_IN (data_sram_en, 0, 0);
    AUTO_IN (data_sram_wen, 3, 0);
    AUTO_IN (data_sram_addr, 31, 0);
    AUTO_IN (data_sram_wdata, 31, 0);
    AUTO_OUT(data_sram_rdata, 31, 0);
    nscscc_sram() {
        // reset all pointer to zero
        memset(this,0,sizeof(*this));
    }
    void update_input(nscscc_sram_ref &ref) {
        inst_sram_en    = ref.inst_sram_en;
        inst_sram_wen   = ref.inst_sram_wen;
        inst_sram_addr  = ref.inst_sram_addr;
        inst_sram_wdata = ref.inst_sram_wdata;
        data_sram_en    = ref.data_sram_en;
        data_sram_wen   = ref.data_sram_wen;
        data_sram_addr  = ref.data_sram_addr;
        data_sram_wdata = ref.data_sram_wdata;
    }
    void update_output(nscscc_sram_ref &ref) {
        ref.inst_sram_rdata = inst_sram_rdata;
        ref.data_sram_rdata = data_sram_rdata;
    }
};

nscscc_sram_ref::nscscc_sram_ref(nscscc_sram &sram):
    inst_sram_en    (sram.inst_sram_en),
    inst_sram_wen   (sram.inst_sram_wen),
    inst_sram_addr  (sram.inst_sram_addr),
    inst_sram_wdata (sram.inst_sram_wdata),
    inst_sram_rdata (sram.inst_sram_rdata),
    data_sram_en    (sram.data_sram_en),
    data_sram_wen   (sram.data_sram_wen),
    data_sram_addr  (sram.data_sram_addr),
    data_sram_wdata (sram.data_sram_wdata),
    data_sram_rdata (sram.data_sram_rdata)
{}


#endif
