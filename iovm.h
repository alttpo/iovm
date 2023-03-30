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
                    set c to m[p++] (translate 0 -> 256, else use 1..255)

                    read_cb(t, &a[t], c);
                    // expected behavior:
                    //  for n=0; n<c; n++ {
                    //      read(t, a[t]++)
                    //  }

  5=READ_N:     reads bytes from target
                    set c to m[p++] (translate 0 -> 256, else use 1..255)

                    read_n_cb(t, a[t], c);

                    // expected behavior:
                    //  set tmp = a[t]
                    //  for n=0; n<c; n++ {
                    //      read(t, tmp++)
                    //  }

  6=WRITE:      writes bytes to target
                    set c to m[p++] (translate 0 -> 256, else use 1..255)

                    // write while advancing a[t]:
                    write_cb(t, &a[t], c, &m[p]);

                    // expected behavior:
                    //  for n=0; n<c; n++ {
                    //      write(t, a[t]++, m[p++])
                    //  }

  7=WRITE_N:    writes bytes to target
                    set c to m[p++] (translate 0 -> 256, else use 1..255)

                    // write without advancing a[t]:
                    write_n_cb(t, a[t], c, &m[p]);

                    // expected behavior:
                    //  set tmp=a[t]
                    //  for n=0; n<c; n++ {
                    //      write(t, tmp++, m[p++])
                    //  }

  8=WHILE_NEQ:  waits while read(t, a[t]) != m[p]
                    set q to m[p++]

                    // compare with `!=`
                    while_neq_cb(t, a[t], q);

                    // expected behavior:
                    //  while (read(t, a[t]) != q) {}

  9=WHILE_EQ:   waits while read(t, a[t]) == m[p]
                    set q to m[p++]

                    // compare with `==`
                    while_eq_cb(t, a[t], Q);

                    // expected behavior:
                    //  while (read(t, a[t]) == q) {}

  10..31:       reserved
*/

#include <stdint.h>

#define IOVM1_INST_OPCODE(x)    ((x)&31)
#define IOVM1_INST_TARGET(x)    (((x)>>5)&7)

#define IOVM1_INST_END (0)

#define IOVM1_MKINST(o, t) ( \
     ((uint8_t)(o)&31) | \
    (((uint8_t)(t)&7)<<5) )

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
// reads from `target` at 24-bit address `*r_address` for `len` bytes in the range [1..256]
typedef void (*iovm1_read_n_f)(struct iovm1_t *vm, iovm1_target target, uint32_t address, unsigned len);
typedef void (*iovm1_write_f)(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, const uint8_t *i_data, unsigned len);
typedef void (*iovm1_write_n_f)(struct iovm1_t *vm, iovm1_target target, uint32_t address, const uint8_t *i_data, unsigned len);
typedef void (*iovm1_while_neq_f)(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison);
typedef void (*iovm1_while_eq_f)(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison);
#else
// required function implementations by user:

// reads bytes from target and advances the address.
// inputs:
//  `target`        identifies target to read from, range [0..3]
//  `r_address`     points to the 24-bit address managed by IOVM for the given target
//  `*r_address`    current 24-bit address for the given target to begin reading at
//  `len`           length in bytes to read, range [1..256]
// outputs:
//  `*r_address`    must be updated to be input `*r_address` + `len`
void iovm1_read_cb(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, unsigned len);

// reads bytes from target but does not advance the address.
// inputs:
//  `target`        identifies target to read from, range [0..3]
//  `address`       current 24-bit address for the given target to begin reading at
//  `len`           length in bytes to read, range [1..256]
void iovm1_read_n_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, unsigned len);

// writes bytes from procedure memory to target and advances the address.
// inputs:
//  `target`        identifies target to read from, range [0..3]
//  `r_address`     points to the 24-bit address managed by IOVM for the given target
//  `*r_address`    current 24-bit address for the given target to begin reading at
//  `i_data`        pointer to procedure memory to transfer data from
//  `len`           length in bytes to read, range [1..256]
// outputs:
//  `*r_address`    must be updated to be input `*r_address` + `len`
void iovm1_write_cb(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, const uint8_t *i_data, unsigned len);

