#include <stdio.h>
#include <stdint.h>

#include "iovm.h"

int tests_passed = 0;
int tests_failed = 0;

#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

#define VERIFY_EQ_INT(expected, got, name) \
    do if ((expected) != (got)) { \
        fprintf(stdout, "L" STRINGIZE(__LINE__) ": expected " name " of %d 0x%x; got %d 0x%x\n", expected, expected, got, got); \
        return 1; \
    } while (0)

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

int test_iovm1_response_size_0(void) {
    int r;
    struct iovm1_t vm;
    uint32_t emit_size;
    uint8_t prgm[] = {
        IOVM1_INST_END
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_response_size(&vm, &emit_size);
    VERIFY_EQ_INT(0, r, "iovm1_response_size() return value");
    VERIFY_EQ_INT(0, emit_size, "emit_size");

    return 0;
}

int test_iovm1_response_size_1(void) {
    int r;
    struct iovm1_t vm;
    uint32_t emit_size;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 0, 0, 0, IOVM1_TARGET_SRAM),
        IOVM1_INST_END
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_response_size(&vm, &emit_size);
    VERIFY_EQ_INT(0, r, "iovm1_response_size() return value");
    VERIFY_EQ_INT(1, emit_size, "emit_size");

    return 0;
}

int test_iovm1_response_size_512(void) {
    int r;
    struct iovm1_t vm;
    uint32_t emit_size;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 1, 0, IOVM1_TARGET_SRAM),
        0, // 256
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 1, 0, IOVM1_TARGET_SRAM),
        0, // 256
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, 1, 1, 0, IOVM1_TARGET_SRAM),
        0, // 256
        IOVM1_INST_END
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_response_size(&vm, &emit_size);
    VERIFY_EQ_INT(0, r, "iovm1_response_size() return value");
    VERIFY_EQ_INT(512, emit_size, "emit_size");

    return 0;
}

