#include "ints.h"

#include "loop.h"

#ifdef __CUDACC__
#include <thrust/reduce.h>
#include <thrust/device_ptr.h>
#include <thrust/functional.h>
#include <thrust/transform.h>
#include <thrust/reduce.h>
#include <thrust/sort.h>
#else
#include <stdlib.h>
#endif

#ifdef __CUDACC__

unsigned uints_max(unsigned const* a, unsigned n)
{
  unsigned max = 0;
  thrust::device_ptr<unsigned const> p(a);
  max = thrust::reduce(p, p + n, INT_MIN, thrust::maximum<unsigned>());
  return max;
}

unsigned* uints_exscan(unsigned const* a, unsigned n)
{
  unsigned * o = LOOP_MALLOC(unsigned , n + 1);
  thrust::device_ptr<unsigned const> inp(a);
  thrust::device_ptr<unsigned> outp(o);
  thrust::exclusive_scan(inp, inp + n, outp);
  /* fixup the last element quirk */
  unsigned sum = thrust::reduce(inp, inp + n);
  CUDACALL(cudaMemcpy(o + n, &sum, sizeof(unsigned), cudaMemcpyHostToDevice));
  return o;
}

unsigned* uints_negate(unsigned const* a, unsigned n)
{
  unsigned* o = LOOP_MALLOC(unsigned, n);
  thrust::device_ptr<unsigned> p2(o);
  thrust::device_ptr<unsigned const> p1(a);
  thrust::transform(p1, p1 + n, p2, thrust::negate<unsigned>());
  return o;
}

unsigned uints_sum(unsigned const* a, unsigned n)
{
  unsigned sum = 0;
  thrust::device_ptr< unsigned int const> p (a);
  sum = thrust::reduce(p, p + n, (unsigned)0, thrust::plus<unsigned>());
  return sum;
}

unsigned long ulongs_max(unsigned long const* a, unsigned n)
{
  unsigned long max = 0;
  thrust::device_ptr< unsigned long const> p(a);
  max = thrust::reduce(p, p + n, LONG_MIN, thrust::maximum<unsigned long>());
  return max;
}

#else

unsigned uints_max(unsigned const* a, unsigned n)
{
  unsigned max = 0;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] > max)
      max = a[i];
  return max;
}

unsigned* uints_exscan(unsigned const* a, unsigned n)
{
  unsigned* o = LOOP_MALLOC(unsigned, (n + 1));
  unsigned sum = 0;
  o[0] = 0;
  for (unsigned i = 0; i < n; ++i) {
    sum += a[i];
    o[i + 1] = sum;
  }
  return o;
}

unsigned uints_sum(unsigned const* a, unsigned n)
{
  unsigned sum = 0;
  for (unsigned i = 0; i < n; ++i)
    sum += a[i];
  return sum;
}

unsigned long ulongs_max(unsigned long const* a, unsigned n)
{
  unsigned long max = 0;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] > max)
      max = a[i];
  return max;
}

#endif

unsigned* uints_linear(unsigned n)
{
  unsigned* ones = uints_filled(n, 1);
  unsigned* linear = uints_exscan(ones, n);
  loop_free(ones);
  return linear;
}

LOOP_KERNEL(unscan_kern, unsigned const* a, unsigned* o)
  o[i] = a[i + 1] - a[i];
}

unsigned* uints_unscan(unsigned const* a, unsigned n)
{
  unsigned* o = LOOP_MALLOC(unsigned, n);
  LOOP_EXEC(unscan_kern, n, a, o);
  return o;
}

LOOP_KERNEL(negate_kern, unsigned const* a, unsigned* o)
  o[i] = !a[i];
}

unsigned* uints_negate(unsigned const* a, unsigned n)
{
  unsigned* o = LOOP_MALLOC(unsigned, n);
  LOOP_EXEC(negate_kern, n, a, o);
  return o;
}

unsigned* uints_negate_offsets(unsigned const* a, unsigned n)
{
  unsigned* unscanned = uints_unscan(a, n);
  unsigned* negated = uints_negate(unscanned, n);
  loop_free(unscanned);
  unsigned* out = uints_exscan(negated, n);
  loop_free(negated);
  return out;
}

LOOP_KERNEL(fill_kern, unsigned* a, unsigned v)
  a[i] = v;
}

unsigned* uints_filled(unsigned n, unsigned v)
{
  unsigned* a = LOOP_MALLOC(unsigned, n);
  LOOP_EXEC(fill_kern, n, a, v);
  return a;
}

LOOP_KERNEL(expand_kern, unsigned const* a, unsigned width,
    unsigned const* offsets, unsigned* o)
  unsigned first = offsets[i];
  unsigned end = offsets[i + 1];
  for (unsigned j = first; j < end; ++j)
    for (unsigned k = 0; k < width; ++k)
      o[j * width + k] = a[i * width + k];
}

unsigned* uints_expand(unsigned n, unsigned const* a,
    unsigned width, unsigned const* offsets)
{
  unsigned nout = offsets[n];
  unsigned* o = LOOP_MALLOC(unsigned, nout);
  LOOP_EXEC(expand_kern, n, a, width, offsets, o);
  return o;
}

LOOP_KERNEL(shuffle_kern, unsigned const* a, unsigned width,
    unsigned const* out_of_in, unsigned* o)
  unsigned j = out_of_in[i];
  for (unsigned k = 0; k < width; ++k)
    o[j * width + k] = a[i * width + k];
}

unsigned* uints_shuffle(unsigned n, unsigned const* a,
    unsigned width, unsigned const* out_of_in)
{
  unsigned* o = LOOP_MALLOC(unsigned, n);
  LOOP_EXEC(shuffle_kern, n, a, width, out_of_in, o);
  return o;
}

LOOP_KERNEL(unshuffle_kern, unsigned const* a, unsigned width,
    unsigned const* out_of_in, unsigned* o)
  unsigned j = out_of_in[i];
  for (unsigned k = 0; k < width; ++k)
    o[i * width + k] = a[j * width + k];
}

unsigned* uints_unshuffle(unsigned n, unsigned const* a,
    unsigned width, unsigned const* out_of_in)
{
  unsigned* o = LOOP_MALLOC(unsigned, n);
  LOOP_EXEC(unshuffle_kern, n, a, width, out_of_in, o);
  return o;
}
