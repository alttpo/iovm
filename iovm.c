/*
   iovm.c: trivial I/O virtual machine execution engine
*/

#include <string.h>
#include <stdint.h>

#include "iovm.h"

struct iovm1_t {
    enum iovm1_state  s;  // current state

    uint8_t     x;  // current instruction byte

    int         p;  // pointer into data[]
    int         c;  // counter
    uint8_t     m;  // M register
    uint8_t     q;  // comparison byte for WHILE_NEQ

    void        *userdata;
    int         user_last_error;

    uint32_t    emit_size;

    unsigned    stream_offs;
    uint8_t     data[IOVM1_MAX_SIZE];
};

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

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *data, unsigned len) {
    if (s != IOVM1_STATE_INIT) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    // bounds checking:
    if (len > IOVM1_MAX_SIZE) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    // copy in program data:
    memcpy(d, data, len);

    s = IOVM1_STATE_LOADED;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_load_stream(struct iovm1_t *vm, const uint8_t *data, unsigned len) {
    if (s > IOVM1_STATE_LOAD_STREAMING) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    // bounds checking:
    if (vm->stream_offs + len > IOVM1_MAX_SIZE) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    // copy in program data:
    memcpy(d + vm->stream_offs, data, len);

    vm->stream_offs += len;

    s = IOVM1_STATE_LOAD_STREAMING;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_load_stream_complete(struct iovm1_t *vm) {
    if (s != IOVM1_STATE_LOAD_STREAMING) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    s = IOVM1_STATE_LOADED;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_verify(struct iovm1_t *vm) {
    if (s != IOVM1_STATE_LOADED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    int p = 0;
    uint32_t size = 0;
    while (p < IOVM1_MAX_SIZE) {
        uint8_t x = d[p++];
        if (x == IOVM1_INST_END) {
            break;
        }

        enum iovm1_opcode o = IOVM1_INST_OPCODE(x);
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
                return IOVM1_ERROR_VM_UNKNOWN_OPCODE;
        }
    }

    vm->emit_size = size;
    s = IOVM1_STATE_VERIFIED;
    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_emit_size(struct iovm1_t *vm, uint32_t *o_size) {
    if (s < IOVM1_STATE_VERIFIED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    *o_size = vm->emit_size;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_set_userdata(struct iovm1_t *vm, void *userdata) {
    vm->userdata = userdata;
    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_get_userdata(struct iovm1_t *vm, void **o_userdata) {
    *o_userdata = vm->userdata;
    return IOVM1_SUCCESS;
}

#define x vm->x
#define p vm->p
#define c vm->c
#define m vm->m
#define q vm->q

enum iovm1_error iovm1_exec_reset(struct iovm1_t *vm) {
    if (s < IOVM1_STATE_VERIFIED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }
    if (s >= IOVM1_STATE_EXECUTE_NEXT && s < IOVM1_STATE_ENDED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    s = IOVM1_STATE_RESET;
    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_exec_step(struct iovm1_t *vm) {
    enum iovm1_opcode o;

    if (s < IOVM1_STATE_VERIFIED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    switch (s) {
        case IOVM1_STATE_INIT:
        case IOVM1_STATE_LOADED:
            // must be VERIFIED before executing:
            return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
        case IOVM1_STATE_VERIFIED:
            // RESET must be an observable state between loading and executing the first instruction:
            s = IOVM1_STATE_RESET;
            break;
        case IOVM1_STATE_RESET:
            // initialize registers:
            x = 0;

            p = 0;
            c = 0;
            m = 0;
            q = 0;

            vm->user_last_error = 0;

            s = IOVM1_STATE_EXECUTE_NEXT;
            break;
        case IOVM1_STATE_WHILE_NEQ_LOOP_END:
            s = IOVM1_STATE_EXECUTE_NEXT;
            // purposely fall through to execute next instruction:
        case IOVM1_STATE_EXECUTE_NEXT:
            x = d[p++];
            if (x == IOVM1_INST_END) {
                s = IOVM1_STATE_ENDED;
                return IOVM1_SUCCESS;
            }

            o = IOVM1_INST_OPCODE(x);
            switch (o) {
                case IOVM1_OPCODE_SETADDR: {
                    uint32_t lo = d[p++];
                    uint32_t hi = d[p++];
                    uint32_t bk = d[p++];
                    vm->user_last_error = iovm1_target_set_address(
                        vm,
                        IOVM1_INST_TARGET(x),
                        lo | (hi << 8) | (bk << 16)
                    );
                    break;
                }
                case IOVM1_OPCODE_WHILE_NEQ:
                    q = d[p++];
                    s = IOVM1_STATE_WHILE_NEQ_LOOP_ITER;
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
                        vm->user_last_error = iovm1_user_read(
                            vm,
                            IOVM1_INST_TARGET(x),
                            IOVM1_INST_ADVANCE(x),
                            c,
                            m
                        );
                    } else {
                        s = IOVM1_STATE_WRITE_LOOP_ITER;
                    }
                    break;
                default:
                    // unknown opcode:
                    return IOVM1_ERROR_VM_UNKNOWN_OPCODE;
            }
            break;
        case IOVM1_STATE_WHILE_NEQ_LOOP_ITER:
            // read from target and do not advance address:
            vm->user_last_error = iovm1_target_read(
                vm,
                IOVM1_INST_TARGET(x),
                0,
                &m
            );

            if (IOVM1_INST_IMMED(x)) {
                if (m != q) {
                    s = IOVM1_STATE_WHILE_NEQ_LOOP_END;
                }
            } else {
                if (m == q) {
                    s = IOVM1_STATE_WHILE_NEQ_LOOP_END;
                }
            }
            break;
        case IOVM1_STATE_READ_LOOP_ITER:
            if (IOVM1_INST_IMMED(x)) {
                m = d[p++];
            } else {
                // read from target and possibly advance address:
                vm->user_last_error = iovm1_target_read(
                    vm,
                    IOVM1_INST_TARGET(x),
                    IOVM1_INST_ADVANCE(x),
                    &m
                );
            }

            // emit response byte:
            vm->user_last_error = iovm1_emit(vm, m);

            if (--c == 0) {
                s = IOVM1_STATE_READ_LOOP_END;
            }
            break;
        case IOVM1_STATE_WRITE_LOOP_ITER:
            if (IOVM1_INST_IMMED(x)) {
                m = d[p++];
            }

            // write data to target and possibly advance address:
            vm->user_last_error = iovm1_target_write(
                vm,
                IOVM1_INST_TARGET(x),
                IOVM1_INST_ADVANCE(x),
                m
            );

            if (--c == 0) {
                s = IOVM1_STATE_WRITE_LOOP_END;
            }
            break;
        case IOVM1_STATE_ENDED:
            return IOVM1_SUCCESS;
        default:
            return IOVM1_ERROR_OUT_OF_RANGE;
    }
    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_exec_while_abort(struct iovm1_t *vm) {
    if (s < IOVM1_STATE_VERIFIED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    if (s == IOVM1_STATE_WHILE_NEQ_LOOP_ITER) {
        s = IOVM1_STATE_WHILE_NEQ_LOOP_END;
        return IOVM1_SUCCESS;
    } else {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }
}

enum iovm1_state iovm1_exec_state(struct iovm1_t *vm) {
    return s;
}

#undef m
#undef c
#undef p
#undef x
#undef s
#undef d