// writes bytes from procedure memory to target but does not advance the address.
// inputs:
//  `target`        identifies target to read from, range [0..3]
//  `r_address`     current 24-bit address for the given target to begin reading at
//  `i_data`        pointer to procedure memory to transfer data from
//  `len`           length in bytes to read, range [1..256]
void iovm1_write_n_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, const uint8_t *i_data, unsigned len);

// loops while reading a byte from target while it != comparison byte. does not advance the address.
// inputs:
//  `target`        identifies target to read from, range [0..3]
//  `address`       current 24-bit address for the given target to read from
//  `comparison`    comparison byte
void iovm1_while_neq_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison);

// loops while reading a byte from target while it == comparison byte. does not advance the address.
// inputs:
//  `target`        identifies target to read from, range [0..3]
//  `address`       current 24-bit address for the given target to read from
//  `comparison`    comparison byte
void iovm1_while_eq_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison);
#endif

// iovm1_t definition:

struct iovm1_t {
    // length of linear memory
    unsigned            len;
    // linear memory containing procedure instructions and immediate data
    const uint8_t *     m;

    // current state
    enum iovm1_state    s;

    // pointer into m[]
    unsigned            p;
    // target addresses
    uint32_t            a[IOVM1_TARGET_COUNT];

    // total number of bytes to read
    uint32_t            total_read;
    // total number of bytes to write
    uint32_t            total_write;

#ifdef IOVM1_USE_USERDATA
    const void *const  *userdata;
#endif

#ifdef IOVM1_USE_CALLBACKS
    iovm1_read_f        read_cb;
    iovm1_read_n_f      read_n_cb;
    iovm1_write_f       write_cb;
    iovm1_write_n_f     write_n_cb;
    iovm1_while_neq_f   while_neq_cb;
    iovm1_while_eq_f    while_eq_cb;
#endif
};

// core functions:

void iovm1_init(struct iovm1_t *vm);

#ifdef IOVM1_USE_CALLBACKS
enum iovm1_error iovm1_set_read_cb(struct iovm1_t *vm, iovm1_read_f cb);
enum iovm1_error iovm1_set_read_n_cb(struct iovm1_t *vm, iovm1_read_n_f cb);
enum iovm1_error iovm1_set_write_cb(struct iovm1_t *vm, iovm1_write_f cb);
enum iovm1_error iovm1_set_write_n_cb(struct iovm1_t *vm, iovm1_write_n_f cb);
enum iovm1_error iovm1_set_while_neq_cb(struct iovm1_t *vm, iovm1_while_neq_f cb);
enum iovm1_error iovm1_set_while_eq_cb(struct iovm1_t *vm, iovm1_while_eq_f cb);
#endif

#ifdef IOVM1_USE_USERDATA
enum iovm1_error iovm1_set_userdata(struct iovm1_t *vm, const void *userdata);
enum iovm1_error iovm1_get_userdata(struct iovm1_t *vm, const void **o_userdata);
#endif

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *proc, unsigned len);

enum iovm1_error iovm1_verify(struct iovm1_t *vm);

enum iovm1_error iovm1_get_total_read(struct iovm1_t *vm, uint32_t *o_bytes_read);
enum iovm1_error iovm1_get_total_write(struct iovm1_t *vm, uint32_t *o_bytes_write);

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

    vm->p = 0;
    for (unsigned t = 0; t < IOVM1_TARGET_COUNT; t++) {
        vm->a[t] = 0;
    }

#ifdef IOVM1_USE_USERDATA
    vm->userdata = 0;
#endif

#ifdef IOVM1_USE_CALLBACKS
    vm->read_cb = 0;
    vm->read_n_cb = 0;
    vm->write_cb = 0;
    vm->write_n_cb = 0;
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

