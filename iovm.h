#ifndef IOVM_H
#define IOVM_H

/*
    iovm.h: trivial I/O virtual machine execution engine

    uses user-provided callbacks to perform custom read/write I/O tasks against various memory targets.

instructions:

   76 54 3210
  [-- tt oooo]

    o = opcode
    t = target
    - = reserved for future extension

memory:
    M[32...]:   linear memory of procedure, at least 32 bytes
    A[4]:       24-bit address for each target, indexed by `t`

 registers:
    P:          points to current byte in M

opcodes (o):
  0=END:        ends procedure

  1=SETOFFS:    sets target address 16-bit offset within bank
                    set lo = M[P++]
                    set hi = M[P++] << 8
                    set A[t] = (A[t] & 0xFF0000) | hi | lo

  2=SETBANK:    sets target address 8-bit bank
                    // replace bank byte:
                    set bk = M[P++] << 16
                    set A[t] = bk | (A[t] & 0x00FFFF)

  3=READ:       reads bytes from target
                    set C to M[P++] (translate 0 -> 256, else use 1..255)

                    // invoke user-supplied callback function:
                    read_cb(t, &A[t], C);
                    // expected behavior:
                    //for n=0; n<C; n++ {
                    //    read(t, A[t]++)
                    //}

  4=WRITE:      writes bytes to target
                    set C to M[P++] (translate 0 -> 256, else use 1..255)

                    // invoke user-supplied callback function:
                    write_cb(t, &A[t], C, &M[P]);
                    // expected behavior:
                    //for n=0; n<C; n++ {
                    //    write(t, A[t]++, M[P++])
                    //}

  5=WHILE_NEQ:  waits while read(t, A[t]) != M[P]
                    set Q to M[P++]

                    // invoke user-supplied callback function:
                    while_neq_cb(t, A[t], Q);
                    // expected behavior:
                    //while (read(t, A[t]) != Q) {}

  6=WHILE_EQ:   waits while read(t, A[t]) == M[P]
                    set Q to M[P++]

                    // invoke user-supplied callback function:
                    while_eq_cb(t, A[t], Q);
                    // expected behavior:
                    //while (read(t, A[t]) == Q) {}

  7..15:        reserved
*/

#include <stdint.h>

#define IOVM1_INST_OPCODE(x)    ((x)&15)
#define IOVM1_INST_TARGET(x)    (((x)>>4)&3)

#define IOVM1_INST_END (0)

#define IOVM1_MKINST(o, t) ( \
     ((uint8_t)(o)&15) | \
    (((uint8_t)(t)&3)<<4) )

enum iovm1_opcode {
    IOVM1_OPCODE_END,
    IOVM1_OPCODE_SETOFFS,
    IOVM1_OPCODE_SETBANK,
    IOVM1_OPCODE_READ,
    IOVM1_OPCODE_WRITE,
    IOVM1_OPCODE_WHILE_NEQ,
    IOVM1_OPCODE_WHILE_EQ
};

typedef unsigned iovm1_target;

#define IOVM1_TARGET_COUNT  (4)

enum iovm1_state {
    IOVM1_STATE_INIT,
    IOVM1_STATE_LOAD_STREAMING,
    IOVM1_STATE_LOADED,
    IOVM1_STATE_VERIFIED,
    IOVM1_STATE_RESET,
    IOVM1_STATE_EXECUTE_NEXT,
    IOVM1_STATE_ENDED
};

enum iovm1_error {
    IOVM1_SUCCESS,
    IOVM1_ERROR_OUT_OF_RANGE,
    IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE,
    IOVM1_ERROR_VM_UNKNOWN_OPCODE,
};

// forward declaration of iovm1_t so it can be referred to by pointers:

struct iovm1_t;

#ifdef IOVM1_USE_CALLBACKS
// callback typedefs:

typedef void (*iovm1_read_f)(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, unsigned len);
typedef void (*iovm1_write_f)(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, const uint8_t *i_data, unsigned len);
typedef void (*iovm1_while_neq_f)(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison);
typedef void (*iovm1_while_eq_f)(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison);
#else
// required function implementations by user:

void iovm1_read_cb(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, unsigned len);
void iovm1_write_cb(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, const uint8_t *i_data, unsigned len);
void iovm1_while_neq_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison);
void iovm1_while_eq_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison);
#endif

// iovm1_t definition:

struct iovm1_t {
    enum iovm1_state    s;      // current state

    unsigned            p;      // pointer into m[]
    uint32_t            a[4];   // target addresses

    const void *const  *userdata;

#ifdef IOVM1_USE_CALLBACKS
    iovm1_read_f        read_cb;
    iovm1_write_f       write_cb;
    iovm1_while_neq_f   while_neq_cb;
    iovm1_while_eq_f    while_eq_cb;
#endif

    uint32_t            total_read;     // total number of bytes to read
    uint32_t            total_write;    // total number of bytes to write

    // length of linear memory
    unsigned            len;
    // linear memory containing procedure instructions and immediate data
    const uint8_t *     m;
};

// core functions:

void iovm1_init(struct iovm1_t *vm);

