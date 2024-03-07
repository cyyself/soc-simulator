#ifndef TILELINK_HPP
#define TILELINK_HPP

#include <cstdint>
#include <set>
#include <cstring>
#include "auto_sig.hpp"


template <unsigned int A_WIDTH, unsigned int W_WIDTH, unsigned int O_WIDTH,
          unsigned int Z_WIDTH>
struct tilelink;

template <unsigned int A_WIDTH, unsigned int W_WIDTH, unsigned int O_WIDTH,
          unsigned int Z_WIDTH>
struct tilelink_ptr {
    // a
    AUTO_IN (*a_bits_opcode , 2         , 0)    = NULL;
    AUTO_IN (*a_bits_param  , 2         , 0)    = NULL;
    AUTO_IN (*a_bits_size   , Z_WIDTH   , 0)    = NULL;
    AUTO_IN (*a_bits_source , O_WIDTH   , 0)    = NULL;
    AUTO_IN (*a_bits_address, A_WIDTH   , 0)    = NULL;
    AUTO_IN (*a_bits_mask   , W_WIDTH   , 0)    = NULL;
    AUTO_IN (*a_bits_data   , W_WIDTH*8 , 0)    = NULL;
    AUTO_IN (*a_bits_corrupt, 0         , 0)    = NULL;
    AUTO_IN (*a_valid       , 0         , 0)    = NULL;
    AUTO_OUT(*a_ready       , 0         , 0)    = NULL;
    // d
    AUTO_OUT(*d_bits_opcode , 2         , 0)    = NULL;
    AUTO_OUT(*d_bits_param  , 2         , 0)    = NULL;
    AUTO_OUT(*d_bits_size   , Z_WIDTH   , 0)    = NULL;
    AUTO_OUT(*d_bits_source , O_WIDTH   , 0)    = NULL;
    // no sink here for TL-UH
    AUTO_OUT(*d_bits_denied , 0         , 0)    = NULL;
    AUTO_OUT(*d_bits_data   , W_WIDTH*8 , 0)    = NULL;
    AUTO_OUT(*d_bits_corrupt, 0         , 0)    = NULL;
    AUTO_OUT(*d_valid       , 0         , 0)    = NULL;
    AUTO_IN (*d_ready       , 0         , 0)    = NULL;
    bool check() {
        std::set <void*> s;
        s.insert((void*)a_bits_opcode);
        s.insert((void*)a_bits_param);
        s.insert((void*)a_bits_size);
        s.insert((void*)a_bits_source);
        s.insert((void*)a_bits_address);
        s.insert((void*)a_bits_mask);
        s.insert((void*)a_bits_data);
        s.insert((void*)a_bits_corrupt);
        s.insert((void*)a_valid);
        s.insert((void*)a_ready);
        s.insert((void*)d_bits_opcode);
        s.insert((void*)d_bits_param);
        s.insert((void*)d_bits_size);
        s.insert((void*)d_bits_source);
        s.insert((void*)d_bits_denied);
        s.insert((void*)d_bits_data);
        s.insert((void*)d_bits_corrupt);
        s.insert((void*)d_valid);
        s.insert((void*)d_ready);
        return s.size() == 19;
    }
};

template <unsigned int A_WIDTH, unsigned int W_WIDTH, unsigned int O_WIDTH,
          unsigned int Z_WIDTH>
