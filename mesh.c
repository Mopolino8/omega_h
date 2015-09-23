#include "mesh.h"
#include <assert.h>        // for assert
#include <string.h>        // for memcpy, memset
#include "loop.h"          // for free, malloc, calloc
#include "bridge_graph.h"  // for bridge_dual_graph, bridge_graph
#include "derive_faces.h"  // for derive_faces
#include "field.h"         // for find_field, fields, add_field, free_field
#include "graph.h"         // for graph (ptr only), free_graph, const_graph
#include "label.h"         // for labels, add_label, find_label, free_labels
#include "reflect_down.h"  // for get_dual, reflect_down
#include "star.h"          // for get_star
#include "tables.h"        // for the_box_conns, the_box_coords, the_box_n...
#include "up_from_down.h"  // for up_from_down

struct up {
  unsigned* offsets;
  unsigned* adj;
  unsigned* directions;
};

struct mesh {
  unsigned elem_dim;
  int padding__;
  unsigned counts[4];
  unsigned* down[4][4];
  struct up* up[4][4];
  struct graph* star[4][4];
  unsigned* dual;
  struct fields fields[4];
  struct labels labels[4];
};

static struct up* new_up(unsigned* offsets, unsigned* adj, unsigned* directions)
{
  struct up* u = loop_malloc(sizeof(*u));
  u->offsets = offsets;
  u->adj = adj;
  u->directions = directions;
  return u;
}

static void free_up(struct up* u)
{
  if (!u)
    return;
  loop_free(u->offsets);
  loop_free(u->adj);
  loop_free(u->directions);
  loop_free(u);
}

struct mesh* new_mesh(unsigned elem_dim)
{
  struct mesh* m = loop_host_malloc(sizeof(*m));
  memset(m, 0, sizeof(*m));
  m->elem_dim = elem_dim;
  return m;
}

struct mesh* new_box_mesh(unsigned elem_dim)
{
  struct mesh* m = new_mesh(elem_dim);
  unsigned nelems = the_box_nelems[elem_dim];
  unsigned nverts = the_box_nverts[elem_dim];
  mesh_set_ents(m, 0, nverts, 0);
  unsigned verts_per_elem = the_down_degrees[elem_dim][0];
  unsigned nbytes = sizeof(unsigned) * verts_per_elem * nelems;
  unsigned* verts_of_elems = loop_malloc(nbytes);
  memcpy(verts_of_elems, the_box_conns[elem_dim], nbytes);
  nbytes = sizeof(double) * 3 * nverts;
  double* coords = loop_malloc(nbytes);
  memcpy(coords, the_box_coords[elem_dim], nbytes);
  mesh_set_ents(m, elem_dim, nelems, verts_of_elems);
  mesh_add_field(m, 0, "coordinates", 3, coords);
  return m;
}

unsigned mesh_dim(struct mesh* m)
{
  return m->elem_dim;
}

unsigned mesh_count(struct mesh* m, unsigned dim)
{
  if (dim > m->elem_dim)
    return 0;
  /* intermediate entities are counted when we derive their
   * vertices, so trigger this process if necessary
   */
  if (dim)
    mesh_ask_down(m, dim, 0);
  return m->counts[dim];
}

void free_mesh(struct mesh* m)
{
  for (unsigned high_dim = 1; high_dim <= m->elem_dim; ++high_dim)
    for (unsigned low_dim = 0; low_dim < high_dim; ++low_dim) {
      loop_free(m->down[high_dim][low_dim]);
      free_up(m->up[low_dim][high_dim]);
      free_graph(m->star[low_dim][high_dim]);
    }
  loop_free(m->dual);
  for (unsigned d = 0; d < 4; ++d) {
    free_fields(&m->fields[d]);
    free_labels(&m->labels[d]);
  }
  loop_free(m);
}

struct const_field* mesh_find_field(struct mesh* m, unsigned dim,
    char const* name)
{
  return find_field(&m->fields[dim], name);
}

struct const_label* mesh_find_label(struct mesh* m, unsigned dim,
    char const* name)
{
  return find_label(&m->labels[dim], name);
}

static void set_down(struct mesh* m, unsigned high_dim, unsigned low_dim,
    unsigned* adj)
{
  assert( ! m->down[high_dim][low_dim]);
  m->down[high_dim][low_dim] = adj;
}

