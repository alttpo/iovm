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

    host MUST implement host_* named functions.

memory:
    m[...]:             program memory, at least 1 byte

    NOTE: entire program MUST be buffered into memory before execution starts to avoid timing delays between and
    during instruction execution.

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
        c  = m[p++]
        // memory address in 24-bit little-endian byte order:
        lo = m[p++]
        hi = m[p++] << 8
        bk = m[p++] << 16
        a  = bk | hi | lo
        // length of read in bytes (treat 0 as 256, else 1..255)
        l_raw = m[p++]
        l  = translate_zero_byte(lr)

        {
            uint8_t dm[256];
            uint8_t *d = dm;
            // initialize memory controller for chip and starting address:
            host_memory_init(vm, c, a);
            // perform entire read:
            while (l--)
                *d++ = host_memory_read_auto_advance(vm);
            // send read data back to client:
            host_send_read(vm, l_raw, d);
        }

 -----------------------
  1=WRITE:              writes bytes to memory chip
     765432 10
    [------ 01]

        // memory chip identifier (0..255)
        c  = m[p++]
        // memory address in 24-bit little-endian byte order:
        lo = m[p++]
        hi = m[p++] << 8
        bk = m[p++] << 16
        a  = bk | hi | lo
        // length of read in bytes (treat 0 as 256, else 1..255)
        l  = translate_zero_byte(m[p++])

        {
            // initialize memory controller for chip and starting address:
            host_memory_init(vm, c, a);
            // perform entire write:
            while (l--)
                host_memory_write_auto_advance(vm, m[p++]);
        }

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
        c  = m[p++]
        // memory address in 24-bit little-endian byte order:
        lo = m[p++]
        hi = m[p++] << 8
        bk = m[p++] << 16
        a  = bk | hi | lo
        // comparison byte
        v  = m[p++]
        // comparison mask
        k  = m[p++]

        {
            // initialize memory controller for chip and starting address:
            host_memory_init(vm, c, a);
            host_timer_reset(vm);

            // perform loop to wait until comparison byte matches value:
            while (!host_timer_elapsed(vm)) {
                if (comparison_funcs[q](host_memory_read_no_advance(vm) & k, v)) {
                    // successful exit:
                    return;
                }
            }

            // timed out; send an abort message back to the client:
            host_send_abort(vm);
        }

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
        c  = m[p++]
        // memory address in 24-bit little-endian byte order:
        lo = m[p++]
        hi = m[p++] << 8
        bk = m[p++] << 16
        a  = bk | hi | lo
        // comparison byte
        v  = m[p++]
        // comparison mask
        k  = m[p++]

        {
            // initialize memory controller for chip and starting address:
            host_memory_init(vm, c, a);
            // perform single byte read and compare:
            if ( comparison_funcs[q]((host_memory_read_no_advance(vm) & k), v) ) {
                // successful exit:
                return;
            }

            // send an abort message to client:
            host_send_abort(vm);
        }
*/

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t uint24_t;

enum iovm1_opcode {
    IOVM1_OPCODE_READ,
    IOVM1_OPCODE_WRITE,
    IOVM1_OPCODE_WAIT_UNTIL,
    IOVM1_OPCODE_ABORT_IF
};

enum iovm1_cmp_operator {
    IOVM1_CMP_EQ,
    IOVM1_CMP_NEQ,
    IOVM1_CMP_LT,
    IOVM1_CMP_NLT,
    IOVM1_CMP_GT,
    IOVM1_CMP_NGT
};

#define IOVM1_INST_OPCODE(x)        ((enum iovm1_opcode) ((x)&3))
#define IOVM1_INST_CMP_OPERATOR(x)  ((enum iovm1_cmp_operator) (((x)>>2)&7))

#define IOVM1_MK_WAIT_UNTIL(q) (  \
        IOVM1_OPCODE_WAIT_UNTIL | \
        ((q)&7)<<2                \
    )

#define IOVM1_MK_ABORT_IF(q) (  \
        IOVM1_OPCODE_ABORT_IF | \
        ((q)&7)<<2              \
    )

enum iovm1_memory_chip {
    MEM_SNES_WRAM,
    MEM_SNES_VRAM,
    MEM_SNES_CGRAM,
    MEM_SNES_OAM,
    MEM_SNES_ARAM,
    MEM_SNES_2C00,
    MEM_SNES_ROM,
    MEM_SNES_SRAM,
};

enum iovm1_state {
    IOVM1_STATE_INIT,
    IOVM1_STATE_LOADED,
    IOVM1_STATE_RESET,
    IOVM1_STATE_EXECUTE_NEXT,
    IOVM1_STATE_READ,
    IOVM1_STATE_WRITE,
    IOVM1_STATE_WAIT,
    IOVM1_STATE_ENDED,
    // any state after IOVM1_STATE_ENDED is considered errored:
    IOVM1_STATE_ERRORED,
};

