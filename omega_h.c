#include "include/omega_h.h"

#include "adapt.h"
#include "comm.h"
#include "ghost_mesh.h"
#include "loop.h"
#include "mark.h"
#include "mesh.h"
#include "parallel_mesh.h"
#include "size.h"
#include "tag.h"
#include "tables.h"
#include "vtk_io.h"

void osh_init(void)
{
  comm_init();
}

void osh_fini(void)
{
  comm_fini();
}

void osh_free(osh_t m)
{
  free_mesh((struct mesh*)m);
}

osh_t osh_read_vtk(char const* filename)
{
  return (osh_t) read_mesh_vtk(filename);
}

void osh_write_vtk(osh_t m, char const* filename)
{
  write_mesh_vtk((struct mesh*)m, filename);
}

osh_t osh_new(unsigned elem_dim)
{
  /* TODO: right now only APF uses this function,
     so its set to FULL/parallel for their convenience. */
  return (osh_t) new_mesh(elem_dim, MESH_FULL, 1);
}

unsigned* osh_build_ents(osh_t m, unsigned ent_dim, unsigned nents)
{
  unsigned nverts_per_ent = 0; 
  if (ent_dim)
    nverts_per_ent = the_down_degrees[ent_dim][0];
  unsigned* conn = LOOP_MALLOC(unsigned, nents * nverts_per_ent);
  /* this relies on the fact that mesh_set_ents doesn't expect
     the contents of "conn" to be filled in yet */
  mesh_set_ents((struct mesh*)m, ent_dim, nents, conn);
  return conn;
}

unsigned osh_dim(osh_t m)
{
  return mesh_dim((struct mesh*)m);
}

unsigned osh_nelems(osh_t m)
{
  return mesh_count((struct mesh*)m,
      mesh_dim((struct mesh*)m));
}

unsigned osh_nverts(osh_t m)
{
  return mesh_count((struct mesh*)m, 0);
}

unsigned osh_count(osh_t m, unsigned dim)
{
  return mesh_count((struct mesh*)m, dim);
}

unsigned const* osh_down(osh_t m, unsigned high_dim, unsigned low_dim)
{
  return mesh_ask_down((struct mesh*)m, high_dim, low_dim);
}

unsigned const* osh_up(osh_t m, unsigned low_dim, unsigned high_dim)
{
  return mesh_ask_up((struct mesh*)m, low_dim, high_dim)->adj;
}

unsigned const* osh_up_offs(osh_t m, unsigned low_dim, unsigned high_dim)
{
  return mesh_ask_up((struct mesh*)m, low_dim, high_dim)->offsets;
}

unsigned const* osh_star(osh_t m, unsigned low_dim, unsigned high_dim)
{
  return mesh_ask_star((struct mesh*)m, low_dim, high_dim)->adj;
}

unsigned const* osh_star_offs(osh_t m, unsigned low_dim, unsigned high_dim)
{
  return mesh_ask_star((struct mesh*)m, low_dim, high_dim)->offsets;
}

unsigned const* osh_up_dirs(osh_t m, unsigned low_dim, unsigned high_dim)
{
  return mesh_ask_up((struct mesh*)m, low_dim, high_dim)->directions;
}

double const* osh_coords(osh_t m)
{
  return mesh_find_tag((struct mesh*)m, 0, "coordinates")->d.f64;
}

unsigned const* osh_own_rank(osh_t m, unsigned dim)
{
  return mesh_ask_own_ranks((struct mesh*)m, dim);
}

unsigned const* osh_own_id(osh_t m, unsigned dim)
{
  return mesh_ask_own_ids((struct mesh*)m, dim);
}

unsigned long const* osh_global(osh_t m, unsigned dim)
{
  return mesh_ask_globals((struct mesh*)m, dim);
}