struct tilelink_ref {
    // a
    AUTO_IN (&a_bits_opcode , 2         , 0);
    AUTO_IN (&a_bits_param  , 2         , 0);
    AUTO_IN (&a_bits_size   , Z_WIDTH   , 0);
    AUTO_IN (&a_bits_source , O_WIDTH   , 0);
    AUTO_IN (&a_bits_address, A_WIDTH   , 0);
    AUTO_IN (&a_bits_mask   , W_WIDTH   , 0);
    AUTO_IN (&a_bits_data   , W_WIDTH*8 , 0);
    AUTO_IN (&a_bits_corrupt, 0         , 0);
    AUTO_IN (&a_valid       , 0         , 0);
    AUTO_OUT(&a_ready      , 0         , 0);
    // d
    AUTO_OUT(&d_bits_opcode , 2         , 0);
    AUTO_OUT(&d_bits_param  , 2         , 0);
    AUTO_OUT(&d_bits_size   , Z_WIDTH   , 0);
    AUTO_OUT(&d_bits_source , O_WIDTH   , 0);
    // no sink here for TL-UH
    AUTO_OUT(&d_bits_denied , 0         , 0);
    AUTO_OUT(&d_bits_data   , W_WIDTH*8 , 0);
    AUTO_OUT(&d_bits_corrupt, 0         , 0);
    AUTO_OUT(&d_valid       , 0         , 0);
    AUTO_IN (&d_ready       , 0         , 0);

