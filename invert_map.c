#include "invert_map.h"


#include "arrays.h"
#include "ints.h"
#include "loop.h"

#include <thrust/sort.h>

struct Counter
{
  unsigned origin;
  unsigned value;
};

LOOP_KERNEL(count,
    unsigned const* in,
    unsigned* counts)
  loop_atomic_increment(&(counts[in[i]]));
}

LOOP_KERNEL(fill,
	unsigned* offset,
    unsigned* out,
    Counter* counter)
  out[i] = counter[i].origin;
  if(i!=0)
  {
    if( counter[i].value != counter[i-1].value)
    {
      offset[counter[i].value] = i;
    }
  }
}

/* the atomic increments give the fill
   kernel some non-determinism,
   which we can counteract by sorting the sub-arrays.
   given the assumption that these sub-arrays are small
   (about a dozen entries),
   we use a hand-coded selection sort.
   there is a reward for someone who comes up with
   an equally efficient implementation that is deterministic
   from the start */





LOOP_KERNEL( assign ,const unsigned * in, struct Counter * ref )
  ref[i].origin = i;
  ref[i].value  = in[i];
}




#include <thrust/device_vector.h>
#include <thrust/fill.h>

__host__ __device__ bool operator<(const Counter &A, const Counter &B)
{
  return (A.value < B.value);
}

void invert_map(
    unsigned nin,
    unsigned const* in,
    unsigned nout,
    unsigned** p_out,
    unsigned** p_offsets)
{
  struct Counter* counters = LOOP_MALLOC( struct Counter, nin);
  LOOP_EXEC( assign, nin , in, counters );

  thrust::sort(
      thrust::device_ptr<struct Counter> (counters),
      thrust::device_ptr<struct Counter> (counters+nin));

  unsigned* aoffsets =uints_filled(nout+1, 0);

  unsigned* out = LOOP_MALLOC(unsigned, nin);
  unsigned i = nin;
  LOOP_EXEC(fill, nin,aoffsets, out, counters);
  CUDACALL(cudaMemcpy(aoffsets + nout, &(i), sizeof(unsigned), cudaMemcpyHostToDevice));

  loop_free(counters);
  *p_out = out;
  *p_offsets = aoffsets;
}



#if 0



LOOP_KERNEL(sort,
    unsigned* offsets,
    unsigned* out)
  unsigned first = offsets[i];
  unsigned end = offsets[i + 1];
  for (unsigned j = first; j < end; ++j) {
    unsigned min_k = j;
    for (unsigned k = j + 1; k < end; ++k)
      if (out[k] < out[min_k])
        min_k = k;
    unsigned tmp = out[j];
    out[j] = out[min_k];
    out[min_k] = tmp;
  }
}

LOOP_KERNEL( copy , unsigned const* in , unsigned * out)
  out[i] = in[i];
}

void Count_Sort_Dance(unsigned const*in , struct Counter*  out, unsigned nin)
{
  struct Counter** ref = LOOP_MALLOC( struct Counter* , nin);
  //LOOP_EXEC( assign, nin , out );
  unsigned * n_sorted = LOOP_MALLOC( unsigned , nin);
  //unsigned *a = (unsigned*)malloc( sizeof(int) * nin);
  LOOP_EXEC( copy , nin , in , n_sorted);
  LOOP_EXEC( Pointer_assign, nin, out, ref);


  //thrust::device_vector<unsigned> a_sorted( n_sorted, n_sorted+nin);
  //thrust::device_vector<Counter*>
  /*
  thrust::fill(thrust::device_ptr<unsigned> (n_sorted),
		  thrust::device_ptr<unsigned> (n_sorted+nin),
		  0);
  printf(" DID I DIE!?!?!?\n");
  */

  cudaError_t ret = cudaGetLastError();
  assert(ret == cudaSuccess);

  thrust::stable_sort_by_key(
    thrust::device_ptr<unsigned> (n_sorted) ,
    thrust::device_ptr<unsigned> (n_sorted+nin),
    thrust::device_ptr<struct Counter*> (ref) );

  LOOP_EXEC( assign, nin , ref );
  struct Counter * refa = (struct Counter*)malloc(sizeof(struct Counter)*nin);

  int i;


  LOOP_EXEC( count_work , nin, ref , n_sorted);
  cudaMemcpy(refa, out , sizeof(struct Counter) *nin, cudaMemcpyDeviceToHost);
  printf("SORTED?\n");
  for(i =0 ; i< nin ; i++) printf("%i\t", refa[i].origin);
  printf("\n");


  loop_free(ref);

  loop_free(n_sorted);
}

void invert_map(
    unsigned nin,
    unsigned const* in,
    unsigned nout,
    unsigned** p_out,
    unsigned** p_offsets)
{
  unsigned* counts = uints_filled(nout, 0);
  LOOP_EXEC(count, nin, in, counts);
  struct Counter* counters = LOOP_MALLOC( struct Counter , nin);
  Count_Sort_Dance( in, counters , nin);
  unsigned* offsets = uints_exscan(counts, nout);
  unsigned* out = LOOP_MALLOC(unsigned, nin);
  loop_free(counts);
  counts = uints_filled(nout, 0);

  LOOP_EXEC(fill, nin, in, offsets, counts, out, counters);
  loop_free(counts);
  int i = 0;
  loop_free(counters);

  *p_out = out;
  *p_offsets = offsets;
}
#endif
