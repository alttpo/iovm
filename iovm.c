#include "iovm.h"

#ifdef __cplusplus
extern "C" {
#endif

// iovm implementation

void iovm1_init(struct iovm1_t *vm) {
    vm->s = IOVM1_STATE_INIT;

#ifdef IOVM1_USE_USERDATA
    vm->userdata = 0;
#endif

    vm->m.ptr = 0;
    vm->m.len = 0;
    vm->m.off = 0;
    vm->next_off = 0;
}

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *proc, unsigned len) {
    if (vm->s != IOVM1_STATE_INIT) {
        return IOVM1_ERROR_INVALID_OPERATION_FOR_STATE;
    }

    // bounds checking:
    if (!proc) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->m.ptr = proc;
    vm->m.len = len;
    vm->m.off = 0;
    vm->next_off = 0;

    vm->s = IOVM1_STATE_LOADED;

    return IOVM1_SUCCESS;
}

#ifdef IOVM1_USE_USERDATA
void iovm1_set_userdata(struct iovm1_t *vm, void *userdata) {
    vm->userdata = userdata;
}

void *iovm1_get_userdata(struct iovm1_t *vm) {
    return vm->userdata;
}
#endif

enum iovm1_error iovm1_exec_reset(struct iovm1_t *vm) {
    if (vm->s < IOVM1_STATE_LOADED) {
        return IOVM1_ERROR_INVALID_OPERATION_FOR_STATE;
    }
    if (vm->s >= IOVM1_STATE_EXECUTE_NEXT && vm->s < IOVM1_STATE_ENDED) {
        return IOVM1_ERROR_INVALID_OPERATION_FOR_STATE;
    }

    vm->s = IOVM1_STATE_RESET;
    return IOVM1_SUCCESS;
}

enum iovm1_error host_memory_try_read_byte(struct iovm1_t *vn, enum iovm1_memory_chip c, uint24_t a, uint8_t *b);

// executes the next IOVM instruction
enum iovm1_error iovm1_exec(struct iovm1_t *vm) {
    // first check here to handle read/write/wait instructions -- for lower latency between loop iterations:
    switch (vm->s) {
        case IOVM1_STATE_ERRORED:
            // maintain errored state until explicit reset:
            return vm->e;
        case IOVM1_STATE_READ: {
        do_read:
            vm->e = host_memory_read_state_machine(vm);
            if (vm->e != IOVM1_SUCCESS) {
                vm->s = IOVM1_STATE_ERRORED;
                host_send_end(vm);
                return vm->e;
            }

            if (vm->rd.os == IOVM1_OPSTATE_COMPLETED) {
                // start next instruction:
                vm->s = IOVM1_STATE_EXECUTE_NEXT;
                vm->e = IOVM1_SUCCESS;
                break;
            }

            // host wants to be called back again:
            vm->e = IOVM1_SUCCESS;
            return vm->e;
        }
        case IOVM1_STATE_WRITE: {
        do_write:
            vm->e = host_memory_write_state_machine(vm);
            if (vm->e != IOVM1_SUCCESS) {
                vm->s = IOVM1_STATE_ERRORED;
                host_send_end(vm);
                return vm->e;
            }

            if (vm->wr.os == IOVM1_OPSTATE_COMPLETED) {
                // write complete; start next instruction:
                vm->s = IOVM1_STATE_EXECUTE_NEXT;
                vm->e = IOVM1_SUCCESS;
                break;
            }

            // host wants to be called back again:
            vm->e = IOVM1_SUCCESS;
            return vm->e;
        }
        case IOVM1_STATE_WAIT: {
        do_wait:
            vm->e = host_memory_wait_state_machine(vm);
            if (vm->e != IOVM1_SUCCESS) {
                vm->s = IOVM1_STATE_ERRORED;
                host_send_end(vm);
                return vm->e;
            }

            if (vm->wa.os == IOVM1_OPSTATE_COMPLETED) {
                // wait complete; start next instruction:
                vm->s = IOVM1_STATE_EXECUTE_NEXT;
                vm->e = IOVM1_SUCCESS;
                break;
            }

            // host wants to be called back again:
            vm->e = IOVM1_SUCCESS;
            return vm->e;
        }
        default:
            // on first execution, state machine lands here:
            if (vm->s < IOVM1_STATE_LOADED) {
                // must be LOADED before executing:
                vm->e = IOVM1_ERROR_INVALID_OPERATION_FOR_STATE;
                return vm->e;
            }
            if (vm->s == IOVM1_STATE_LOADED) {
                vm->s = IOVM1_STATE_RESET;
            }
            if (vm->s == IOVM1_STATE_RESET) {
                // reset execution state:
                vm->m.off = 0;
                vm->next_off = 0;
                vm->p = 0;
                vm->e = IOVM1_SUCCESS;
                vm->s = IOVM1_STATE_EXECUTE_NEXT;
            }
            break;
    }

