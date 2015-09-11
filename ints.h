#ifndef INTS_H
#define INTS_H

void ints_zero(unsigned* a, unsigned n);
unsigned* ints_copy(unsigned const* a, unsigned n);
unsigned ints_max(unsigned const* a, unsigned n);
unsigned* ints_exscan(unsigned const* a, unsigned n);
unsigned* ints_unscan(unsigned const* a, unsigned n);
unsigned* ints_negate(unsigned const* a, unsigned n);
unsigned* ints_negate_offsets(unsigned const* a, unsigned n);
void ints_fill(unsigned a[], unsigned n, unsigned v);
unsigned ints_sum(unsigned const* a, unsigned n);

static inline unsigned has(unsigned const a[], unsigned n, unsigned e)
{
  for (unsigned i = 0; i < n; ++i)
    if (a[i] == e)
      return 1;
  return 0;
}

static inline unsigned add_unique(unsigned* a, unsigned n, unsigned e)
{
  if (has(a, n, e))
    return n;
  a[n] = e;
  return n + 1;
}

#endif
