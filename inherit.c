#include "inherit.h"

#include "arrays.h"
#include "infer_class.h"
#include "ints.h"
#include "loop.h"
#include "mesh.h"
#include "refine_topology.h"
#include "tables.h"

static void make_ngen_from_doms(
    unsigned const ndoms[4],
    unsigned* prods_of_doms_offsets[4],
    unsigned ngen[4])
{
  for (unsigned i = 0; i < 4; ++i) {
    if (ndoms[i] && prods_of_doms_offsets[i])
      ngen[i] = prods_of_doms_offsets[i][ndoms[i]];
    else
      ngen[i] = 0;
  }
}

static void make_ngen_offsets(
    unsigned const ngen[4],
    /* out: */
    unsigned ngen_offsets[5])
{
  ngen_offsets[0] = 0;
  for (unsigned i = 0; i < 4; ++i)
    ngen_offsets[i + 1] = ngen_offsets[i] + ngen[i];
}

static unsigned* concat_uints_inherited(
    unsigned width,
    unsigned const ngen_offsets[5],
    unsigned* gen_data[4])
{
  unsigned* out_data = LOOP_MALLOC(unsigned,
      ngen_offsets[4] * width);
  for (unsigned i = 0; i < 4; ++i) {
    loop_memcpy(out_data + ngen_offsets[i] * width,
        gen_data[i],
        (ngen_offsets[i + 1] - ngen_offsets[i]) * width * sizeof(unsigned));
    loop_free(gen_data[i]);
  }
  return out_data;
}

static void inherit_uints_direct(
    unsigned width,
    unsigned ndoms[4],
    unsigned const* dom_data[4],
    unsigned* prods_of_doms_offsets[4],
    /* out: */
    unsigned* gen_data[4])
{
  for (unsigned i = 0; i < 4; ++i) {
    if (ndoms[i] && dom_data[i] && prods_of_doms_offsets[i]) {
      gen_data[i] = uints_expand(ndoms[i], width,
          dom_data[i], prods_of_doms_offsets[i]);
    } else {
      gen_data[i] = 0;
    }
  }
}

static void set_zero(
    unsigned dom_dim,
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4])
{
  ndoms[dom_dim] = 0;
  prods_of_doms_offsets[dom_dim] = 0;
}

static void set_expanded_offsets(
    struct mesh* m,
    unsigned src_dim,
    unsigned prod_dim,
    unsigned dom_dim,
    unsigned* gen_dom_offsets[4],
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4])
{
  ndoms[dom_dim] = mesh_count(m, dom_dim);
  unsigned prods_per_dom = get_prods_per_dom(dom_dim, src_dim, prod_dim);
  prods_of_doms_offsets[dom_dim] = uints_scale(gen_dom_offsets[dom_dim],
      ndoms[dom_dim] + 1, prods_per_dom);
}

void setup_refine(
    struct mesh* m,
    unsigned src_dim,
    unsigned prod_dim,
    unsigned* gen_dom_offsets[4],
    /* out: */
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4],
    unsigned ngen_offsets[5])
{
  unsigned nprods = mesh_count(m, prod_dim);
  ndoms[0] = nprods;
  prods_of_doms_offsets[0] = uints_negate_offsets(
      gen_dom_offsets[prod_dim], nprods);
  if (mesh_get_rep(m) == MESH_FULL) {
    unsigned start = (src_dim > prod_dim) ? src_dim : prod_dim;
    for (unsigned d = 1; d < start; ++d)
      set_zero(d, ndoms, prods_of_doms_offsets);
    for (unsigned d = start; d <= mesh_dim(m); ++d)
      set_expanded_offsets(m, src_dim, prod_dim, d, gen_dom_offsets,
          ndoms, prods_of_doms_offsets);
  } else {
    unsigned dom_dim = (prod_dim == 0) ? src_dim : mesh_dim(m);
    for (unsigned d = 1; d <= mesh_dim(m); ++d)
      if (d != dom_dim)
        set_zero(d, ndoms, prods_of_doms_offsets);
    set_expanded_offsets(m, src_dim, prod_dim, dom_dim, gen_dom_offsets,
        ndoms, prods_of_doms_offsets);
  }
  for (unsigned d = mesh_dim(m) + 1; d < 4; ++d)
    set_zero(d, ndoms, prods_of_doms_offsets);
  unsigned ngen[4];
  make_ngen_from_doms(ndoms, prods_of_doms_offsets, ngen);
  make_ngen_offsets(ngen, ngen_offsets);
}

