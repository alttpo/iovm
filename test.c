#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "iovm.h"

int tests_passed = 0;
int tests_failed = 0;

#define VERIFY_EQ_INT(expected, got, name) \
    do if ((expected) != (got)) { \
        fprintf(stdout, "L%d: expected %s of %u 0x%x; got %u 0x%x\n", __LINE__, name, expected, expected, got, got); \
        return 1; \
    } while (0)

///////////////////////////////////////////////////////////////////////////////////////////
// FAKE callback implementation:
///////////////////////////////////////////////////////////////////////////////////////////

#define FAKE_TARGET_2 2
#define FAKE_TARGET_3 3

struct fake {
    int count;

    struct iovm1_state_t pre;
    struct iovm1_state_t post;
};

struct fake fake_read;
struct fake fake_write;
struct fake fake_while_neq;
struct fake fake_while_eq;

struct fake fake_default = {};

void fake_reset(void) {
    fake_read = fake_default;
    fake_write = fake_default;
    fake_while_eq = fake_default;
    fake_while_neq = fake_default;
}

void iovm1_read_cb(struct iovm1_state_t *cb_state) {
    fake_read.count++;
    fake_read.pre = *cb_state;

    cb_state->address += cb_state->len;

    fake_read.post = *cb_state;
}

void iovm1_write_cb(struct iovm1_state_t *cb_state) {
    fake_write.count++;
    fake_write.pre = *cb_state;

    cb_state->address += cb_state->len;
    cb_state->i_data.off += cb_state->len;

    fake_write.post = *cb_state;
}

void iovm1_while_neq_cb(struct iovm1_state_t *cb_state) {
    fake_while_neq.count++;
    fake_while_neq.pre = *cb_state;
    fake_while_neq.post = *cb_state;
}

void iovm1_while_eq_cb(struct iovm1_state_t *cb_state) {
    fake_while_eq.count++;
    fake_while_eq.pre = *cb_state;
    fake_while_eq.post = *cb_state;
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

int test_reset_from_loaded(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // can move from LOADED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_reset_from_execute_fails(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 0),
        0x01,
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "pre");

    // cannot move from EXECUTE_NEXT to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "pre");

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// TEST CODE FOR iovm1_exec:
///////////////////////////////////////////////////////////////////////////////////////////

int test_end(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_db() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    // should end:
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_setbank_setoffs(struct iovm1_t *vm) {
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
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_db() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");
    VERIFY_EQ_INT(0xF50010, (int) vm->a[FAKE_TARGET_2], "a[t]");

    // should end:
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_while_neq(struct iovm1_t *vm) {
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
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "pre");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    VERIFY_EQ_INT(1, fake_while_neq.count, "while_neq_cb() invocations");
    VERIFY_EQ_INT(target, fake_while_neq.pre.target, "while_neq_cb(_, target, _, _)");
    VERIFY_EQ_INT(0, fake_while_neq.pre.address, "while_neq_cb(_, _, address, _)");
    VERIFY_EQ_INT(0x55, (int) fake_while_neq.pre.comparison, "while_neq_cb(_, _, _, comparison)");

    // should end:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_while_eq(struct iovm1_t *vm) {
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
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "pre");

    // WHILE_EQ:

    // verify invocations:
    VERIFY_EQ_INT(0, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    VERIFY_EQ_INT(1, fake_while_eq.count, "while_eq_cb() invocations");
    VERIFY_EQ_INT(target, fake_while_eq.pre.target, "while_eq_cb(_, target, _, _)");
    VERIFY_EQ_INT(0, fake_while_eq.pre.address, "while_eq_cb(_, _, address, _)");
    VERIFY_EQ_INT(0x55, (int) fake_while_eq.pre.comparison, "while_eq_cb(_, _, _, comparison)");

    // should end:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_read_target_2(struct iovm1_t *vm) {
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
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "pre");

    // READ:

    // verify invocations:
    VERIFY_EQ_INT(1, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(target, fake_read.pre.target, "read_cb(_, target, _, _)");
    VERIFY_EQ_INT(2, fake_read.pre.len, "read_cb(_, _, _, len)");
    VERIFY_EQ_INT(0, (unsigned)fake_read.pre.address, "a[t]");
    VERIFY_EQ_INT(2, (unsigned)fake_read.post.address, "a[t]");

    // should end:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_read_target_3(struct iovm1_t *vm) {
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
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "pre");

    // READ:

    // verify invocations:
    VERIFY_EQ_INT(1, fake_read.count, "read_cb() invocations");
    VERIFY_EQ_INT(target, fake_read.pre.target, "read_cb(_, target, _, _)");
    VERIFY_EQ_INT(2, fake_read.pre.len, "read_cb(_, _, _, len)");
    VERIFY_EQ_INT(0, (unsigned)fake_read.pre.address, "a[t]");
    VERIFY_EQ_INT(2, (unsigned)fake_read.post.address, "a[t]");

    // should end:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_write_target_2(struct iovm1_t *vm) {
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
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "pre");

    // WRITE:

    // verify invocations:
    VERIFY_EQ_INT(1, fake_write.count, "write_cb() invocations");
    VERIFY_EQ_INT(target, fake_write.pre.target, "write_cb(_, target, _, _, _)");
    VERIFY_EQ_INT(2, (unsigned)(fake_write.pre.i_data.off), "write_cb(_, _, _, i_data, _)");
    VERIFY_EQ_INT(2, fake_write.pre.len, "write_cb(_, _, _, _, len)");
    VERIFY_EQ_INT(0, (unsigned)fake_write.pre.address, "a[t]");
    VERIFY_EQ_INT(2, (unsigned)fake_write.post.address, "a[t]");

    // should end:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_write_target_3(struct iovm1_t *vm) {
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
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(vm), "pre");

    // WRITE:

    // verify invocations:
    VERIFY_EQ_INT(1, fake_write.count, "write_cb() invocations");
    VERIFY_EQ_INT(target, fake_write.pre.target, "write_cb(_, target, _, _, _)");
    VERIFY_EQ_INT(2, (unsigned)(fake_write.pre.i_data.off), "write_cb(_, _, _, i_data, _)");
    VERIFY_EQ_INT(2, fake_write.pre.len, "write_cb(_, _, _, _, len)");
    VERIFY_EQ_INT(0, (unsigned)fake_write.pre.address, "a[t]");
    VERIFY_EQ_INT(2, (unsigned)fake_write.post.address, "a[t]");

    // should end:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_reset_from_end(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    // can move from VERIFIED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "pre");

    return 0;
}

int test_reset_retry(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_INST_END
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "pre");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

    // can move from ENDED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "pre");

    // execute again:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "pre");

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

    // misc tests:
    run_test(test_reset_from_loaded)
    run_test(test_reset_from_execute_fails)

    // exec tests:
    run_test(test_end)
    run_test(test_setbank_setoffs)
    run_test(test_while_neq)
    run_test(test_while_eq)
    run_test(test_read_target_2)
    run_test(test_read_target_3)
    run_test(test_write_target_2)
    run_test(test_write_target_3)
    run_test(test_reset_from_end)
    run_test(test_reset_retry)

    return 0;
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    run_test_suite();

    fprintf(stdout, "ran tests; %d succeeded, %d failed\n", tests_passed, tests_failed);

    return 0;
}