enum iovm1_error iovm1_set_read_n_cb(struct iovm1_t *vm, iovm1_read_n_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->read_n_cb = cb;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_set_write_cb(struct iovm1_t *vm, iovm1_write_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->write_cb = cb;

    return IOVM1_SUCCESS;
}

enum iovm1_error iovm1_set_write_n_cb(struct iovm1_t *vm, iovm1_write_n_f cb) {
    if (!cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }

    vm->write_n_cb = cb;

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
#  define IOVM1_INVOKE_CALLBACK(name, ...) vm->name(vm, __VA_ARGS__)
#else
#  define IOVM1_INVOKE_CALLBACK(name, ...) iovm1_##name(vm, __VA_ARGS__)
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
    if (!vm->read_n_cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }
    if (!vm->write_cb) {
        return IOVM1_ERROR_OUT_OF_RANGE;
    }
    if (!vm->write_n_cb) {
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
        uint8_t x = m[p++];
        enum iovm1_opcode o = IOVM1_INST_OPCODE(x);
        switch (o) {
            case IOVM1_OPCODE_END:
                // verified only when END opcode reached:
                s = IOVM1_STATE_VERIFIED;
                return IOVM1_SUCCESS;
            case IOVM1_OPCODE_SETADDR:
                p += 3;
                break;
            case IOVM1_OPCODE_SETOFFS:
                p += 2;
                break;
            case IOVM1_OPCODE_SETBANK:
                p += 1;
                break;
            case IOVM1_OPCODE_READ:
            case IOVM1_OPCODE_READ_N: {
                unsigned c = m[p++];
                if (c == 0) { c = 256; }
                vm->total_read += c;
                break;
            }
            case IOVM1_OPCODE_WRITE:
            case IOVM1_OPCODE_WRITE_N: {
                unsigned c = m[p++];
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

// executes the IOVM procedure instructions up to and including the next callback and then returns immediately after
static inline enum iovm1_error iovm1_exec(struct iovm1_t *vm) {
    enum iovm1_opcode o;

    if (s < IOVM1_STATE_VERIFIED) {
        // must be VERIFIED before executing:
        return IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE;
    }
    if (s == IOVM1_STATE_VERIFIED) {
        s = IOVM1_STATE_RESET;
    }
    if (s == IOVM1_STATE_RESET) {
        // initialize registers:
        p = 0;

        s = IOVM1_STATE_EXECUTE_NEXT;
    }

    while (s == IOVM1_STATE_EXECUTE_NEXT) {
        uint8_t x = m[p++];

        o = IOVM1_INST_OPCODE(x);
        if (o == IOVM1_OPCODE_END) {
            s = IOVM1_STATE_ENDED;
            return IOVM1_SUCCESS;
        }

        unsigned t = IOVM1_INST_TARGET(x);
        switch (o) {
            case IOVM1_OPCODE_SETADDR: {
                uint32_t lo = (uint32_t)(m[p++]);
                uint32_t hi = (uint32_t)(m[p++]) << 8;
                uint32_t bk = (uint32_t)(m[p++]) << 16;
                a[t] = bk | hi | lo;
                break;
            }
            case IOVM1_OPCODE_SETOFFS: {
                uint32_t lo = (uint32_t)(m[p++]);
                uint32_t hi = (uint32_t)(m[p++]) << 8;
                a[t] = (a[t] & 0xFF0000) | hi | lo;
                break;
            }
            case IOVM1_OPCODE_SETBANK: {
                uint32_t bk = (uint32_t)(m[p++]) << 16;
                a[t] = (a[t] & 0x00FFFF) | bk;
                break;
            }
            case IOVM1_OPCODE_READ: {
                unsigned c = m[p++];
                if (c == 0) { c = 256; }

                IOVM1_INVOKE_CALLBACK(read_cb, t, &a[t], c);
                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_READ_N: {
                unsigned c = m[p++];
                if (c == 0) { c = 256; }

                IOVM1_INVOKE_CALLBACK(read_n_cb, t, a[t], c);
                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WRITE: {
                unsigned c = m[p++];
                if (c == 0) { c = 256; }

                IOVM1_INVOKE_CALLBACK(write_cb, t, &a[t], &m[p], c);
                p += c;
                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WRITE_N: {
                unsigned c = m[p++];
                if (c == 0) { c = 256; }

                IOVM1_INVOKE_CALLBACK(write_n_cb, t, a[t], &m[p], c);
                p += c;
                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WHILE_NEQ: {
                uint8_t q = m[p++];

                IOVM1_INVOKE_CALLBACK(while_neq_cb, t, a[t], q);
                return IOVM1_SUCCESS;
            }
            case IOVM1_OPCODE_WHILE_EQ: {
                uint8_t q = m[p++];

                IOVM1_INVOKE_CALLBACK(while_eq_cb, t, a[t], q);
                return IOVM1_SUCCESS;
            }
            default:
                // unknown opcode:
                return IOVM1_ERROR_VM_UNKNOWN_OPCODE;
        }
    }

    return IOVM1_SUCCESS;
}

#undef a
#undef p
#undef s
#undef m

#endif
