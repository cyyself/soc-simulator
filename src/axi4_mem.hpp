#ifndef AXI4_MEM
#define AXI4_MEM

#include "axi4_slave.hpp"

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64, unsigned int ID_WIDTH = 4>
class axi4_mem : axi4_slave<A_WIDTH,D_WIDTH,ID_WIDTH>  {
    public:
        axi4_mem(unsigned long size_bytes) {
            if (size_bytes % (D_WIDTH / 8)) size_bytes += 8 - (size_bytes % (D_WIDTH / 8));
            mem = new unsigned char[size_bytes];
            mem_size = size_bytes;
        }
        axi4_mem(unsigned long size_bytes, const unsigned char *init_binary, unsigned long init_binary_len):axi4_mem(size_bytes) {
            assert(init_binary_len <= size_bytes);
            memcpy(mem,init_binary,init_binary_len);
        }
        ~axi4_mem() {
            delete [] mem;
        }
    private:
        axi_resp do_read(AUTO_SIG(start_addr,A_WIDTH-1,0), AUTO_SIG(size,A_WIDTH-1,0), unsigned char* buffer) {
            if (start_addr + size <= mem_size) {
                memcpy(buffer,&mem[start_addr],size);
                return RESP_OKEY;
            }
            else {
                memset(buffer,0xcc,size);
                return RESP_SLVERR;
            }
        }
        unsigned char *mem;
        unsigned long mem_size;
};

#endif