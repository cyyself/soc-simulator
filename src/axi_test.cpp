#include "axi4.hpp"
#include "axi4_slave.hpp"
#include "axi4_mem.hpp"


#include <iostream>

using std::cout;
using std::endl;

void dump_axi_read(axi4_ref<64,64,4> &axi) {
    cout << "arid\t" << (uint64_t)axi.arid << endl;
    cout << "araddr\t" << (uint64_t)axi.araddr << endl;
    cout << "arlen\t" << (uint64_t)axi.arlen << endl;
    cout << "arsize\t" << (uint64_t)axi.arsize << endl;
    cout << "arburst\t" << (uint64_t)axi.arburst << endl;
    cout << "arvalid\t" << (uint64_t)axi.arvalid << endl;
    cout << "arready\t" << (uint64_t)axi.arready << endl;
    cout << "rid\t" << (uint64_t)axi.rid << endl;
    cout << "rdata\t" << (uint64_t)axi.rdata << endl;
    cout << "rresp\t" << (uint64_t)axi.rresp << endl;
    cout << "rlast\t" << (uint64_t)axi.rlast << endl;
    cout << "rvalid\t" << (uint64_t)axi.rvalid << endl;
    cout << "rready\t" << (uint64_t)axi.rready << endl;
}

int main() {
    axi4_mem<64,64,4> mem(8192);
    axi4<64,64,4> axi;
    axi4_ref<64,64,4> axi_ref(axi);
    axi_ref.arid = 0xa;
    axi_ref.araddr = 0;
    axi_ref.arlen = 3;
    axi_ref.arsize = 3;
    axi_ref.arburst = BURST_FIXED;
    axi_ref.arvalid = 1;
    axi_ref.rready = 1;
    mem.beat(axi_ref); // reset
    for (int i=0;i<10;i++) {
        mem.beat(axi_ref);
        dump_axi_read(axi_ref);
        printf("----------\n");
    }
    return 0;
}