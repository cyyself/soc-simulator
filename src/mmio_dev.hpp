#ifndef MMIO_DEV_H
#define MMIO_DEV_H

#include "axi4.hpp"
class mmio_dev {
public:
    virtual bool do_read (uint64_t start_addr, uint64_t size, char* buffer) = 0;
    virtual bool do_write(uint64_t start_addr, uint64_t size, const char* buffer) = 0;
};

#endif