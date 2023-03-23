#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "iovm.h"

int tests_passed = 0;
int tests_failed = 0;

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define VERIFY_EQ_INT(expected, got, name) \
    do if ((expected) != (got)) { \
        fprintf(stdout, "L%d: expected %s of %d 0x%x; got %d 0x%x\n", __LINE__, name, expected, expected, got, got); \
        return 1; \
    } while (0)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
static int verify_eq_bytes(
    const uint8_t *expectedArr,
    const uint8_t *gotArr,
    unsigned len,
    unsigned line,
    const char *name
) {
    for (unsigned i = 0; i < len; i++) {
        int expected = expectedArr[i];
        int got = gotArr[i];

        if (expected != got) {
            fprintf(
                stdout,
                "L%d: expected %s[%d] of %d 0x%x; got %d 0x%x\n",
                line,
                name,
                i,
                expected, expected,
                got, got
            );
            return 1;
        }
    }
    return 0;
}
#pragma clang diagnostic pop

#define VERIFY_EQ_BYTES(expected, got, len, name) verify_eq_bytes(expected, got, len, __LINE__, name)

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
}

void fake_read_cb(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, unsigned len) {
    fake_read.count++;
    fake_read.vm = vm;
    fake_read.target = target;
    fake_read.r_address = r_address;
    fake_read.len = len;

    assert(target < IOVM1_TARGET_COUNT);

    *r_address += len;
}

void fake_write_cb(struct iovm1_t *vm, iovm1_target target, uint32_t *r_address, const uint8_t *i_data, unsigned len) {
    fake_write.count++;
    fake_write.vm = vm;
    fake_write.target = target;
    fake_write.r_address = r_address;
    fake_write.i_data = i_data;
    fake_write.len = len;

    assert(target < IOVM1_TARGET_COUNT);

    *r_address += len;
}

void fake_while_neq_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison) {
    fake_while_neq.count++;
    fake_while_neq.vm = vm;
    fake_while_neq.target = target;
    fake_while_neq.address = address;
    fake_while_neq.comparison = comparison;

    assert(target < IOVM1_TARGET_COUNT);
}

void fake_while_eq_cb(struct iovm1_t *vm, iovm1_target target, uint32_t address, uint8_t comparison) {
    fake_while_eq.count++;
    fake_while_eq.vm = vm;
    fake_while_eq.target = target;
    fake_while_eq.address = address;
    fake_while_eq.comparison = comparison;

    assert(target < IOVM1_TARGET_COUNT);
}

