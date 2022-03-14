#ifndef AXI4_SLAVE
#define AXI4_SLAVE

#include "axi4.hpp"

#include <queue>
#include <algorithm>
#include <utility>
#include <vector>

template <unsigned int A_WIDTH = 64, unsigned int D_WIDTH = 64, unsigned int ID_WIDTH = 4>
class axi4_slave {
    static_assert(D_WIDTH <= 64, "D_WIDTH should be <= 64.");
    static_assert(A_WIDTH <= 64, "A_WIDTH should be <= 64.");
    public:
        void beat(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            read_channel(pin);
            write_channel(pin);
        }
    protected:
        virtual axi_resp do_read (unsigned long start_addr, unsigned long size, unsigned char* buffer) = 0;
        virtual axi_resp do_write(unsigned long start_addr, unsigned long size, const unsigned char* buffer) = 0;
    private:
        unsigned int D_bytes = D_WIDTH / 8;
    private:
        bool read_busy = false; // during trascation except last
        bool read_last = false; // wait rready and free
        bool read_wait = false; // ar ready, but waiting the last read to ready
        bool last_arready = false;
        unsigned long   r_start_addr;
        AUTO_SIG(       arid        ,ID_WIDTH-1,0);
        axi_burst_type  r_burst_type;
        unsigned int    r_each_len;
        unsigned int    r_nr_trans;
        unsigned int    r_cur_trans;
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
            if (pin.rready || r_cur_trans == 0) {
                r_cur_trans += 1;
                update = true;
                if (r_cur_trans == r_nr_trans) {
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
                    pin.rdata = *(AUTO_SIG(*,D_WIDTH-1,0))(&r_data[(r_start_addr % 4096) - (r_start_addr % D_bytes)]);
                }
                else { // INCR, WRAP
                    pin.rresp = r_resp;
                    unsigned long cur_align_addr = (r_start_addr % 4096) - (r_start_addr % D_bytes) + (r_cur_trans - 1) * r_each_len;
                    cur_align_addr -= cur_align_addr % D_bytes;
                    pin.rdata = *(AUTO_SIG(*,D_WIDTH-1,0))(&r_data[cur_align_addr]);
                }
            }
        }

        void read_init(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            arid            = pin.arid;
            r_burst_type    = static_cast<axi_burst_type>(pin.arburst);
            r_each_len      = 1 << pin.arsize;
            r_nr_trans      = pin.arlen + 1;
            r_start_addr    = pin.araddr;
            r_cur_trans     = 0;
            r_tot_len       = ( (r_burst_type == BURST_FIXED) ? r_each_len : r_each_len * r_nr_trans) - (r_start_addr % r_each_len); // first beat can be unaligned
            r_early_err     = !read_check();
            if (!r_early_err && r_burst_type != BURST_FIXED) 
                r_resp = do_read(static_cast<unsigned long>(r_start_addr), static_cast<unsigned long>(r_tot_len), &r_data[r_start_addr % 4096] );
        }

        void read_channel(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            // Read step 1. release old transaction
            if (read_last && pin.rvalid) {
                read_last = false;
                pin.rvalid = 0;     // maybe change in the following code
                pin.rlast = 0;
                if (read_wait) {
                    read_wait = false;
                    read_busy = true;
                }
            }
            // Read step 2. set arready before new address come, it will change read_busy and read_wait status
            pin.arready = !read_busy && !read_wait && !last_arready;
            // Read step 3. check new address come
            if (!read_busy && !read_wait && pin.arvalid && !last_arready) {
                read_init(pin);
                if (read_last) read_wait = true;
                else read_busy = true;
                last_arready = true;
            }
            else last_arready = false;
            // Read step 4. do read trascation
            if (read_busy) read_beat(pin);
        }
    private:
        bool write_busy = false;
        bool b_busy     = false;
        unsigned int    w_start_addr;
        AUTO_SIG(       awid        ,ID_WIDTH-1,0);
        axi_burst_type  w_burst_type;
        unsigned int    w_each_len;
        int             w_nr_trans;
        int             w_cur_trans;
        unsigned int    w_tot_len;
        bool            w_out_ready;
        bool            w_early_err;
        axi_resp        w_resp;
        unsigned char   w_buffer[D_WIDTH/8];
        bool write_check() {
            if (w_burst_type == BURST_RESERVED || w_burst_type == BURST_FIXED) return false;
            if (w_burst_type == BURST_WRAP && (w_start_addr % w_tot_len)) return false;
            unsigned long rem_addr = 4096 - (w_start_addr % 4096);
            if (w_tot_len > rem_addr) return false;
            if (w_each_len > D_WIDTH / 8) return false;
            return true;
        }
        void write_init(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            awid            = pin.awid;
            w_burst_type    = static_cast<axi_burst_type>(pin.awburst);
            w_each_len      = 1 << pin.awsize;
            w_nr_trans      = pin.awlen + 1;
            w_start_addr    = pin.awaddr;
            w_cur_trans     = 0;
            w_tot_len       = w_each_len * w_nr_trans - (w_start_addr % w_each_len);
            w_early_err     = !write_check();
            w_resp          = RESP_OKEY;
        }
        // pair<start,len>
        std::vector<std::pair<int,int> > strb_to_range(AUTO_IN (wstrb,(D_WIDTH/8)-1, 0), int st_pos, int ed_pos) {
            std::vector<std::pair<int,int> > res;
            int l = st_pos;
            while (l < ed_pos) {
                if ((wstrb >> l) & 1) {
                    int r = l;
                    while ((wstrb >> r) & 1 && r < ed_pos) r ++;
                    res.emplace_back(l,r-l);
                    l = r + 1;
                }
                else l ++;
            }
            return res;
        }
        void write_beat(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            if (pin.wvalid) {
                w_cur_trans += 1;
                if (w_cur_trans == w_nr_trans) {
                    write_busy = false;
                    b_busy = true;
                    /*
                    if (!pin.wlast) {
                        w_early_err = true;
                        assert(false);
                    }
                    */
                }
                if (w_early_err) return;
                unsigned long addr_base = w_cur_trans == 1 ? w_start_addr : (w_start_addr - (w_start_addr % w_each_len) + (w_cur_trans - 1) * w_each_len);
                unsigned long in_data_pos = addr_base % D_bytes;
                unsigned long rem_data_pos = w_each_len - (in_data_pos % w_each_len);
                std::vector<std::pair<int,int> > range = strb_to_range(pin.wstrb,in_data_pos,in_data_pos+rem_data_pos);
                for (std::pair<int,int> sub_range : range) {
                    int &addr = sub_range.first;
                    int &len  = sub_range.second;
                    memcpy(w_buffer,&(pin.wdata),sizeof(pin.wdata));
                    w_resp = static_cast<axi_resp>(static_cast<int>(w_resp) | static_cast<int>(do_write(addr_base+addr,len,w_buffer+in_data_pos)));
                }
            }
        }
        void b_beat(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            pin.bid = awid;
            pin.bresp = w_early_err ? RESP_SLVERR : w_resp;
            if (pin.bready) b_busy = false;
        }
        void write_channel(axi4_ref <A_WIDTH,D_WIDTH,ID_WIDTH> &pin) {
            pin.wready = 1;
            pin.awready = 0; // initial set to zero
            pin.bvalid = b_busy;
            if (b_busy) {
                b_beat(pin);
            }
            if (!write_busy && !b_busy) {
                if (pin.awvalid) {
                    pin.awready = 1;
                    write_init(pin);
                    write_busy = true;
                }
            }
            if (write_busy) {
                write_beat(pin);
            }
        }
};

#endif
