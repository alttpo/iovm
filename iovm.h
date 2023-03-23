#ifndef SD2SNES_IOVM_H
#define SD2SNES_IOVM_H


/*
executes a VM procedure to accomplish custom tasks which require low-latency access to FPGA.

instructions:

   76 54 3210
  [-- tt oooo]

    o = opcode
    t = target
    - = reserved

memory:
    M[64...]:   linear memory of procedure, at least 64 bytes
    A[4]:       24-bit address for each target, indexed by `t`

 registers:
    P:          points to current byte in M
    Q:          comparison byte
    C:          loop counter

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
                    cb_read(t, &A[t], C);
                    // expected behavior:
                    //for n=0; n<C; n++ {
                    //    read(t, A[t]++)
                    //}

  4=WRITE:      writes bytes to target
                    set C to M[P++] (translate 0 -> 256, else use 1..255)

                    // invoke user-supplied callback function:
                    cb_write(t, &A[t], C, &M[P]);
                    // expected behavior:
                    //for n=0; n<C; n++ {
                    //    write(t, A[t]++, M[P++])
                    //}

  5=WHILE_NEQ:  waits while read(t) != M[P]
                    set Q to M[P++]

                    // invoke user-supplied callback function:
                    cb_while_neq(t, A[t], Q);
                    // expected behavior:
                    //while (read(t, A[t]) != Q) {}

  6=WHILE_EQ:   waits while read_non_advancing(t) == M[P]
                    set Q to M[P++]

                    // invoke user-supplied callback function:
                    cb_while_eq(t, A[t], Q);
                    // expected behavior:
                    //while (read(t, A[t]) == Q) {}

  7..15:        reserved
*/

// IOVM1_MAX_SIZE can be overridden
#ifndef IOVM1_MAX_SIZE
#  define IOVM1_MAX_SIZE 512
#elif IOVM1_MAX_SIZE < 32
#  error("IOVM1_MAX_SIZE must be at least 32 bytes")
#endif

#define IOVM1_INST_END          (0)
#define IOVM1_INST_OPCODE(x)    ((x)&7)
#define IOVM1_INST_ADVANCE(x)   (((x)>>4)&1)
#define IOVM1_INST_REPEAT(x)    (((x)>>5)&1)
#define IOVM1_INST_IMMED(x)     (((x)>>6)&1)
#define IOVM1_INST_TARGET(x)    (((x)>>7)&1)

#define IOVM1_MKINST(o, v, r, i, t) ((uint8_t)(o&7) | ((uint8_t)(v&1)<<4) | ((uint8_t)(r&1)<<5) | ((uint8_t)(i&1)<<6) | ((uint8_t)(t&1)<<7))

enum iovm1_opcode_e {
    IOVM1_OPCODE_SETADDR,
    IOVM1_OPCODE_WHILE_NEQ,
    IOVM1_OPCODE_READ,
    IOVM1_OPCODE_WRITE
};

enum iovm1_target_e {
    IOVM1_TARGET_SRAM,
    IOVM1_TARGET_SNESCMD
};

enum iovm1_state_e {
    IOVM1_STATE_INIT,
    IOVM1_STATE_LOAD_STREAMING,
    IOVM1_STATE_LOADED,
    IOVM1_STATE_VERIFIED,
    IOVM1_STATE_RESET,
    IOVM1_STATE_EXECUTE_NEXT,
    IOVM1_STATE_READ_LOOP_ITER,
    IOVM1_STATE_READ_LOOP_END,
    IOVM1_STATE_WRITE_LOOP_ITER,
    IOVM1_STATE_WRITE_LOOP_END,
    IOVM1_STATE_WHILE_NEQ_LOOP_ITER,
    IOVM1_STATE_WHILE_NEQ_LOOP_END,
    IOVM1_STATE_ENDED
};

enum iovm1_error_e {
    IOVM1_SUCCESS,
    IOVM1_ERROR_OUT_OF_RANGE,
    IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE,
    IOVM1_ERROR_VM_UNKNOWN_OPCODE,
};

struct iovm1_t {
    enum iovm1_state_e  s;  // current state

    uint8_t     x;  // current instruction byte

    int         p;  // pointer into data[]
    int         c;  // counter
    uint8_t     m;  // M register
    uint8_t     q;  // comparison byte for WHILE_NEQ

    void        *userdata;
    int         user_last_error;

    uint32_t    emit_size;

    unsigned    stream_offs;
    uint8_t     data[IOVM1_MAX_SIZE];
};

// core functions:

void iovm1_init(struct iovm1_t *vm);
enum iovm1_error_e iovm1_load(struct iovm1_t *vm, const uint8_t *data, unsigned len);
enum iovm1_error_e iovm1_load_stream(struct iovm1_t *vm, const uint8_t *data, unsigned len);
enum iovm1_error_e iovm1_load_stream_complete(struct iovm1_t *vm);
enum iovm1_error_e iovm1_verify(struct iovm1_t *vm);

enum iovm1_error_e iovm1_emit_size(struct iovm1_t *vm, uint32_t *size);
enum iovm1_error_e iovm1_set_userdata(struct iovm1_t *vm, void *userdata);
enum iovm1_error_e iovm1_get_userdata(struct iovm1_t *vm, void **o_userdata);

enum iovm1_error_e iovm1_exec_reset(struct iovm1_t *vm);
enum iovm1_error_e iovm1_exec_step(struct iovm1_t *vm);
enum iovm1_error_e iovm1_exec_while_abort(struct iovm1_t *vm);

static inline enum iovm1_state_e iovm1_exec_state(struct iovm1_t *vm) { return vm->s; }
static inline int iovm1_exec_user_last_error(struct iovm1_t *vm) { return vm->user_last_error; }
static inline void iovm1_exec_user_last_error_clear(struct iovm1_t *vm) { vm->user_last_error = 0; }

// external interface:

int iovm1_target_set_address(struct iovm1_t *vm, enum iovm1_target_e target, uint32_t address);
int iovm1_target_read(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t *o_data);
int iovm1_target_write(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t data);
int iovm1_emit(struct iovm1_t *vm, uint8_t data);

#endif //SD2SNES_IOVM_H
