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

typedef enum iovm1_memory_chip iovm1_memory_chip_t;

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

struct bslice {
    const uint8_t *ptr;
    uint32_t len;
    uint32_t off;
};

struct iovm1_t;

// host interface:

// initialize memory controller to point at specific memory chip and a starting address within it
extern enum iovm1_error host_memory_init(struct iovm1_t *vm, iovm1_memory_chip_t c, uint24_t a);
// validate the addresses of a read operation with the given length against the last host_memory_init() call
extern enum iovm1_error host_memory_read_validate(struct iovm1_t *vm, int l);
// validate the addresses of a write operation with the given length against the last host_memory_init() call
extern enum iovm1_error host_memory_write_validate(struct iovm1_t *vm, int l);

// read a byte and advance the chip address forward by 1 byte
extern uint8_t host_memory_read_auto_advance(struct iovm1_t *vm);
// read a byte and do not advance the chip address; useful for continuously polling a specific address
extern uint8_t host_memory_read_no_advance(struct iovm1_t *vm);
// write a byte and advance the chip address forward by 1 byte
extern void host_memory_write_auto_advance(struct iovm1_t *vm, uint8_t b);

// send a program-end message to the client
extern void host_send_end(struct iovm1_t *vm);
// send an abort message to the client
extern void host_send_abort(struct iovm1_t *vm);
// send a read-complete message to the client with the fully read data up to 256 bytes in length
extern void host_send_read(struct iovm1_t *vm, uint8_t l, uint8_t *d);

// initialize a host-side countdown timer to a timeout value for WAIT operation, e.g. duration of a single video frame
extern void host_timer_reset(struct iovm1_t *vm);
// checks if the host-side countdown timer has elapsed down to or below 0
extern bool host_timer_elapsed(struct iovm1_t *vm);
// stops the host-side countdown timer and releases any resources
extern void host_timer_cleanup(struct iovm1_t *vm);

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

    // instruction state:
    union {
        struct {
            iovm1_memory_chip_t c;
            uint24_t a;
            uint8_t l_raw;
            int l;
            uint8_t *d;
            uint8_t dm[256];
        } rd;
        struct {
            iovm1_memory_chip_t c;
            uint24_t a;
            uint8_t l_raw;
            int l;
        } wr;
        struct {
            iovm1_memory_chip_t c;
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

#ifdef __cplusplus
}
#endif

#endif //IOVM_H
