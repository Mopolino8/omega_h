#include "classify_box.h"
#include <math.h>    // for fabs
#include <stdlib.h>  // for malloc
#include "field.h"   // for const_field
#include "mesh.h"    // for mesh_add_nodal_label, mesh_count, mesh_find_noda...

#define EPSILON 1e-10

unsigned* classify_box(
    unsigned nverts,
    double const* coords)
{
  unsigned* out = malloc(sizeof(unsigned) * nverts);
  for (unsigned i = 0; i < nverts; ++i) {
    double const* x = coords + i * 3;
    unsigned classif_dim = 3;
    for (unsigned j = 0; j < 3; ++j)
      if ((fabs(x[j] - 0) < EPSILON) ||
          (fabs(x[j] - 1) < EPSILON))
        --classif_dim;
    out[i] = classif_dim;
  }
  return out;
}

void mesh_classify_box(struct mesh* m)
{
  mesh_add_nodal_label(m, "class_dim",
      classify_box(mesh_count(m, 0),
        mesh_find_nodal_field(m, "coordinates")->data));
}