double* osh_new_field(osh_t m, unsigned dim, char const* name, unsigned ncomps)
{
  if (mesh_find_tag((struct mesh*)m, dim, name))
    return osh_get_field(m, dim, name);
  double* data = LOOP_MALLOC(double, ncomps * mesh_count((struct mesh*)m, dim));
  mesh_add_tag((struct mesh*)m, dim, TAG_F64, name, ncomps, data);
  return data;
}

double* osh_get_field(osh_t m, unsigned dim, char const* name)
{
  return mesh_find_tag((struct mesh*)m, dim, name)->d.f64;
}

void osh_free_field(osh_t m, char const* name)
{
  mesh_free_tag((struct mesh*)m, 0, name);
}

unsigned osh_nfields(osh_t om, unsigned dim)
{
  struct mesh* m = (struct mesh*) om;
  unsigned n = 0;
  for (unsigned i = 0; i < mesh_count_tags(m, dim); ++i)
    if (mesh_get_tag(m, dim, i)->type == TAG_F64)
      ++n;
  return n;
}

char const* osh_field(osh_t om, unsigned dim, unsigned i)
{
  struct mesh* m = (struct mesh*) om;
  unsigned n = 0;
  for (unsigned j = 0; j < mesh_count_tags(m, dim); ++j)
    if (mesh_get_tag(m, dim, j)->type == TAG_F64)
      if (i == n++)
        return mesh_get_tag(m, dim, j)->name;
  return 0;
}

unsigned osh_components(osh_t m, unsigned dim, char const* name)
{
  return mesh_find_tag((struct mesh*)m, dim, name)->ncomps;
}

unsigned* osh_new_label(osh_t m, unsigned dim, char const* name, unsigned ncomps)
{
  if (mesh_find_tag((struct mesh*)m, dim, name))
    return osh_get_label(m, dim, name);
  unsigned* data = LOOP_MALLOC(unsigned, ncomps * mesh_count((struct mesh*)m, dim));
  mesh_add_tag((struct mesh*)m, dim, TAG_U32, name, ncomps, data);
  return data;
}

unsigned* osh_get_label(osh_t m, unsigned dim, char const* name)
{
  return mesh_find_tag((struct mesh*)m, dim, name)->d.u32;
}

unsigned long* osh_new_global(osh_t m, unsigned dim)
{
  unsigned nents = mesh_count((struct mesh*)m, dim);
  unsigned long* global = LOOP_MALLOC(unsigned long, nents);
  mesh_set_globals((struct mesh*)m, dim, global);
  return global;
}

void osh_accumulate_to_owner(osh_t m, char const* name)
{
  mesh_accumulate_tag((struct mesh*)m, 0, name);
}

void osh_conform(osh_t m, char const* name)
{
  mesh_conform_tag((struct mesh*)m, 0, name);
}

LOOP_KERNEL(mark_or_kern,
    unsigned const* to_mark,
    unsigned* marked)
  if (to_mark[i])
    marked[i] = 1;
}

void osh_mark_classified(osh_t m, unsigned ent_dim,
    unsigned class_dim, unsigned class_id, unsigned* marked)
{
  unsigned* to_mark = mesh_mark_class((struct mesh*)m, ent_dim,
      class_dim, class_id);
  unsigned nents = osh_count(m, ent_dim);
  LOOP_EXEC(mark_or_kern, nents, to_mark, marked);
  loop_free(to_mark);
}

void osh_free_label(osh_t m, char const* name)
{
  mesh_free_tag((struct mesh*)m, 0, name);
}

void osh_ghost(osh_t m, unsigned nlayers)
{
  ghost_mesh((struct mesh*)m, nlayers);
}

unsigned osh_adapt(osh_t m,
    double size_ratio_floor,
    double good_element_quality,
    unsigned nsliver_layers,
    unsigned max_passes)
{
  return mesh_adapt((struct mesh*)m,
      size_ratio_floor,
      good_element_quality,
      nsliver_layers,
      max_passes);
}

void osh_identity_size(osh_t m, char const* name)
{
  mesh_identity_size_field((struct mesh*)m, name);
}
