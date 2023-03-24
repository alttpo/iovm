#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "iovm.h"

int tests_passed = 0;
int tests_failed = 0;

#define VERIFY_EQ_INT(expected, got, name) \
    do if ((expected) != (got)) { \
        fprintf(stdout, "L%d: expected %s of %d 0x%x; got %d 0x%x\n", __LINE__, name, expected, expected, got, got); \
        return 1; \
    } while (0)

///////////////////////////////////////////////////////////////////////////////////////////
// FAKE callback implementation:
///////////////////////////////////////////////////////////////////////////////////////////

#define FAKE_TARGET_2 2
#define FAKE_TARGET_3 3

struct {
    int count;

    struct iovm1_t *vm;
    iovm1_target target;
    uint32_t *r_address;
    unsigned len;
} fake_read;

struct {
    int count;

    struct iovm1_t *vm;
    iovm1_target target;
    uint32_t *r_address;
    const uint8_t *i_data;
    unsigned len;
} fake_write;

struct {
    int count;

    struct iovm1_t *vm;
    iovm1_target target;
    uint32_t address;
    uint8_t comparison;
} fake_while_neq;

struct {
    int count;

    struct iovm1_t *vm;
    iovm1_target target;
    uint32_t address;
    uint8_t comparison;
} fake_while_eq;

void fake_reset(void) {
    fake_read.count = 0;
    fake_read.vm = 0;
    fake_read.target = 0;
    fake_read.r_address = 0;
    fake_read.len = 0;

    fake_write.count = 0;
    fake_write.vm = 0;
    fake_write.target = 0;
    fake_write.r_address  = 0;
    fake_write.i_data  = 0;
    fake_write.len  = 0;

    fake_while_eq.count = 0;
    fake_while_eq.vm = 0;
    fake_while_eq.target = 0;
    fake_while_eq.address = 0;
    fake_while_eq.comparison = 0;

    fake_while_neq.count = 0;
    fake_while_neq.vm = 0;
    fake_while_neq.target = 0;
    fake_while_neq.address = 0;
    fake_while_neq.comparison = 0;
}

void iovm1_read_cb(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, unsigned len) {
    fake_read.count++;
    fake_read.vm = vm;
    fake_read.target = target;
    fake_read.r_address = r_address;
    fake_read.len = len;

    assert(target < IOVM1_TARGET_COUNT);

    *r_address += len;
}

void iovm1_write_cb(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, const uint8_t *i_data, unsigned len) {
    fake_write.count++;
    fake_write.vm = vm;
    fake_write.target = target;
    fake_write.r_address = r_address;
    fake_write.i_data = i_data;
    fake_write.len = len;

    assert(target < IOVM1_TARGET_COUNT);

    *r_address += len;
}

void iovm1_while_neq_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison) {
    fake_while_neq.count++;
    fake_while_neq.vm = vm;
    fake_while_neq.target = target;
    fake_while_neq.address = address;
    fake_while_neq.comparison = comparison;

    assert(target < IOVM1_TARGET_COUNT);
}

void iovm1_while_eq_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison) {
    fake_while_eq.count++;
    fake_while_eq.vm = vm;
    fake_while_eq.target = target;
    fake_while_eq.address = address;
    fake_while_eq.comparison = comparison;

    assert(target < IOVM1_TARGET_COUNT);
}

void fake_init_test(struct iovm1_t *vm) {
    iovm1_init(vm);
#ifdef IOVM_USE_CALLBACKS
    iovm1_set_read_cb(vm, iovm1_read_cb);
    iovm1_set_write_cb(vm, iovm1_write_cb);
    iovm1_set_while_neq_cb(vm, iovm1_while_neq_cb);
    iovm1_set_while_eq_cb(vm, iovm1_while_eq_cb);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////
// TEST CODE:
///////////////////////////////////////////////////////////////////////////////////////////

int test_get_total_read_0(struct iovm1_t *vm) {
    int r;
    uint32_t total_read;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    r = iovm1_get_total_read(vm, &total_read);
    VERIFY_EQ_INT(0, r, "iovm1_get_total_read() return value");
    VERIFY_EQ_INT(0, (int) total_read, "total_read");

    return 0;
}

int test_get_total_read_1(struct iovm1_t *vm) {
    int r;
    uint32_t total_read;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, FAKE_TARGET_2),
        0x01,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    r = iovm1_get_total_read(vm, &total_read);
    VERIFY_EQ_INT(0, r, "iovm1_get_total_read() return value");
    VERIFY_EQ_INT(1, (int) total_read, "total_read");

    return 0;
}

