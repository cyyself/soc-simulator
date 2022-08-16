#ifndef MMIO_MEM_H
#define MMIO_MEM_H

#include "axi4.hpp"
#include "mmio_dev.hpp"

#include <fstream>
#include <iostream>

extern bool running;

class mmio_mem : public mmio_dev  {
    public:
        mmio_mem(size_t size_bytes) {
            mem = new unsigned char[size_bytes];
            mem_size = size_bytes;
        }
        mmio_mem(size_t size_bytes, const unsigned char *init_binary, size_t init_binary_len): mmio_mem(size_bytes) {
            // Initalize memory 
            assert(init_binary_len <= size_bytes);
            memcpy(mem,init_binary,init_binary_len);
        }
        mmio_mem(size_t size_bytes, const char *init_file): mmio_mem(size_bytes) {
            std::ifstream file(init_file,std::ios::in | std::ios::binary | std::ios::ate);
            size_t file_size = file.tellg();
            file.seekg(std::ios_base::beg);
            if (file_size > mem_size) {
                std::cerr << "mmio_mem size is not big enough for init file." << std::endl;
                file_size = size_bytes;
            }
            file.read((char*)mem,file_size);
        }
        ~mmio_mem() {
            delete [] mem;
        }
        bool do_read(uint64_t start_addr, uint64_t size, uint8_t* buffer) {
            if (start_addr + size <= mem_size) {
                memcpy(buffer,&mem[start_addr],size);
                return true;
            }
            else if (allow_warp) {
                start_addr %= mem_size;
                if (start_addr + size <= mem_size) {
                    memcpy(buffer,&mem[start_addr],size);
                    return true;
                }
                else return false;
            }
            else return false;
        }
        bool do_write(uint64_t start_addr, uint64_t size, const uint8_t* buffer) {
            if (start_addr + size <= mem_size) {
                memcpy(&mem[start_addr],buffer,size);
                if (diff_mem_write) {
                    for (int i=0;i<size;i++) if (mem[start_addr+i] != diff_mem[start_addr+i]) {
                        running = false;
                        printf("Error writeback cache at addr %x\n",start_addr+i);
                    }
                }
                return true;
            }
            else if (allow_warp) {
                start_addr %= mem_size;
                if (start_addr + size <= mem_size) {
                    memcpy(&mem[start_addr],buffer,size);
                    if (diff_mem_write) {
                        for (int i=0;i<size;i++) if (mem[start_addr+i] != diff_mem[start_addr+i]) {
                            running = false;
                            printf("Error writeback cache at addr %x\n",start_addr+i);
                        }
                    }
                    return true;
                }
                else return false;
            }
            else return false;
        }
        void load_binary(uint64_t start_addr, const char *init_file) {
            std::ifstream file(init_file,std::ios::in | std::ios::binary | std::ios::ate);
            size_t file_size = file.tellg();
            file.seekg(std::ios_base::beg);
            if (start_addr >= mem_size || file_size > mem_size - start_addr) {
                std::cerr << "memory size is not big enough for init file." << std::endl;
                file_size = mem_size;
            }
            file.read((char*)mem+start_addr,file_size);
        }
        void save_binary(const char *filename) {
            std::ofstream file(filename, std::ios::out | std::ios::binary);
            file.write((char*)mem, mem_size);
        }
        void set_allow_warp(bool value) {
            allow_warp = true;
        }
        unsigned char *get_mem_ptr() {
            return mem;
        }
        void set_diff_mem(unsigned char *diff_mem_addr) {
            diff_mem = diff_mem_addr;
            diff_mem_write = true;
        }
    private:
        bool diff_mem_write = false;
        unsigned char *mem;
        unsigned char *diff_mem;
        size_t mem_size;
        bool allow_warp = false;
};

#endif