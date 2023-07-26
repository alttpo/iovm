#ifndef IOVM_H
#define IOVM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    iovm.h: memory I/O virtual machine

    host provides a callback function to implement the I/O instructions.

    two main I/O operations are read and write

instructions:

   76543210
  [--ccoooo]

    o = opcode  [0..15]
    c = channel [0..3]

memory:
    m[...]:             program memory, at least 1 byte

registers:
    u32     p:          points to current byte in m
                        initial value = 0

    u24     a[0..3]:    24-bit memory target address register
                        initial value = 0

    u8      tv[0..3]:   memory target and auto-advance register
                        initial value = 0

                        [76543210]
                         v-tttttt

                            v = auto-advance address on read/write by len
                            t = memory target (0..63)

    u16     len[0..3]:  transfer length register (0..256)
                        initial value = 0
    u32     tim[0..3]:  timeout register in host-defined time units
                        initial value = 0
    u8      cmp[0..3]:  comparison value register
                        initial value = 0
    u8      msk[0..3]:  comparison mask register
                        initial value = 255

opcodes (o):
  0=END:                ends procedure

  1=SETA8:              sets address register to 8-bit value
        set lo = m[p++]
        set a[c] = lo

  2=SETA16:             sets address register to 16-bit value
        set lo = m[p++]
        set hi = m[p++] << 8
        set a[c] = hi | lo

  3=SETA24:             sets address register to 24-bit value
        set lo = m[p++]
        set hi = m[p++] << 8
        set bk = m[p++] << 16
        set a[c] = bk | hi | lo

  4=SETTV:              sets memory target and auto-advance bit of address register
        set tv[c] = m[p++]

  5=SETLEN:             sets transfer length register
        set len = m[p++]
        if len == 0 then set len = 256

  6=SETCMPMSK:          sets comparison value and comparison mask registers
        set cmp = m[p++]
        set msk = m[p++]

  7=SETTIM:             sets wait timeout in host-defined duration units
        set b0 = m[p++]
        set b1 = m[p++] << 8
        set b2 = m[p++] << 16
        set b3 = m[p++] << 24
        set tim = b3 | b2 | b1 | b0

  8=READ:               I/O. reads `len` bytes from memory target.

  9=WRITE:              I/O. writes bytes to memory target from next `len` bytes of program memory.

  10=WAIT_WHILE_NEQ:    I/O. waits while (read_byte(tv[c], a[c]) & msk) != cmp or until `tim` expires.

  11=WAIT_WHILE_EQ:     I/O. waits while (read_byte(tv[c], a[c]) & msk) == cmp or until `tim` expires.

  12=WAIT_WHILE_LT:     I/O. waits while (read_byte(tv[c], a[c]) & msk) < cmp or until `tim` expires.

  13=WAIT_WHILE_GT:     I/O. waits while (read_byte(tv[c], a[c]) & msk) > cmp or until `tim` expires.

  14=WAIT_WHILE_LTE:    I/O. waits while (read_byte(tv[c], a[c]) & msk) <= cmp or until `tim` expires.

  15=WAIT_WHILE_GTE:    I/O. waits while (read_byte(tv[c], a[c]) & msk) >= cmp or until `tim` expires.


cbs:                    callback state struct
    bool    initial;    // host sets this. true if initial callback invocation; false if subsequent callback
    bool    complete;   // callback sets this. callback will be invoked until true
    u32     p;          // program memory address
    u8      *m;         // program memory
    u8      o;          // opcode
    u8      c;          // channel
    u8      t;          // memory target identifier
    bool    v;          // auto-advance address
    u32     a;          // 24-bit address into memory target
    u16     len;        // remaining transfer length
    u32     tim;        // remaining timeout
    u8      cmp;        // comparison value
    u8      msk;        // comparison mask

*/

#include <stdint.h>
#include <stdbool.h>

enum iovm1_opcode {
    IOVM1_OPCODE_END,
    IOVM1_OPCODE_SETA8,
    IOVM1_OPCODE_SETA16,
    IOVM1_OPCODE_SETA24,
    IOVM1_OPCODE_SETTV,
    IOVM1_OPCODE_SETLEN,
    IOVM1_OPCODE_SETCMPMSK,
    IOVM1_OPCODE_SETTIM,
    IOVM1_OPCODE_READ,
    IOVM1_OPCODE_WRITE,
    IOVM1_OPCODE_WAIT_WHILE_NEQ,
    IOVM1_OPCODE_WAIT_WHILE_EQ,
    IOVM1_OPCODE_WAIT_WHILE_LT,
    IOVM1_OPCODE_WAIT_WHILE_GT,
    IOVM1_OPCODE_WAIT_WHILE_LTE,
    IOVM1_OPCODE_WAIT_WHILE_GTE
};

typedef uint8_t iovm1_register;

#define IOVM1_CHANNEL_COUNT     (4)

#define IOVM1_INST_OPCODE(x)    ((enum iovm1_opcode) ((x)&15))
#define IOVM1_INST_CHANNEL(x)   ((iovm1_register) (((x)>>4)&3))

#define IOVM1_INST_END (0)

#define IOVM1_MKINST(o, c) ( \
     ((uint8_t)(o)&15) | \
    (((uint8_t)(c)&3)<<4) )

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
    IOVM1_SUCCESS = 0,

    IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE,
    IOVM1_ERROR_VM_UNKNOWN_OPCODE,
    IOVM1_ERROR_VM_INVALID_MEMORY_ACCESS,

    IOVM1_ERROR_OUT_OF_RANGE = 128,
    IOVM1_ERROR_NO_DATA,
    IOVM1_ERROR_BUFFER_TOO_SMALL,
};

struct bslice {
    const uint8_t *ptr;
    uint32_t len;
    uint32_t off;
};

struct iovm1_t;

struct iovm1_callback_state_t {
    bool                initial;    // ro. true only on initial callback; false otherwise
    bool                complete;   // rw. callback will be invoked until true

    unsigned            p;          // rw. program memory address
    const uint8_t       *m;         // ro. program memory

    enum iovm1_opcode   o;          // ro. opcode
    uint8_t             c;          // ro. channel

    iovm1_target        t;          // ro. memory target identifier
    bool                v;          // ro. auto-advance address
    uint32_t            a;          // rw. 24-bit address into memory target

    unsigned            len;        // rw. remaining transfer length
    unsigned            tim;        // rw. remaining timeout
    uint8_t             cmp;        // ro. comparison byte
    uint8_t             msk;        // ro. comparison mask
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
    uint32_t    a[IOVM1_CHANNEL_COUNT];     // 24-bit address
    uint8_t     tv[IOVM1_CHANNEL_COUNT];    // target identifier
    uint16_t    len[IOVM1_CHANNEL_COUNT];   // transfer length
    uint32_t    tim[IOVM1_CHANNEL_COUNT];   // timeout
    uint8_t     cmp[IOVM1_CHANNEL_COUNT];   // comparison byte
    uint8_t     msk[IOVM1_CHANNEL_COUNT];   // comparison mask

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
enum iovm1_error iovm1_set_opcode_cb(struct iovm1_t *vm, iovm1_callback_f cb);
#endif

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