    while (vm->s == IOVM1_STATE_EXECUTE_NEXT) {
        vm->m.off = vm->next_off;
        vm->p = vm->m.off;

        if (vm->m.off >= vm->m.len) {
            vm->s = IOVM1_STATE_ENDED;
            vm->e = IOVM1_SUCCESS;
            host_send_end(vm);
            return vm->e;
        }

        // read instruction byte:
        uint8_t x = vm->m.ptr[vm->m.off++];

        // instruction opcode:
        uint8_t o = IOVM1_INST_OPCODE(x);
        switch (o) {
            case IOVM1_OPCODE_READ: {
                vm->next_off = vm->m.off + 5;

                // memory chip identifier:
                vm->rd.c = (enum iovm1_memory_chip)vm->m.ptr[vm->m.off++];
                // 24-bit address:
                uint24_t lo = (uint24_t)(vm->m.ptr[vm->m.off++]);
                uint24_t hi = (uint24_t)(vm->m.ptr[vm->m.off++]) << 8;
                uint24_t bk = (uint24_t)(vm->m.ptr[vm->m.off++]) << 16;
                vm->rd.a = bk | hi | lo;
                // length of read in bytes:
                vm->rd.l_raw = vm->m.ptr[vm->m.off++];
                // translate 0 -> 256:
                vm->rd.l = vm->rd.l_raw;
                if (vm->rd.l == 0) { vm->rd.l = 256; }

                // perform entire read:
                vm->s = IOVM1_STATE_READ;
                vm->rd.os = IOVM1_OPSTATE_INIT;
                goto do_read;
            }
            case IOVM1_OPCODE_WRITE: {
                vm->next_off = vm->m.off + 5;

                // memory chip identifier:
                vm->wr.c = (enum iovm1_memory_chip)vm->m.ptr[vm->m.off++];
                // 24-bit address:
                uint24_t lo = (uint24_t)(vm->m.ptr[vm->m.off++]);
                uint24_t hi = (uint24_t)(vm->m.ptr[vm->m.off++]) << 8;
                uint24_t bk = (uint24_t)(vm->m.ptr[vm->m.off++]) << 16;
                vm->wr.a = bk | hi | lo;

                // length of read in bytes:
                vm->wr.l_raw = vm->m.ptr[vm->m.off++];
                // translate 0 -> 256:
                vm->wr.l = vm->wr.l_raw;
                if (vm->wr.l == 0) { vm->wr.l = 256; }

                vm->next_off += vm->wr.l;

                // perform entire write:
                vm->s = IOVM1_STATE_WRITE;
                vm->wr.os = IOVM1_OPSTATE_INIT;
                vm->wr.p = vm->m.off;
                goto do_write;
            }
            case IOVM1_OPCODE_WAIT_UNTIL: {
                vm->next_off = vm->m.off + 6;

                vm->wa.q = IOVM1_INST_CMP_OPERATOR(x);

                // memory chip identifier:
                vm->wa.c = (enum iovm1_memory_chip)vm->m.ptr[vm->m.off++];
                // 24-bit address:
                uint24_t lo = (uint24_t)(vm->m.ptr[vm->m.off++]);
                uint24_t hi = (uint24_t)(vm->m.ptr[vm->m.off++]) << 8;
                uint24_t bk = (uint24_t)(vm->m.ptr[vm->m.off++]) << 16;
                vm->wa.a = bk | hi | lo;

                // comparison byte
                vm->wa.v  = vm->m.ptr[vm->m.off++];
                // comparison mask
                vm->wa.k  = vm->m.ptr[vm->m.off++];

                // perform loop to wait until (comparison byte & mask) successfully compares to value:
                vm->s = IOVM1_STATE_WAIT;
                vm->wa.os = IOVM1_OPSTATE_INIT;
                goto do_wait;
            }
            case IOVM1_OPCODE_ABORT_IF: {
                vm->next_off = vm->m.off + 6;

                enum iovm1_cmp_operator q = IOVM1_INST_CMP_OPERATOR(x);

                // memory chip identifier:
                enum iovm1_memory_chip c = (enum iovm1_memory_chip)vm->m.ptr[vm->m.off++];
                // 24-bit address:
                uint24_t lo = (uint24_t)(vm->m.ptr[vm->m.off++]);
                uint24_t hi = (uint24_t)(vm->m.ptr[vm->m.off++]) << 8;
                uint24_t bk = (uint24_t)(vm->m.ptr[vm->m.off++]) << 16;
                uint24_t a = bk | hi | lo;

                // comparison byte
                uint8_t v  = vm->m.ptr[vm->m.off++];
                // comparison mask
                uint8_t k  = vm->m.ptr[vm->m.off++];

                uint8_t b;

                // try to read a byte from memory chip:
                if ((vm->e = host_memory_try_read_byte(vm, c, a, &b)) != IOVM1_SUCCESS) {
                    vm->s = IOVM1_STATE_ERRORED;
                    host_send_end(vm);
                    return vm->e;
                }

                // test comparison byte against mask and value:
                if (iovm1_memory_cmp(q, b & k, v)) {
                    // abort if true; send an abort message back to the client:
                    vm->s = IOVM1_STATE_ERRORED;
                    vm->e = IOVM1_ERROR_ABORTED;
                    host_send_end(vm);

                    return vm->e;
                }

                // do not abort if false:
                vm->e = IOVM1_SUCCESS;
                return vm->e;
            }
            default:
                // unknown opcode:
                vm->e = IOVM1_ERROR_UNKNOWN_OPCODE;
                vm->s = IOVM1_STATE_ERRORED;
                host_send_end(vm);
                return vm->e;
        }
    }

    vm->e = IOVM1_SUCCESS;
    return vm->e;
}

#ifdef __cplusplus
}
#endif
