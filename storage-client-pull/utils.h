#ifndef __trevi__utils__
#define __trevi__utils__

#include <emmintrin.h>

#include "symbol.h"

static unsigned int
calculate_number_of_fragments (unsigned int blob_size, unsigned short symbol_size)
{
  unsigned int number_of_fragments;
  if (blob_size % SYMBOL_SIZE != 0) {
    number_of_fragments = (blob_size / symbol_size) + 1;
  } else {
    number_of_fragments = blob_size / symbol_size;
  }
  return number_of_fragments;
}

static unsigned short
calculate_size_of_last_fragment (unsigned int blob_size, unsigned short symbol_size)
{
  unsigned short size_of_last_fragment;
  if (blob_size % SYMBOL_SIZE != 0) {
    size_of_last_fragment = blob_size % symbol_size;
  } else {
    size_of_last_fragment = symbol_size;
  }
  return size_of_last_fragment;
}

static void
trevi_chunk_xor_128 (unsigned char *src, unsigned char *dst, unsigned int size)
{
  __m128i *cast_src = (__m128i *) src;
  __m128i *cast_dst = (__m128i *) dst;
  for (unsigned int i = 0; i < size / sizeof(__m128i ); i++) {
    __m128i xmm1 = _mm_load_si128 (cast_src + i);
    __m128i xmm2 = _mm_load_si128 (cast_dst + i);
    xmm2 = _mm_xor_si128 (xmm1, xmm2);     //  XOR  4 32-bit words
    _mm_store_si128 (cast_dst + i, xmm2);
  }
}

#endif /* defined(__trevi__utils__) */
