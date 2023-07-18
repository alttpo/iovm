#include "iovm.h"

#ifdef __cplusplus
extern "C" {
#endif

// iovm implementation

void iovm1_init(struct iovm1_t *vm) {
    vm->s = IOVM1_STATE_INIT;

    for (unsigned r = 0; r < IOVM1_REGISTER_COUNT; r++) {
        vm->a[r] = 0;
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

// executes the IOVM procedure instructions up to and including the next callback and then returns immediately after
enum iovm1_error iovm1_exec(struct iovm1_t *vm) {
    if (vm->s == IOVM1_STATE_RESUME_CALLBACK) {
        // continually invoke the callback function until it sets completed=true
        IOVM1_INVOKE_CALLBACK(vm, &vm->cbs);

        // handle completion:
        if (vm->cbs.completed) {
            switch (vm->cbs.o) {
                case IOVM1_OPCODE_READ:
                    // update register's address post-completion:
                    vm->a[vm->cbs.r] = vm->cbs.a;
                    break;
                case IOVM1_OPCODE_WRITE:
                    // update register's address post-completion:
                    vm->a[vm->cbs.r] = vm->cbs.a;
                    // fallthrough:
                case IOVM1_OPCODE_WRITE_N:
                    // update program pointer after WRITE or WRITE_N:
                    vm->m.off = vm->cbs.p;
                    break;
                default:
                    break;
            }

            vm->s = IOVM1_STATE_EXECUTE_NEXT;
        }

        return IOVM1_SUCCESS;
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
        vm->cbs.r = 0;
        vm->cbs.a = 0;
        vm->cbs.t = 0;
        vm->cbs.p = 0;
        vm->cbs.c = 0;
        vm->cbs.len = 0;
        vm->cbs.completed = false;

        vm->s = IOVM1_STATE_EXECUTE_NEXT;
    }

    while (vm->s == IOVM1_STATE_EXECUTE_NEXT) {
        uint8_t x = vm->m.ptr[vm->m.off++];

        vm->cbs.o = IOVM1_INST_OPCODE(x);
        if (vm->cbs.o == IOVM1_OPCODE_END) {
            vm->s = IOVM1_STATE_ENDED;
            return IOVM1_SUCCESS;
        }

        vm->cbs.r = IOVM1_INST_REGISTER(x);
        vm->cbs.a = vm->a[vm->cbs.r];
        vm->cbs.t = vm->t[vm->cbs.r];
        vm->cbs.p = vm->m.off;
        vm->cbs.completed = false;

        switch (vm->cbs.o) {
            case IOVM1_OPCODE_SETADDR: {
                vm->t[vm->cbs.r] = vm->m.ptr[vm->m.off++];
                uint32_t lo = (uint32_t)(vm->m.ptr[vm->m.off++]);
                uint32_t hi = (uint32_t)(vm->m.ptr[vm->m.off++]) << 8;
                uint32_t bk = (uint32_t)(vm->m.ptr[vm->m.off++]) << 16;
                vm->a[vm->cbs.r] = bk | hi | lo;
                break;
            }
            case IOVM1_OPCODE_READ:
            case IOVM1_OPCODE_READ_N:
            case IOVM1_OPCODE_WRITE:
            case IOVM1_OPCODE_WRITE_N: {
                vm->cbs.len = vm->m.ptr[vm->m.off++];
                if (vm->cbs.len == 0) { vm->cbs.len = 256; }

                vm->cbs.p = vm->m.off;
                vm->s = IOVM1_STATE_RESUME_CALLBACK;

                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WHILE_NEQ:
            case IOVM1_OPCODE_WHILE_EQ: {
                vm->cbs.c = vm->m.ptr[vm->m.off++];

                vm->cbs.p = vm->m.off;
                vm->s = IOVM1_STATE_RESUME_CALLBACK;

                return IOVM1_SUCCESS;
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
