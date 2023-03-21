#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "iovm.h"

int iovm1_target_set_address(struct iovm1_t *vm, enum iovm1_target_e target, uint32_t address) { return 0; }
int iovm1_target_read(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t *o_data) { return 0; }
int iovm1_target_write(struct iovm1_t *vm, enum iovm1_target_e target, int advance, uint8_t data) { return 0; }
int iovm1_emit(struct iovm1_t *vm, uint8_t data) { return 0; }

int main(int argc, char **argv) {
    int r;
    uint8_t tp_end[] = { 0 };
    struct iovm1_t vm;

    r = iovm1_load(&vm, 1, tp_end);
    assert(r == 0);
    assert(iovm1_state(&vm) == IOVM1_STATE_LOADED);

    // first execution initializes registers:
    r = iovm1_exec_step(&vm);
    assert(r == 0);
    assert(iovm1_state(&vm) == IOVM1_STATE_EXECUTING);

    // should end:
    r = iovm1_exec_step(&vm);
    assert(r == 0);
    assert(iovm1_state(&vm) == IOVM1_STATE_ENDED);

    return 0;
}
