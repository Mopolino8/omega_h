#include <assert.h>

#include "bcast.h"
#include "comm.h"
#include "ints.h"
#include "loop.h"
#include "mesh.h"
#include "migrate_mesh.h"
#include "parallel_mesh.h"
#include "subset.h"
#include "vtk.h"

int main()
{
  comm_init();
  assert(comm_size() == 2);
  struct mesh* m = 0;
  if (comm_rank() == 0) {
    m = new_box_mesh(2);
    mesh_count(m, 1); //trigger edge creation
  }
  m = bcast_mesh_metadata(m);
  mesh_make_parallel(m);
  write_mesh_vtk(m, "before.pvtu");
  unsigned n = 1;
  if (comm_rank() == 0) {
    unsigned recvd_elem_ranks[1] = {0};
    unsigned recvd_elem_ids[1] = {0};
    migrate_mesh(&m, n, recvd_elem_ranks, recvd_elem_ids);
  } else {
    unsigned recvd_elem_ranks[1] = {0};
    unsigned recvd_elem_ids[1] = {1};
    migrate_mesh(&m, n, recvd_elem_ranks, recvd_elem_ids);
  }
  write_mesh_vtk(m, "after.pvtu");
  free_mesh(m);
  comm_fini();
}
