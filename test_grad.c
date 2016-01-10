#include "algebra.h"
#include "element_gradients.h"
#include "eval_field.h"
#include "mesh.h"
#include "recover_by_volume.h"
#include "refine_by_size.h"
#include "size_from_hessian.h"
#include "vtk.h"

static void dye_fun(double const* coords, double* v)
{
  double x[3];
  double const l[3] = {.25, .5, .5};
  double const r[3] = {.75, .5, .5};
  double dir = 1;
  subtract_vectors(coords, l, x, 3);
  if (vector_norm(x, 3) > .25) {
    dir = -1;
    subtract_vectors(coords, r, x, 3);
  }
  if (vector_norm(x, 3) > .25) {
    v[0] = 0;
    return;
  }
  v[0] = 4 * dir * (.25 - vector_norm(x, 3));
}

static void size_fun(double const* x, double* s)
{
  (void) x;
  s[0] = 0.1;
}

int main()
{
  struct mesh* m = new_box_mesh(3);
  mesh_eval_field(m, 0, "adapt_size", 1, size_fun);
  while (refine_by_size(&m, 0));
  mesh_eval_field(m, 0, "dye", 1, dye_fun);
  mesh_element_gradients(m, "dye");
  mesh_recover_by_volume(m, "grad_dye");
  mesh_element_gradients(m, "grad_dye");
  mesh_recover_by_volume(m, "grad_grad_dye");
  mesh_free_tag(m, 0, "adapt_size");
  double weight[1] = {0.05 / 75.0};
  mesh_size_from_hessian(m, "grad_grad_dye", weight, 0.05, 0.1);
  write_mesh_vtk(m, "grad.vtu");
  free_mesh(m);
}
