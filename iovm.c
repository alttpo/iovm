/*
   iovm.c: I/O virtual machine to interact with FPGA-SPI
*/

/*
executes a VM procedure to accomplish custom tasks which require low-latency access to FPGA.

instructions:

   x x x x x xxx
  [t i r v - ooo] {data[...]}

    o = opcode (0..7)
    v = advance target address after read/write
    r = repeat mode
    i = immediate data mode (or invert mode)
    t = target (0 = SRAM, 1 = SNESCMD)
    - = reserved
    x = all 8 instruction bits

memory:
    data[]:     linear memory of procedure, at least 64 bytes
    A[2]:       address for each target, indexed by `t`

 registers:
    P:          points to current byte in `data`
    M:          data byte
    Q:          comparison byte
    C:          loop counter

opcodes (o):
  0=SETADDR:    sets target address (24-bit)
                    // all instruction bits == 0 ENDS the procedure:
                    if x==0 {
                        END
                    }
                    set lo = data[P++]
                    set hi = data[P++]
                    set bk = data[P++]
                    set A[t] = lo | (hi<<8) | (bk<<16)

  1=WHILE_NEQ:  waits while read(t) != data[P]
                    set Q to data[P++]
                    // i flag inverts check from `!=` to `==`
                    if i==1 {
                        while ((M = read(t)) == Q) ;
                    } else {
                        while ((M = read(t)) != Q) ;
                    }

  2=READ:       reads bytes and emits into USB response packet
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

  4..7:         reserved

 */

#include <string.h>
#include <stdint.h>

#include "iovm.h"

#define d vm->data
#define s vm->s

void iovm1_init(struct iovm1_t *vm) {
    s = IOVM1_STATE_INIT;

    vm->x = 0;

    vm->p = 0;
    vm->c = 0;
    vm->m = 0;
    vm->q = 0;

    vm->userdata = 0;
    vm->emit_size = 0;
    vm->stream_offs = 0;

    // initialize program memory:
    memset(d, 0, IOVM1_MAX_SIZE);
}

int iovm1_load(struct iovm1_t *vm, const uint8_t *data, unsigned len) {
    if (s != IOVM1_STATE_INIT) {
        return -1;
    }

    // bounds checking:
    if (len > IOVM1_MAX_SIZE) {
        return -1;
    }

    // copy in program data:
    memcpy(d, data, len);

    s = IOVM1_STATE_LOADED;

    return 0;
}

int iovm1_load_stream(struct iovm1_t *vm, const uint8_t *data, unsigned len) {
    if (s > IOVM1_STATE_LOAD_STREAMING) {
        return -1;
    }

    // bounds checking:
    if (vm->stream_offs + len > IOVM1_MAX_SIZE) {
        return -1;
    }

    // copy in program data:
    memcpy(d + vm->stream_offs, data, len);

    vm->stream_offs += len;

    s = IOVM1_STATE_LOAD_STREAMING;

    return 0;
}

int iovm1_load_stream_complete(struct iovm1_t *vm) {
    if (s != IOVM1_STATE_LOAD_STREAMING) {
        return -1;
    }

    s = IOVM1_STATE_LOADED;

    return 0;
}

int iovm1_verify(struct iovm1_t *vm) {
    if (s != IOVM1_STATE_LOADED) {
        return -1;
    }

    int p = 0;
    uint32_t size = 0;
    while (p < IOVM1_MAX_SIZE) {
        uint8_t x = d[p++];
        if (x == IOVM1_INST_END) {
            break;
        }

        enum iovm1_opcode_e o = IOVM1_INST_OPCODE(x);
        switch (o) {
            case IOVM1_OPCODE_SETADDR:
                p += 3;
                break;
            case IOVM1_OPCODE_WHILE_NEQ:
                p++;
                break;
            case IOVM1_OPCODE_READ: {
                int c;
                if (IOVM1_INST_REPEAT(x)) {
                    c = vm->data[p++];
                    if (c == 0) { c = 256; }
                } else {
                    c = 1;
                }
                // calculate the size of the response:
                size += c;
                if (IOVM1_INST_IMMED(x)) {
                    p += c;
                }
                break;
            }
            case IOVM1_OPCODE_WRITE: {
                int c;
                if (IOVM1_INST_REPEAT(x)) {
                    c = vm->data[p++];
                    if (c == 0) { c = 256; }
                } else {
                    c = 1;
                }
                if (IOVM1_INST_IMMED(x)) {
                    p += c;
                }
                break;
            }
            default:
                return -1;
        }
    }

    vm->emit_size = size;
    s = IOVM1_STATE_VERIFIED;
    return 0;
}

