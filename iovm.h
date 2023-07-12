#ifndef IOVM_H
#define IOVM_H

/*
    iovm.h: trivial I/O virtual machine execution engine

    user provides callback functions to perform custom read/write I/O tasks against various memory targets.
    callbacks are free to implement behavior however they wish so long as the function contracts are satisfied.
    it is recommended to place deadline timers on the implementations of while_neq_cb and while_eq_cb callbacks.
    read_cb and write_cb must always complete.

instructions:

   765 43210
  [ttt ooooo]

    o = opcode (0..31)
    t = target (0..7)
    - = reserved for future extension

memory:
    m[1...]:    linear memory of procedure, at least 1 byte
    a[8]:       24-bit address for each target, indexed by `t`

 registers:
    p:          points to current byte in m

opcodes (o):
  0=END:        ends procedure

  1=SETADDR:    sets target address 24-bit
                    set lo = m[p++]
                    set hi = m[p++] << 8
                    set bk = m[p++] << 16
                    set a[t] = bk | hi | lo

  2=SETOFFS:    sets target address 16-bit offset within bank
                    set lo = m[p++]
                    set hi = m[p++] << 8
                    set a[t] = (a[t] & 0xFF0000) | hi | lo

  3=SETBANK:    sets target address 8-bit bank
                    // replace bank byte:
                    set bk = m[p++] << 16
                    set a[t] = bk | (a[t] & 0x00FFFF)

  4=READ:       reads bytes from target
                    set len to m[p++] (translate 0 -> 256, else use 1..255)

                    read_cb(t, &a[t], len);
                    // expected behavior:
                    //  for n=0; n<c; n++ {
                    //      read(t, a[t]++)
                    //  }

  5=READ_N:     reads bytes from target without advancing address after complete
                    set len to m[p++] (translate 0 -> 256, else use 1..255)

                    read_n_cb(t, a[t], len);

                    // expected behavior:
                    //  set tmp = a[t]
                    //  for n=0; n<c; n++ {
                    //      read(t, tmp++)
                    //  }

  6=WRITE:      writes bytes to target
                    set len to m[p++] (translate 0 -> 256, else use 1..255)

                    // write while advancing a[t]:
                    write_cb(t, &a[t], len, &m[p]);

                    // expected behavior:
                    //  for n=0; n<c; n++ {
                    //      write(t, a[t]++, m[p++])
                    //  }

  7=WRITE_N:    writes bytes to target without advancing address after complete
                    set len to m[p++] (translate 0 -> 256, else use 1..255)

                    // write without advancing a[t]:
                    write_n_cb(t, a[t], len, &m[p]);

                    // expected behavior:
                    //  set tmp=a[t]
                    //  for n=0; n<c; n++ {
                    //      write(t, tmp++, m[p++])
                    //  }

  8=WHILE_NEQ:  waits while read_byte(t, a[t]) != m[p]
                    set q to m[p++]

                    // compare with `!=`
                    while_neq_cb(t, a[t], q);

                    // expected behavior:
                    //  while (read(t, a[t]) != q) {}

  9=WHILE_EQ:   waits while read_byte(t, a[t]) == m[p]
                    set q to m[p++]

                    // compare with `==`
                    while_eq_cb(t, a[t], Q);

                    // expected behavior:
                    //  while (read(t, a[t]) == q) {}

  10..31:       reserved
*/

#include <stdint.h>

enum iovm1_opcode {
    IOVM1_OPCODE_END,
    IOVM1_OPCODE_SETADDR,
    IOVM1_OPCODE_SETOFFS,
    IOVM1_OPCODE_SETBANK,
    IOVM1_OPCODE_READ,
    IOVM1_OPCODE_READ_N,
    IOVM1_OPCODE_WRITE,
    IOVM1_OPCODE_WRITE_N,
    IOVM1_OPCODE_WHILE_NEQ,
    IOVM1_OPCODE_WHILE_EQ
};

typedef unsigned iovm1_target;

#define IOVM1_TARGET_COUNT  (8)

#define IOVM1_INST_OPCODE(x)    ((enum iovm1_opcode) ((x)&31))
#define IOVM1_INST_TARGET(x)    ((iovm1_target) (((x)>>5)&7))

#define IOVM1_INST_END (0)

#define IOVM1_MKINST(o, t) ( \
     ((uint8_t)(o)&31) | \
    (((uint8_t)(t)&7)<<5) )

enum iovm1_state {
    IOVM1_STATE_INIT,
    IOVM1_STATE_LOADED,
    IOVM1_STATE_RESET,
    IOVM1_STATE_EXECUTE_NEXT,
    IOVM1_STATE_STALLED,
    IOVM1_STATE_ENDED
};

