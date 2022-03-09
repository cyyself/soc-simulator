#ifndef AXI_MEM
#define AXI_MEM

#include "axi4.hpp"

#include <queue>

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64, unsigned int ID_WIDTH = 4>
class axi4_slave {
    public:
        void beat(axi4 <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            switch (status) {
                case READY:
                    if (*(pin.awvalid) && *(pin.wvalid)) {
                        // enter writing
                        status = WRITING;
                    }
                    else if (pin.arvalid) {
                        // enter reading
                        status = READING;
                    }
                    break;
                case READING:
                case WRITING:
                default:
                    assert(false);
            }
            out_bresp();
        }
    protected:
        virtual axi_resp do_read(AUTO_SIG(start_addr,A_WIDTH-1,0), AUTO_SIG(size,A_WIDTH-1,0), unsigned char* buffer) = NULL;
        enum slave_status {
            READY, READING, WRITING
        } status = READY;
    protected: bresp
        struct bresp_each {
            AUTO_OUT(bid    ,ID_WIDTH-1,0);
            AUTO_OUT(bresp  ,ID_WIDTH-1,0);
        };
        struct bresp_info_data {
            std::queue <bresp_each> queue;
            bool wait_ready = false;
        } bresp_info;
        void out_bresp(axi4 <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            if (bresp_info.wait_ready) {
                if (pin.bready) {
                    bresp_info.wait_ready = false;
                    pin.bvalid = 0;
                    queue.pop();
                }
            }
            else if (!bresp_info.queue.empty()) {
                bresp_each bresp_data = bresp_info.queue.front();
                pin.bid = bresp_data.bid;
                pin.bresp = bresp_data.bresp;
                pin.bready = 1;
            }
        }
};

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
                memcpy(buffer,mem[start_addr],size);
                return RESP_OKEY;
            }
            else return RESP_SLVERR;
        }
        unsigned char *mem;
        unsigned long mem_size;
};

#endif