#ifdef IOVM1_USE_CALLBACKS
enum iovm1_error iovm1_set_read_cb(struct iovm1_t *vm, iovm1_read_f cb);
enum iovm1_error iovm1_set_write_cb(struct iovm1_t *vm, iovm1_write_f cb);
enum iovm1_error iovm1_set_while_neq_cb(struct iovm1_t *vm, iovm1_while_neq_f cb);
enum iovm1_error iovm1_set_while_eq_cb(struct iovm1_t *vm, iovm1_while_eq_f cb);
#endif

enum iovm1_error iovm1_set_userdata(struct iovm1_t *vm, const void *userdata);
enum iovm1_error iovm1_get_userdata(struct iovm1_t *vm, const void **o_userdata);

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *proc, unsigned len);

enum iovm1_error iovm1_verify(struct iovm1_t *vm);

enum iovm1_error iovm1_get_total_read(struct iovm1_t *vm, uint32_t *o_bytes_read);
enum iovm1_error iovm1_get_total_write(struct iovm1_t *vm, uint32_t *o_bytes_write);

enum iovm1_error iovm1_get_target_address(struct iovm1_t *vm, iovm1_target target, uint32_t *o_address);

enum iovm1_error iovm1_exec_reset(struct iovm1_t *vm);
enum iovm1_error iovm1_exec_step(struct iovm1_t *vm);

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

    vm->p = 0;
    for (unsigned t = 0; t < IOVM1_TARGET_COUNT; t++) {
        vm->a[t] = 0;
    }

    vm->userdata = 0;

#ifdef IOVM1_USE_CALLBACKS
    vm->read_cb = 0;
    vm->write_cb = 0;
    vm->while_neq_cb = 0;
    vm->while_eq_cb = 0;
#endif

    vm->total_read = 0;
    vm->total_write = 0;

    vm->len = 0;
    m = 0;
}

#ifdef IOVM1_USE_CALLBACKS
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
#endif

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

#ifdef IOVM1_USE_CALLBACKS
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
#endif

    unsigned p = 0;
    while (p < vm->len) {
        uint32_t x = *(uint32_t *)(&m[p++]);
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
                c = ((x>>8)&0xFF);
                if (c == 0) { c = 256; }
                p++;
                vm->total_read += c;
                break;
            }
            case IOVM1_OPCODE_WRITE: {
                unsigned c;
                c = ((x>>8)&0xFF);
                if (c == 0) { c = 256; }
                p++;
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

#define p vm->p
#define a vm->a

enum iovm1_error iovm1_exec_step(struct iovm1_t *vm) {
    enum iovm1_opcode o;
    uint32_t x;
    unsigned t;

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
            p = 0;

            s = IOVM1_STATE_EXECUTE_NEXT;
            break;
        case IOVM1_STATE_EXECUTE_NEXT:
            x = *(uint32_t *)(&m[p++]);
            t = IOVM1_INST_TARGET(x);
            o = IOVM1_INST_OPCODE(x);
            switch (o) {
                case IOVM1_OPCODE_END:
                    s = IOVM1_STATE_ENDED;
                    return IOVM1_SUCCESS;
                case IOVM1_OPCODE_SETOFFS: {
                    uint32_t lo = (uint32_t)((x>>8)&0xFF);
                    uint32_t hi = (uint32_t)((x>>16)&0xFF) << 8;
                    a[t] = (a[t] & 0xFF0000) | hi | lo;
                    p += 2;
                    break;
                }
                case IOVM1_OPCODE_SETBANK: {
                    uint32_t bk = (uint32_t)((x>>8)&0xFF) << 16;
                    a[t] = (a[t] & 0x00FFFF) | bk;
                    p++;
                    break;
                }
                case IOVM1_OPCODE_READ: {
                    unsigned c;
                    c = ((x>>8)&0xFF);
                    if (c == 0) { c = 256; }
                    p++;

#ifdef IOVM1_USE_CALLBACKS
                    vm->read_cb(vm, t, &a[t], c);
#else
                    iovm1_read_cb(vm, t, &a[t], c);
#endif
                    break;
                }
                case IOVM1_OPCODE_WRITE: {
                    unsigned c;
                    c = ((x>>8)&0xFF);
                    if (c == 0) { c = 256; }
                    p++;

#ifdef IOVM1_USE_CALLBACKS
                    vm->write_cb(vm, t, &a[t], &m[p], c);
#else
                    iovm1_write_cb(vm, t, &a[t], &m[p], c);
#endif
                    p += c;
                    break;
                }
                case IOVM1_OPCODE_WHILE_NEQ: {
                    uint8_t q;
                    q = ((x>>8)&0xFF);
                    p++;

#ifdef IOVM1_USE_CALLBACKS
                    vm->while_neq_cb(vm, t, a[t], q);
#else
                    iovm1_while_neq_cb(vm, t, a[t], q);
#endif
                    break;
                }
                case IOVM1_OPCODE_WHILE_EQ: {
                    uint8_t q;
                    q = ((x>>8)&0xFF);
                    p++;

#ifdef IOVM1_USE_CALLBACKS
                    vm->while_eq_cb(vm, t, a[t], q);
#else
                    iovm1_while_eq_cb(vm, t, a[t], q);
#endif
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
#undef s
#undef m

#endif
