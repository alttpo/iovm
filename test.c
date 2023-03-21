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

enum fake_methods_e {
    fake_iovm1_target_set_address,
    fake_iovm1_target_read,
    fake_iovm1_target_write,
    fake_iovm1_emit,
    fake_method_count
};
int fake_invocations[fake_method_count] = {0};

uint8_t fake_last_emitted = 0;
struct {
    uint32_t address;

    uint8_t got_write;

    uint8_t expected_read;
    uint8_t got_read;
} fake_target[4] = {0, 0, 0, 0};

void fake_reset(void) {
    fake_last_emitted = 0;
    for (enum fake_methods_e m = 0; m < fake_method_count; m++) {
        fake_invocations[m] = 0;
    }
    for (int t = 0; t < 4; t++) {
        fake_target[t].address = 0;
        fake_target[t].got_write = 0;
        fake_target[t].expected_read = 0;
        fake_target[t].got_read = 0;
    }
}

int iovm1_target_set_address(struct iovm1_t *vm, enum iovm1_target_e target, uint32_t address) {
    (void) vm;
    if (target > IOVM1_TARGET_SNESCMD) {
        return -1;
    }

    fake_invocations[fake_iovm1_target_set_address]++;
    fake_target[target].address = address;

    return 0;
}

int iovm1_target_read(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t *o_data) {
    (void) vm;
    if (target > IOVM1_TARGET_SNESCMD) {
        return -1;
    }

    fake_invocations[fake_iovm1_target_read]++;
    *o_data = fake_target[target].expected_read;
    fake_target[target].got_read = *o_data;
    if (advance) {
        fake_target[target].address++;
    }

    return 0;
}

int iovm1_target_write(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t data) {
    (void) vm;
    if (target > IOVM1_TARGET_SNESCMD) {
        return -1;
    }

    fake_invocations[fake_iovm1_target_write]++;
    fake_target[target].got_write = data;
    if (advance) {
        fake_target[target].address++;
    }

    return 0;
}

int iovm1_emit(struct iovm1_t *vm, uint8_t data) {
    (void) vm;

    fake_invocations[fake_iovm1_emit]++;
    fake_last_emitted = data;

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// TEST CODE:
///////////////////////////////////////////////////////////////////////////////////////////

int test_end() {
    int r;
    struct iovm1_t vm;
    uint8_t tp_end[] = {0};

    r = iovm1_load(&vm, 1, tp_end);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_invocations[fake_iovm1_target_set_address], "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_invocations[fake_iovm1_target_read], "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_invocations[fake_iovm1_target_write], "iovm1_target_write() invocations");
    VERIFY_EQ_INT(0, fake_invocations[fake_iovm1_emit], "iovm1_emit() invocations");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_sram() {
    int r;
    struct iovm1_t vm;
    uint8_t tp_end[] = {
        IOVM1_MKOPCODE(IOVM1_OPCODE_READ, 1, 0, 0, IOVM1_TARGET_SRAM)
    };

    r = iovm1_load(&vm, 1, tp_end);
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
    fake_target[IOVM1_TARGET_SRAM].expected_read = 0xAA;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_invocations[fake_iovm1_target_set_address], "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(1, fake_invocations[fake_iovm1_target_read], "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_invocations[fake_iovm1_target_write], "iovm1_target_write() invocations");
    VERIFY_EQ_INT(1, fake_invocations[fake_iovm1_emit], "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT((int)fake_target[IOVM1_TARGET_SRAM].expected_read, (int)fake_target[IOVM1_TARGET_SRAM].got_read, "byte read");
    VERIFY_EQ_INT((int)fake_last_emitted, (int)fake_target[IOVM1_TARGET_SRAM].expected_read, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_snescmd() {
    int r;
    struct iovm1_t vm;
    uint8_t tp_end[] = {
        IOVM1_MKOPCODE(IOVM1_OPCODE_READ, 1, 0, 0, IOVM1_TARGET_SNESCMD)
    };

    r = iovm1_load(&vm, 1, tp_end);
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
    fake_target[IOVM1_TARGET_SNESCMD].expected_read = 0x55;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTING, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_invocations[fake_iovm1_target_set_address], "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(1, fake_invocations[fake_iovm1_target_read], "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_invocations[fake_iovm1_target_write], "iovm1_target_write() invocations");
    VERIFY_EQ_INT(1, fake_invocations[fake_iovm1_emit], "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT((int)fake_target[IOVM1_TARGET_SNESCMD].expected_read, (int)fake_target[IOVM1_TARGET_SNESCMD].got_read, "byte read");
    VERIFY_EQ_INT((int)fake_last_emitted, (int)fake_target[IOVM1_TARGET_SNESCMD].expected_read, "byte emitted");

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
    run_test(test_read_sram)
    run_test(test_read_snescmd)

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
