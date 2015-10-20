#include "comm.h"

#include <assert.h>

#include "loop.h"

#if USE_MPI
#include <mpi.h>

struct comm {
  MPI_Comm c;
};

#define CALL(f) do { int err = (f); assert(err == MPI_SUCCESS); } while(0)

static struct comm world = { MPI_COMM_WORLD };
static struct comm self = { MPI_COMM_SELF };
static struct comm* using = &world;

void comm_init(void)
{
  CALL(MPI_Init(NULL,NULL));
}

void comm_fini(void)
{
  CALL(MPI_Finalize());
}

struct comm* comm_world(void)
{
  return &world;
}

struct comm* comm_self(void)
{
  return &self;
}

struct comm* comm_using(void)
{
  return using;
}

void comm_use(struct comm* c)
{
  using = c;
}

struct comm* comm_split(struct comm* c, unsigned group, unsigned rank)
{
  struct comm* c2 = LOOP_HOST_MALLOC(struct comm, 1);
  CALL(MPI_Comm_split(c->c, (int) group, (int) rank, &c2->c));
  return c2;
}

struct comm* comm_graph(struct comm* c, unsigned ndests, unsigned const* dests,
    unsigned const* counts)
{
  struct comm* c2 = LOOP_HOST_MALLOC(struct comm, 1);
  int n = (int) ndests;
  int sources[1];
  MPI_Comm_rank(c->c, sources);
  int degrees[1] = {n};
  int* destinations = LOOP_HOST_MALLOC(int, ndests);
  for (unsigned i = 0; i < ndests; ++i)
    destinations[i] = (int) dests[i];
  int* weights = LOOP_HOST_MALLOC(int, ndests);
  for (unsigned i = 0; i < ndests; ++i)
    weights[i] = (int) counts[i];
  CALL(MPI_Dist_graph_create(c->c, n, sources, degrees, destinations,
        weights, MPI_INFO_NULL, 0, &c2->c));
  return c2;
}

void comm_adjacent(struct comm* c, unsigned* nin, unsigned** in,
    unsigned** incounts, unsigned* nout, unsigned** out, unsigned** outcounts)
{
  int indegree, outdegree, weighted;
  CALL(MPI_Dist_graph_neighbors_count(c->c, &indegree, &outdegree, &weighted));
  assert(weighted != 0);
  *nin = (unsigned) indegree;
  *nout = (unsigned) outdegree;
  int* sources = LOOP_HOST_MALLOC(int, *nin);
  int* sourceweights = LOOP_HOST_MALLOC(int, *nin);
  int* destinations = LOOP_HOST_MALLOC(int, *nout);
  int* destweights = LOOP_HOST_MALLOC(int, *nout);
  CALL(MPI_Dist_graph_neighbors(c->c, indegree, sources, sourceweights,
        outdegree, destinations, destweights));
  *in = LOOP_HOST_MALLOC(unsigned, *nin);
  *incounts = LOOP_HOST_MALLOC(unsigned, *nin);
  for (unsigned i = 0; i < *nin; ++i) {
    (*in)[i] = (unsigned) sources[i];
    (*incounts)[i] = (unsigned) sourceweights[i];
  }
  loop_host_free(sources);
  loop_host_free(sourceweights);
  *out = LOOP_HOST_MALLOC(unsigned, *nout);
  *outcounts = LOOP_HOST_MALLOC(unsigned, *nout);
  for (unsigned i = 0; i < *nout; ++i) {
    (*out)[i] = (unsigned) destinations[i];
    (*outcounts)[i] = (unsigned) destweights[i];
  }
  loop_host_free(destinations);
  loop_host_free(destweights);
}

void comm_free(struct comm* c)
{
  assert(c != &world);
  assert(c != &self);
  CALL(MPI_Comm_free(&c->c));
  loop_host_free(c);
}

unsigned comm_rank(void)
{
  int rank;
  CALL(MPI_Comm_rank(using->c, &rank));
  return (unsigned) rank;
}

unsigned comm_size(void)
{
  int size;
  CALL(MPI_Comm_size(using->c, &size));
  return (unsigned) size;
}

void comm_add_doubles(double* p, unsigned n)
{
  CALL(MPI_Allreduce(p, MPI_IN_PLACE, (int) n, MPI_DOUBLE, MPI_SUM, using->c));
}

unsigned long comm_add_ulong(unsigned long x)
{
  CALL(MPI_Allreduce(&x, MPI_IN_PLACE, 1, MPI_UNSIGNED_LONG, MPI_SUM,
        using->c));
  return x;
}

unsigned long comm_exscan_ulong(unsigned long x)
{
  CALL(MPI_Exscan(&x, MPI_IN_PLACE, 1, MPI_UNSIGNED_LONG, MPI_SUM,
        using->c));
  if (!comm_rank())
    x = 0;
  return x;
}

unsigned long comm_max_ulong(unsigned long x)
{
  CALL(MPI_Allreduce(&x, MPI_IN_PLACE, 1, MPI_UNSIGNED_LONG, MPI_MAX,
        using->c));
  return x;
}

#else

void comm_init(void)
{
}

void comm_fini(void)
{
}

struct comm* comm_world(void)
{
  return (struct comm*)1;
}

struct comm* comm_self(void)
{
  return (struct comm*)1;
}

struct comm* comm_using(void)
{
  return (struct comm*)1;
}

void comm_use(struct comm* c)
{
  (void)c;
}

struct comm* comm_split(struct comm* c, unsigned group, unsigned rank)
{
  (void)c;
  assert(group == 0);
  assert(rank == 0);
  return (struct comm*)1;
}

static unsigned graph_ndests = 0;
static unsigned graph_counts = 0;

struct comm* comm_graph(struct comm* c, unsigned ndests, unsigned const* dests,
    unsigned const* counts)
{
  (void)c;
  (void)counts;
  assert(ndests <= 1);
  if (ndests == 1)
    assert(dests[0] == 0);
  graph_ndests = ndests;
  graph_counts = counts[0];
  return (struct comm*)2;
}

void comm_adjacent(struct comm* c, unsigned* nin, unsigned** in,
    unsigned** incounts, unsigned* nout, unsigned** out, unsigned** outcounts)
{
  assert(c == (struct comm*)2);
  if (graph_ndests == 0) {
    *nin = *nout = 0;
    *in = *incounts = *out = *outcounts = 0;
  } else {
    assert(graph_ndests == 1);
    *nin = *nout = 1;
    *in = LOOP_HOST_MALLOC(unsigned, 1);
    *incounts = LOOP_HOST_MALLOC(unsigned, 1);
    *out = LOOP_HOST_MALLOC(unsigned, 1);
    *outcounts = LOOP_HOST_MALLOC(unsigned, 1);
    (*in)[0] = (*out)[0] = 0;
    (*incounts)[0] = (*outcounts)[0] = graph_counts;
  }
}

void comm_free(struct comm* c)
{
  (void)c;
}

unsigned comm_rank(void)
{
  return 0;
}

unsigned comm_size(void)
{
  return 1;
}

void comm_add_doubles(double* p, unsigned n)
{
  (void)p;
  (void)n;
}

unsigned long comm_add_ulong(unsigned long x)
{
  return x;
}

unsigned long comm_exscan_ulong(unsigned long x)
{
  (void)x;
  return 0;
}

unsigned long comm_max_ulong(unsigned long x)
{
  return x;
}

#endif
