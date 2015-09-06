#ifndef COARSEN_COMMON_H
#define COARSEN_COMMON_H

struct mesh;

unsigned coarsen_common(
    struct mesh** p_m,
    unsigned* col_codes,
    double quality_floor,
    unsigned require_better);

#endif
