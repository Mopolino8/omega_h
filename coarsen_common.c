#include "coarsen_common.h"
#include "collapse_codes.h"
#include "ints.h"
#include "check_collapse_class.h"
#include "coarsen_qualities.h"
#include "collapses_to_verts.h"
#include "indset.h"
#include "collapses_to_elements.h"
#include "coarsen_topology.h"
#include "concat.h"
#include "subset.h"
#include "tables.h"
#include "quality.h"
#include "mesh.h"
#include <stdlib.h>

#include <assert.h>
#include <stdio.h>

unsigned coarsen_common(
    struct mesh** p_m,
    unsigned* col_codes,
    double quality_floor,
    double size_ratio_floor)
{
  struct mesh* m = *p_m;
  printf("coarsen: input quality %f\n", mesh_min_quality(m));
  unsigned nedges = mesh_count(m, 1);
  unsigned const* verts_of_edges = mesh_ask_down(m, 1, 0);
  double const* coords = mesh_find_nodal_field(m, "coordinates")->data;
  unsigned nverts = mesh_count(m, 0);
  unsigned elem_dim = mesh_dim(m);
  unsigned const* class_dim = mesh_find_nodal_label(m, "class_dim")->data;
  unsigned const* verts_of_elems = mesh_ask_down(m, elem_dim, 0);
  unsigned const* verts_of_verts_offsets = mesh_ask_star(m, 0, elem_dim)->offsets;
  unsigned const* verts_of_verts = mesh_ask_star(m, 0, elem_dim)->adj;
  unsigned const* elems_of_edges_offsets = mesh_ask_up(m, 1, elem_dim)->offsets;
  unsigned const* elems_of_edges = mesh_ask_up(m, 1, elem_dim)->adj;
  unsigned const* elems_of_edges_directions =
    mesh_ask_up(m, 1, elem_dim)->directions;
  check_collapse_class(elem_dim, nedges, col_codes, class_dim,
      verts_of_elems, verts_of_edges, verts_of_verts_offsets, verts_of_verts,
      elems_of_edges_offsets, elems_of_edges, elems_of_edges_directions);
  unsigned const* elems_of_verts_offsets = mesh_ask_up(m, 0, elem_dim)->offsets;
  unsigned const* elems_of_verts = mesh_ask_up(m, 0, elem_dim)->adj;
  unsigned const* elems_of_verts_directions = mesh_ask_up(m, 0, elem_dim)->directions;
  double* quals_of_edges = coarsen_qualities(elem_dim, nedges, col_codes,
      verts_of_elems, verts_of_edges, elems_of_verts_offsets,
      elems_of_verts, elems_of_verts_directions, coords, quality_floor);
  if (ints_max(col_codes, nedges) == DONT_COLLAPSE) {
    /* early return #2: all small edges failed their classif/quality checks */
    free(quals_of_edges);
    return 0;
  }
  /* from this point forward, some edges will definitely collapse */
  unsigned* candidates;
  unsigned* gen_vert_of_verts;
  double* qual_of_verts;
  unsigned const* edges_of_verts_offsets = mesh_ask_up(m, 0, 1)->offsets;
  unsigned const* edges_of_verts = mesh_ask_up(m, 0, 1)->adj;
  unsigned const* edges_of_verts_directions = mesh_ask_up(m, 0, 1)->directions;
  collapses_to_verts(nverts, verts_of_edges, edges_of_verts_offsets,
      edges_of_verts, edges_of_verts_directions, col_codes, quals_of_edges,
      &candidates, &gen_vert_of_verts, &qual_of_verts);
  free(quals_of_edges);
  unsigned* indset = find_indset(nverts, verts_of_verts_offsets, verts_of_verts,
      candidates, qual_of_verts);
  free(candidates);
  free(qual_of_verts);
  unsigned* gen_offset_of_verts = ints_exscan(indset, nverts);
  free(indset);
  unsigned nelems = mesh_count(m, elem_dim);
  unsigned* gen_offset_of_elems;
  unsigned* gen_vert_of_elems;
  unsigned* gen_direction_of_elems;
  unsigned* offset_of_same_elems;
  collapses_to_elements(elem_dim, nelems, verts_of_elems, gen_offset_of_verts,
      gen_vert_of_verts, &gen_offset_of_elems, &gen_vert_of_elems,
      &gen_direction_of_elems, &offset_of_same_elems);
  free(gen_vert_of_verts);
  unsigned* offset_of_same_verts = ints_negate_offsets(
      gen_offset_of_verts, nverts);
  unsigned nverts_out = offset_of_same_verts[nverts];
  free(gen_offset_of_verts);
  unsigned ngen_elems;
  unsigned* verts_of_gen_elems;
  coarsen_topology(elem_dim, nelems, verts_of_elems, gen_offset_of_elems,
      gen_vert_of_elems, gen_direction_of_elems, &ngen_elems,
      &verts_of_gen_elems);
  {
    double mq = min_element_quality(elem_dim, ngen_elems,
        verts_of_gen_elems, coords);
    printf("coarsen: generated quality %f\n", mq);
  }
  free(gen_offset_of_elems);
  free(gen_vert_of_elems);
  free(gen_direction_of_elems);
  unsigned nelems_out;
  unsigned* verts_of_elems_out;
  concat_verts_of_elems(elem_dim, nelems, ngen_elems, verts_of_elems,
      offset_of_same_elems, verts_of_gen_elems,
      &nelems_out, &verts_of_elems_out);
  free(offset_of_same_elems);
  free(verts_of_gen_elems);
  /* remap element vertices to account for vertex removal */
  unsigned verts_per_elem = the_down_degrees[elem_dim][0];
  for (unsigned i = 0; i < nelems_out * verts_per_elem; ++i)
    verts_of_elems_out[i] = offset_of_same_verts[verts_of_elems_out[i]];
  struct mesh* m_out = new_mesh(elem_dim);
  mesh_set_ents(m_out, 0, nverts_out, 0);
  mesh_set_ents(m_out, elem_dim, nelems_out, verts_of_elems_out);
  for (unsigned i = 0; i < mesh_count_nodal_fields(m); ++i) {
    struct const_field* f = mesh_get_nodal_field(m, i);
    double* vals_out = doubles_subset(nverts, f->ncomps, f->data,
        offset_of_same_verts);
    mesh_add_nodal_field(m_out, f->name, f->ncomps, vals_out);
  }
  unsigned* class_dim_out = ints_subset(nverts, 1, class_dim,
      offset_of_same_verts);
  mesh_add_nodal_label(m_out, "class_dim", class_dim_out);
  free(offset_of_same_verts);
  free_mesh(m);
  *p_m = m_out;
  return 1;
}
