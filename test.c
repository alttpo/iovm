#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "iovm.h"

int tests_passed = 0;
int tests_failed = 0;

#define VERIFY(e, msg, ...) \
    if (!(e)) {                                     \
        fprintf(stderr, msg "\n", ##__VA_ARGS__);   \
        return 1;                                   \
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
    uint8_t byte_written;
    uint8_t byte_to_read;
} fake_target[4] = {0, 0, 0, 0};


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
    *o_data = fake_target[target].byte_to_read;
    return 0;
}

int iovm1_target_write(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t data) {
    (void) vm;
    if (target > IOVM1_TARGET_SNESCMD) {
        return -1;
    }

    fake_invocations[fake_iovm1_target_write]++;
    fake_target[target].byte_written = data;
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

int test_trivial_procedure() {
    int r;
    struct iovm1_t vm;
    uint8_t tp_end[] = {0};

    r = iovm1_load(&vm, 1, tp_end);
    VERIFY(r == 0, "expected successful iovm1_load()");
    VERIFY(iovm1_state(&vm) == IOVM1_STATE_LOADED, "expected state == IOVM1_STATE_LOADED");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY(r == 0, "expected successful iovm1_exec_step()");
    VERIFY(iovm1_state(&vm) == IOVM1_STATE_EXECUTING, "expected state == IOVM1_STATE_EXECUTING");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY(r == 0, "expected successful iovm1_exec_step()");
    VERIFY(iovm1_state(&vm) == IOVM1_STATE_ENDED, "expected state == IOVM1_STATE_ENDED");

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// main runner:
///////////////////////////////////////////////////////////////////////////////////////////

#define run_test(name) \
    fprintf(stdout, "running test: " #name "\n"); \
    if ((r = test_##name())) { \
        fprintf(stdout, "test failed\n"); \
        tests_failed++; \
        return r; \
    } else { \
        fprintf(stdout, "test passed\n"); \
        tests_passed++; \
    }

int run_test_suite() {
    int r;

    run_test(trivial_procedure)

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