enum iovm1_error {
    IOVM1_SUCCESS,
    IOVM1_ERROR_OUT_OF_RANGE,
    IOVM1_ERROR_INVALID_OPERATION_FOR_STATE,
    IOVM1_ERROR_UNKNOWN_OPCODE,
    IOVM1_ERROR_TIMED_OUT,
    IOVM1_ERROR_ABORTED,
    IOVM1_ERROR_MEMORY_CHIP_UNDEFINED,
    IOVM1_ERROR_MEMORY_CHIP_ADDRESS_OUT_OF_RANGE,
    IOVM1_ERROR_MEMORY_CHIP_NOT_READABLE,
    IOVM1_ERROR_MEMORY_CHIP_NOT_WRITABLE,
};

enum iovm1_opstate {
    IOVM1_OPSTATE_INIT,
    IOVM1_OPSTATE_CONTINUE,
    IOVM1_OPSTATE_COMPLETED,
};

struct bslice {
    const uint8_t *ptr;
    uint32_t len;
    uint32_t off;
};

struct iovm1_t;

// host interface:

// advance memory-read state machine, use `vm->rd` for tracking state
extern enum iovm1_error host_memory_read_state_machine(struct iovm1_t *vm);
// advance memory-write state machine, use `vm->wr` for tracking state
extern enum iovm1_error host_memory_write_state_machine(struct iovm1_t *vm);
// advance memory-wait state machine, use `vm->wa` for tracking state, use `iovm1_memory_wait_test_byte` for comparison func
extern enum iovm1_error host_memory_wait_state_machine(struct iovm1_t *vm);

// try to read a byte from a memory chip, return byte in `*b` if successful
extern enum iovm1_error host_memory_try_read_byte(struct iovm1_t *vm, enum iovm1_memory_chip c, uint24_t a, uint8_t *b);

// send a program-end message to the client
extern void host_send_end(struct iovm1_t *vm);

// iovm1_t definition:

struct iovm1_t {
    // linear memory containing procedure instructions and immediate data
    struct bslice m;

    // current state
    enum iovm1_state s;
    enum iovm1_error e;

#ifdef IOVM1_USE_USERDATA
    void *userdata;
#endif

    // offset of current executing opcode:
    uint32_t p;

    // offset of next opcode:
    uint32_t next_off;

    // instruction state:
    union {
        // read
        struct {
            enum iovm1_opstate os;
            enum iovm1_memory_chip c;
            uint24_t a;
            uint8_t l_raw;
            int l;
        } rd;
        // write
        struct {
            enum iovm1_opstate os;
            enum iovm1_memory_chip c;
            uint24_t a;
            uint8_t l_raw;
            int l;
            // offset into vm->m.ptr to source data from
            uint32_t p;
        } wr;
        // wait
        struct {
            enum iovm1_opstate os;
            enum iovm1_memory_chip c;
            uint24_t a;
            uint8_t v;
            uint8_t k;
            enum iovm1_cmp_operator q;
        } wa;
    };
};

// core functions:

void iovm1_init(struct iovm1_t *vm);

#ifdef IOVM1_USE_USERDATA
void iovm1_set_userdata(struct iovm1_t *vm, void *userdata);
void *iovm1_get_userdata(struct iovm1_t *vm);
#endif

enum iovm1_error iovm1_load(struct iovm1_t *vm, const uint8_t *proc, unsigned len);

enum iovm1_error iovm1_exec_reset(struct iovm1_t *vm);

static inline enum iovm1_state iovm1_get_exec_state(struct iovm1_t *vm) {
    return vm->s;
}

enum iovm1_error iovm1_exec(struct iovm1_t *vm);

static inline bool iovm1_memory_cmp(enum iovm1_cmp_operator q, uint8_t a, uint8_t b) {
    switch (q) {
        case IOVM1_CMP_EQ: return a == b;
        case IOVM1_CMP_NEQ: return a != b;
        case IOVM1_CMP_LT: return a < b;
        case IOVM1_CMP_NLT: return a >= b;
        case IOVM1_CMP_GT: return a > b;
        case IOVM1_CMP_NGT: return a <= b;
        default: return false;
    }
}

// tests the read byte `b` with the current wait operation's comparison function and bit mask
static inline bool iovm1_memory_wait_test_byte(struct iovm1_t *vm, uint8_t a) {
    return iovm1_memory_cmp(vm->wa.q, a & vm->wa.k, vm->wa.v);
}

#ifdef __cplusplus
}
#endif

#endif //IOVM_H
