#include "iovm.h"

#ifdef __cplusplus
extern "C" {
#endif

// iovm implementation

#define s vm->s
#define m vm->m

void iovm1_init(struct iovm1_t *vm) {
    s = IOVM1_STATE_INIT;

    for (unsigned t = 0; t < IOVM1_TARGET_COUNT; t++) {
        vm->a[t] = 0;
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

    vm->cb_state.vm = vm;

    m.ptr = 0;
    m.len = 0;
    m.off = 0;
}

#ifdef IOVM1_USE_CALLBACKS
enum iovm1_error iovm1_set_read_cb(struct iovm1_t *vm, iovm1_callback_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->read_cb = cb;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_set_write_cb(struct iovm1_t *vm, iovm1_callback_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->write_cb = cb;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_set_while_neq_cb(struct iovm1_t *vm, iovm1_callback_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->while_neq_cb = cb;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_set_while_eq_cb(struct iovm1_t *vm, iovm1_callback_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->while_eq_cb = cb;

    return IOVM1_SUCCESS;
}
#  define IOVM1_INVOKE_CALLBACK(name, ...) vm->name(__VA_ARGS__)
#else
#  define IOVM1_INVOKE_CALLBACK(name, ...) iovm1_##name(__VA_ARGS__)
#endif

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *proc, unsigned len) {
    if (s != IOVM1_STATE_INIT) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    // bounds checking:
    if (!proc) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    m.ptr = proc;
    m.len = len;
    m.off = 0;

    s = IOVM1_STATE_LOADED;

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
    if (s < IOVM1_STATE_LOADED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }
    if (s >= IOVM1_STATE_EXECUTE_NEXT && s < IOVM1_STATE_ENDED) {
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }

    s = IOVM1_STATE_RESET;
    return IOVM1_SUCCESS;
}

#define a vm->a
#define cb_state vm->cb_state
#define t cb_state.target

// executes the IOVM procedure instructions up to and including the next callback and then returns immediately after
enum iovm1_error iovm1_exec(struct iovm1_t *vm) {
    if (s < IOVM1_STATE_LOADED) {
        // must be VERIFIED before executing:
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }
    if (s == IOVM1_STATE_LOADED) {
        s = IOVM1_STATE_RESET;
    }
    if (s == IOVM1_STATE_RESET) {
        // initialize registers:
        m.off = 0;

        s = IOVM1_STATE_EXECUTE_NEXT;
    }

    while (s == IOVM1_STATE_EXECUTE_NEXT) {
        uint32_t last_pc = m.off;
        uint8_t x = m.ptr[m.off++];

        cb_state.opcode = IOVM1_INST_OPCODE(x);
        if (cb_state.opcode == IOVM1_OPCODE_END) {
            s = IOVM1_STATE_ENDED;
            return IOVM1_SUCCESS;
        }

        t = IOVM1_INST_TARGET(x);
        switch (cb_state.opcode) {
            case IOVM1_OPCODE_SETADDR: {
                uint32_t lo = (uint32_t)(m.ptr[m.off++]);
                uint32_t hi = (uint32_t)(m.ptr[m.off++]) << 8;
                uint32_t bk = (uint32_t)(m.ptr[m.off++]) << 16;
                a[t] = bk | hi | lo;
                break;
            }
            case IOVM1_OPCODE_SETOFFS: {
                uint32_t lo = (uint32_t)(m.ptr[m.off++]);
                uint32_t hi = (uint32_t)(m.ptr[m.off++]) << 8;
                a[t] = (a[t] & 0xFF0000) | hi | lo;
                break;
            }
            case IOVM1_OPCODE_SETBANK: {
                uint32_t bk = (uint32_t)(m.ptr[m.off++]) << 16;
                a[t] = (a[t] & 0x00FFFF) | bk;
                break;
            }
            case IOVM1_OPCODE_READ: {
                cb_state.len = m.ptr[m.off++];
                if (cb_state.len == 0) { cb_state.len = 256; }

                cb_state.i_data = m;
                cb_state.address = a[t];
                IOVM1_INVOKE_CALLBACK(read_cb, &cb_state);
                a[t] = cb_state.address;

                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_READ_N: {
                cb_state.len = m.ptr[m.off++];
                if (cb_state.len == 0) { cb_state.len = 256; }

                cb_state.i_data = m;
                cb_state.address = a[t];
                IOVM1_INVOKE_CALLBACK(read_cb, &cb_state);

                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WRITE: {
                uint32_t c = m.ptr[m.off++];
                if (c == 0) { c = 256; }

                cb_state.len = c;
                cb_state.i_data = m;
                cb_state.address = a[t];
                IOVM1_INVOKE_CALLBACK(write_cb, &cb_state);

                a[t] = cb_state.address;
                m = cb_state.i_data;

                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WRITE_N: {
                uint32_t c = m.ptr[m.off++];
                if (c == 0) { c = 256; }

                cb_state.len = c;
                cb_state.i_data = m;
                cb_state.address = a[t];
                IOVM1_INVOKE_CALLBACK(write_cb, &cb_state);

                m = cb_state.i_data;

                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WHILE_NEQ: {
                cb_state.comparison = m.ptr[m.off++];

                cb_state.i_data = m;
                cb_state.address = a[t];
                // mark subsequent callback as completed by default:
                cb_state.completed = true;
                IOVM1_INVOKE_CALLBACK(while_neq_cb, &cb_state);

                // repeat the instruction if the callback failed or aborted:
                if (!cb_state.completed) {
                    m.off = last_pc;
                }

                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WHILE_EQ: {
                cb_state.comparison = m.ptr[m.off++];

                cb_state.i_data = m;
                cb_state.address = a[t];
                // mark subsequent callback as completed by default:
                cb_state.completed = true;
                IOVM1_INVOKE_CALLBACK(while_eq_cb, &cb_state);

                // repeat the instruction if the callback failed or aborted:
                if (!cb_state.completed) {
                    m.off = last_pc;
                }

                return IOVM1_SUCCESS;
            }
            default:
                // unknown opcode:
                return IOVM1_ERROR_VM_UNKNOWN_OPCODE;
        }
    }

    return IOVM1_SUCCESS;
}

#undef cb_state
#undef a
#undef s
#undef m

#ifdef __cplusplus
}
#endif
