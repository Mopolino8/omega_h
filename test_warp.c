#define _XOPEN_SOURCE 500
#include "mesh.h"
#include "classify_box.h"
#include "refine_by_size.h"
#include "vtk.h"
#include "algebra.h"
#include "warp_to_limit.h"
#include "eval_field.h"
#include "split_sliver_tris.h"
#include "coarsen_by_size.h"
#include "quality.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

static double size_fun(double const x[])
{
  return 0.1;
}

static double the_rotation = M_PI / 4.;

static void warp_fun(double const coords[3], double v[])
{
  double x[3];
  double const mid[3] = {.5, .5, 0};
  subtract_vectors(coords, mid, x, 3);
  double polar_a = atan2(x[1], x[0]);
  double polar_r = vector_norm(x, 3);
  double rot_a = 0;
  if (polar_r < .5)
    rot_a = the_rotation * (2 * (.5 - polar_r));
  double dest_a = polar_a + rot_a;
  double dst[3];
  dst[0] = cos(dest_a) * polar_r;
  dst[1] = sin(dest_a) * polar_r;
  dst[2] = 0;
  subtract_vectors(dst, x, v, 3);
}

static void dye_fun(double const coords[3], double v[])
{
  double x[3];
  double const l[3] = {.25, .5, 0};
  double const r[3] = {.75, .5, 0};
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

static void warped_adapt(struct mesh** p_m)
{
  struct mesh* m = *p_m;
  unsigned done_warp = 0;
  for (unsigned j = 0; j < 40; ++j) {
    if (!done_warp) {
      done_warp = mesh_warp_to_limit(m, 0.1);
      printf("warp\n");
      write_vtk_step(m);
    }
    unsigned did_refine =
      refine_by_size(&m, size_fun);
    if (did_refine) {
      printf("refine\n");
      write_vtk_step(m);
    }
    unsigned did_sliver =
      split_sliver_tris(&m, 0.4, 1.0 / 5.0);
    if (did_sliver) {
      printf("sliver\n");
      write_vtk_step(m);
    }
    unsigned did_coarsen =
      coarsen_by_size(&m, size_fun,
          mesh_min_quality(m), 1.0 / 3.0);
    if (did_coarsen) {
      printf("coarsen\n");
      write_vtk_step(m);
    }
    if (done_warp && !did_refine && !did_sliver && !did_coarsen) {
      *p_m = m;
      return;
    }
  }
  printf("40 operations couldn't converge !\n");
  abort();
}

int main()
{
  struct mesh* m = new_box_mesh(2);
  while (refine_by_size(&m, size_fun));
  mesh_classify_box(m);
  start_vtk_steps("warp");
  mesh_eval_field(m, "dye", 1, dye_fun);
  for (unsigned i = 0; i < 2; ++i) {
    for (unsigned j = 0; j < 4; ++j) {
      mesh_eval_field(m, "warp", 3, warp_fun);
      printf("new warp field\n");
      if (i == 0 && j == 0)
        write_vtk_step(m);
      warped_adapt(&m);
      mesh_free_nodal_field(m, "warp");
    }
    the_rotation = -the_rotation;
  }
  free_mesh(m);
}