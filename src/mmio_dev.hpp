#ifndef MMIO_DEV_H
#define MMIO_DEV_H

#include "axi4.hpp"
class mmio_dev {
public:
    virtual axi_resp do_read (unsigned long start_addr, unsigned long size, unsigned char* buffer) = 0;
    virtual axi_resp do_write(unsigned long start_addr, unsigned long size, const unsigned char* buffer) = 0;
};

#endif