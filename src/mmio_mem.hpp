#ifndef MMIO_MEM_H
#define MMIO_MEM_H

#include "axi4.hpp"
#include "mmio_dev.hpp"

#include <fstream>
#include <filesystem>
#include <iostream>

class mmio_mem : public mmio_dev  {
    public:
        mmio_mem(unsigned long size_bytes) {
            mem = new unsigned char[size_bytes];
            mem_size = size_bytes;
        }
        mmio_mem(unsigned long size_bytes, const unsigned char *init_binary, unsigned long init_binary_len):mmio_mem(size_bytes) {
            assert(init_binary_len <= size_bytes);
            memcpy(mem,init_binary,init_binary_len);
        }
        mmio_mem(unsigned long size_bytes, const char *init_file):mmio_mem(size_bytes) {
            unsigned long file_size = std::filesystem::file_size(init_file);
            if (file_size > mem_size) {
                std::cerr << "mmio_mem size is not big enough for init file." << std::endl;
                file_size = size_bytes;
            }
            std::ifstream file(init_file,std::ios::in | std::ios::binary);
            file.read((char*)mem,file_size);
        }
        ~mmio_mem() {
            delete [] mem;
        }
        axi_resp do_read(unsigned long start_addr, unsigned long size, unsigned char* buffer) {
            if (start_addr + size <= mem_size) {
                memcpy(buffer,&mem[start_addr],size);
                return RESP_OKEY;
            }
            else return RESP_DECERR;
        }
        axi_resp do_write(unsigned long start_addr, unsigned long size, const unsigned char* buffer) {
            if (start_addr + size <= mem_size) {
                memcpy(&mem[start_addr],buffer,size);
                return RESP_OKEY;
            }
            else return RESP_DECERR;
        }
    private:
        unsigned char *mem;
        unsigned long mem_size;
};

#endif