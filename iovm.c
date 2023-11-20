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

typedef bool (*comparison_func_t)(uint8_t a, uint8_t b);

static bool cmp_eq   (uint8_t a, uint8_t b) { return a == b; }
static bool cmp_neq  (uint8_t a, uint8_t b) { return a != b; }
static bool cmp_lt   (uint8_t a, uint8_t b) { return a < b; }
static bool cmp_nlt  (uint8_t a, uint8_t b) { return a >= b; }
static bool cmp_gt   (uint8_t a, uint8_t b) { return a > b; }
static bool cmp_ngt  (uint8_t a, uint8_t b) { return a <= b; }
static bool cmp_false(uint8_t a, uint8_t b) { return false; }

static comparison_func_t comparison_funcs[8] = {
    cmp_eq,
    cmp_neq,
    cmp_lt,
    cmp_nlt,
    cmp_gt,
    cmp_ngt,
    // undefined:
    cmp_false,
    cmp_false
};

// executes the next IOVM instruction
enum iovm1_error iovm1_exec(struct iovm1_t *vm) {
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
        vm->s = IOVM1_STATE_EXECUTE_NEXT;
    }

    while (vm->s == IOVM1_STATE_EXECUTE_NEXT) {
        if (vm->m.off >= vm->m.len) {
            vm->s = IOVM1_STATE_ENDED;
            vm->e = IOVM1_SUCCESS;
            host_send_end(vm);
            return vm->e;
        }

        // read instruction byte:
        vm->p = vm->m.off++;
        uint8_t x = vm->m.ptr[vm->p];

        // instruction opcode:
        uint8_t o = IOVM1_INST_OPCODE(x);
        switch (o) {
            case IOVM1_OPCODE_READ: {
                // memory chip identifier:
                iovm1_memory_chip_t c = vm->m.ptr[vm->m.off++];
                // 24-bit address:
                uint24_t lo = (uint24_t)(vm->m.ptr[vm->m.off++]);
                uint24_t hi = (uint24_t)(vm->m.ptr[vm->m.off++]) << 8;
                uint24_t bk = (uint24_t)(vm->m.ptr[vm->m.off++]) << 16;
                uint24_t a = bk | hi | lo;
                // length of read in bytes:
                uint8_t l_raw = vm->m.ptr[vm->m.off++];
                int l = l_raw;
                if (l == 0) { l = 256; }

                // TODO: determine a better place for this buffer
                uint8_t dm[256];
                uint8_t *d = dm;

                // initialize memory controller for chip and starting address:
                if ((vm->e = host_memory_init(vm, c, a)) != IOVM1_SUCCESS) {
                    return vm->e;
                }
                // validate read:
                if ((vm->e = host_memory_read_validate(vm, l)) != IOVM1_SUCCESS) {
                    return vm->e;
                }

                // perform entire read:
                while (l-- > 0) {
                    *d++ = host_memory_read_auto_advance(vm);
                }

                // send read data back to client:
                host_send_read(vm, l_raw, d);

                vm->e = IOVM1_SUCCESS;
                return vm->e;
            }
            case IOVM1_OPCODE_WRITE: {
                // memory chip identifier:
                iovm1_memory_chip_t c = vm->m.ptr[vm->m.off++];
                // 24-bit address:
                uint24_t lo = (uint24_t)(vm->m.ptr[vm->m.off++]);
                uint24_t hi = (uint24_t)(vm->m.ptr[vm->m.off++]) << 8;
                uint24_t bk = (uint24_t)(vm->m.ptr[vm->m.off++]) << 16;
                uint24_t a = bk | hi | lo;

                // length of read in bytes:
                uint8_t l_raw = vm->m.ptr[vm->m.off++];
                int l = l_raw;
                if (l == 0) { l = 256; }

                // initialize memory controller for chip and starting address:
                if ((vm->e = host_memory_init(vm, c, a)) != IOVM1_SUCCESS) {
                    return vm->e;
                }
                // validate write:
                if ((vm->e = host_memory_write_validate(vm, l)) != IOVM1_SUCCESS) {
                    return vm->e;
                }

                // perform entire write:
                while (l-- > 0) {
                    host_memory_write_auto_advance(vm, vm->m.ptr[vm->m.off++]);
                }

                vm->e = IOVM1_SUCCESS;
                return vm->e;
            }
            case IOVM1_OPCODE_WAIT_UNTIL: {
                enum iovm1_cmp_operator q = IOVM1_INST_CMP_OPERATOR(x);

                // memory chip identifier:
                iovm1_memory_chip_t c = vm->m.ptr[vm->m.off++];
                // 24-bit address:
                uint24_t lo = (uint24_t)(vm->m.ptr[vm->m.off++]);
                uint24_t hi = (uint24_t)(vm->m.ptr[vm->m.off++]) << 8;
                uint24_t bk = (uint24_t)(vm->m.ptr[vm->m.off++]) << 16;
                uint24_t a = bk | hi | lo;

                // comparison byte
                uint8_t v  = vm->m.ptr[vm->m.off++];
                // comparison mask
                uint8_t k  = vm->m.ptr[vm->m.off++];

                // initialize memory controller for chip and starting address:
                if ((vm->e = host_memory_init(vm, c, a)) != IOVM1_SUCCESS) {
                    return vm->e;
                }
                // validate read:
                if ((vm->e = host_memory_read_validate(vm, 1)) != IOVM1_SUCCESS) {
                    return vm->e;
                }

                // perform loop to wait until comparison byte matches value:
                host_timer_reset(vm);
                while (!host_timer_elapsed(vm)) {
                    if (comparison_funcs[q](host_memory_read_no_advance(vm) & k, v)) {
                        // successful exit:
                        vm->e = IOVM1_SUCCESS;
                        return vm->e;
                    }
                }

                // timed out; send an abort message back to the client:
                vm->s = IOVM1_STATE_ENDED;
                vm->e = IOVM1_ERROR_TIMED_OUT;
                host_send_abort(vm);

                return vm->e;
            }
            case IOVM1_OPCODE_ABORT_IF: {
                enum iovm1_cmp_operator q = IOVM1_INST_CMP_OPERATOR(x);

                // memory chip identifier:
                iovm1_memory_chip_t c = vm->m.ptr[vm->m.off++];
                // 24-bit address:
                uint24_t lo = (uint24_t)(vm->m.ptr[vm->m.off++]);
                uint24_t hi = (uint24_t)(vm->m.ptr[vm->m.off++]) << 8;
                uint24_t bk = (uint24_t)(vm->m.ptr[vm->m.off++]) << 16;
                uint24_t a = bk | hi | lo;

                // comparison byte
                uint8_t v  = vm->m.ptr[vm->m.off++];
                // comparison mask
                uint8_t k  = vm->m.ptr[vm->m.off++];

                // initialize memory controller for chip and starting address:
                if ((vm->e = host_memory_init(vm, c, a)) != IOVM1_SUCCESS) {
                    return vm->e;
                }
                // validate read:
                if ((vm->e = host_memory_read_validate(vm, 1)) != IOVM1_SUCCESS) {
                    return vm->e;
                }

                // perform loop to wait until comparison byte matches value:
                if (comparison_funcs[q](host_memory_read_no_advance(vm) & k, v)) {
                    // successful exit:
                    vm->e = IOVM1_SUCCESS;
                    return vm->e;
                }

                // failed check; send an abort message back to the client:
                vm->s = IOVM1_STATE_ENDED;
                vm->e = IOVM1_ERROR_ABORTED;
                host_send_abort(vm);

                return vm->e;
            }
            default:
                // unknown opcode:
                vm->e = IOVM1_ERROR_UNKNOWN_OPCODE;
                return vm->e;
        }
    }

    vm->e = IOVM1_SUCCESS;
    return vm->e;
}

#ifdef __cplusplus
}
#endif
