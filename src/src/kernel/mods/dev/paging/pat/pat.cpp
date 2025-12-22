#include "pat.h"
#include "../../port.cpp"

void setup_pat(void) {
    uint32_t lo, hi;
    cpu_get_MSR(PAT_MSR, &lo, &hi);
    hi = PAT_WRITE_COMBINING << 24 | PAT_WRITE_PROTECT << 16 | PAT_WRITE_COMBINING << 8 | PAT_WRITE_PROTECT;
    cpu_set_MSR(PAT_MSR, lo, hi);
}