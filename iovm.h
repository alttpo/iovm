#ifndef IOVM_H
#define IOVM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
    iovm.h: trivial I/O virtual machine execution engine

    features:
        * max of 16 instruction opcodes
        * no branching instructions

    host provides a callback function to implement read/write I/O tasks against memory targets. callbacks are
    re-entrant.

    the virtual machine consists of 16 registers which are 24-bit address into memory targets identified by an
    8-bit unsigned integer. it is recommended to keep memory targets as linear address spaces.

instructions:

   7654 3210
  [rrrr oooo]

    o = opcode   [0..15]
    r = register [0..15]

memory:
    m[...]:         program memory, at least 1 byte

registers:
    u32 p:          points to current byte in m
    u8  t[0..15]:   target identifier for each register
    u24 a[0..15]:   target 24-bit address for each register

cbs:                callback state struct
    u8      o;          // opcode
    u8      r;          // register
    u8      t;          // target
    u24     a;          // address
    u32     len;        // length of read/write
    u8      c;          // comparison byte
    u32     p;          // program memory address
    bool    completed;  // callback completed

opcodes (o):
  0=END:                ends procedure

  1=SETADDR:            sets register to target and 24-bit address within
        set t[r] = m[p++]
        set lo = m[p++]
        set hi = m[p++] << 8
        set bk = m[p++] << 16
        set a[r] = bk | hi | lo

  2=READ:               reads bytes from target; advance address after completed
        set cbs.len = m[p++] // (translate 0 -> 256, else use 1..255)
        set cbs.t = t[r]
        set cbs.a = a[r]
        set cbs.p = p
        set cbs.completed = false
        cb(vm, cbs)
        if cbs.completed {
            a[r] = cbs.a;
        }

  3=READ_N:             reads bytes from target; no advance address after completed
        set cbs.len = m[p++] // (translate 0 -> 256, else use 1..255)
        set cbs.t = t[r]
        set cbs.a = a[r]
        set cbs.p = p
        set cbs.completed = false
        cb(vm, cbs)

  4=WRITE:              writes bytes to target; advance address after completed
        set cbs.len = m[p++] // (translate 0 -> 256, else use 1..255)
        set cbs.t = t[r]
        set cbs.a = a[r]
        set cbs.p = p
        set cbs.completed = false
        cb(vm, cbs)
        if cbs.completed {
            a[r] = cbs.a;
        }

  5=WRITE_N:            writes bytes to target without advancing address after complete
        set cbs.len = m[p++] // (translate 0 -> 256, else use 1..255)
        set cbs.t = t[r]
        set cbs.a = a[r]
        set cbs.p = p
        set cbs.completed = false
        cb(vm, cbs);

  6=WAIT_WHILE_NEQ:     waits while read_byte(t, a[t]) != m[p]
        set cbs.c = m[p++]
        set cbs.len = 0
        set cbs.t = t[r]
        set cbs.a = a[r]
        set cbs.p = p
        set cbs.completed = false
        cb(vm, cbs);

        // expected behavior:
        //  while (read_byte(cbs.t, cbs.a) != cbs.c) {}

  7=WAIT_WHILE_EQ:      waits while read_byte(t, a[t]) == m[p]
        set cbs.c = m[p++]
        set cbs.len = 0
        set cbs.t = t[r]
        set cbs.a = a[r]
        set cbs.p = p
        set cbs.completed = false
        cb(vm, cbs);

        // expected behavior:
        //  while (read_byte(cbs.t, cbs.a) == cbs.c) {}

  8..15:        reserved
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
