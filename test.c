#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "iovm.h"

int tests_passed = 0;
int tests_failed = 0;

#define VERIFY_EQ_INT(expected, got, name) \
    if ((expected) != (got)) { \
        fprintf(stderr, "expected " name " of %d 0x%x; got %d 0x%x\n", expected, expected, got, got); \
        return 1; \
    }

///////////////////////////////////////////////////////////////////////////////////////////
// FAKE implementation:
///////////////////////////////////////////////////////////////////////////////////////////

struct {
    int count;
    struct iovm1_t *vm;
    enum iovm1_target_e target;
    uint32_t address;
} fake_iovm1_target_set_address;

struct {
    int count;
    struct iovm1_t *vm;
    enum iovm1_target_e target;
    int advance;
    uint8_t *o_data;
} fake_iovm1_target_read;

struct {
    int count;
    struct iovm1_t *vm;
    enum iovm1_target_e target;
    int advance;
    uint8_t data;
} fake_iovm1_target_write;

struct {
    int count;
    struct iovm1_t *vm;
    uint8_t data;
} fake_iovm1_emit;

uint8_t fake_last_emitted;
struct {
    uint32_t address;

    uint8_t expected_read;

    uint8_t last_write;
    uint8_t last_read;
} fake_target[4];

void fake_reset(void) {
    fake_iovm1_target_set_address.count = 0;
    fake_iovm1_target_set_address.vm = 0;
    fake_iovm1_target_set_address.target = 0;
    fake_iovm1_target_set_address.address = 0;

    fake_iovm1_target_read.count = 0;
    fake_iovm1_target_read.vm = 0;
    fake_iovm1_target_read.target = 0;
    fake_iovm1_target_read.advance = 0;
    fake_iovm1_target_read.o_data = 0;

    fake_iovm1_target_write.count = 0;
    fake_iovm1_target_write.vm = 0;
    fake_iovm1_target_write.target = 0;
    fake_iovm1_target_write.advance = 0;
    fake_iovm1_target_write.data = 0;

    fake_iovm1_emit.count = 0;
    fake_iovm1_emit.vm = 0;
    fake_iovm1_emit.data = 0;

    fake_last_emitted = 0;
    for (int t = 0; t < 4; t++) {
        fake_target[t].address = 0;
        fake_target[t].expected_read = 0;
        fake_target[t].last_write = 0;
        fake_target[t].last_read = 0;
    }
}

int iovm1_target_set_address(struct iovm1_t *vm, enum iovm1_target_e target, uint32_t address) {
    fake_iovm1_target_set_address.count++;
    fake_iovm1_target_set_address.vm = vm;
    fake_iovm1_target_set_address.target = target;
    fake_iovm1_target_set_address.address = address;

    if (target > IOVM1_TARGET_SNESCMD) {
        return -1;
    }

    fake_target[target].address = address;

    return 0;
}

int iovm1_target_read(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t *o_data) {
    fake_iovm1_target_read.count++;
    fake_iovm1_target_read.vm = vm;
    fake_iovm1_target_read.target = target;
    fake_iovm1_target_read.advance = advance;
    fake_iovm1_target_read.o_data = o_data;

    if (target > IOVM1_TARGET_SNESCMD) {
        return -1;
    }

    *o_data = fake_target[target].expected_read;
    fake_target[target].last_read = fake_target[target].expected_read;

    if (advance) {
        fake_target[target].address++;
    }

    return 0;
}

int iovm1_target_write(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t data) {
    fake_iovm1_target_write.count++;
    fake_iovm1_target_write.vm = vm;
    fake_iovm1_target_write.target = target;
    fake_iovm1_target_write.advance = advance;
    fake_iovm1_target_write.data = data;

    if (target > IOVM1_TARGET_SNESCMD) {
        return -1;
    }

    fake_target[target].last_write = data;
    if (advance) {
        fake_target[target].address++;
    }

    return 0;
}

int iovm1_emit(struct iovm1_t *vm, uint8_t data) {
    fake_iovm1_emit.count++;
    fake_iovm1_emit.vm = vm;
    fake_iovm1_emit.data = data;

    fake_last_emitted = data;

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// TEST CODE:
///////////////////////////////////////////////////////////////////////////////////////////

int test_end() {
    int r;
    struct iovm1_t vm;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_END, 0, 0, 0, 0)
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_non_repeat_non_immed_sram() {
    int r;
    struct iovm1_t vm;
    int target = IOVM1_TARGET_SRAM;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 0, 0, target)
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READING, iovm1_state(&vm), "state");

    // performs READ:
    fake_target[target].expected_read = 0xAA;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0xAA, (int)*fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(1, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(1, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT((int) fake_target[target].expected_read, (int) fake_target[target].last_read, "byte read");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_non_repeat_non_immed_snescmd() {
    int r;
    struct iovm1_t vm;
    int target = IOVM1_TARGET_SNESCMD;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 0, 0, target)
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READING, iovm1_state(&vm), "state");

    // performs READ:
    fake_target[target].expected_read = 0x55;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0x55, (int)*fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(1, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(1, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT(0x55, (int) fake_target[target].last_read, "byte read");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_write_non_repeat_immed_sram() {
    int r;
    struct iovm1_t vm;
    int target = IOVM1_TARGET_SRAM;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, 1, 0, 1, IOVM1_TARGET_SRAM),
        0x99,
        0
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // entered WRITING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITING, iovm1_state(&vm), "state");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_write.target, "iovm1_target_write(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_write.advance, "iovm1_target_write(_, _, advance, _)");
    VERIFY_EQ_INT(0x99, (int)fake_iovm1_target_write.data, "iovm1_target_write(_, _, _, data)");

    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(1, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT(0x99, (int) fake_target[target].last_write, "byte written");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_write_non_repeat_immed_snescmd() {
    int r;
    struct iovm1_t vm;
    int target = IOVM1_TARGET_SNESCMD;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, 1, 0, 1, target),
        0xBB,
        0
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // entered WRITING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITING, iovm1_state(&vm), "state");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_write.target, "iovm1_target_write(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_write.advance, "iovm1_target_write(_, _, advance, _)");
    VERIFY_EQ_INT(0xBB, (int)fake_iovm1_target_write.data, "iovm1_target_write(_, _, _, data)");

    VERIFY_EQ_INT(0, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(1, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT(0xBB, (int) fake_target[target].last_write, "byte written");
    VERIFY_EQ_INT((int) fake_last_emitted, (int) fake_target[target].expected_read, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// main runner:
///////////////////////////////////////////////////////////////////////////////////////////

#define run_test(name) \
    { \
        fake_reset(); \
        fprintf(stdout, "running test: " #name "\n"); \
        if ((r = name())) { \
            fprintf(stdout, "test failed\n"); \
            tests_failed++; \
            return r; \
        } else { \
            fprintf(stdout, "test passed\n"); \
            tests_passed++; \
        } \
    }

int run_test_suite() {
    int r;

    run_test(test_end)
    run_test(test_read_non_repeat_non_immed_sram)
    run_test(test_read_non_repeat_non_immed_snescmd)
    run_test(test_write_non_repeat_immed_sram)
    run_test(test_write_non_repeat_immed_snescmd)

    return 0;
}

int main(int argc, char **argv) {
    int r;

    (void) argc;
    (void) argv;

    run_test_suite();

    fprintf(stdout, "ran tests; %d succeeded, %d failed\n", tests_passed, tests_failed);

    return 0;
}
