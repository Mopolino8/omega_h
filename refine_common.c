#include "refine_common.h"

#include <assert.h>

#include "concat.h"
#include "graph.h"
#include "indset.h"
#include "ints.h"
#include "loop.h"
#include "mesh.h"
#include "quality.h"
#include "refine_class.h"
#include "refine_nodal.h"
#include "refine_qualities.h"
#include "refine_topology.h"
#include "splits_to_elements.h"
#include "tag.h"

static void refine_verts(struct mesh* m, struct mesh* m_out,
    unsigned src_dim, unsigned const* gen_offset_of_srcs)
{
  unsigned nverts = mesh_count(m, 0);
  unsigned nsrcs = mesh_count(m, src_dim);
  unsigned nsplit_srcs = gen_offset_of_srcs[nsrcs];
  unsigned nverts_out = nverts + nsplit_srcs;
  mesh_set_ents(m_out, 0, nverts_out, 0);
  unsigned const* verts_of_srcs = mesh_ask_down(m, src_dim, 0);
  for (unsigned i = 0; i < mesh_count_tags(m, 0); ++i) {
    struct const_tag* t = mesh_get_tag(m, 0, i);
    if (t->type != TAG_F64)
      continue;
    double* gen_vals = refine_nodal(src_dim, nsrcs, verts_of_srcs,
        gen_offset_of_srcs, t->ncomps, t->d.f64);
    double* vals_out = concat_doubles(t->ncomps, t->d.f64, nverts,
        gen_vals, nsplit_srcs);
    loop_free(gen_vals);
    mesh_add_tag(m_out, 0, t->type, t->name, t->ncomps, vals_out);
  }
  refine_class(m, m_out, src_dim, gen_offset_of_srcs);
}

unsigned refine_common(
    struct mesh** p_m,
    unsigned src_dim,
    unsigned* candidates,
    double qual_floor,
    unsigned require_better)
{
  struct mesh* m = *p_m;
  unsigned elem_dim = mesh_dim(m);
  unsigned nsrcs = mesh_count(m, src_dim);
  if (!uints_max(candidates, nsrcs))
    return 0;
  double* src_quals = mesh_refine_qualities(m, src_dim, candidates,
      qual_floor, require_better);
  if (!uints_max(candidates, nsrcs)) {
    loop_free(src_quals);
    return 0;
  }
  unsigned* gen_offset_of_srcs = mesh_indset_offsets(m, src_dim, candidates,
      src_quals);
  loop_free(src_quals);
  unsigned nverts = mesh_count(m, 0);
  unsigned* gen_vert_of_srcs = LOOP_MALLOC(unsigned, nsrcs);
  for (unsigned i = 0; i < nsrcs; ++i)
    if (gen_offset_of_srcs[i] != gen_offset_of_srcs[i + 1])
      gen_vert_of_srcs[i] = nverts + gen_offset_of_srcs[i];
  struct mesh* m_out = new_mesh(elem_dim);
  refine_verts(m, m_out, src_dim, gen_offset_of_srcs);
  unsigned* gen_offset_of_elems;
  unsigned* gen_direction_of_elems;
  unsigned* gen_vert_of_elems;
  mesh_splits_to_elements(m, src_dim, gen_offset_of_srcs, gen_vert_of_srcs,
      &gen_offset_of_elems, &gen_direction_of_elems, &gen_vert_of_elems);
  loop_free(gen_vert_of_srcs);
  loop_free(gen_offset_of_srcs);
  unsigned ngen_elems;
  unsigned* verts_of_gen_elems;
  mesh_refine_topology(m, src_dim, elem_dim,
      gen_offset_of_elems, gen_vert_of_elems, gen_direction_of_elems,
      &ngen_elems, &verts_of_gen_elems);
  loop_free(gen_vert_of_elems);
  loop_free(gen_direction_of_elems);
  unsigned nelems = mesh_count(m, elem_dim);
  unsigned* offset_of_same_elems = uints_negate_offsets(
      gen_offset_of_elems, nelems);
  loop_free(gen_offset_of_elems);
  unsigned const* verts_of_elems = mesh_ask_down(m, elem_dim, 0);
  unsigned nelems_out;
  unsigned* verts_of_elems_out;
  concat_verts_of_elems(elem_dim, nelems, ngen_elems, verts_of_elems,
      offset_of_same_elems, verts_of_gen_elems,
      &nelems_out, &verts_of_elems_out);
  mesh_set_ents(m_out, elem_dim, nelems_out, verts_of_elems_out);
  loop_free(offset_of_same_elems);
  loop_free(verts_of_gen_elems);
  free_mesh(m);
  *p_m = m_out;
  return 1;
}
