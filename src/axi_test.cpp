#include "axi4.hpp"
#include "axi4_slave.hpp"
#include "axi4_mem.hpp"


#include <iostream>

using std::cout;
using std::endl;

void dump_axi_read(axi4_ref<64,64,4> &axi) {
    cout << "arid\t" << (unsigned long)axi.arid << endl;
    cout << "araddr\t" << (unsigned long)axi.araddr << endl;
    cout << "arlen\t" << (unsigned long)axi.arlen << endl;
    cout << "arsize\t" << (unsigned long)axi.arsize << endl;
    cout << "arburst\t" << (unsigned long)axi.arburst << endl;
    cout << "arvalid\t" << (unsigned long)axi.arvalid << endl;
    cout << "arready\t" << (unsigned long)axi.arready << endl;
    cout << "rid\t" << (unsigned long)axi.rid << endl;
    cout << "rdata\t" << (unsigned long)axi.rdata << endl;
    cout << "rresp\t" << (unsigned long)axi.rresp << endl;
    cout << "rlast\t" << (unsigned long)axi.rlast << endl;
    cout << "rvalid\t" << (unsigned long)axi.rvalid << endl;
    cout << "rready\t" << (unsigned long)axi.rready << endl;
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