void setup_coarsen(
    struct mesh* m,
    unsigned ent_dim,
    unsigned* gen_offset_of_ents,
    unsigned* offset_of_same_ents,
    /* out: */
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4])
{
  unsigned nents = mesh_count(m, ent_dim);
  ndoms[0] = nents;
  prods_of_doms_offsets[0] = offset_of_same_ents;
  for (unsigned d = 1; d < 4; ++d)
    if (d != ent_dim)
      set_zero(d, ndoms, prods_of_doms_offsets);
  ndoms[ent_dim] = nents;
  prods_of_doms_offsets[ent_dim] = gen_offset_of_ents;
}

static struct const_tag* setup_uint_tag(
    struct mesh* m,
    char const* name,
    unsigned prod_dim,
    unsigned const* dom_data[4])
{
  struct const_tag* t = mesh_find_tag(m, prod_dim, name);
  dom_data[0] = t->d.u32;
  for (unsigned d = 1; d <= mesh_dim(m); ++d) {
    if (mesh_find_tag(m, d, name))
      dom_data[d] = mesh_find_tag(m, d, name)->d.u32;
    else
      dom_data[d] = 0;
  }
  for (unsigned d = mesh_dim(m) + 1; d < 4; ++d)
    dom_data[d] = 0;
  return t;
}

static void inherit_uint_tag(
    struct mesh* m,
    struct mesh* m_out,
    unsigned prod_dim,
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4],
    char const* name)
{
  unsigned const* dom_data[4];
  struct const_tag* t;
  t = setup_uint_tag(m, name, prod_dim, dom_data);
  unsigned ngen[4];
  make_ngen_from_doms(ndoms, prods_of_doms_offsets, ngen);
  unsigned ngen_offsets[5];
  make_ngen_offsets(ngen, ngen_offsets);
  unsigned* gen_data[4];
  inherit_uints_direct(t->ncomps, ndoms, dom_data, prods_of_doms_offsets,
      gen_data);
  unsigned* data_out = concat_uints_inherited(t->ncomps, ngen_offsets, gen_data);
  mesh_add_tag(m_out, prod_dim, TAG_U32, t->name, t->ncomps, data_out);
}

void refine_class(
    struct mesh* m_in,
    struct mesh* m_out,
    unsigned src_dim,
    unsigned prod_dim,
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4])
{
  if (!mesh_find_tag(m_in, 0, "class_dim"))
    return;
  if (mesh_get_rep(m_in) == MESH_REDUCED) {
    if (prod_dim != 0)
      return;
    mesh_ask_class(m_in, src_dim);
  }
  inherit_uint_tag(m_in, m_out, prod_dim, ndoms, prods_of_doms_offsets,
      "class_dim");
  if (mesh_find_tag(m_in, 0, "class_id"))
    inherit_uint_tag(m_in, m_out, prod_dim, ndoms, prods_of_doms_offsets,
        "class_id");
}

void coarsen_class(
    struct mesh* m_in,
    struct mesh* m_out,
    unsigned ent_dim,
    unsigned ndoms[4],
    unsigned* prods_of_doms_offsets[4])
{
  if (!mesh_find_tag(m_in, 0, "class_dim"))
    return;
  inherit_uint_tag(m_in, m_out, ent_dim, ndoms, prods_of_doms_offsets,
      "class_dim");
  if (mesh_find_tag(m_in, 0, "class_id"))
    inherit_uint_tag(m_in, m_out, ent_dim, ndoms, prods_of_doms_offsets,
        "class_id");
}

void concat_verts_of_elems(
    unsigned elem_dim,
    unsigned nelems,
    unsigned ngen_elems,
    unsigned const* verts_of_elems,
    unsigned const* offset_of_same_elems,
    unsigned* verts_of_gen_elems,
    unsigned* nelems_out,
    unsigned** verts_of_elems_out)
{
  unsigned verts_per_elem = the_down_degrees[elem_dim][0];
  unsigned nsame_elems = offset_of_same_elems[nelems];
  *nelems_out = nsame_elems + ngen_elems;
  unsigned* verts_of_same_elems = uints_expand(nelems, verts_per_elem,
      verts_of_elems, offset_of_same_elems);
  *verts_of_elems_out = concat_uints(verts_per_elem,
      verts_of_same_elems, nsame_elems,
      verts_of_gen_elems, ngen_elems);
  loop_free(verts_of_same_elems);
}