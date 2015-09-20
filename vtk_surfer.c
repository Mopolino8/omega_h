#include "vtk.h"
#include "mesh.h"
#include "mark.h"
#include "subset.h"
#include "loop.h"
#include "ints.h"
#include "field.h"
#include "recover_by_volume.h"
#include <assert.h>
#include <stdio.h>

static char const* help_str =
"         ~~~    O__\n"
"       ~~~     /|\n"
"      ~~~~~     |\\\n"
"     ~~~~~~~~ =========\n"
"   ~~~~~~~~~~~~~~~\n"
"~~~~~~~~~~~~~~~~~~~~~~~\n"
"\nNAME\n"
"\tvtk_surfer.exe -- extract surface from large VTK files\n"
"\nSYNOPSIS\n"
"\tvtk_surfer.exe input.vtu output.vtu\n"
"\nDESCRIPTION\n"
"\tvtk_surfer will read a single mesh part,\n"
"\tproject all element fields to nodes,\n"
"\tand write out the surface nodes and triangles.\n";


int main(int argc, char** argv)
{
  if (argc != 3) {
    printf("%s", help_str);
    return -1;
  }
  struct mesh* m = read_vtk(argv[1]);
  for (unsigned i = 0; i < mesh_count_fields(m, mesh_dim(m)); ++i) {
    struct const_field* f = mesh_get_field(m, mesh_dim(m), i);
    mesh_recover_by_volume(m, f->name);
  }
  unsigned d = mesh_dim(m);
  unsigned ns = mesh_count(m, d - 1);
  unsigned* pb = mesh_mark_part_boundary(m);
  unsigned* off = ints_exscan(pb, ns);
  loop_free(pb);
  struct mesh* sm = subset_mesh(m, d - 1, off);
  free_mesh(m);
  loop_free(off);
  write_vtk(sm, argv[2]);
  free_mesh(sm);
}