int test_get_total_read_512(struct iovm1_t *vm) {
    int r;
    uint32_t total_read;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, FAKE_TARGET_2),
        0, // 256
        IOVM1_MKINST(IOVM1_OPCODE_READ, FAKE_TARGET_2),
        0, // 256
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, FAKE_TARGET_2),
        1,
        0xAA,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    r = iovm1_get_total_read(vm, &total_read);
    VERIFY_EQ_INT(0, r, "iovm1_get_total_read() return value");
    VERIFY_EQ_INT(512, (int) total_read, "total_read");

    return 0;
}

int test_reset_from_verified(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // can move from VERIFIED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_reset_from_loaded_fails(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    // cannot move from LOADED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_reset_from_execute_fails(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // cannot move from EXECUTE_NEXT to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// TEST CODE FOR iovm1_exec_step:
///////////////////////////////////////////////////////////////////////////////////////////

int test_step_end(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_db() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_step_setbank_setoffs(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_SETBANK, FAKE_TARGET_2),
        0xF5,
        IOVM1_MKINST(IOVM1_OPCODE_SETOFFS, FAKE_TARGET_2),
        0x10,
        0x00,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // execute SETBANK:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // execute SETADDR:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_db() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");
    VERIFY_EQ_INT(0xF50010, (int) vm->a[FAKE_TARGET_2], "a[t]");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_step_while_neq(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WHILE_NEQ, target),
        0x55,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // WHILE_NEQ:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    VERIFY_EQ_INT(1, fake_while_neq.count, "while_neq_cb() invocations");
    VERIFY_EQ_INT(target, fake_while_neq.target, "while_neq_cb(_, target, _, _)");
    VERIFY_EQ_INT(0, fake_while_neq.address, "while_neq_cb(_, _, address, _)");
    VERIFY_EQ_INT(0x55, (int) fake_while_neq.comparison, "while_neq_cb(_, _, _, comparison)");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_step_while_eq(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WHILE_EQ, target),
        0x55,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // WHILE_EQ:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    VERIFY_EQ_INT(1, fake_while_eq.count, "while_eq_cb() invocations");
    VERIFY_EQ_INT(target, fake_while_eq.target, "while_eq_cb(_, target, _, _)");
    VERIFY_EQ_INT(0, fake_while_eq.address, "while_eq_cb(_, _, address, _)");
    VERIFY_EQ_INT(0x55, (int) fake_while_eq.comparison, "while_eq_cb(_, _, _, comparison)");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_step_read_target_2(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, target),
        0x02,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // READ:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(1, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(target, fake_read.target, "read_cb(_, target, _, _)");
    VERIFY_EQ_INT(target, (int)(fake_read.r_address - vm->a), "read_cb(_, _, r_address, _)");
    VERIFY_EQ_INT(2, fake_read.len, "read_cb(_, _, _, len)");
    VERIFY_EQ_INT(2, *fake_read.r_address, "a[t]");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_step_read_target_3(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_3;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, target),
        0x02,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // READ:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(1, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(target, fake_read.target, "read_cb(_, target, _, _)");
    VERIFY_EQ_INT(target, (int)(fake_read.r_address - vm->a), "read_cb(_, _, r_address, _)");
    VERIFY_EQ_INT(2, fake_read.len, "read_cb(_, _, _, len)");
    VERIFY_EQ_INT(2, *fake_read.r_address, "a[t]");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_step_write_target_2(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, target),
        0x02,
        0xAA,
        0x55,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // WRITE:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(1, fake_write.count, "write_cb() invocations");
    VERIFY_EQ_INT(target, fake_write.target, "write_cb(_, target, _, _, _)");
    VERIFY_EQ_INT(target, (int)(fake_write.r_address - vm->a), "write_cb(_, _, r_address, _, _)");
    VERIFY_EQ_INT(2, (int)(fake_write.i_data - vm->m), "write_cb(_, _, _, i_data, _)");
    VERIFY_EQ_INT(2, fake_write.len, "write_cb(_, _, _, _, len)");
    VERIFY_EQ_INT(2, *fake_write.r_address, "a[t]");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_step_write_target_3(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_3;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, target),
        0x02,
        0xAA,
        0x55,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // WRITE:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(1, fake_write.count, "write_cb() invocations");
    VERIFY_EQ_INT(target, fake_write.target, "write_cb(_, target, _, _, _)");
    VERIFY_EQ_INT(target, (int)(fake_write.r_address - vm->a), "write_cb(_, _, r_address, _, _)");
    VERIFY_EQ_INT(2, (int)(fake_write.i_data - vm->m), "write_cb(_, _, _, i_data, _)");
    VERIFY_EQ_INT(2, fake_write.len, "write_cb(_, _, _, _, len)");
    VERIFY_EQ_INT(2, *fake_write.r_address, "a[t]");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_step_reset_from_end(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    // can move from VERIFIED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// TEST CODE FOR iovm1_exec_until_callback:
///////////////////////////////////////////////////////////////////////////////////////////

int test_until_callback_unverified_fails(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    // skip verify

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_end(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_db() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    // should end:
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_setbank_setoffs(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_SETBANK, FAKE_TARGET_2),
        0xF5,
        IOVM1_MKINST(IOVM1_OPCODE_SETOFFS, FAKE_TARGET_2),
        0x10,
        0x00,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_db() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");
    VERIFY_EQ_INT(0xF50010, (int) vm->a[FAKE_TARGET_2], "a[t]");

    // should end:
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_while_neq(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WHILE_NEQ, target),
        0x55,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    VERIFY_EQ_INT(1, fake_while_neq.count, "while_neq_cb() invocations");
    VERIFY_EQ_INT(target, fake_while_neq.target, "while_neq_cb(_, target, _, _)");
    VERIFY_EQ_INT(0, fake_while_neq.address, "while_neq_cb(_, _, address, _)");
    VERIFY_EQ_INT(0x55, (int) fake_while_neq.comparison, "while_neq_cb(_, _, _, comparison)");

    // should end:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_while_eq(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WHILE_EQ, target),
        0x55,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // WHILE_EQ:

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    VERIFY_EQ_INT(1, fake_while_eq.count, "while_eq_cb() invocations");
    VERIFY_EQ_INT(target, fake_while_eq.target, "while_eq_cb(_, target, _, _)");
    VERIFY_EQ_INT(0, fake_while_eq.address, "while_eq_cb(_, _, address, _)");
    VERIFY_EQ_INT(0x55, (int) fake_while_eq.comparison, "while_eq_cb(_, _, _, comparison)");

    // should end:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_read_target_2(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, target),
        0x02,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // READ:

    // verify invocations:
    VERIFY_EQ_INT(1, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(target, fake_read.target, "read_cb(_, target, _, _)");
    VERIFY_EQ_INT(target, (int)(fake_read.r_address - vm->a), "read_cb(_, _, r_address, _)");
    VERIFY_EQ_INT(2, fake_read.len, "read_cb(_, _, _, len)");
    VERIFY_EQ_INT(2, *fake_read.r_address, "a[t]");

    // should end:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_read_target_3(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_3;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, target),
        0x02,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // READ:

    // verify invocations:
    VERIFY_EQ_INT(1, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(target, fake_read.target, "read_cb(_, target, _, _)");
    VERIFY_EQ_INT(target, (int)(fake_read.r_address - vm->a), "read_cb(_, _, r_address, _)");
    VERIFY_EQ_INT(2, fake_read.len, "read_cb(_, _, _, len)");
    VERIFY_EQ_INT(2, *fake_read.r_address, "a[t]");

    // should end:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_write_target_2(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, target),
        0x02,
        0xAA,
        0x55,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // WRITE:

    // verify invocations:
    VERIFY_EQ_INT(1, fake_write.count, "write_cb() invocations");
    VERIFY_EQ_INT(target, fake_write.target, "write_cb(_, target, _, _, _)");
    VERIFY_EQ_INT(target, (int)(fake_write.r_address - vm->a), "write_cb(_, _, r_address, _, _)");
    VERIFY_EQ_INT(2, (int)(fake_write.i_data - vm->m), "write_cb(_, _, _, i_data, _)");
    VERIFY_EQ_INT(2, fake_write.len, "write_cb(_, _, _, _, len)");
    VERIFY_EQ_INT(2, *fake_write.r_address, "a[t]");

    // should end:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_write_target_3(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_3;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, target),
        0x02,
        0xAA,
        0x55,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "state");

    // WRITE:

    // verify invocations:
    VERIFY_EQ_INT(1, fake_write.count, "write_cb() invocations");
    VERIFY_EQ_INT(target, fake_write.target, "write_cb(_, target, _, _, _)");
    VERIFY_EQ_INT(target, (int)(fake_write.r_address - vm->a), "write_cb(_, _, r_address, _, _)");
    VERIFY_EQ_INT(2, (int)(fake_write.i_data - vm->m), "write_cb(_, _, _, i_data, _)");
    VERIFY_EQ_INT(2, fake_write.len, "write_cb(_, _, _, _, len)");
    VERIFY_EQ_INT(2, *fake_write.r_address, "a[t]");

    // should end:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_reset_from_end(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_until_callback() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    // can move from VERIFIED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_until_callback_reset_retry(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    r = iovm1_verify(vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    // can move from ENDED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // execute again:
    r = iovm1_exec_until_callback(vm);
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// main runner:
///////////////////////////////////////////////////////////////////////////////////////////

#define run_test(name) \
    { \
        fake_reset(); \
        fprintf(stdout, "running test: " #name "\n"); \
        if ((r = name(&vm))) { \
            fprintf(stdout, "test failed\n"); \
            tests_failed++; \
            return r; \
        } else { \
            fprintf(stdout, "test passed\n"); \
            tests_passed++; \
        } \
    }

int run_test_suite(void) {
    int r;
    struct iovm1_t vm;

    run_test(test_get_total_read_0)
    run_test(test_get_total_read_1)
    run_test(test_get_total_read_512)
    run_test(test_reset_from_verified)
    run_test(test_reset_from_loaded_fails)
    run_test(test_reset_from_execute_fails)

    // exec_step tests:
    run_test(test_step_end)
    run_test(test_step_setbank_setoffs)
    run_test(test_step_while_neq)
    run_test(test_step_while_eq)
    run_test(test_step_read_target_2)
    run_test(test_step_read_target_3)
    run_test(test_step_write_target_2)
    run_test(test_step_write_target_3)
    run_test(test_step_reset_from_end)

    // exec_until_callback tests:
    run_test(test_until_callback_unverified_fails)
    run_test(test_until_callback_end)
    run_test(test_until_callback_setbank_setoffs)
    run_test(test_until_callback_while_neq)
    run_test(test_until_callback_while_eq)
    run_test(test_until_callback_read_target_2)
    run_test(test_until_callback_read_target_3)
    run_test(test_until_callback_write_target_2)
    run_test(test_until_callback_write_target_3)
    run_test(test_until_callback_reset_from_end)
    run_test(test_until_callback_reset_retry)

    return 0;
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    run_test_suite();

    fprintf(stdout, "ran tests; %d succeeded, %d failed\n", tests_passed, tests_failed);

    return 0;
}
