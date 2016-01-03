#ifndef INHERIT_H
#define INHERIT_H

struct mesh;

void setup_refine(
    struct mesh* m,
    unsigned src_dim,
    unsigned prod_dim,
    unsigned* gen_dom_offsets[4],
    /* out: */
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4],
    unsigned ngen_offsets[5]);

void refine_class(
    struct mesh* m_in,
    struct mesh* m_out,
    unsigned src_dim,
    unsigned prod_dim,
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4]);

void setup_coarsen(
    struct mesh* m,
    unsigned ent_dim,
    unsigned* gen_offset_of_ents,
    unsigned* offset_of_same_ents,
    /* out: */
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4]);

void coarsen_class(
    struct mesh* m_in,
    struct mesh* m_out,
    unsigned ent_dim,
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4]);

void concat_verts_of_elems(
    unsigned elem_dim,
    unsigned nelems,
    unsigned ngen_elems,
    unsigned const* verts_of_elems,
    unsigned const* offset_of_same_elems,
    unsigned* verts_of_gen_elems,
    unsigned* nelems_out,
    unsigned** verts_of_elems_out);

#endif