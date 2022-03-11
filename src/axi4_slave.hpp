#ifndef AXI4_SLAVE
#define AXI4_SLAVE

#include "axi4.hpp"

#include <queue>
#include <algorithm>

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64, unsigned int ID_WIDTH = 4>
class axi4_slave {
    static_assert(D_WIDTH <= 64, "D_WIDTH should be <= 64.");
    static_assert(A_WIDTH <= 64, "A_WIDTH should be <= 64.");
    public:
        void beat(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
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
                    if (read_wait) {
                        read_wait = false;
                        read_busy = true;
                    }
                }
                // set arready before new address come, it will change read_busy and read_wait status
                pin.arready = !read_busy && !read_wait;
                // check new address come
                if (!read_busy && !read_wait && pin.arvalid) {
                    read_init(pin);
                    if (read_last) read_wait = true;
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
        unsigned int D_bytes = D_WIDTH / 8;
    private:
        bool read_busy = false; // during trascation except last
        bool read_last = false; // wait rready and free
        bool read_wait = false; // ar ready, but waiting the last read to ready
        unsigned long   r_start_addr;
        AUTO_SIG(       arid        ,ID_WIDTH-1,0);
        axi_burst_type  r_burst_type;
        unsigned int    r_each_len;
        int             r_nr_trans;
        int             r_cur_trans;
        unsigned int    r_tot_len;
        bool            r_out_ready;
        bool            r_early_err;
        axi_resp        r_resp;
        unsigned char   r_data[4096];

        bool read_check() {
            if (r_burst_type == BURST_RESERVED) return false;
            if (r_burst_type == BURST_WRAP && (r_start_addr % r_tot_len)) return false;
            unsigned long rem_addr = 4096 - (r_start_addr % 4096);
            if (r_tot_len > rem_addr) return false;
            if (r_each_len > D_WIDTH / 8) return false;
            return true;
        }

        void read_beat(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            pin.rid = arid;
            pin.rvalid  = 1;
            bool update = false;
            if (pin.rready || r_cur_trans == -1) {
                r_cur_trans += 1;
                update = true;
                if (r_cur_trans + 1 == r_nr_trans) {
                    read_last = true;
                    read_busy = false;
                }
            }
            pin.rlast = read_last;
            if (update) {
                if (r_early_err) {
                    pin.rresp = RESP_SLVERR;
                    pin.rdata = 0;
                }
                else if (r_burst_type == BURST_FIXED) {
                    pin.rresp = do_read(static_cast<unsigned long>(r_start_addr), static_cast<unsigned long>(r_tot_len), &r_data[r_start_addr % 4096]);
                    pin.rdata = *(AUTO_SIG(*,D_WIDTH-1,0))(&r_data[r_start_addr - (r_start_addr % D_bytes)]);
                }
                else { // INCR, WRAP
                    pin.rresp = r_resp;
                    pin.rdata = *(AUTO_SIG(*,D_WIDTH-1,0))(&r_data[r_start_addr - (r_start_addr % D_bytes)]);
                }
            }
        }

        void read_init(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            arid            = pin.arid;
            r_burst_type    = static_cast<axi_burst_type>(pin.arburst);
            r_each_len      = 1 << pin.arsize;
            r_nr_trans      = pin.arlen + 1;
            r_start_addr    = pin.araddr;
            r_cur_trans     = -1;
            if (r_burst_type == BURST_WRAP) r_start_addr = pin.araddr;
            r_tot_len       = ( (r_burst_type == BURST_FIXED) ? r_each_len : r_each_len * r_nr_trans) - (r_start_addr % r_each_len); // first beat can be unaligned
            r_early_err     = !read_check();
            if (!r_early_err && r_burst_type != BURST_FIXED) 
                r_resp = do_read(static_cast<unsigned long>(r_start_addr), static_cast<unsigned long>(r_each_len * r_nr_trans), &r_data[r_start_addr % 4096] );
        }

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
