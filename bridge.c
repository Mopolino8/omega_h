#include "bridge.h"
#include "ints.h"
#include <assert.h>
#include <stdlib.h>

unsigned* bridge_graph(
    unsigned nverts,
    unsigned const* offsets,
    unsigned const* edges)
{
  unsigned nhalf_edges = offsets[nverts];
  assert(nhalf_edges % 2 == 0);
  unsigned nedges = nhalf_edges / 2;
  unsigned* half_degrees = malloc(sizeof(unsigned) * nverts);
  for (unsigned i = 0; i < nverts; ++i) {
    unsigned first_edge = offsets[i];
    unsigned end_edge = offsets[i + 1];
    unsigned half_degree = 0;
    for (unsigned j = first_edge; j < end_edge; ++j)
      if (i < edges[j])
        ++half_degree;
    half_degrees[i] = half_degree;
  }
  unsigned* half_offsets = ints_exscan(half_degrees, nverts);
  free(half_degrees);
  unsigned* out = malloc(sizeof(unsigned) * nedges * 2);
  for (unsigned i = 0; i < nverts; ++i) {
    unsigned first_edge = offsets[i];
    unsigned end_edge = offsets[i + 1];
    unsigned* vert_out = out + half_offsets[i] * 2;
    for (unsigned j = first_edge; j < end_edge; ++j)
      if (i < edges[j]) {
        vert_out[0] = i;
        vert_out[1] = edges[j];
        vert_out += 2;
      }
  }
  return out;
}