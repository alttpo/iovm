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
// FAKE host implementation:
///////////////////////////////////////////////////////////////////////////////////////////

struct fake {
    iovm1_memory_chip_t c;
    uint24_t a;

    uint8_t l;
    uint8_t data[256];
};

struct fake fake_default = {};
struct fake fake_host;

void fake_reset(void) {
    fake_host = fake_default;
}

void fake_init_test(struct iovm1_t *vm) {
    iovm1_init(vm);
}

// host interface implementation:

// initialize memory controller to point at specific memory chip and a starting address within it
enum iovm1_error host_memory_init(struct iovm1_t *vm, iovm1_memory_chip_t c, uint24_t a) {
    fake_host.c = c;
    fake_host.a = a;
    return IOVM1_SUCCESS;
}
// validate the addresses of a read operation with the given length against the last host_memory_init() call
enum iovm1_error host_memory_start_read(struct iovm1_t *vm, int l) {
    return IOVM1_SUCCESS;
}
// validate the addresses of a write operation with the given length against the last host_memory_init() call
enum iovm1_error host_memory_start_write(struct iovm1_t *vm, int l) {
    return IOVM1_SUCCESS;
}

// read a byte and advance the chip address forward by 1 byte
uint8_t host_memory_read_auto_advance(struct iovm1_t *vm) {
    return 0;
}
// read a byte and do not advance the chip address; useful for continuously polling a specific address
uint8_t host_memory_read_no_advance(struct iovm1_t *vm) {
    return 0;
}
// write a byte and advance the chip address forward by 1 byte
void host_memory_write_auto_advance(struct iovm1_t *vm, uint8_t b) {
}

// send an abort message to the client
void host_send_abort(struct iovm1_t *vm) {}
// send a read-complete message to the client with the fully read data up to 256 bytes in length
void host_send_read(struct iovm1_t *vm, uint8_t l, uint8_t *d) {}
// send a program-end message to the client
void host_send_end(struct iovm1_t *vm) {}

// initialize a host-side countdown timer to a timeout value for WAIT operation, e.g. duration of a single video frame
void host_timer_reset(struct iovm1_t *vm) {}
// checks if the host-side countdown timer has elapsed down to or below 0
bool host_timer_elapsed(struct iovm1_t *vm) {
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////
// TEST CODE:
///////////////////////////////////////////////////////////////////////////////////////////

int test_reset_from_loaded(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    // can move from LOADED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_reset_from_execute_fails(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
        IOVM1_OPCODE_READ,
        0x01,
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ, iovm1_get_exec_state(vm), "state");

    // invoke callback:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ, iovm1_get_exec_state(vm), "state");

    // cannot move from EXECUTE_NEXT to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(IOVM1_ERROR_INVALID_OPERATION_FOR_STATE, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ, iovm1_get_exec_state(vm), "state");

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
// TEST CODE FOR iovm1_exec:
///////////////////////////////////////////////////////////////////////////////////////////

int test_end(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(IOVM1_SUCCESS, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(IOVM1_SUCCESS, r, "iovm1_exec() return value");

    // verify invocations:
    //VERIFY_EQ_INT(0, fake_read.count, "read_db() invocations");
    //VERIFY_EQ_INT(0, fake_write.count, "write_cb() invocations");

    // should end:
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_reset_from_end(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    // can move from VERIFIED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    return 0;
}

int test_reset_retry(struct iovm1_t *vm) {
    int r;
    uint8_t proc[] = {
    };

    fake_init_test(vm);

    r = iovm1_load(vm, proc, sizeof(proc));
    VERIFY_EQ_INT(IOVM1_SUCCESS, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_get_exec_state(vm), "state");

    // first execution:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(IOVM1_SUCCESS, r, "iovm1_exec() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_get_exec_state(vm), "state");

    // can move from ENDED to RESET:
    r = iovm1_exec_reset(vm);
    VERIFY_EQ_INT(IOVM1_SUCCESS, r, "iovm1_exec_reset() return value");
    VERIFY_EQ_INT(IOVM1_STATE_RESET, iovm1_get_exec_state(vm), "state");

    // execute again:
    r = iovm1_exec(vm);
    VERIFY_EQ_INT(IOVM1_SUCCESS, r, "iovm1_exec() return value");
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

    // misc tests:
    run_test(test_reset_from_loaded)
    run_test(test_reset_from_execute_fails)

    // exec tests:
    run_test(test_end)
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
