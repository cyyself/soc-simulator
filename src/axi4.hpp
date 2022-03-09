#ifndef AXI4_HPP
#define AXI4_HPP

#include "verilated.h"

#define AUTO_SIG(name, msb, lsb) \
    std::conditional <(msb-lsb+1) <=  8, CData, \
    std::conditional <(msb-lsb+1) <= 16, SData, \
    std::conditional <(msb-lsb+1) <= 32, IData, \
    std::conditional <(msb-lsb+1) <= 64, QData, \
    VlWide<(msb-lsb+32)/32> > > > > name

#define AUTO_IN(name, msb, lsb)  AUTO_SIG(name, msb, lsb)
#define AUTO_OUT(name, msb, lsb) AUTO_SIG(name, msb, lsb)


template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64, unsigned int ID_WIDTH = 4>
struct axi4_ptr {
    static_assert(__builtin_popcount(D_WIDTH) == 1,"D_WIDTH should be the power of 2.");
    static_assert(D_WIDTH >= 8,"D_WIDTH should be larger or equal to 8.");
    // aw
    AUTO_IN (*awid      ,ID_WIDTH-1, 0)     = NULL;
    AUTO_IN (*awaddr    ,A_WIDTH-1, 0)      = NULL;
    AUTO_IN (*awlen     ,7, 0)              = NULL;
    AUTO_IN (*awsize    , 2, 0)             = NULL;
    AUTO_IN (*awburst   , 1, 0)             = NULL;
    AUTO_IN (*awlock    , 0, 0)             = NULL;
    AUTO_IN (*awcache   , 3, 0)             = NULL;
    AUTO_IN (*awprot    , 2, 0)             = NULL;
    AUTO_IN (*awqos     , 3, 0)             = NULL;
    AUTO_IN (*awvalid   , 0, 0)             = NULL;
    AUTO_OUT(*awready   , 0, 0)             = NULL;
    // w
    AUTO_IN (*wdata     ,D_WIDTH-1, 0)      = NULL;
    AUTO_IN (*wstrb     ,(D_WIDTH/8)-1, 0)  = NULL;
    AUTO_IN (*wlast     , 0, 0)             = NULL;
    AUTO_IN (*wvalid    , 0, 0)             = NULL;
    AUTO_OUT(*wready    , 0, 0)             = NULL;
    // b
    AUTO_OUT(*bid       ,ID_WIDTH-1, 0)     = NULL;
    AUTO_OUT(*bresp     , 1, 0)             = NULL;
    AUTO_OUT(*bvalid    , 0, 0)             = NULL;
    AUTO_IN (*bready    , 0, 0)             = NULL;
    // ar
    AUTO_IN (*arid      ,ID_WIDTH-1, 0)     = NULL;
    AUTO_IN (*araddr    ,A_WIDTH-1, 0)      = NULL;
    AUTO_IN (*arlen     ,(D_WIDTH/8)-1, 0)  = NULL;
    AUTO_IN (*arsize    , 2, 0)             = NULL;
    AUTO_IN (*arburst   , 1, 0)             = NULL;
    AUTO_IN (*arlock    , 0, 0)             = NULL;
    AUTO_IN (*arcache   , 3, 0)             = NULL;
    AUTO_IN (*arprot    , 2, 0)             = NULL;
    AUTO_IN (*arqos     , 3, 0)             = NULL;
    AUTO_IN (*arvalid   , 0, 0)             = NULL;
    AUTO_OUT(*arready   , 0, 0)             = NULL;
    // r
    AUTO_OUT(*rid       ,ID_WIDTH-1, 0)     = NULL;
    AUTO_OUT(*rdata     ,D_WIDTH-1, 0)      = NULL;
    AUTO_OUT(*rresp     , 1, 0)             = NULL;
    AUTO_OUT(*rlast     , 0, 0)             = NULL;
    AUTO_OUT(*rvalid    , 0, 0)             = NULL;
    AUTO_IN (*rready    , 0, 0)             = NULL;
    bool check() {
        std::set <void*> s;
        // aw
        s.insert((void*)awid);
        s.insert((void*)awaddr);
        s.insert((void*)awlen);
        s.insert((void*)awsize);
        s.insert((void*)awburst);
        s.insert((void*)awlock);
        s.insert((void*)awcache);
        s.insert((void*)awprot);
        s.insert((void*)awqos);
        s.insert((void*)awvalid);
        s.insert((void*)awready);
        // w
        s.insert((void*)wdata);
        s.insert((void*)wstrb);
        s.insert((void*)wlast);
        s.insert((void*)wvalid);
        s.insert((void*)wready);
        // b
        s.insert((void*)bid);
        s.insert((void*)bresp);
        s.insert((void*)bvalid);
        s.insert((void*)bready);
        // ar
        s.insert((void*)arid);
        s.insert((void*)araddr);
        s.insert((void*)arlen);
        s.insert((void*)arsize);
        s.insert((void*)arburst);
        s.insert((void*)arlock);
        s.insert((void*)arcache);
        s.insert((void*)arprot);
        s.insert((void*)arqos);
        s.insert((void*)arvalid);
        s.insert((void*)arready);
        // r
        s.insert((void*)rid);
        s.insert((void*)rdata);
        s.insert((void*)rresp);
        s.insert((void*)rlast);
        s.insert((void*)rvalid);
        s.insert((void*)rready);
        return s.size() == 37 && s.count(NULL) == 0;
    }
};

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64, unsigned int ID_WIDTH = 4>
struct axi4 {
    AUTO_IN (&awid      ,ID_WIDTH-1, 0);
    AUTO_IN (&awaddr    ,A_WIDTH-1, 0);
    AUTO_IN (&awlen     ,7, 0);
    AUTO_IN (&awsize    , 2, 0);
    AUTO_IN (&awburst   , 1, 0);
    AUTO_IN (&awlock    , 0, 0);
    AUTO_IN (&awcache   , 3, 0);
    AUTO_IN (&awprot    , 2, 0);
    AUTO_IN (&awqos     , 3, 0);
    AUTO_IN (&awvalid   , 0, 0);
    AUTO_OUT(&awready   , 0, 0);
    // w
    AUTO_IN (&wdata     ,D_WIDTH-1, 0);
    AUTO_IN (&wstrb     ,(D_WIDTH/8)-1, 0);
    AUTO_IN (&wlast     , 0, 0);
    AUTO_IN (&wvalid    , 0, 0);
    AUTO_OUT(&wready    , 0, 0);
    // b
    AUTO_OUT(&bid       ,ID_WIDTH-1, 0);
    AUTO_OUT(&bresp     , 1, 0);
    AUTO_OUT(&bvalid    , 0, 0);
    AUTO_IN (&bready    , 0, 0);
    // ar
    AUTO_IN (&arid      ,ID_WIDTH-1, 0);
    AUTO_IN (&araddr    ,A_WIDTH-1, 0);
    AUTO_IN (&arlen     ,(D_WIDTH/8)-1, 0);
    AUTO_IN (&arsize    , 2, 0);
    AUTO_IN (&arburst   , 1, 0);
    AUTO_IN (&arlock    , 0, 0);
    AUTO_IN (&arcache   , 3, 0);
    AUTO_IN (&arprot    , 2, 0);
    AUTO_IN (&arqos     , 3, 0);
    AUTO_IN (&arvalid   , 0, 0);
    AUTO_OUT(&arready   , 0, 0);
    // r
    AUTO_OUT(&rid       ,ID_WIDTH-1, 0);
    AUTO_OUT(&rdata     ,D_WIDTH-1, 0);
    AUTO_OUT(&rresp     , 1, 0);
    AUTO_OUT(&rlast     , 0, 0);
    AUTO_OUT(&rvalid    , 0, 0);
    AUTO_IN (&rready    , 0, 0);
    axi4(axi4_ptr <A_WIDTH,D_WIDTH,ID_WIDTH> ptr):
        awid    (*(ptr.awid)),
        awaddr  (*(ptr.awaddr)),
        awlen   (*(ptr.awlen)),
        awsize  (*(ptr.awsize)),
        awburst (*(ptr.awburst)),
        awlock  (*(ptr.awlock)),
        awcache (*(ptr.awcache)),
        awprot  (*(ptr.awprot)),
        awqos   (*(ptr.awqos)),
        awvalid (*(ptr.awvalid)),
        awready (*(ptr.awready)),
        wdata   (*(ptr.wdata)),
        wstrb   (*(ptr.wstrb)),
        wlast   (*(ptr.wlast)),
        wvalid  (*(ptr.wvalid)),
        wready  (*(ptr.wready)),
        bid     (*(ptr.bid)),
        bresp   (*(ptr.bresp)),
        bvalid  (*(ptr.bvalid)),
        bready  (*(ptr.bready)),
        arid    (*(ptr.arid)),
        araddr  (*(ptr.araddr)),
        arlen   (*(ptr.arlen)),
        arsize  (*(ptr.arsize)),
        arburst (*(ptr.arburst)),
        arlock  (*(ptr.arlock)),
        arcache (*(ptr.arcache)),
        arprot  (*(ptr.arprot)),
        arqos   (*(ptr.arqos)),
        arvalid (*(ptr.arvalid)),
        arready (*(ptr.arready)),
        rid     (*(ptr.rid)),
        rdata   (*(ptr.rdata)),
        rresp   (*(ptr.rresp)),
        rlast   (*(ptr.rlast)),
        rvalid  (*(ptr.rvalid)),
        rready  (*(ptr.rready))
    {}
};

enum axi_resp {
    RESP_OKEY   = 0,
    RESP_EXOKEY = 1,
    RESP_SLVERR = 2,
    RESP_DECERR = 3
};

#endif
