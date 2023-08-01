#include "iovm.h"

#ifdef __cplusplus
extern "C" {
#endif

// iovm implementation

void iovm1_init(struct iovm1_t *vm) {
    vm->s = IOVM1_STATE_INIT;

    for (unsigned c = 0; c < IOVM1_CHANNEL_COUNT; c++) {
        vm->a[c] = 0;
        vm->tdu[c] = 0;
        vm->len[c] = 0;
        vm->tim[c] = 0;
        vm->cmp[c] = 0;
        vm->msk[c] = 0xFF;
    }

#ifdef IOVM1_USE_USERDATA
    vm->userdata = 0;
#endif

#ifdef IOVM1_USE_CALLBACKS
    vm->read_cb = 0;
    vm->write_cb = 0;
    vm->while_neq_cb = 0;
    vm->while_eq_cb = 0;
#endif

    vm->m.ptr = 0;
    vm->m.len = 0;
    vm->m.off = 0;
}

#ifdef IOVM1_USE_CALLBACKS
enum iovm1_error iovm1_set_opcode_cb(struct iovm1_t *vm, iovm1_callback_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->opcode_cb = cb;

    return IOVM1_SUCCESS;
}
#  define IOVM1_INVOKE_CALLBACK(...) vm->opcode_cb(__VA_ARGS__)
#else
#  define IOVM1_INVOKE_CALLBACK(...) iovm1_opcode_cb(__VA_ARGS__)
#endif

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *proc, unsigned len) {
    if (vm->s != IOVM1_STATE_INIT) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
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
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }
    if (vm->s >= IOVM1_STATE_EXECUTE_NEXT && vm->s < IOVM1_STATE_ENDED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    vm->s = IOVM1_STATE_RESET;
    return IOVM1_SUCCESS;
}

static enum iovm1_error iovm1_exec_callback(struct iovm1_t *vm) {
    // continually invoke the callback function until it sets completed=true
    IOVM1_INVOKE_CALLBACK(vm, &vm->cbs);

    // clear initial run flag:
    vm->cbs.initial = false;

    // handle completion:
    if (vm->cbs.complete) {
        switch (vm->cbs.o) {
            case IOVM1_OPCODE_READ:
                if ((vm->tdu[vm->cbs.c] & 0x80) != 0) {
                    // update register's address post-completion:
                    vm->a[vm->cbs.c] = vm->cbs.a;
                }
                break;
            case IOVM1_OPCODE_WRITE:
                if ((vm->tdu[vm->cbs.c] & 0x80) != 0) {
                    // update register's address post-completion:
                    vm->a[vm->cbs.c] = vm->cbs.a;
                }
                // update program pointer after WRITE:
                vm->m.off = vm->cbs.p;
                break;
            default:
                break;
        }

        // move to next instruction:
        vm->s = IOVM1_STATE_EXECUTE_NEXT;
    }

    return IOVM1_SUCCESS;
}

// executes the IOVM procedure instructions up to and including the next callback and then returns immediately after
enum iovm1_error iovm1_exec(struct iovm1_t *vm) {
    if (vm->s == IOVM1_STATE_RESUME_CALLBACK) {
        return iovm1_exec_callback(vm);
    }

    if (vm->s < IOVM1_STATE_LOADED) {
        // must be VERIFIED before executing:
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }
    if (vm->s == IOVM1_STATE_LOADED) {
        vm->s = IOVM1_STATE_RESET;
    }
    if (vm->s == IOVM1_STATE_RESET) {
        // initialize registers:
        vm->m.off = 0;
        vm->cbs.o = (enum iovm1_opcode) 0;
        vm->cbs.c = 0;
        vm->cbs.a = 0;
        vm->cbs.tdu = 0;
        vm->cbs.t = 0;
        vm->cbs.d = false;
        vm->cbs.u = false;
        vm->cbs.p = 0;
        vm->cbs.cmp = 0;
        vm->cbs.msk = 0xFF;
        vm->cbs.len = 0;
        vm->cbs.tim = 0;
        vm->cbs.initial = false;
        vm->cbs.complete = false;

        vm->s = IOVM1_STATE_EXECUTE_NEXT;
    }

