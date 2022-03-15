#ifndef UARTLITE_H
#define UARTLITE_H

#include "axi4.hpp"
#include "mmio_dev.hpp"
#include <algorithm>

#define SR_TX_FIFO_FULL         (1<<3) /* transmit FIFO full */
#define SR_TX_FIFO_EMPTY        (1<<2) /* transmit FIFO empty */
#define SR_RX_FIFO_VALID_DATA   (1<<0) /* data in receive FIFO */
#define SR_RX_FIFO_FULL         (1<<1) /* receive FIFO full */

#define ULITE_CONTROL_RST_TX	0x01
#define ULITE_CONTROL_RST_RX	0x02

struct uartlite_regs {
    unsigned int rx_fifo;
    unsigned int tx_fifo;
    unsigned int status;
    unsigned int control;
};

class uartlite : public mmio_dev  {
    public:
        uartlite() {
            memset(&regs,0,sizeof(regs));
            regs.status = SR_TX_FIFO_EMPTY;
        }
        axi_resp do_read(unsigned long start_addr, unsigned long size, unsigned char* buffer) {
            //printf("mmio read %08lx size %lu\n",start_addr,size);
            //fflush(stdout);
            if (start_addr + size > sizeof(regs)) return RESP_DECERR;
            memcpy(buffer,((char*)(&regs))+start_addr,std::min(size,sizeof(regs)-start_addr));
            if (start_addr <= offsetof(uartlite_regs,rx_fifo) && offsetof(uartlite_regs,rx_fifo) <= start_addr + size) {
                // TODO: refresh rx fifo
            }
            return RESP_OKEY;
        }
        axi_resp do_write(unsigned long start_addr, unsigned long size, const unsigned char* buffer) {
            //printf("mmio write %08lx size %lu\n",start_addr,size);
            //for (int i=0;i<size;i++) printf("%02x",buffer[i]);
            //printf("\n");
            //fflush(stdout);
            if (start_addr + size > sizeof(regs)) return RESP_DECERR;
            memcpy(((char*)(&regs))+start_addr,buffer,std::min(size,sizeof(regs)-start_addr));
            if (start_addr <= offsetof(uartlite_regs,tx_fifo) && offsetof(uartlite_regs,tx_fifo) <= start_addr + size) {
                printf("%c",regs.tx_fifo);
            }
            return RESP_OKEY;
        }
    private:
        uartlite_regs regs;
};

#endif