#ifndef SD2SNES_IOVM_H
#define SD2SNES_IOVM_H

#define IOVM1_MAX_SIZE 512

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
    IOVM1_STATE_UNLOADED,
    IOVM1_STATE_LOADED,
    IOVM1_STATE_EXECUTE_NEXT,
    IOVM1_STATE_READ_LOOP_ITER,
    IOVM1_STATE_READ_LOOP_END,
    IOVM1_STATE_WRITE_LOOP_ITER,
    IOVM1_STATE_WRITE_LOOP_END,
    IOVM1_STATE_WHILE_NEQ_LOOP_ITER,
    IOVM1_STATE_WHILE_NEQ_LOOP_END,
    IOVM1_STATE_WHILE_EQ_LOOP_ITER,
    IOVM1_STATE_WHILE_EQ_LOOP_END,
    IOVM1_STATE_ENDED,
    IOVM1_STATE_ERRORED
};

struct iovm1_t {
    enum iovm1_state_e  s;

    uint8_t x;  // current instruction

    int     p;  // pointer to data[]
    int     c;  // counter
    uint8_t m;  // M byte
    uint8_t q;  // comparison byte for WHILE_NEQ

    void    *userdata;

    uint8_t  data[IOVM1_MAX_SIZE];
};

// core functions:

int iovm1_load(struct iovm1_t *vm, unsigned len, const uint8_t *data);
int iovm1_response_size(struct iovm1_t *vm, uint32_t *size);
int iovm1_set_userdata(struct iovm1_t *vm, void *userdata);
int iovm1_get_userdata(struct iovm1_t *vm, void **o_userdata);
static inline enum iovm1_state_e iovm1_state(struct iovm1_t *vm) { return vm->s; }
int iovm1_reset(struct iovm1_t *vm);
int iovm1_exec_step(struct iovm1_t *vm);

// external interface:

int iovm1_target_set_address(struct iovm1_t *vm, enum iovm1_target_e target, uint32_t address);
int iovm1_target_read(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t *o_data);
int iovm1_target_write(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t data);
int iovm1_emit(struct iovm1_t *vm, uint8_t data);

#endif //SD2SNES_IOVM_H
