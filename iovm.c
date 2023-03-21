/*
   iovm.c: I/O virtual machine to interact with FPGA-SPI
*/

/*
executes a VM procedure to accomplish custom tasks which require low-latency access to FPGA.

registers:
    P:          points to current byte in program stream
    M:          current data byte
    C:          loop counter
    A[4]:       holds address for each target, indexed by `t`

instructions:

  [tt i r v ooo] {data[...]}

    o = opcode (0..7)
    v = advance target address after read/write
    r = repeat mode
    i = immediate data mode
    t = target (0 = SRAM, 1 = SNESCMD, 2 = reserved, 3 = reserved)

opcodes (o):
  0=END:        ends procedure

  1=SETADDR:    sets target address (24-bit)
                    set lo = data[P++]
                    set hi = data[P++]
                    set bk = data[P++]
                    set A[t] = lo | (hi<<8) | (bk<<16)

  2=READ:       reads bytes into USB response packet
                    if r==1 {
                        set C to data[P++] (translate 0 -> 256, else use 1..255)
                    } else {
                        set C to 1
                    }

                    for n=0; n<C; n++ {
                        if i==1 {
                            set M to data[P++]
                        } else {
                            set M to read(t)
                            if v==1 { set A[t] += 1 }
                        }

                        append M byte to USB response packet
                    }

  3=WRITE:      writes bytes to target
                    if r==1 {
                        set C to data[P++] (translate 0 -> 256, else use 1..255)
                    } else {
                        set C to 1
                    }

                    for n=0; n<C; n++ {
                        if i==1 {
                            set M to data[P++]
                        }

                        write(t, M)
                        if t==SNESCMD { set A[t] += 1 }   # SNESCMD writes always advance address
                          elseif v==1 { set A[t] += 1 }
                    }

  4=WHILE_NEQ:  waits while read(t) != data[P]
                    while ((M = read(t)) != data[P]) ;

  5=WHILE_EQ:   waits while read(t) == data[P]
                    while ((M = read(t)) == data[P]) ;

  6..7:         reserved

 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "iovm.h"

#define d vm->data
#define s vm->s

int iovm1_load(struct iovm1_t *vm, unsigned len, const uint8_t *data) {
    s = IOVM1_STATE_UNLOADED;

    // initialize program memory:
    memset(d, 0, IOVM1_MAX_SIZE);

    // error checking:
    if (len > IOVM1_MAX_SIZE) {
        return -1;
    }

    // copy in program data:
    memcpy(d, data, len);

    s = IOVM1_STATE_LOADED;

    return 0;
}

int iovm1_response_size(struct iovm1_t *vm, uint32_t *o_size) {
    if (s == IOVM1_STATE_UNLOADED) {
        return -1;
    }

    int p = 0;
    uint32_t size = 0;
    while (p < IOVM1_MAX_SIZE) {
        uint8_t x = d[p++];
        enum iovm1_opcode_e o = IOVM1_INST_OPCODE(x);
        switch (o) {
            case IOVM1_OPCODE_END:
                goto exit;
            case IOVM1_OPCODE_SETADDR: {
                p += 3;
                break;
            }
            case IOVM1_OPCODE_READ: {
                int c = 1;
                if (IOVM1_INST_REPEAT(x)) {
                    c = vm->data[p++];
                    if (c == 0) { c = 256; }
                }
                // calculate the size of the response:
                size += c;
                if (IOVM1_INST_IMMED(x)) {
                    p += c;
                }
                break;
            }
            case IOVM1_OPCODE_WRITE: {
                int c = 1;
                if (IOVM1_INST_REPEAT(x)) {
                    c = vm->data[p++];
                    if (c == 0) { c = 256; }
                }
                if (IOVM1_INST_IMMED(x)) {
                    p += c;
                }
            }
            case IOVM1_OPCODE_WHILE_EQ:
            case IOVM1_OPCODE_WHILE_NEQ: {
                p++;
                break;
            }
        }
    }

    exit:
    *o_size = size;
    return 0;
}

int iovm1_set_userdata(struct iovm1_t *vm, void *userdata) {
    vm->userdata = userdata;
    return 0;
}

int iovm1_get_userdata(struct iovm1_t *vm, void **o_userdata) {
    *o_userdata = vm->userdata;
    return 0;
}

#define x vm->x
#define p vm->p
#define c vm->c
#define m vm->m
#define q vm->q

int iovm1_reset(struct iovm1_t *vm) {
    if (s == IOVM1_STATE_UNLOADED) {
        return -1;
    }

    s = IOVM1_STATE_LOADED;
    return 0;
}

int iovm1_exec_step(struct iovm1_t *vm) {
    enum iovm1_opcode_e o;
    int r;

    switch (s) {
        case IOVM1_STATE_UNLOADED:
            return -1;
        case IOVM1_STATE_LOADED:
            // initialize registers:
            x = 0;

            p = 0;
            c = 0;
            m = 0;
            q = 0;

            s = IOVM1_STATE_EXECUTING;
            return 0;
        case IOVM1_STATE_EXECUTING:
            x = d[p++];
            o = IOVM1_INST_OPCODE(x);
            switch (o) {
                case IOVM1_OPCODE_END:
                    s = IOVM1_STATE_ENDED;
                    return 0;
                case IOVM1_OPCODE_SETADDR: {
                    uint32_t lo = d[p++];
                    uint32_t hi = d[p++];
                    uint32_t bk = d[p++];
                    r = iovm1_target_set_address(
                        vm,
                        IOVM1_INST_TARGET(x),
                        lo | (hi << 8) | (bk << 16)
                    );
                    if (r) {
                        s = IOVM1_STATE_ERRORED;
                        return r;
                    }
                    return 0;
                }
                case IOVM1_OPCODE_READ:
                case IOVM1_OPCODE_WRITE:
                    if (IOVM1_INST_REPEAT(x)) {
                        c = d[p++];
                        if (c == 0) { c = 256; }
                    } else {
                        c = 1;
                    }
                    //assert(c > 0);

                    if (o == IOVM1_OPCODE_READ) {
                        s = IOVM1_STATE_READING;
                    } else {
                        s = IOVM1_STATE_WRITING;
                    }
                    return 0;
                case IOVM1_OPCODE_WHILE_NEQ:
                    q = d[p++];
                    s = IOVM1_STATE_WAITING_WHILE_NEQ;
                    return 0;
                case IOVM1_OPCODE_WHILE_EQ:
                    q = d[p++];
                    s = IOVM1_STATE_WAITING_WHILE_EQ;
                    return 0;
            }
            return -1;
        case IOVM1_STATE_READING:
            if (IOVM1_INST_IMMED(x)) {
                m = d[p++];
            } else {
                // read from target and possibly advance address:
                r = iovm1_target_read(
                    vm,
                    IOVM1_INST_TARGET(x),
                    IOVM1_INST_ADVANCE(x),
                    &m
                );
                if (r) {
                    s = IOVM1_STATE_ERRORED;
                    return r;
                }
            }

            // emit response byte:
            r = iovm1_emit(vm, m);
            if (r) {
                s = IOVM1_STATE_ERRORED;
                return r;
            }

            if (--c == 0) {
                s = IOVM1_STATE_EXECUTING;
            }
            return 0;
        case IOVM1_STATE_WRITING:
            if (IOVM1_INST_IMMED(x)) {
                m = d[p++];
            }

            // write data to target and possibly advance address:
            r = iovm1_target_write(
                vm,
                IOVM1_INST_TARGET(x),
                IOVM1_INST_ADVANCE(x),
                m
            );
            if (r) {
                s = IOVM1_STATE_ERRORED;
                return r;
            }

            if (--c == 0) {
                s = IOVM1_STATE_EXECUTING;
            }
            return 0;
        case IOVM1_STATE_WAITING_WHILE_NEQ:
            // read from target and do not advance address:
            r = iovm1_target_read(
                vm,
                IOVM1_INST_TARGET(x),
                0,
                &m
            );
            if (r) {
                s = IOVM1_STATE_ERRORED;
                return r;
            }

            if (m == q) {
                s = IOVM1_STATE_EXECUTING;
            }
            return 0;
        case IOVM1_STATE_WAITING_WHILE_EQ:
            // read from target and do not advance address:
            r = iovm1_target_read(
                vm,
                IOVM1_INST_TARGET(x),
                0,
                &m
            );
            if (r) {
                s = IOVM1_STATE_ERRORED;
                return r;
            }

            if (m != q) {
                s = IOVM1_STATE_EXECUTING;
            }
            return 0;
        case IOVM1_STATE_ENDED:
            return 0;
        case IOVM1_STATE_ERRORED:
            return -1;
        default:
            return -1;
    }
}

#undef m
#undef c
#undef p
#undef x
#undef s
#undef d