int test_end(void) {
    int r;
    struct iovm1_t vm;
    uint8_t prgm[] = {
        IOVM1_INST_END
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

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

int test_read_non_repeat_immed(void) {
    int r;
    struct iovm1_t vm;
    int target = IOVM1_TARGET_SRAM;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 0, 1, target),
        0x11
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_state(&vm), "state");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(1, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(0, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT(0x11, (int) fake_last_emitted, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_repeat_immed(void) {
    int r;
    struct iovm1_t vm;
    int target = IOVM1_TARGET_SRAM;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 1, 1, target),
        2,
        0x11,
        0x22
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_state(&vm), "state");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(1, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(0, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT(0x11, (int) fake_last_emitted, "byte emitted");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(2, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(0, (int) fake_target[target].address, "address");
    VERIFY_EQ_INT(0x22, (int) fake_last_emitted, "byte emitted");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_repeat_256_immed(void) {
    int r;
    struct iovm1_t vm;
    int target = IOVM1_TARGET_SRAM;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 1, 1, target),
        0, // treated as 256
        // assume remaining bytes are 0s
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");

    // performs READ 256 times:
    for (int n = 0; n < 256; n++) {
        VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_state(&vm), "state");

        r = iovm1_exec_step(&vm);
        VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");

        // verify invocations:
        VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
        VERIFY_EQ_INT(0, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
        VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
        VERIFY_EQ_INT(n + 1, fake_iovm1_emit.count, "iovm1_emit() invocations");

        // verify expected behavior:
        VERIFY_EQ_INT(0, (int) fake_target[target].address, "address");
        VERIFY_EQ_INT(0, (int) fake_last_emitted, "byte emitted");
    }

    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_state(&vm), "state");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_non_repeat_non_immed_sram(void) {
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
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_state(&vm), "state");

    // performs READ:
    fake_target[target].expected_read = 0xAA;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_state(&vm), "state");

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

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_repeat_non_immed_sram(void) {
    int r;
    struct iovm1_t vm;
    int target = IOVM1_TARGET_SRAM;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 1, 0, target),
        0x02
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_state(&vm), "state");

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

    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_state(&vm), "state");

    // should end:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_non_repeat_non_immed_snescmd(void) {
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
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_state(&vm), "state");

    // performs READ:
    fake_target[target].expected_read = 0x55;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(0, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(target, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0x55, (int) *fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

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

int test_read_repeat_non_immed_snescmd(void) {
    int r;
    struct iovm1_t vm;
    int target = IOVM1_TARGET_SNESCMD;
    uint8_t prgm[] = {
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 1, 0, target),
        0x02
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // entered READING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_state(&vm), "state");

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
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_state(&vm), "state");

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
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_write_non_repeat_immed_sram(void) {
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
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // entered WRITING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_ITER, iovm1_state(&vm), "state");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_END, iovm1_state(&vm), "state");

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
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_write_non_repeat_immed_snescmd(void) {
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
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // entered WRITING state:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_ITER, iovm1_state(&vm), "state");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_END, iovm1_state(&vm), "state");

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
    VERIFY_EQ_INT(IOVM1_STATE_ENDED, iovm1_state(&vm), "state");

    return 0;
}

int test_read_sram_m_write_snescmd(void) {
    int r;
    struct iovm1_t vm;
    uint8_t prgm[] = {
        // sram.addr <- $F50D00
        IOVM1_MKINST(IOVM1_OPCODE_SETADDR, 0, 0, 1, IOVM1_TARGET_SRAM),
        0x00,
        0x0D,
        0xF5,
        // snescmd.addr <- $002DFF
        IOVM1_MKINST(IOVM1_OPCODE_SETADDR, 0, 0, 1, IOVM1_TARGET_SNESCMD),
        0xFF,
        0x2D,
        0x00,
        // read 0x11 from immediate
        IOVM1_MKINST(IOVM1_OPCODE_READ, 1, 0, 0, IOVM1_TARGET_SRAM),
        // write M to SNESCMD
        IOVM1_MKINST(IOVM1_OPCODE_WRITE, 1, 0, 0, IOVM1_TARGET_SNESCMD),
    };

    r = iovm1_load(&vm, sizeof(prgm), prgm);
    VERIFY_EQ_INT(0, r, "iovm1_load() return value");
    VERIFY_EQ_INT(IOVM1_STATE_LOADED, iovm1_state(&vm), "state");

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");

    // SETADDR:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");
    VERIFY_EQ_INT(0xF50D00, (int) fake_target[IOVM1_TARGET_SRAM].address, "sram address");

    // SETADDR:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_EXECUTE_NEXT, iovm1_state(&vm), "state");
    VERIFY_EQ_INT(0x2DFF, (int) fake_target[IOVM1_TARGET_SNESCMD].address, "snescmd address");

    // READ:
    fake_target[IOVM1_TARGET_SRAM].expected_read = 0x11;
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_ITER, iovm1_state(&vm), "state");

    // performs READ:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_READ_LOOP_END, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(2, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_read.count, "iovm1_target_read() invocations");
    VERIFY_EQ_INT(IOVM1_TARGET_SRAM, fake_iovm1_target_read.target, "iovm1_target_read(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_read.advance, "iovm1_target_read(_, _, advance, _)");
    VERIFY_EQ_INT(0x11, (int) *fake_iovm1_target_read.o_data, "iovm1_target_read(_, _, _, *o_data)");

    VERIFY_EQ_INT(0, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(1, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(0xF50D01, (int) fake_target[IOVM1_TARGET_SRAM].address, "sram address");
    VERIFY_EQ_INT(0x11, (int) fake_last_emitted, "byte emitted");

    // WRITE:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_ITER, iovm1_state(&vm), "state");

    // WRITE perform:
    r = iovm1_exec_step(&vm);
    VERIFY_EQ_INT(0, r, "iovm1_exec_step() return value");
    VERIFY_EQ_INT(IOVM1_STATE_WRITE_LOOP_END, iovm1_state(&vm), "state");

    // verify invocations:
    VERIFY_EQ_INT(2, fake_iovm1_target_set_address.count, "iovm1_target_set_address() invocations");
    VERIFY_EQ_INT(1, fake_iovm1_target_read.count, "iovm1_target_read() invocations");

    VERIFY_EQ_INT(1, fake_iovm1_target_write.count, "iovm1_target_write() invocations");
    VERIFY_EQ_INT(IOVM1_TARGET_SNESCMD, fake_iovm1_target_write.target, "iovm1_target_write(_, target, _, _)");
    VERIFY_EQ_INT(1, fake_iovm1_target_write.advance, "iovm1_target_write(_, _, advance, _)");
    VERIFY_EQ_INT(0x11, (int) fake_iovm1_target_write.data, "iovm1_target_write(_, _, _, data)");

    VERIFY_EQ_INT(1, fake_iovm1_emit.count, "iovm1_emit() invocations");

    // verify expected behavior:
    VERIFY_EQ_INT(0xF50D01, (int) fake_target[IOVM1_TARGET_SRAM].address, "sram address");
    VERIFY_EQ_INT(0x2E00, (int) fake_target[IOVM1_TARGET_SNESCMD].address, "snescmd address");
    VERIFY_EQ_INT(0x11, (int) fake_target[IOVM1_TARGET_SNESCMD].last_write, "snescmd last write");

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

int run_test_suite(void) {
    int r;

    run_test(test_iovm1_response_size_0)
    run_test(test_iovm1_response_size_1)
    run_test(test_iovm1_response_size_512)
    run_test(test_end)
    run_test(test_read_non_repeat_immed)
    run_test(test_read_repeat_immed)
    run_test(test_read_repeat_256_immed)
    run_test(test_read_non_repeat_non_immed_sram)
    run_test(test_read_repeat_non_immed_sram)
    run_test(test_read_non_repeat_non_immed_snescmd)
    run_test(test_read_repeat_non_immed_snescmd)
    run_test(test_write_non_repeat_immed_sram)
    run_test(test_write_non_repeat_immed_snescmd)
    run_test(test_read_sram_m_write_snescmd)

    return 0;
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    run_test_suite();

    fprintf(stdout, "ran tests; %d succeeded, %d failed\n", tests_passed, tests_failed);

    return 0;
}