unsigned const* mesh_ask_down(struct mesh* m, unsigned high_dim, unsigned low_dim)
{
  assert(low_dim < high_dim);
  if (m->down[high_dim][low_dim])
    return m->down[high_dim][low_dim];
  if (low_dim) { /* deriving intermediate downward adjacency */
    unsigned nhighs = m->counts[high_dim];
    unsigned const* verts_of_highs = mesh_ask_down(m, high_dim, 0);
    struct const_up* lows_of_verts = mesh_ask_up(m, 0, low_dim);
    unsigned* lows_of_highs = reflect_down(high_dim, low_dim, nhighs,
        verts_of_highs, lows_of_verts->offsets, lows_of_verts->adj);
    set_down(m, high_dim, low_dim, lows_of_highs);
  } else { /* deriving intermediate entities (entity to vertex connectivity) */
    if (high_dim == 1) { /* deriving edges */
      struct const_graph* verts_of_verts = mesh_ask_star(m, 0, m->elem_dim);
      unsigned nverts = m->counts[0];
      unsigned nedges;
      unsigned* verts_of_edges;
      bridge_graph(nverts, verts_of_verts->offsets, verts_of_verts->adj,
          &nedges, &verts_of_edges);
      mesh_set_ents(m, 1, nedges, verts_of_edges);
    } else { /* deriving faces in 3D */
      assert(high_dim == 2);
      assert(m->elem_dim == 3);
      unsigned const* elems_of_elems = mesh_ask_dual(m);
      unsigned nelems = m->counts[m->elem_dim];
      unsigned const* verts_of_elems = m->down[m->elem_dim][0];
      unsigned nfaces;
      unsigned* elems_of_faces;
      unsigned* elem_face_of_faces;
      bridge_dual_graph(m->elem_dim, nelems, elems_of_elems,
          &nfaces, &elems_of_faces, &elem_face_of_faces);
      unsigned* verts_of_faces = derive_faces(nfaces, verts_of_elems,
          elems_of_faces, elem_face_of_faces);
      loop_free(elems_of_faces);
      loop_free(elem_face_of_faces);
      mesh_set_ents(m, 2, nfaces, verts_of_faces);
    }
  }
  return m->down[high_dim][low_dim];
}

static void set_up(struct mesh* m, unsigned low_dim, unsigned high_dim,
    struct up* adj)
{
  assert( ! m->up[low_dim][high_dim]);
  m->up[low_dim][high_dim] = adj;
}

struct const_up* mesh_ask_up(struct mesh* m, unsigned low_dim, unsigned high_dim)
{
  assert(low_dim < high_dim);
  if (m->up[low_dim][high_dim])
    return (struct const_up*) m->up[low_dim][high_dim];
  unsigned const* lows_of_highs = mesh_ask_down(m, high_dim, low_dim);
  unsigned nhighs = m->counts[high_dim];
  unsigned nlows = m->counts[low_dim];
  unsigned* offsets;
  unsigned* highs_of_lows;
  unsigned* directions;
  up_from_down(high_dim, low_dim, nhighs, nlows, lows_of_highs,
      &offsets, &highs_of_lows, &directions);
  set_up(m, low_dim, high_dim, new_up(offsets, highs_of_lows, directions));
  return (struct const_up*) m->up[low_dim][high_dim];
}

static void set_star(struct mesh* m, unsigned low_dim, unsigned high_dim,
    struct graph* adj)
{
  assert( ! m->star[low_dim][high_dim]);
  m->star[low_dim][high_dim] = adj;
}

struct const_graph* mesh_ask_star(struct mesh* m, unsigned low_dim, unsigned high_dim)
{
  assert(low_dim < high_dim);
  if (m->star[low_dim][high_dim])
    return (struct const_graph*) m->star[low_dim][high_dim];
  unsigned const* lows_of_highs = mesh_ask_down(m, high_dim, low_dim);
  struct const_up* highs_of_lows = mesh_ask_up(m, low_dim, high_dim);
  unsigned nlows = m->counts[low_dim];
  unsigned* offsets;
  unsigned* adj;
  get_star(low_dim, high_dim, nlows, highs_of_lows->offsets, highs_of_lows->adj,
      lows_of_highs, &offsets, &adj);
  set_star(m, low_dim, high_dim, new_graph(offsets, adj));
  return (struct const_graph*) m->star[low_dim][high_dim];
}

static void set_dual(struct mesh* m, unsigned* adj)
{
  assert( ! m->dual);
  m->dual = adj;
}

unsigned const* mesh_ask_dual(struct mesh* m)
{
  if (m->dual)
    return m->dual;
  unsigned nelems = m->counts[m->elem_dim];
  unsigned const* verts_of_elems = m->down[m->elem_dim][0];
  struct const_up* elems_of_verts = mesh_ask_up(m, 0, m->elem_dim);
  unsigned* elems_of_elems = get_dual(m->elem_dim, nelems, verts_of_elems,
      elems_of_verts->offsets, elems_of_verts->adj);
  set_dual(m, elems_of_elems);
  return m->dual;
}

void mesh_set_ents(struct mesh* m, unsigned dim, unsigned n, unsigned* verts)
{
  m->counts[dim] = n;
  set_down(m, dim, 0, verts);
}

struct const_field* mesh_add_field(struct mesh* m, unsigned dim, char const* name,
    unsigned ncomps, double* data)
{
  return add_field(&m->fields[dim], name, ncomps, data);
}

void mesh_free_field(struct mesh* m, unsigned dim, char const* name)
{
  remove_field(&m->fields[dim], name);
}

struct const_label* mesh_add_label(struct mesh* m, unsigned dim, char const* name,
    unsigned* data)
{
  return add_label(&m->labels[dim], name, data);
}

unsigned mesh_count_fields(struct mesh* m, unsigned dim)
{
  return m->fields[dim].n;
}

struct const_field* mesh_get_field(struct mesh* m, unsigned dim, unsigned i)
{
  return get_field(&m->fields[dim], i);
}

unsigned mesh_count_labels(struct mesh* m, unsigned dim)
{
  return m->labels[dim].n;
}

struct const_label* mesh_get_label(struct mesh* m, unsigned dim, unsigned i)
{
  return get_label(&m->labels[dim], i);
}