void fake_init_test(struct iovm1_t *vm) {
    iovm1_init(vm);
    iovm1_set_read_cb(vm, fake_read_cb);
    iovm1_set_write_cb(vm, fake_write_cb);
    iovm1_set_while_neq_cb(vm, fake_while_neq_cb);
    iovm1_set_while_eq_cb(vm, fake_while_eq_cb);
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

int test_end(struct iovm1_t *vm) {
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
    VERIFY_EQ_INT(0, fake_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "iovm1_target_write() invocations");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

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
    VERIFY_EQ_INT(0, fake_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(0xF50010, (int) vm->a[FAKE_TARGET_2], "a[t]");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

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

    VERIFY_EQ_INT(1, fake_while_neq.count, "while_neq_cb() invocations");
    VERIFY_EQ_INT(target, fake_while_neq.target, "while_neq_cb(_, target, _, _)");
    VERIFY_EQ_INT(0, fake_while_neq.address, "while_neq_cb(_, _, address, _)");
    VERIFY_EQ_INT(0x55, (int) fake_while_neq.comparison, "while_neq_cb(_, _, _, comparison)");

    VERIFY_EQ_INT(0, fake_write.count, "iovm1_target_write() invocations");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

#if 0
int test_while_eq(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        // advance, repeat, immed
        IOVM1_MKINST(IOVM1_OPCODE_WHILE_NEQ, 0, 0, 1, target),
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

    // entered WHILE_NEQ state:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WHILE_NEQ_LOOP_ITER, iovm1_get_exec_state(vm), "state");

    // executing one WHILE_NEQ loop iteration:
    fake_target[target].expected_read = 0x55;
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WHILE_NEQ_LOOP_ITER, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0x55, (int) *fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // now read what is expected:
    fake_target[target].expected_read = 0xAA;
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WHILE_NEQ_LOOP_END, iovm1_get_exec_state(vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(2, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0xAA, (int) *fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // should end:
    r = iovm1_exec_step(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_reset_from_verified(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_SETADDR, 0, 0, 1, FAKE_TARGET_2),
        0x10,
        0x00,
        0xF5,
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

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    return 0;
}

int test_reset_from_loaded_fails(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_SETADDR, 0, 0, 1, FAKE_TARGET_2),
        0x10,
        0x00,
        0xF5,
        IOVM1_INST_END
    };

    fake_init_test(&vm);

    r = iovm1_load(&vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(&vm), "state");

    // cannot move from LOADED to RESET:
    r = iovm1_exec_reset(&vm);
    VERIFY_EQ_INT(IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    return 0;
}

int test_reset_from_execute_fails(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_SETADDR, 0, 0, 1, FAKE_TARGET_2),
        0x10,
        0x00,
        0xF5,
        IOVM1_INST_END
    };

    fake_init_test(&vm);

    r = iovm1_load(&vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(&vm), "state");

    r = iovm1_verify(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(&vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(&vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(&vm), "state");

    // cannot move from EXECUTE_NEXT to RESET:
    r = iovm1_exec_reset(&vm);
    VERIFY_EQ_INT(IOVM1_ERROR_VM_INVALID_OPERATION_FOR_STATE, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    return 0;
}

int test_reset_from_end(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_SETADDR, 0, 0, 1, FAKE_TARGET_2),
        0x10,
        0x00,
        0xF5,
        IOVM1_INST_END
    };

    fake_init_test(&vm);

    r = iovm1_load(&vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(&vm), "state");

    r = iovm1_verify(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(&vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(&vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(&vm), "state");

    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(&vm), "state");

    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(1, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // can move from VERIFIED to RESET:
    r = iovm1_exec_reset(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(&vm), "state");

    return 0;
}

int test_read_sram(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 1, 0, target),
        0x02
    };

    fake_init_test(&vm);

    r = iovm1_load(&vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(&vm), "state");

    r = iovm1_verify(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(&vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(&vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_get_exec_state(&vm), "state");

    // performs READ:
    fake_target[target].expected_read = 0xAA;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0xAA, (int) *fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(1, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(1, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT((int) fake_target[target].expected_read, (int) fake_target[target].last_read, "byte read");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    fake_target[target].expected_read = 0xBB;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(2, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0xBB, (int) *fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(2, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(2, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT((int) fake_target[target].expected_read, (int) fake_target[target].last_read, "byte read");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_get_exec_state(&vm), "state");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(&vm), "state");

    return 0;
}

int test_read_snescmd(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_3;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 1, 0, target),
        0x02
    };

    fake_init_test(&vm);

    r = iovm1_load(&vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(&vm), "state");

    r = iovm1_verify(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(&vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(&vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_get_exec_state(&vm), "state");

    // performs READ:
    fake_target[target].expected_read = 0xAA;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0xAA, (int) *fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(1, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(1, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT((int) fake_target[target].expected_read, (int) fake_target[target].last_read, "byte read");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    fake_target[target].expected_read = 0xBB;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_get_exec_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(2, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0xBB, (int) *fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(2, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(2, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT((int) fake_target[target].expected_read, (int) fake_target[target].last_read, "byte read");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(&vm), "state");

    return 0;
}

int test_write_sram(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_2;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, 1, 0, 1, FAKE_TARGET_2),
        0x99,
        0
    };

    fake_init_test(&vm);

    r = iovm1_load(&vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(&vm), "state");

    r = iovm1_verify(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(&vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(&vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(&vm), "state");

    // entered WRITING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_ITER, iovm1_get_exec_state(&vm), "state");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_END, iovm1_get_exec_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_write.target, "iovm1_target_write(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_write.advance, "iovm1_target_write(_, _, advance, _)");
    VERIFY_EQ_INT(0x99, (int) fake_iovm1_target_write.data, "iovm1_target_write(_, _, _, data)");

    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(1, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT(0x99, (int) fake_target[target].last_write, "byte written");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(&vm), "state");

    return 0;
}

int test_write_snescmd(struct iovm1_t *vm) {
    int r;
    int target = FAKE_TARGET_3;
    uint8_t proc[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, 1, 0, 1, target),
        0xBB,
        0
    };

    fake_init_test(&vm);

    r = iovm1_load(&vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(&vm), "state");

    r = iovm1_verify(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_verify() return value");
    VERIFY_EQ_INT(IOVM1_STATE_VERIFIED, iovm1_get_exec_state(&vm), "state");

    // first execution moves to RESET:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(&vm), "state");

    // RESET initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_get_exec_state(&vm), "state");

    // entered WRITING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_ITER, iovm1_get_exec_state(&vm), "state");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_END, iovm1_get_exec_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_write.target, "iovm1_target_write(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_write.advance, "iovm1_target_write(_, _, advance, _)");
    VERIFY_EQ_INT(0xBB, (int) fake_iovm1_target_write.data, "iovm1_target_write(_, _, _, data)");

    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(1, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT(0xBB, (int) fake_target[target].last_write, "byte written");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(&vm), "state");

    return 0;
}
#endif

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
    run_test(test_end)
    run_test(test_setbank_setoffs)
    run_test(test_while_neq)
#if 0
    run_test(test_while_eq)
    run_test(test_reset_from_verified)
    run_test(test_reset_from_loaded_fails)
    run_test(test_reset_from_execute_fails)
    run_test(test_reset_from_end)
    run_test(test_read_sram)
    run_test(test_read_snescmd)
    run_test(test_write_sram)
    run_test(test_write_snescmd)
#endif

    return 0;
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    run_test_suite();

    fprintf(stdout, "ran tests; %d succeeded, %d failed\n", tests_passed, tests_failed);

    return 0;
}