int iovm1_emit_size(struct iovm1_t *vm, uint32_t *o_size) {
    if (s < IOVM1_STATE_VERIFIED) {
        return -1;
    }

    *o_size = vm->emit_size;

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

int iovm1_exec_reset(struct iovm1_t *vm) {
    if (s < IOVM1_STATE_VERIFIED) {
        return -1;
    }
    if (s != IOVM1_STATE_ENDED) {
        return -1;
    }

    s = IOVM1_STATE_RESET;
    return 0;
}

int iovm1_exec_step(struct iovm1_t *vm) {
    enum iovm1_opcode_e o;
    int r;

    if (s < IOVM1_STATE_VERIFIED) {
        return -1;
    }

    switch (s) {
        case IOVM1_STATE_INIT:
        case IOVM1_STATE_LOADED:
            // must be VERIFIED before executing:
            return -1;
        case IOVM1_STATE_VERIFIED:
            s = IOVM1_STATE_RESET;
            // purposely fall through to initialize registers:
        case IOVM1_STATE_RESET:
            // initialize registers:
            x = 0;

            p = 0;
            c = 0;
            m = 0;
            q = 0;

            s = IOVM1_STATE_EXECUTE_NEXT;
            break;
        case IOVM1_STATE_READ_LOOP_END:
        case IOVM1_STATE_WRITE_LOOP_END:
        case IOVM1_STATE_WHILE_NEQ_LOOP_END:
        case IOVM1_STATE_WHILE_EQ_LOOP_END:
            s = IOVM1_STATE_EXECUTE_NEXT;
            // purposely fall through to execute next instruction:
        case IOVM1_STATE_EXECUTE_NEXT:
            x = d[p++];
            if (x == IOVM1_INST_END) {
                s = IOVM1_STATE_ENDED;
                return 0;
            }
            o = IOVM1_INST_OPCODE(x);
            switch (o) {
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
                    break;
                }
                case IOVM1_OPCODE_WHILE_NEQ:
                    q = d[p++];
                    if (IOVM1_INST_IMMED(x)) {
                        s = IOVM1_STATE_WHILE_EQ_LOOP_ITER;
                    } else {
                        s = IOVM1_STATE_WHILE_NEQ_LOOP_ITER;
                    }
                    break;
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
                        s = IOVM1_STATE_READ_LOOP_ITER;
                    } else {
                        s = IOVM1_STATE_WRITE_LOOP_ITER;
                    }
                    break;
                default:
                    // unknown opcode:
                    return -1;
            }
            break;
        case IOVM1_STATE_READ_LOOP_ITER:
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
                s = IOVM1_STATE_READ_LOOP_END;
            }
            break;
        case IOVM1_STATE_WRITE_LOOP_ITER:
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
                s = IOVM1_STATE_WRITE_LOOP_END;
            }
            break;
        case IOVM1_STATE_WHILE_NEQ_LOOP_ITER:
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
                s = IOVM1_STATE_WHILE_NEQ_LOOP_END;
            }
            break;
        case IOVM1_STATE_WHILE_EQ_LOOP_ITER:
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
                s = IOVM1_STATE_WHILE_EQ_LOOP_END;
            }
            break;
        case IOVM1_STATE_ENDED:
            return 0;
        case IOVM1_STATE_ERRORED:
            return -1;
        default:
            return -1;
    }
    return 0;
}

int iovm1_exec_while_abort(struct iovm1_t *vm) {
    if (s == IOVM1_STATE_WHILE_NEQ_LOOP_ITER) {
        s = IOVM1_STATE_WHILE_NEQ_LOOP_END;
        return 0;
    } else if (s == IOVM1_STATE_WHILE_EQ_LOOP_ITER) {
        s = IOVM1_STATE_WHILE_EQ_LOOP_END;
        return 0;
    } else {
        return -1;
    }
}

#undef m
#undef c
#undef p
#undef x
#undef s
#undef d
