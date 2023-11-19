#ifndef IOVM_H
#define IOVM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    iovm.h: low-latency embedded I/O virtual machine execution engine

    features / restrictions:
        * max of 4 instruction opcodes
        * no branching instructions
        * no state carried across instructions

    host MUST provide host_* named functions to implement the required memory controller interface.

memory:
    m[...]:             program memory, at least 1 byte

    NOTE: entire program must be buffered into memory before execution starts

instruction byte format:

   765432 10
  [?????? oo]

    o = opcode              [0..3]
    ? = varies by opcode

opcodes (o):
-----------------------
  0=READ:               reads bytes from memory chip
     765432 10
    [------ 00]

        // memory chip identifier (0..255)
        set c  = m[p++]
        // memory address in 24-bit little-endian byte order:
        set lo = m[p++]
        set hi = m[p++] << 8
        set bk = m[p++] << 16
        set a  = bk | hi | lo
        // length of read in bytes (treat 0 as 256, else 1..255)
        set l  = translate_zero_byte(m[p++])

        // initialize memory controller for chip and starting address:
        host_memory_init(c, a);
        // perform entire read:
        while (l--)
            host_send_byte(host_memory_read_auto_advance());

 -----------------------
  1=WRITE:              writes bytes to memory chip
     765432 10
    [------ 01]

        // memory chip identifier (0..255)
        set c  = m[p++]
        // memory address in 24-bit little-endian byte order:
        set lo = m[p++]
        set hi = m[p++] << 8
        set bk = m[p++] << 16
        set a  = bk | hi | lo
        // length of read in bytes (treat 0 as 256, else 1..255)
        set l  = translate_zero_byte(m[p++])

        // initialize memory controller for chip and starting address:
        host_memory_init(c, a);
        // perform entire write:
        while (l--)
            host_memory_write_auto_advance(m, &p);

-----------------------
  2=WAIT_UNTIL:         waits until a byte read from a memory chip compares to a value -- for read/write timing purposes
     765 432 10
    [--- qqq 10]
        q = comparison operator [0..7]
            0 =        EQ; equals
            1 =       NEQ; not equals
            2 =        LT; less than
            3 =       NLT; not less than
            4 =        GT; greater than
            5 =       NGT; not greater than
            6 = undefined; returns false
            7 = undefined; returns false

        // memory chip identifier (0..255)
        set c  = m[p++]
        // memory address in 24-bit little-endian byte order:
        set lo = m[p++]
        set hi = m[p++] << 8
        set bk = m[p++] << 16
        set a  = bk | hi | lo
        // comparison byte
        set v  = m[p++]
        // comparison mask
        set k  = m[p++]

        // initialize memory controller for chip and starting address:
        host_memory_init(c, a);
        host_timer_reset();
        // perform loop to wait until comparison byte matches value:
        while ( !host_timer_elapsed() && !comparison_func[q](host_memory_read_no_advance() & k, v) ) {}

-----------------------
  3=ABORT_IF:           reads a byte from a memory chip and compares to a value; if true, aborts program execution
     765 432 10
    [--- qqq 11]
        q = comparison operator [0..7]
            0 =        EQ; equals
            1 =       NEQ; not equals
            2 =        LT; less than
            3 =       NLT; not less than
            4 =        GT; greater than
            5 =       NGT; not greater than
            6 = undefined; returns false
            7 = undefined; returns false

        // memory chip identifier (0..255)
        set c  = m[p++]
        // memory address in 24-bit little-endian byte order:
        set lo = m[p++]
        set hi = m[p++] << 8
        set bk = m[p++] << 16
        set a  = bk | hi | lo
        // comparison byte
        set v  = m[p++]
        // comparison mask
        set k  = m[p++]

        // initialize memory controller for chip and starting address:
        host_memory_init(c, a);
        if ( comparison_func[q]((host_memory_read_no_advance() & k), v) )
            abort();
*/

#include <stdint.h>
#include <stdbool.h>

enum iovm1_opcode {
    IOVM1_OPCODE_END,
    IOVM1_OPCODE_SETADDR,
    IOVM1_OPCODE_READ,
    IOVM1_OPCODE_READ_N,
    IOVM1_OPCODE_WRITE,
    IOVM1_OPCODE_WRITE_N,
    IOVM1_OPCODE_WHILE_NEQ,
    IOVM1_OPCODE_WHILE_EQ
};

typedef uint8_t iovm1_register;

#define IOVM1_REGISTER_COUNT    (16)

#define IOVM1_INST_OPCODE(x)    ((enum iovm1_opcode) ((x)&15))
#define IOVM1_INST_REGISTER(x)  ((iovm1_register) (((x)>>4)&15))

#define IOVM1_INST_END (0)

#define IOVM1_MKINST(o, r) ( \
     ((uint8_t)(o)&15) | \
    (((uint8_t)(r)&15)<<4) )

typedef uint8_t iovm1_target;

enum iovm1_state {
    IOVM1_STATE_INIT,
    IOVM1_STATE_LOADED,
    IOVM1_STATE_RESET,
    IOVM1_STATE_EXECUTE_NEXT,
    IOVM1_STATE_RESUME_CALLBACK,
    IOVM1_STATE_ENDED
};

enum iovm1_error {
    IOVM1_SUCCESS,
    IOVM1_ERROR_OUT_OF_RANGE,
    IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE,
    IOVM1_ERROR_VM_UNKNOWN_OPCODE,
};

struct bslice {
    const uint8_t *ptr;
    uint32_t len;
    uint32_t off;
};

struct iovm1_t;

struct iovm1_callback_state_t {
    struct iovm1_t      *vm;    // vm struct

    enum iovm1_opcode   o;      // opcode
    uint8_t             r;      // register number used for address

    iovm1_target        t;      // 8-bit identifier of memory target
    uint32_t            a;      // 24-bit address into memory target

    unsigned            len;    // length remaining of read/write
    uint8_t             c;      // comparison byte
    unsigned            p;      // program memory address

    bool completed;             // whether callback is complete
};

#ifdef IOVM1_USE_CALLBACKS
// callback typedef:

typedef void (*iovm1_callback_f)(struct iovm1_t *vm, struct iovm1_callback_state_t *cbs);
#else
// required function implementations by user:

// handle all opcode callbacks (switch on cbs->o):
void iovm1_opcode_cb(struct iovm1_t *vm, struct iovm1_callback_state_t *cbs);

#endif

// iovm1_t definition:

struct iovm1_t {
    // linear memory containing procedure instructions and immediate data
    struct bslice m;

    // current state
    enum iovm1_state s;

    // registers:
    uint32_t t[IOVM1_REGISTER_COUNT];   // target identifier
    uint32_t a[IOVM1_REGISTER_COUNT];   // 24-bit address

    // state for callback resumption:
    struct iovm1_callback_state_t cbs;

#ifdef IOVM1_USE_USERDATA
    void *userdata;
#endif

#ifdef IOVM1_USE_CALLBACKS
    iovm1_callback_f        opcode_cb;
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
void iovm1_set_userdata(struct iovm1_t *vm, void *userdata);
void *iovm1_get_userdata(struct iovm1_t *vm);
#endif

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *proc, unsigned len);

enum iovm1_error iovm1_get_target_address(struct iovm1_t *vm, iovm1_target target, uint32_t *o_address);

enum iovm1_error iovm1_exec_reset(struct iovm1_t *vm);

static inline enum iovm1_state iovm1_get_exec_state(struct iovm1_t *vm) {
    return vm->s;
}

enum iovm1_error iovm1_exec(struct iovm1_t *vm);

#ifdef __cplusplus
}
#endif

#endif //IOVM_H