enum iovm1_error {
    IOVM1_SUCCESS,
    IOVM1_ERROR_OUT_OF_RANGE,
    IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE,
    IOVM1_ERROR_VM_UNKNOWN_OPCODE,
};

// required byte stream:

enum iovm1_stream_error_e {
    IOVM1_STREAM_OK,
    IOVM1_STREAM_EOF,
    IOVM1_STREAM_STALLED
};
enum iovm1_stream_error_e iovm1_stream_in(uint32_t i_size, uint32_t *o_size, uint8_t *o_bytes);

struct bslice {
    const uint8_t  *ptr;
    uint32_t        len;
    uint32_t        off;
};

// forward declaration of iovm1_t so it can be referred to by pointers:

struct iovm1_t;
struct iovm1_state_t {
    enum iovm1_opcode   opcode;
    iovm1_target        target;

    uint32_t            address;
    unsigned            len;

    struct bslice       i_data;

    uint8_t             comparison;
};

#ifdef IOVM1_USE_CALLBACKS
// callback typedef:

typedef void (*iovm1_callback_f)(struct iovm1_state_t *cb_state);
#else
// required function implementations by user:

// reads bytes from target.
void iovm1_read_cb(struct iovm1_state_t *cb_state);

// writes bytes from procedure memory to target.
void iovm1_write_cb(struct iovm1_state_t *cb_state);

// loops while reading a byte from target while it != comparison byte.
void iovm1_while_neq_cb(struct iovm1_state_t *cb_state);

// loops while reading a byte from target while it == comparison byte.
void iovm1_while_eq_cb(struct iovm1_state_t *cb_state);
#endif

// iovm1_t definition:

struct iovm1_t {
    // linear memory containing procedure instructions and immediate data
    struct bslice       m;

    // current state
    enum iovm1_state    s;
    // target addresses
    uint32_t            a[IOVM1_TARGET_COUNT];

    // state for resumption:
    struct iovm1_state_t    cb_state;

#ifdef IOVM1_USE_USERDATA
    const void *const       *userdata;
#endif

#ifdef IOVM1_USE_CALLBACKS
    iovm1_callback_f        read_cb;
    iovm1_callback_f        write_cb;
    iovm1_callback_f        while_neq_cb;
    iovm1_callback_f        while_eq_cb;
#endif
};

// core functions:

void iovm1_init(struct iovm1_t *vm);

#ifdef IOVM1_USE_CALLBACKS
enum iovm1_error iovm1_set_read_cb(struct iovm1_t *vm, iovm1_callback_f cb);
enum iovm1_error iovm1_set_write_cb(struct iovm1_t *vm, iovm1_callback_f cb);
enum iovm1_error iovm1_set_while_neq_cb(struct iovm1_t *vm, iovm1_callback_f cb);
enum iovm1_error iovm1_set_while_eq_cb(struct iovm1_t *vm, iovm1_callback_f cb);
#endif

#ifdef IOVM1_USE_USERDATA
enum iovm1_error iovm1_set_userdata(struct iovm1_t *vm, const void *userdata);
enum iovm1_error iovm1_get_userdata(struct iovm1_t *vm, const void **o_userdata);
#endif

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *proc, unsigned len);

enum iovm1_error iovm1_get_target_address(struct iovm1_t *vm, iovm1_target target, uint32_t *o_address);

enum iovm1_error iovm1_exec_reset(struct iovm1_t *vm);

static inline enum iovm1_state iovm1_get_exec_state(struct iovm1_t *vm) {
    return vm->s;
}

#endif //IOVM_H

#ifndef IOVM_NO_IMPL

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
enum iovm1_error iovm1_set_userdata(struct iovm1_t *vm, const void *userdata) {
    vm->userdata = userdata;
    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_get_userdata(struct iovm1_t *vm, const void **o_userdata) {
    *o_userdata = vm->userdata;
    return IOVM1_SUCCESS;
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
static inline enum iovm1_error iovm1_exec(struct iovm1_t *vm) {
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

                if (m.off + c >= m.len) {
                    // stall until the next buffer comes in:
                    s = IOVM1_STATE_STALLED;
                }

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

                if (m.off + c >= m.len) {
                    // stall until the next buffer comes in:
                    s = IOVM1_STATE_STALLED;
                    cb_state.len -= (cb_state.i_data.off - m.off);
                }

                m = cb_state.i_data;

                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WHILE_NEQ: {
                cb_state.comparison = m.ptr[m.off++];

                cb_state.i_data = m;
                cb_state.address = a[t];
                IOVM1_INVOKE_CALLBACK(while_neq_cb, &cb_state);

                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WHILE_EQ: {
                cb_state.comparison = m.ptr[m.off++];

                cb_state.i_data = m;
                cb_state.address = a[t];
                IOVM1_INVOKE_CALLBACK(while_eq_cb, &cb_state);

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

#endif
