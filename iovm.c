/*
   iovm.c: trivial I/O virtual machine execution engine
*/

#include <stdint.h>

#include "iovm.h"

#define s vm->s
#define m vm->m

void iovm1_init(struct iovm1_t *vm) {
    s = IOVM1_STATE_INIT;

    vm->x = 0;
    vm->p = 0;
    for (unsigned t = 0; t < IOVM1_TARGET_COUNT; t++) {
        vm->a[t] = 0;
    }

    vm->userdata = 0;

    vm->read_cb = 0;
    vm->write_cb = 0;
    vm->while_neq_cb = 0;
    vm->while_eq_cb = 0;

    vm->total_read = 0;
    vm->total_write = 0;

    vm->len = 0;
    m = 0;
}

enum iovm1_error iovm1_set_read_cb(struct iovm1_t *vm, iovm1_read_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->read_cb = cb;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_set_write_cb(struct iovm1_t *vm, iovm1_write_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->write_cb = cb;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_set_while_neq_cb(struct iovm1_t *vm, iovm1_while_neq_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->while_neq_cb = cb;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_set_while_eq_cb(struct iovm1_t *vm, iovm1_while_eq_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->while_eq_cb = cb;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *proc, unsigned len) {
    if (s != IOVM1_STATE_INIT) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    // bounds checking:
    if (!proc) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    m = proc;
    vm->len = len;

    s = IOVM1_STATE_LOADED;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_verify(struct iovm1_t *vm) {
    if (s != IOVM1_STATE_LOADED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    // all callbacks are required:
    if (!vm->read_cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }
    if (!vm->write_cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }
    if (!vm->while_neq_cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }
    if (!vm->while_eq_cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    unsigned p = 0;
    while (p < vm->len) {
        uint8_t x = m[p++];
        enum iovm1_opcode o = IOVM1_INST_OPCODE(x);
        switch (o) {
            case IOVM1_OPCODE_END:
                // verified only when END opcode reached:
                s = IOVM1_STATE_VERIFIED;
                return IOVM1_SUCCESS;
            case IOVM1_OPCODE_SETOFFS:
                p += 2;
                break;
            case IOVM1_OPCODE_SETBANK:
                p += 1;
                break;
            case IOVM1_OPCODE_READ: {
                unsigned c;
                c = m[p++];
                if (c == 0) { c = 256; }
                vm->total_read += c;
                break;
            }
            case IOVM1_OPCODE_WRITE: {
                unsigned c;
                c = m[p++];
                if (c == 0) { c = 256; }
                p += c;
                vm->total_write += c;
                break;
            }
            case IOVM1_OPCODE_WHILE_NEQ:
            case IOVM1_OPCODE_WHILE_EQ:
                p++;
                break;
            default:
                return IOVM1_ERROR_VM_UNKNOWN_OPCODE;
        }
    }

    // reached end of buffer without END opcode:
    return IOVM1_ERROR_OUT_OF_RANGE;
}

enum iovm1_error iovm1_set_userdata(struct iovm1_t *vm, const void *userdata) {
    vm->userdata = userdata;
    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_get_userdata(struct iovm1_t *vm, const void **o_userdata) {
    *o_userdata = vm->userdata;
    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_get_total_read(struct iovm1_t *vm, uint32_t *o_bytes_read) {
    if (s < IOVM1_STATE_VERIFIED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    *o_bytes_read = vm->total_read;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_get_total_write(struct iovm1_t *vm, uint32_t *o_bytes_write) {
    if (s < IOVM1_STATE_VERIFIED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    *o_bytes_write = vm->total_write;

    return IOVM1_SUCCESS;
}

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

#define x vm->x
#define p vm->p
#define a vm->a

enum iovm1_error iovm1_exec_step(struct iovm1_t *vm) {
    enum iovm1_opcode o;
    unsigned t;

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

            s = IOVM1_STATE_EXECUTE_NEXT;
            break;
        case IOVM1_STATE_EXECUTE_NEXT:
            x = m[p++];
            t = IOVM1_INST_TARGET(x);
            o = IOVM1_INST_OPCODE(x);
            switch (o) {
                case IOVM1_OPCODE_END:
                    s = IOVM1_STATE_ENDED;
                    return IOVM1_SUCCESS;
                case IOVM1_OPCODE_SETOFFS: {
                    uint32_t lo = (uint32_t)m[p++];
                    uint32_t hi = (uint32_t)m[p++] << 8;
                    a[t] = (a[t] & 0xFF0000) | hi | lo;
                    break;
                }
                case IOVM1_OPCODE_SETBANK: {
                    uint32_t bk = (uint32_t)m[p++] << 16;
                    a[t] = (a[t] & 0x00FFFF) | bk;
                    break;
                }
                case IOVM1_OPCODE_READ: {
                    unsigned c = m[p++];
                    if (c == 0) { c = 256; }
                    vm->read_cb(vm, t, &a[t], c);
                    break;
                }
                case IOVM1_OPCODE_WRITE: {
                    unsigned c = m[p++];
                    if (c == 0) { c = 256; }
                    vm->write_cb(vm, t, &a[t], &m[p], c);
                    p += c;
                    break;
                }
                case IOVM1_OPCODE_WHILE_NEQ: {
                    uint8_t q = m[p++];
                    vm->while_neq_cb(vm, t, a[t], q);
                    break;
                }
                case IOVM1_OPCODE_WHILE_EQ: {
                    uint8_t q = m[p++];
                    vm->while_eq_cb(vm, t, a[t], q);
                    break;
                }
                default:
                    // unknown opcode:
                    return IOVM1_ERROR_VM_UNKNOWN_OPCODE;
            }
            break;
        case IOVM1_STATE_ENDED:
            return IOVM1_SUCCESS;
        default:
            return IOVM1_ERROR_OUT_OF_RANGE;
    }

    return IOVM1_SUCCESS;
}

#undef a
#undef p
#undef x
#undef s
#undef m
