#include "bridge.h"
#include "ints.h"
#include <assert.h>
#include <stdlib.h>

void bridge_graph(
    unsigned nverts,
    unsigned const adj_offsets[],
    unsigned const adj[],
    unsigned* nedges_out,
    unsigned** verts_of_edges_out)
{
  unsigned nhalf_edges = adj_offsets[nverts];
  assert(nhalf_edges % 2 == 0);
  unsigned nedges = nhalf_edges / 2;
  unsigned* degree_of_verts = malloc(sizeof(unsigned) * nverts);
  for (unsigned i = 0; i < nverts; ++i) {
    unsigned first_adj = adj_offsets[i];
    unsigned end_adj = adj_offsets[i + 1];
    unsigned degree_of_vert = 0;
    for (unsigned j = first_adj; j < end_adj; ++j)
      if (i < adj[j])
        ++degree_of_vert;
    degree_of_verts[i] = degree_of_vert;
  }
  unsigned* bridge_offsets = ints_exscan(degree_of_verts, nverts);
  free(degree_of_verts);
  unsigned* verts_of_edges = malloc(sizeof(unsigned) * nedges * 2);
  for (unsigned i = 0; i < nverts; ++i) {
    unsigned first_adj = adj_offsets[i];
    unsigned end_adj = adj_offsets[i + 1];
    unsigned edge = bridge_offsets[i];
    for (unsigned j = first_adj; j < end_adj; ++j)
      if (i < adj[j]) {
        verts_of_edges[edge * 2 + 0] = i;
        verts_of_edges[edge * 2 + 1] = adj[j];
        ++edge;
      }
  }
  *nedges_out = nedges;
  *verts_of_edges_out = verts_of_edges;
}
