#ifndef UARTLITE_H
#define UARTLITE_H

#include "axi4.hpp"
#include "mmio_dev.hpp"

class uartlite : public mmio_dev  {
    public:
        axi_resp do_read(unsigned long start_addr, unsigned long size, unsigned char* buffer) {
            memset(buffer,0,size);
            return RESP_OKEY;
        }
        axi_resp do_write(unsigned long start_addr, unsigned long size, const unsigned char* buffer) {
            for (int i=0;i<size;i++) printf("%02x",buffer[i]);
            printf("\n");
            return RESP_OKEY;
        }
};

#endif