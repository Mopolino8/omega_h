#ifndef COLLAPSES_TO_ELEMENTS_H
#define COLLAPSES_TO_ELEMENTS_H

void collapses_to_elements(
    unsigned elem_dim,
    unsigned nelems,
    unsigned const* verts_of_elems,
    unsigned const* gen_offset_of_verts,
    unsigned const* gen_vert_of_verts,
    unsigned** gen_offset_of_elems_out,
    unsigned** gen_vert_of_elems_out,
    unsigned** gen_direction_of_elems_out);

#endif