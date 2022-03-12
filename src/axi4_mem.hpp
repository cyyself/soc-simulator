#ifndef AXI4_MEM
#define AXI4_MEM

#include "axi4_slave.hpp"

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64, unsigned int ID_WIDTH = 4>
class axi4_mem : public axi4_slave<A_WIDTH,D_WIDTH,ID_WIDTH>  {
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
    protected:
        axi_resp do_read(unsigned long start_addr, unsigned long size, unsigned char* buffer) {
            printf("-----do read from %lu to %lu\n",start_addr,size);
            if (start_addr + size <= mem_size) {
                memcpy(buffer,&mem[start_addr],size);
                return RESP_OKEY;
            }
            else return RESP_SLVERR;
        }
        axi_resp do_write(unsigned long start_addr, unsigned long size, const unsigned char* buffer) {
            if (start_addr + size <= mem_size) {
                memcpy(&mem[start_addr],buffer,size);
                return RESP_OKEY;
            }
            else return RESP_SLVERR;
        }
        unsigned char *mem;
        unsigned long mem_size;
};

#endif