#ifndef AXI4_SLAVE
#define AXI4_SLAVE

#include "axi4.hpp"

#include <queue>
#include <algorithm>

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64, unsigned int ID_WIDTH = 4>
class axi4_slave {
    static_assert(D_WIDTH <= 64, "D_WIDTH should be <= 64.");
    public:
        void beat(axi4 <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            if (!sync_reset) {
                pin.arready = 1;
                sync_reset = true;
            }
            else {
                // release old transaction
                if (read_last && pin.rvalid) {
                    read_last = false;
                    pin.rvalid = 0;     // maybe change in the following code
                    pin.rlast = 0;
                    if (addr_wait) {
                        addr_wait = false;
                        read_busy = true;
                    }
                }
                // set arready before new address come, it will change read_busy and addr_wait status
                pin.arready = !read_busy && !addr_wait;
                // check new address come
                if (!read_busy && !addr_wait && pin.arvalid) {
                    read_info.init(pin);
                    if (read_last) addr_wait = true;
                    else read_busy = true;
                }
                // do read trascation
                if (read_busy) read_beat(pin);
            }
        }
    protected:
        virtual axi_resp do_read (unsigned long start_addr, unsigned long size, unsigned char* buffer) = 0;
        virtual axi_resp do_write(unsigned long start_addr, unsigned long size, const unsigned char* buffer) = 0;
    private:
        bool sync_reset = false;
        const unsigned int D_bytes = D_WIDTH / 8;
    private:
        bool read_busy = false; // during trascation except last
        bool read_last = false; // wait rready and free
        bool addr_wait = false; // ar ready, but waiting the last read to ready

        struct read_info_data {
            unsigned long   start_addr;
            AUTO_SIG(       arid        ,ID_WIDTH-1,0);
            axi_burst_type  burst_type;
            unsigned int    each_len;
            int             nr_trans;
            int             cur_trans;
            unsigned int    tot_len;
            bool            out_ready;
            bool            early_err;
            axi_resp        resp;
            char data[4096];
            bool check() {
                if (burst_type == BURST_RESERVED) return false;
                if (burst_type == BURST_WRAP && (start_addr % tot_len)) return false;
                unsigned long rem_addr = 4096 - (start_addr % 4096);
                if (tot_len > rem_addr) return false;
                if (each_len > D_WIDTH / 8) return false;
            }

            void read_beat(axi4 <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
                pin.rid = arid;
                pin.rvalid  = 1;
                bool update = false;
                if (pin.rready || cur_trans == -1) {
                    cur_trans += 1;
                    update = true;
                    if (cur_trans + 1 == nr_trans) {
                        read_last = true;
                        read_busy = false;
                    }
                }
                pin.rlast = read_last;
                if (update) {
                    if (early_err) {
                        pin.rresp = RESP_SLVERR;
                        pin.rdata = 0;
                    }
                    else if (burst_type == BURST_FIXED) {
                        pin.rresp = do_read(start_addr, tot_len, &data[start_addr % 4096]);
                        pin.rdata = *(AUTO_SIG(*,D_WIDTH-1,0))(&data[start_addr - (start_addr % D_bytes)]);
                    }
                    else { // INCR, WRAP
                        pin.rresp = resp;
                        pin.rdata = *(AUTO_SIG(*,D_WIDTH-1,0))(&data[start_addr - (start_addr % D_bytes)]);
                    }
                }
            }

            void init(axi4 <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
                arid        = pin.arid;
                burst_type  = pin.arburst;
                each_len    = 1 << pin.arsize;
                nr_trans    = pin.arlen + 1;
                start_addr  = pin.araddr;
                cur_trans   = -1;
                if (burst_type == BURST_WRAP) start_addr = pin.
                tot_len     = ( (burst_type == BURST_FIXED) ? each_len : each_len * nr_trans) - (start_addr % each_len); // first beat can be unaligned
                early_err   = check();
                if (!read_info.early_err && burst_type != BURST_FIXED) 
                    read_info.resp = do_read(read_info.start_addr,each_len * nr_trans,&read_info.data[start_addr % 4096]);
            }

        } read_info;
    /*
    private: write
        bool write_busy = false;
        struct bresp_each {
            AUTO_OUT(bid    ,ID_WIDTH-1,0);
            AUTO_OUT(bresp  ,ID_WIDTH-1,0);
        };
        struct bresp_info_data {
            std::queue <bresp_each> queue;
            bool wait_ready = false;
        } bresp_info;
        void write_beat(axi4 <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {

        }
        void bresp_beat(axi4 <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
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
        */
};

#endif