    while (vm->s == IOVM1_STATE_EXECUTE_NEXT) {
        uint8_t x = vm->m.ptr[vm->m.off++];

        vm->cbs.o = IOVM1_INST_OPCODE(x);
        if (vm->cbs.o == IOVM1_OPCODE_END) {
            vm->s = IOVM1_STATE_ENDED;
            return IOVM1_SUCCESS;
        }

        vm->cbs.c = IOVM1_INST_CHANNEL(x);

        uint32_t b;
        switch (vm->cbs.o) {
            case IOVM1_OPCODE_SETA8:
                b = (uint32_t)(vm->m.ptr[vm->m.off++]);
                vm->a[vm->cbs.c] = b;
                break;
            case IOVM1_OPCODE_SETA16:
                b = (uint32_t)(vm->m.ptr[vm->m.off++]);
                b |= (uint32_t)(vm->m.ptr[vm->m.off++]) << 8;
                vm->a[vm->cbs.c] = b;
                break;
            case IOVM1_OPCODE_SETA24:
                b = (uint32_t)(vm->m.ptr[vm->m.off++]);
                b |= (uint32_t)(vm->m.ptr[vm->m.off++]) << 8;
                b |= (uint32_t)(vm->m.ptr[vm->m.off++]) << 16;
                vm->a[vm->cbs.c] = b;
                break;
            case IOVM1_OPCODE_SETTV:
                vm->tdu[vm->cbs.c] = vm->m.ptr[vm->m.off++];
                break;
            case IOVM1_OPCODE_SETLEN:
                b = (uint32_t)(vm->m.ptr[vm->m.off++]);
                b |= (uint32_t)(vm->m.ptr[vm->m.off++]) << 8;
                vm->len[vm->cbs.c] = b;
                if (vm->len[vm->cbs.c] == 0) {
                    vm->len[vm->cbs.c] = 65536;
                }
                break;
            case IOVM1_OPCODE_SETCMPMSK:
                vm->cmp[vm->cbs.c] = vm->m.ptr[vm->m.off++];
                vm->msk[vm->cbs.c] = vm->m.ptr[vm->m.off++];
                break;
            case IOVM1_OPCODE_SETTIM:
                b = (uint32_t)(vm->m.ptr[vm->m.off++]);
                b |= (uint32_t)(vm->m.ptr[vm->m.off++]) << 8;
                b |= (uint32_t)(vm->m.ptr[vm->m.off++]) << 16;
                b |= (uint32_t)(vm->m.ptr[vm->m.off++]) << 24;
                vm->a[vm->cbs.c] = b;
                break;

            case IOVM1_OPCODE_READ:
            case IOVM1_OPCODE_WRITE:
            case IOVM1_OPCODE_WAIT_WHILE_NEQ:
            case IOVM1_OPCODE_WAIT_WHILE_EQ:
            case IOVM1_OPCODE_WAIT_WHILE_LT:
            case IOVM1_OPCODE_WAIT_WHILE_GT:
            case IOVM1_OPCODE_WAIT_WHILE_LTE:
            case IOVM1_OPCODE_WAIT_WHILE_GTE: {
                // all I/O ops defer to callback for implementation:
                vm->cbs.p = vm->m.off;
                vm->cbs.m = vm->m.ptr;
                vm->cbs.tdu = vm->tdu[vm->cbs.c];
                vm->cbs.t = vm->tdu[vm->cbs.c] & 0x3F;
                vm->cbs.d = (vm->tdu[vm->cbs.c] & 0x40) != 0;
                vm->cbs.u = (vm->tdu[vm->cbs.c] & 0x80) != 0;
                vm->cbs.a = vm->a[vm->cbs.c];
                vm->cbs.len = vm->len[vm->cbs.c];
                vm->cbs.tim = vm->tim[vm->cbs.c];
                vm->cbs.cmp = vm->cmp[vm->cbs.c];
                vm->cbs.msk = vm->msk[vm->cbs.c];
                vm->cbs.initial = true;
                vm->cbs.complete = false;
                vm->s = IOVM1_STATE_RESUME_CALLBACK;

                // execute the callback for at least the first iteration:
                return iovm1_exec_callback(vm);
            }

            default:
                // unknown opcode:
                return IOVM1_ERROR_VM_UNKNOWN_OPCODE;
        }
    }

    return IOVM1_SUCCESS;
}

#ifdef __cplusplus
}
#endif
