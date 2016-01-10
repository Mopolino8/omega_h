#include <assert.h>

#include "gmsh_io.h"
#include "mesh.h"
#include "vtk.h"

int main(int argc, char** argv)
{
  assert(argc == 3);
  struct mesh* m = read_msh(argv[1]);
  write_mesh_vtk(m, argv[2]);
  free_mesh(m);
}