    tilelink_ref(tilelink_ptr <A_WIDTH, W_WIDTH, O_WIDTH, Z_WIDTH> &p):
        a_bits_opcode (*(p.a_bits_opcode)),
        a_bits_param  (*(p.a_bits_param)),
        a_bits_size   (*(p.a_bits_size)),
        a_bits_source (*(p.a_bits_source)),
        a_bits_address(*(p.a_bits_address)),
        a_bits_mask   (*(p.a_bits_mask)),
        a_bits_data   (*(p.a_bits_data)),
        a_bits_corrupt(*(p.a_bits_corrupt)),
        a_valid       (*(p.a_valid)),
        a_ready       (*(p.a_ready),
        d_bits_opcode (*(p.d_bits_opcode)),
        d_bits_param  (*(p.d_bits_param)),
        d_bits_size   (*(p.d_bits_size)),
        d_bits_source (*(p.d_bits_source)),
        d_bits_denied (*(p.d_bits_denied)),
        d_bits_data   (*(p.d_bits_data)),
        d_bits_corrupt(*(p.d_bits_corrupt)),
        d_valid       (*(p.d_valid)),
        d_ready       (*(p.d_ready) {
    }

    tilelink_ref(tilelink <A_WIDTH, W_WIDTH, O_WIDTH, Z_WIDTH> &p);
};

template <unsigned int A_WIDTH, unsigned int W_WIDTH, unsigned int O_WIDTH,
          unsigned int Z_WIDTH>
struct tilelink {
    // a
    AUTO_IN (a_bits_opcode , 2         , 0);
    AUTO_IN (a_bits_param  , 2         , 0);
    AUTO_IN (a_bits_size   , Z_WIDTH   , 0);
    AUTO_IN (a_bits_source , O_WIDTH   , 0);
    AUTO_IN (a_bits_address, A_WIDTH   , 0);
    AUTO_IN (a_bits_mask   , W_WIDTH   , 0);
    AUTO_IN (a_bits_data   , W_WIDTH*8 , 0);
    AUTO_IN (a_bits_corrupt, 0         , 0);
    AUTO_IN (a_valid       , 0         , 0);
    AUTO_OUT(a_ready       , 0         , 0);
    // d
    AUTO_OUT(d_bits_opcode , 2         , 0);
    AUTO_OUT(d_bits_param  , 2         , 0);
    AUTO_OUT(d_bits_size   , Z_WIDTH   , 0);
    AUTO_OUT(d_bits_source , O_WIDTH   , 0);
    // no sink here for TL-UH
    AUTO_OUT(d_bits_denied , 0         , 0);
    AUTO_OUT(d_bits_data   , W_WIDTH*8 , 0);
    AUTO_OUT(d_bits_corrupt, 0         , 0);
    AUTO_OUT(d_valid       , 0         , 0);
    AUTO_IN (d_ready       , 0         , 0);
    
    tilelink() {
        // reset all pointer to zero
        memset(this, 0, sizeof(*this));
    }

    void update_input(tilelink_ref <A_WIDTH, W_WIDTH, O_WIDTH, Z_WIDTH> &p) {
        a_bits_opcode  = p.a_bits_opcode;
        a_bits_param   = p.a_bits_param;
        a_bits_size    = p.a_bits_size;
        a_bits_source  = p.a_bits_source;
        a_bits_address = p.a_bits_address;
        a_bits_mask    = p.a_bits_mask;
        a_bits_data    = p.a_bits_data;
        a_bits_corrupt = p.a_bits_corrupt;
        a_valid        = p.a_valid;
        d_ready        = p.d_ready;
    }

    void update_output(tilelink_ref <A_WIDTH, W_WIDTH, O_WIDTH, Z_WIDTH> &p) {
        p.a_ready        = a_ready;
        p.d_bits_opcode  = d_bits_opcode;
        p.d_bits_param   = d_bits_param;
        p.d_bits_size    = d_bits_size;
        p.d_bits_source  = d_bits_source;
        p.d_bits_denied  = d_bits_denied;
        p.d_bits_data    = d_bits_data;
        p.d_bits_corrupt = d_bits_corrupt;
        p.d_valid        = d_valid;
    }
};

template <unsigned int A_WIDTH, unsigned int W_WIDTH, unsigned int O_WIDTH,
          unsigned int Z_WIDTH>
tilelink_ref(tilelink <A_WIDTH, W_WIDTH, O_WIDTH, Z_WIDTH> &p):
    a_bits_opcode (p.a_bits_opcode),
    a_bits_param  (p.a_bits_param),
    a_bits_size   (p.a_bits_size),
    a_bits_source (p.a_bits_source),
    a_bits_address(p.a_bits_address),
    a_bits_mask   (p.a_bits_mask),
    a_bits_data   (p.a_bits_data),
    a_bits_corrupt(p.a_bits_corrupt),
    a_valid       (p.a_valid),
    a_ready       (p.a_ready),
    d_bits_opcode (p.d_bits_opcode),
    d_bits_param  (p.d_bits_param),
    d_bits_size   (p.d_bits_size),
    d_bits_source (p.d_bits_source),
    d_bits_denied (p.d_bits_denied),
    d_bits_data   (p.d_bits_data),
    d_bits_corrupt(p.d_bits_corrupt),
    d_valid       (p.d_valid),
    d_ready       (p.d_ready) {
}

enum opcode_a {
    TL_A_PutFullData = 0x0,
    TL_A_PutPartialData = 0x1,
    TL_A_ArithmeticData = 0x2,
    TL_A_LogicalData = 0x3,
    TL_A_Get = 0x4,
    TL_A_Intent = 0x5
};

enum opcode_d {
    TL_D_AccessAck = 0x0,
    TL_D_AccessAckData = 0x1,
    TL_D_HintAck = 0x2
};

enum opcode_ArithmeticData {
    TL_PARAM_MIN    = 0x0,
    TL_PARAM_MAX    = 0x1,
    TL_PARAM_MINU   = 0x2,
    TL_PARAM_MAXU   = 0x3,
    TL_PARAM_ADD    = 0x4
};

enum opcode_LogicalData {
    TL_PARAM_XOR    = 0x0,
    TL_PARAM_OR     = 0x1,
    TL_PARAM_AND    = 0x2,
    TL_PARAM_SWAP   = 0x3
};

/*
    Note: packet data and mask alignment is not same as AXI, we have no
    any padding here.
 */

struct a_packet {
    opcode_a opcode;
    uint8_t param;
    uint64_t size;
    uint64_t source;
    uint64_t address;
    std::vector <char> data;
    std::vector <bool> mask;
    bool corrupt;
};

struct d_packet {
    opcode_d opcode;
    uint8_t param;
    uint64_t size;
    uint64_t source;
    std::vector <char> data;
    bool corrupt;
    bool denied;
};

#endif