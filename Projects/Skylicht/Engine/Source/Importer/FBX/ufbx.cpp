#include "pch.h"
#include "ufbx.h"

#ifndef UFBX_UFBX_C_INLCUDED
#define UFBX_UFBX_C_INLCUDED

// -- Configuration

#define UFBXI_MAX_NON_ARRAY_VALUES 7
#define UFBXI_MAX_NODE_DEPTH 64
#define UFBXI_MAX_SKIP_SIZE 0x40000000
#define UFBXI_MAP_MAX_SCAN 32
#define UFBXI_KD_FAST_DEPTH 6

#ifndef UFBXI_MAX_NURBS_ORDER
#define UFBXI_MAX_NURBS_ORDER 128
#endif

// -- Headers

#define _FILE_OFFSET_BITS 64

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <locale.h>

// -- Platform

#if defined(_MSC_VER)
	#define ufbxi_noinline __declspec(noinline)
	#define ufbxi_forceinline __forceinline
	#define ufbxi_restrict __restrict
	#if defined(__cplusplus) && _MSC_VER >= 1900
		#define ufbxi_nodiscard [[nodiscard]]
	#else
		#define ufbxi_nodiscard _Check_return_
	#endif
	#define ufbxi_unused
#elif defined(__GNUC__) || defined(__clang__)
	#define ufbxi_noinline __attribute__((noinline))
	#define ufbxi_forceinline inline __attribute__((always_inline))
	#define ufbxi_restrict __restrict
	#define ufbxi_nodiscard __attribute__((warn_unused_result))
	#define ufbxi_unused __attribute__((unused))
#else
	#define ufbxi_noinline
	#define ufbxi_forceinline
	#define ufbxi_nodiscard
	#define ufbxi_restrict
	#define ufbxi_unused
#endif

#if defined(__GNUC__) && !defined(__clang__)
	#define ufbxi_ignore(cond) (void)!(cond)
#else
	#define ufbxi_ignore(cond) (void)(cond)
#endif

#if defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable: 4200) // nonstandard extension used: zero-sized array in struct/union
	#pragma warning(disable: 4201) // nonstandard extension used: nameless struct/union
	#pragma warning(disable: 4127) // conditional expression is constant
	#pragma warning(disable: 4706) // assignment within conditional expression
	#pragma warning(disable: 4789) // buffer 'type_and_name' of size 8 bytes will be overrun; 16 bytes will be written starting at offset 0
#endif

#if defined(__clang__)
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wmissing-field-initializers"
	#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

#if defined(__GNUC__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#if !defined(ufbx_static_assert)
	#if defined(__cplusplus) && __cplusplus >= 201103
		#define ufbx_static_assert(desc, cond) static_assert(cond, #desc ": " #cond)
	#else
		#define ufbx_static_assert(desc, cond) typedef char ufbxi_static_assert_##desc[(cond)?1:-1]
	#endif
#endif

#if defined(__has_feature)
	#if __has_feature(undefined_behavior_sanitizer) && !defined(UFBX_UBSAN)
		#define UFBX_UBSAN 1
	#endif
#endif

#if defined(__SANITIZE_UNDEFINED__)  && !defined(UFBX_UBSAN)
	#define UFBX_UBSAN 1
#endif

// Don't use unaligned loads with UB-sanitizer
#if defined(UFBX_UBSAN) && !defined(UFBX_NO_UNALIGNED_LOADS)
	#define UFBX_NO_UNALIGNED_LOADS
#endif

// Unaligned little-endian load functions
// On platforms that support unaligned access natively (x86, x64, ARM64) just use normal loads,
// with unaligned attributes, otherwise do manual byte-wise load.

#define ufbxi_read_u8(ptr) (*(const uint8_t*)(ptr))

// Detect support for `__attribute__((aligned(1)))`
#if defined(__clang__) && defined(__APPLE__)
	// Apple overrides Clang versioning, 5.0 here maps to 3.3
	#if __clang_major__ >= 5
		#define UFBXI_HAS_ATTRIBUTE_ALIGNED 1
	#endif
#elif defined(__clang__)
	#if (__clang_major__ >= 4) || (__clang_major__ == 3 && __clang_minor__ >= 3)
		#define UFBXI_HAS_ATTRIBUTE_ALIGNED 1
	#endif
#elif defined(__GNUC__)
	#if __GNUC__ >= 5
		#define UFBXI_HAS_ATTRIBUTE_ALIGNED 1
	#endif
#endif

#if defined(UFBXI_HAS_ATTRIBUTE_ALIGNED)
	#define UFBXI_HAS_UNALIGNED 1
	#define ufbxi_unaligned
	typedef uint16_t __attribute__((aligned(1))) ufbxi_unaligned_u16;
	typedef uint32_t __attribute__((aligned(1))) ufbxi_unaligned_u32;
	typedef uint64_t __attribute__((aligned(1))) ufbxi_unaligned_u64;
	typedef float __attribute__((aligned(1))) ufbxi_unaligned_f32;
	typedef double __attribute__((aligned(1))) ufbxi_unaligned_f64;
#elif defined(_MSC_VER)
	#define UFBXI_HAS_UNALIGNED 1
	#if defined(_M_IX86)
		// MSVC seems to assume all pointers are unaligned for x86
		#define ufbxi_unaligned
	#else
		#define ufbxi_unaligned __unaligned
	#endif
	typedef uint16_t ufbxi_unaligned_u16;
	typedef uint32_t ufbxi_unaligned_u32;
	typedef uint64_t ufbxi_unaligned_u64;
	typedef float ufbxi_unaligned_f32;
	typedef double ufbxi_unaligned_f64;
#endif

#if defined(UFBXI_HAS_UNALIGNED) && (defined(_M_IX86) || defined(__i386__) || defined(_M_X64) || defined(__x86_64__) || defined(_M_ARM64) || defined(__aarch64__) || defined(__wasm__) || defined(__EMSCRIPTEN__)) && !defined(UFBX_NO_UNALIGNED_LOADS) || defined(UFBX_USE_UNALIGNED_LOADS)
	#define ufbxi_read_u16(ptr) (*(const ufbxi_unaligned ufbxi_unaligned_u16*)(ptr))
	#define ufbxi_read_u32(ptr) (*(const ufbxi_unaligned ufbxi_unaligned_u32*)(ptr))
	#define ufbxi_read_u64(ptr) (*(const ufbxi_unaligned ufbxi_unaligned_u64*)(ptr))
	#define ufbxi_read_f32(ptr) (*(const ufbxi_unaligned ufbxi_unaligned_f32*)(ptr))
	#define ufbxi_read_f64(ptr) (*(const ufbxi_unaligned ufbxi_unaligned_f64*)(ptr))
#else
	static ufbxi_forceinline uint16_t ufbxi_read_u16(const void *ptr) {
		const char *p = (const char*)ptr;
		return (uint16_t)(
			(unsigned)(uint8_t)p[0] << 0u |
			(unsigned)(uint8_t)p[1] << 8u );
	}
	static ufbxi_forceinline uint32_t ufbxi_read_u32(const void *ptr) {
		const char *p = (const char*)ptr;
		return (uint32_t)(
			(unsigned)(uint8_t)p[0] <<  0u |
			(unsigned)(uint8_t)p[1] <<  8u |
			(unsigned)(uint8_t)p[2] << 16u |
			(unsigned)(uint8_t)p[3] << 24u );
	}
	static ufbxi_forceinline uint64_t ufbxi_read_u64(const void *ptr) {
		const char *p = (const char*)ptr;
		return (uint64_t)(
			(uint64_t)(uint8_t)p[0] <<  0u |
			(uint64_t)(uint8_t)p[1] <<  8u |
			(uint64_t)(uint8_t)p[2] << 16u |
			(uint64_t)(uint8_t)p[3] << 24u |
			(uint64_t)(uint8_t)p[4] << 32u |
			(uint64_t)(uint8_t)p[5] << 40u |
			(uint64_t)(uint8_t)p[6] << 48u |
			(uint64_t)(uint8_t)p[7] << 56u );
	}
	static ufbxi_forceinline float ufbxi_read_f32(const void *ptr) {
		uint32_t u = ufbxi_read_u32(ptr);
		float f;
		memcpy(&f, &u, 4);
		return f;
	}
	static ufbxi_forceinline double ufbxi_read_f64(const void *ptr) {
		uint64_t u = ufbxi_read_u64(ptr);
		double f;
		memcpy(&f, &u, 8);
		return f;
	}
#endif

#define ufbxi_read_i8(ptr) (int8_t)(ufbxi_read_u8(ptr))
#define ufbxi_read_i16(ptr) (int16_t)(ufbxi_read_u16(ptr))
#define ufbxi_read_i32(ptr) (int32_t)(ufbxi_read_u32(ptr))
#define ufbxi_read_i64(ptr) (int64_t)(ufbxi_read_u64(ptr))

ufbx_static_assert(sizeof_bool, sizeof(bool) == 1);
ufbx_static_assert(sizeof_i8, sizeof(int8_t) == 1);
ufbx_static_assert(sizeof_i16, sizeof(int16_t) == 2);
ufbx_static_assert(sizeof_i32, sizeof(int32_t) == 4);
ufbx_static_assert(sizeof_i64, sizeof(int64_t) == 8);
ufbx_static_assert(sizeof_u8, sizeof(uint8_t) == 1);
ufbx_static_assert(sizeof_u16, sizeof(uint16_t) == 2);
ufbx_static_assert(sizeof_u32, sizeof(uint32_t) == 4);
ufbx_static_assert(sizeof_u64, sizeof(uint64_t) == 8);
ufbx_static_assert(sizeof_f32, sizeof(float) == 4);
ufbx_static_assert(sizeof_f64, sizeof(double) == 8);

// -- Version

#define UFBX_SOURCE_VERSION ufbx_pack_version(0, 1, 1)
const uint32_t ufbx_source_version = UFBX_SOURCE_VERSION;

ufbx_static_assert(source_header_version, UFBX_SOURCE_VERSION/1000u == UFBX_HEADER_VERSION/1000u);

// -- Debug

#if defined(UFBX_DEBUG_BINARY_SEARCH) || defined(UFBX_REGRESSION)
	#define ufbxi_clamp_linear_threshold(v) (2)
#else
	#define ufbxi_clamp_linear_threshold(v) (v)
#endif

#if defined(UFBX_REGRESSION)
	#undef UFBXI_MAX_SKIP_SIZE
	#define UFBXI_MAX_SKIP_SIZE 128

	#undef UFBXI_MAP_MAX_SCAN
	#define UFBXI_MAP_MAX_SCAN 2

	#undef UFBXI_KD_FAST_DEPTH
	#define UFBXI_KD_FAST_DEPTH 2
#endif

// -- Utility

#define ufbxi_arraycount(arr) (sizeof(arr) / sizeof(*(arr)))
#define ufbxi_for(m_type, m_name, m_begin, m_num) for (m_type *m_name = m_begin, *m_name##_end = m_name + (m_num); m_name != m_name##_end; m_name++)
#define ufbxi_for_ptr(m_type, m_name, m_begin, m_num) for (m_type **m_name = m_begin, **m_name##_end = m_name + (m_num); m_name != m_name##_end; m_name++)

// WARNING: Evaluates `m_list` twice!
#define ufbxi_for_list(m_type, m_name, m_list) for (m_type *m_name = (m_list).data, *m_name##_end = m_name + (m_list).count; m_name != m_name##_end; m_name++)
#define ufbxi_for_ptr_list(m_type, m_name, m_list) for (m_type **m_name = (m_list).data, **m_name##_end = m_name + (m_list).count; m_name != m_name##_end; m_name++)

#define ufbxi_string_literal(str) { str, sizeof(str) - 1 }

static ufbxi_forceinline uint32_t ufbxi_min32(uint32_t a, uint32_t b) { return a < b ? a : b; }
static ufbxi_forceinline uint32_t ufbxi_max32(uint32_t a, uint32_t b) { return a < b ? b : a; }
static ufbxi_forceinline uint64_t ufbxi_min64(uint64_t a, uint64_t b) { return a < b ? a : b; }
static ufbxi_forceinline uint64_t ufbxi_max64(uint64_t a, uint64_t b) { return a < b ? b : a; }
static ufbxi_forceinline size_t ufbxi_min_sz(size_t a, size_t b) { return a < b ? a : b; }
static ufbxi_forceinline size_t ufbxi_max_sz(size_t a, size_t b) { return a < b ? b : a; }
static ufbxi_forceinline ufbx_real ufbxi_min_real(ufbx_real a, ufbx_real b) { return a < b ? a : b; }
static ufbxi_forceinline ufbx_real ufbxi_max_real(ufbx_real a, ufbx_real b) { return a < b ? b : a; }

// Stable sort array `m_type m_data[m_size]` using the predicate `m_cmp_lambda(a, b)`
// `m_linear_size` is a hint for how large blocks handle initially do with insertion sort
// `m_tmp` must be a memory buffer with at least the same size and alignment as `m_data`
#define ufbxi_macro_stable_sort(m_type, m_linear_size, m_data, m_tmp, m_size, m_cmp_lambda) do { \
	typedef m_type mi_type; \
	mi_type *mi_src = (mi_type*)(m_tmp); \
	mi_type *mi_data = m_data, *mi_dst = mi_data; \
	size_t mi_block_size = ufbxi_clamp_linear_threshold(m_linear_size), mi_size = m_size; \
	/* Insertion sort in `m_linear_size` blocks */ \
	for (size_t mi_base = 0; mi_base < mi_size; mi_base += mi_block_size) { \
		size_t mi_i_end = mi_base + mi_block_size; \
		if (mi_i_end > mi_size) mi_i_end = mi_size; \
		for (size_t mi_i = mi_base + 1; mi_i < mi_i_end; mi_i++) { \
			size_t mi_j = mi_i; \
			mi_src[0] = mi_dst[mi_i]; \
			for (; mi_j != mi_base; --mi_j) { \
				mi_type *a = &mi_src[0], *b = &mi_dst[mi_j - 1]; \
				if (!( m_cmp_lambda )) break; \
				mi_dst[mi_j] = mi_dst[mi_j - 1]; \
			} \
			mi_dst[mi_j] = mi_src[0]; \
		} \
	} \
	/* Merge sort ping-ponging between `m_data` and `m_tmp` */ \
	for (; mi_block_size < mi_size; mi_block_size *= 2) { \
		mi_type *mi_swap = mi_dst; mi_dst = mi_src; mi_src = mi_swap; \
		for (size_t mi_base = 0; mi_base < mi_size; mi_base += mi_block_size * 2) { \
			size_t mi_i = mi_base, mi_i_end = mi_base + mi_block_size; \
			size_t mi_j = mi_i_end, mi_j_end = mi_j + mi_block_size; \
			size_t mi_k = mi_base; \
			if (mi_i_end > mi_size) mi_i_end = mi_size; \
			if (mi_j_end > mi_size) mi_j_end = mi_size; \
			while ((mi_i < mi_i_end) & (mi_j < mi_j_end)) { \
				mi_type *a = &mi_src[mi_j], *b = &mi_src[mi_i]; \
				if ( m_cmp_lambda ) { \
					mi_dst[mi_k] = *a; mi_j++; \
				} else { \
					mi_dst[mi_k] = *b; mi_i++; \
				} \
				mi_k++; \
			} \
			while (mi_i < mi_i_end) mi_dst[mi_k++] = mi_src[mi_i++]; \
			while (mi_j < mi_j_end) mi_dst[mi_k++] = mi_src[mi_j++]; \
		} \
	} \
	/* Copy the result to `m_data` if we ended up in `m_tmp` */ \
	if (mi_dst != mi_data) memcpy((void*)mi_data, mi_dst, sizeof(mi_type) * mi_size); \
	} while (0)

#define ufbxi_macro_lower_bound_eq(m_type, m_linear_size, m_result_ptr, m_data, m_begin, m_size, m_cmp_lambda, m_eq_lambda) do { \
	typedef m_type mi_type; \
	const mi_type *mi_data = (m_data); \
	size_t mi_lo = m_begin, mi_hi = m_size, mi_linear_size = ufbxi_clamp_linear_threshold(m_linear_size); \
	ufbx_assert(mi_linear_size > 1); \
	/* Binary search until we get down to `m_linear_size` elements */ \
	while (mi_hi - mi_lo > mi_linear_size) { \
			size_t mi_mid = mi_lo + (mi_hi - mi_lo) / 2; \
			const mi_type *a = &mi_data[mi_mid]; \
			if ( m_cmp_lambda ) { mi_lo = mi_mid + 1; } else { mi_hi = mi_mid + 1; } \
	} \
	/* Linearly scan until we find the edge */ \
	for (; mi_lo < mi_hi; mi_lo++) { \
			const mi_type *a = &mi_data[mi_lo]; \
			if ( m_eq_lambda ) { *(m_result_ptr) = mi_lo; break; } \
	} \
	} while (0)

#define ufbxi_macro_upper_bound_eq(m_type, m_linear_size, m_result_ptr, m_data, m_begin, m_size, m_eq_lambda) do { \
	typedef m_type mi_type; \
	const mi_type *mi_data = (m_data); \
	size_t mi_lo = m_begin, mi_hi = m_size, mi_linear_size = ufbxi_clamp_linear_threshold(m_linear_size); \
	ufbx_assert(mi_linear_size > 1); \
	/* Linearly scan with galloping */ \
	for (size_t mi_step = 1; mi_step < 100 && mi_hi - mi_lo > mi_step; mi_step *= 2) { \
		const mi_type *a = &mi_data[mi_lo + mi_step]; \
		if (!( m_eq_lambda )) { mi_hi = mi_lo + mi_step; break; } \
		mi_lo += mi_step; \
	} \
	/* Binary search until we get down to `m_linear_size` elements */ \
	while (mi_hi - mi_lo > mi_linear_size) { \
		size_t mi_mid = mi_lo + (mi_hi - mi_lo) / 2; \
		const mi_type *a = &mi_data[mi_mid]; \
		if ( m_eq_lambda ) { mi_lo = mi_mid + 1; } else { mi_hi = mi_mid + 1; } \
	} \
	/* Linearly scan until we find the edge */ \
	for (; mi_lo < mi_hi; mi_lo++) { \
		const mi_type *a = &mi_data[mi_lo]; \
		if (!( m_eq_lambda )) break; \
	} \
	*(m_result_ptr) = mi_lo; \
	} while (0)

// -- DEFLATE implementation
// Pretty much based on Sean Barrett's `stb_image` deflate

#if !defined(ufbx_inflate)

// Lookup data: [0:13] extra mask [13:17] extra bits [17:32] base value
// Generated by `misc/deflate_lut.py`
static const uint32_t ufbxi_deflate_length_lut[] = {
	0x00060000, 0x00080000, 0x000a0000, 0x000c0000, 0x000e0000, 0x00100000, 0x00120000, 0x00140000, 
	0x00162001, 0x001a2001, 0x001e2001, 0x00222001, 0x00264003, 0x002e4003, 0x00364003, 0x003e4003, 
	0x00466007, 0x00566007, 0x00666007, 0x00766007, 0x0086800f, 0x00a6800f, 0x00c6800f, 0x00e6800f, 
	0x0106a01f, 0x0146a01f, 0x0186a01f, 0x01c6a01f, 0x02040000, 0x00000000, 0x00000000, 
};
static const uint32_t ufbxi_deflate_dist_lut[] = {
	0x00020000, 0x00040000, 0x00060000, 0x00080000, 0x000a2001, 0x000e2001, 0x00124003, 0x001a4003, 
	0x00226007, 0x00326007, 0x0042800f, 0x0062800f, 0x0082a01f, 0x00c2a01f, 0x0102c03f, 0x0182c03f, 
	0x0202e07f, 0x0302e07f, 0x040300ff, 0x060300ff, 0x080321ff, 0x0c0321ff, 0x100343ff, 0x180343ff, 
	0x200367ff, 0x300367ff, 0x40038fff, 0x60038fff, 0x8003bfff, 0xc003bfff, 
};

static const uint8_t ufbxi_deflate_code_length_permutation[] = {
	16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15,
};

#define UFBXI_HUFF_MAX_BITS 16
#define UFBXI_HUFF_MAX_VALUE 288
#define UFBXI_HUFF_FAST_BITS 9
#define UFBXI_HUFF_FAST_SIZE (1 << UFBXI_HUFF_FAST_BITS)
#define UFBXI_HUFF_FAST_MASK (UFBXI_HUFF_FAST_SIZE - 1)

typedef struct {

	// Number of bytes left to read from `read_fn()`
	size_t input_left;

	// User-supplied read callback
	ufbx_read_fn *read_fn;
	void *read_user;

	// Buffer to read to from `read_fn()`, may point to `local_buffer` if user
	// didn't supply a suitable buffer.
	char *buffer;
	size_t buffer_size;

	// Current chunk of data to process, either the initial buffer of input
	// or part of `buffer`.
	const char *chunk_begin;    // < Begin of the buffer
	const char *chunk_ptr;      // < Next bytes to read to `bits`
	const char *chunk_yield;    // < End of data before needing to call `ufbxi_bit_yield()`
	const char *chunk_end;      // < End of data before needing to call `ufbxi_bit_refill()`
	const char *chunk_real_end; // < Actual end of the data buffer

	// Amount of bytes read before the current chunk
	size_t num_read_before_chunk;
	uint64_t progress_bias;
	uint64_t progress_total;
	size_t progress_interval;

	uint64_t bits; // < Buffered bits
	size_t left;   // < Number of valid low bits in `bits`

	// Progress tracking, maybe `NULL` it not requested
	ufbx_progress_fn *progress_fn;
	void *progress_user;

	// When `progress_fn()` returns `false` set the `cancelled` flag and
	// set the buffered bits to `cancel_bits`.
	uint64_t cancel_bits;
	bool cancelled;

	char local_buffer[256];
} ufbxi_bit_stream;

typedef struct {
	uint32_t num_symbols;
	uint16_t sorted_to_sym[UFBXI_HUFF_MAX_VALUE]; // < Sorted symbol index to symbol
	uint16_t past_max_code[UFBXI_HUFF_MAX_BITS];  // < One past maximum code value per bit length
	int16_t code_to_sorted[UFBXI_HUFF_MAX_BITS];  // < Code to sorted symbol index per bit length
	uint16_t fast_sym[UFBXI_HUFF_FAST_SIZE];      // < Fast symbol lookup [0:12] symbol [12:16] bits

	uint32_t end_of_block_bits;
} ufbxi_huff_tree;

typedef struct {
	ufbxi_huff_tree lit_length;
	ufbxi_huff_tree dist;
} ufbxi_trees;

typedef struct {
	bool initialized;
	ufbxi_trees static_trees;
} ufbxi_inflate_retain_imp;

ufbx_static_assert(inflate_retain_size, sizeof(ufbxi_inflate_retain_imp) <= sizeof(ufbx_inflate_retain));

typedef struct {
	ufbxi_bit_stream stream;

	char *out_begin;
	char *out_ptr;
	char *out_end;
} ufbxi_deflate_context;

static ufbxi_forceinline uint32_t
ufbxi_bit_reverse(uint32_t mask, uint32_t num_bits)
{
	ufbx_assert(num_bits <= 16);
	uint32_t x = mask;
	x = (((x & 0xaaaa) >> 1) | ((x & 0x5555) << 1));
	x = (((x & 0xcccc) >> 2) | ((x & 0x3333) << 2));
	x = (((x & 0xf0f0) >> 4) | ((x & 0x0f0f) << 4));
	x = (((x & 0xff00) >> 8) | ((x & 0x00ff) << 8));
	return x >> (16 - num_bits);
}

static ufbxi_noinline const char *
ufbxi_bit_chunk_refill(ufbxi_bit_stream *s, const char *ptr)
{
	// Copy any left-over data to the beginning of `buffer`
	size_t left = s->chunk_real_end - ptr;
	ufbx_assert(left < 64);
	memmove(s->buffer, ptr, left);

	s->num_read_before_chunk += ptr - s->chunk_begin;

	// Read more user data if the user supplied a `read_fn()`, otherwise
	// we assume the initial data chunk is the whole input buffer.
	if (s->read_fn) {
		size_t to_read = ufbxi_min_sz(s->input_left, s->buffer_size - left);
		if (to_read > 0) {
			size_t num_read = s->read_fn(s->read_user, s->buffer + left, to_read);
			// TOOD: IO error, should unify with (currently broken) cancel logic
			if (num_read > to_read) num_read = 0;
			ufbx_assert(s->input_left >= num_read);
			s->input_left -= num_read;
			left += num_read;
		}
	}

	// Pad the rest with zeros
	if (left < 64) {
		memset(s->buffer + left, 0, 64 - left);
		left = 64;
	}

	s->chunk_begin = s->buffer;
	s->chunk_ptr = s->buffer;
	s->chunk_end = s->buffer + left - 8;
	s->chunk_real_end = s->buffer + left;
	return s->buffer;
}

static void ufbxi_bit_stream_init(ufbxi_bit_stream *s, const ufbx_inflate_input *input)
{
	size_t data_size = input->data_size;
	if (data_size > input->total_size) {
		data_size = input->total_size;
	}

	s->read_fn = input->read_fn;
	s->read_user = input->read_user;
	s->progress_fn = input->progress_fn;
	s->progress_user = input->progress_user;
	s->chunk_begin = (const char*)input->data;
	s->chunk_ptr = (const char*)input->data;
	s->chunk_end = (const char*)input->data + data_size - 8;
	s->chunk_real_end = (const char*)input->data + data_size;
	s->input_left = input->total_size - data_size;

	// Use the user buffer if it's large enough, otherwise `local_buffer`
	if (input->buffer_size > sizeof(s->local_buffer)) {
		s->buffer = (char*)input->buffer;
		s->buffer_size = input->buffer_size;
	} else {
		s->buffer = s->local_buffer;
		s->buffer_size = sizeof(s->local_buffer);
	}
	s->num_read_before_chunk = 0;
	s->progress_bias = input->progress_size_before;
	s->progress_total = input->total_size + input->progress_size_before + input->progress_size_after;
	if (!s->progress_fn || input->progress_interval_hint >= SIZE_MAX) {
		s->progress_interval = SIZE_MAX;
	} else if (input->progress_interval_hint > 0) {
		s->progress_interval = (size_t)input->progress_interval_hint;
	} else {
		s->progress_interval = 0x4000;
	}
	s->cancelled = false;

	// Clear the initial bit buffer
	s->bits = 0;
	s->left = 0;

	// If the initial data buffer is not large enough to be read directly
	// from refill the chunk once.
	if (data_size < 64) {
		ufbxi_bit_chunk_refill(s, s->chunk_begin);
	}

	if (s->progress_fn && (size_t)(s->chunk_end - s->chunk_ptr) > s->progress_interval + 8) {
		s->chunk_yield = s->chunk_ptr + s->progress_interval;
	} else {
		s->chunk_yield = s->chunk_end;
	}
}

static ufbxi_noinline const char *
ufbxi_bit_yield(ufbxi_bit_stream *s, const char *ptr)
{
	if (ptr > s->chunk_end) {
		ptr = ufbxi_bit_chunk_refill(s, ptr);
	}

	if (s->progress_fn && (size_t)(s->chunk_end - ptr) > s->progress_interval + 8) {
		s->chunk_yield = ptr + s->progress_interval;
	} else {
		s->chunk_yield = s->chunk_end;
	}

	if (s->progress_fn) {
		size_t num_read = s->num_read_before_chunk + (size_t)(ptr - s->chunk_begin);

		ufbx_progress progress = { s->progress_bias + num_read, s->progress_total };
		if (!s->progress_fn(s->progress_user, &progress)) {
			s->cancelled = true;
			ptr = s->local_buffer;
			memset(s->local_buffer, 0, sizeof(s->local_buffer));
		}
	}

	return ptr;
}

static ufbxi_forceinline void
ufbxi_bit_refill(uint64_t *p_bits, size_t *p_left, const char **p_data, ufbxi_bit_stream *s)
{
	if (*p_data > s->chunk_yield) {
		*p_data = ufbxi_bit_yield(s, *p_data);
		if (s->cancelled) {
			// Force an end-of-block symbol when cancelled so we don't need an
			// extra branch in the chunk decoding loop.
			*p_bits = s->cancel_bits;
		}
	}

	// See https://fgiesen.wordpress.com/2018/02/20/reading-bits-in-far-too-many-ways-part-2/
	// variant 4. This branchless refill guarantees [56,63] bits to be valid in `*p_bits`.
	*p_bits |= ufbxi_read_u64(*p_data) << *p_left;
	*p_data += (63 - *p_left) >> 3;
	*p_left |= 56;
}

static int
ufbxi_bit_copy_bytes(void *dst, ufbxi_bit_stream *s, size_t len)
{
	ufbx_assert(s->left % 8 == 0);
	char *ptr = (char*)dst;

	// Copy the buffered bits first
	while (len > 0 && s->left > 0) {
		*ptr++ = (char)(uint8_t)s->bits;
		len -= 1;
		s->bits >>= 8;
		s->left -= 8;
	}

	// We need to clear the top bits as there may be data
	// read ahead past `s->left` in some cases
	s->bits = 0;

	// Copy the current chunk
	size_t chunk_left = s->chunk_real_end - s->chunk_ptr;
	if (chunk_left >= len) {
		memcpy(ptr, s->chunk_ptr, len);
		s->chunk_ptr += len;
		return 1;
	} else {
		memcpy(ptr, s->chunk_ptr, chunk_left);
		s->chunk_ptr += chunk_left;
		ptr += chunk_left;
		len -= chunk_left;
	}

	// Read extra bytes from user
	if (len > s->input_left) return 0;
	size_t num_read = 0;
	if (s->read_fn) {
		num_read = s->read_fn(s->read_user, ptr, len);
		s->input_left -= num_read;
	}
	return num_read == len;
}

// 0: Success
// -1: Overfull
// -2: Underfull
static ufbxi_noinline ptrdiff_t
ufbxi_huff_build(ufbxi_huff_tree *tree, uint8_t *sym_bits, uint32_t sym_count)
{
	ufbx_assert(sym_count <= UFBXI_HUFF_MAX_VALUE);
	tree->num_symbols = sym_count;

	// Count the number of codes per bit length
	// `bit_counts[0]` contains the number of non-used symbols
	uint32_t bits_counts[UFBXI_HUFF_MAX_BITS];
	memset(bits_counts, 0, sizeof(bits_counts));
	for (uint32_t i = 0; i < sym_count; i++) {
		uint32_t bits = sym_bits[i];
		ufbx_assert(bits < UFBXI_HUFF_MAX_BITS);
		bits_counts[bits]++;
	}
	uint32_t nonzero_sym_count = sym_count - bits_counts[0];

	uint32_t total_syms[UFBXI_HUFF_MAX_BITS];
	uint32_t first_code[UFBXI_HUFF_MAX_BITS];

	tree->code_to_sorted[0] = INT16_MAX;
	tree->past_max_code[0] = 0;
	total_syms[0] = 0;

	// Resolve the maximum code per bit length and ensure that the tree is not
	// overfull or underfull.
	{
		int num_codes_left = 1;
		uint32_t code = 0;
		uint32_t prev_count = 0;
		for (uint32_t bits = 1; bits < UFBXI_HUFF_MAX_BITS; bits++) {
			uint32_t count = bits_counts[bits];
			code = (code + prev_count) << 1;
			first_code[bits] = code;
			tree->past_max_code[bits] = (uint16_t)(code + count);

			uint32_t prev_syms = total_syms[bits - 1];
			total_syms[bits] = prev_syms + count;

			// Each bit level doubles the amount of codes and potentially removes some
			num_codes_left = (num_codes_left << 1) - count;
			if (num_codes_left < 0) {
				return -1;
			}

			if (count > 0) {
				tree->code_to_sorted[bits] = (int16_t)((int)prev_syms - (int)code);
			} else {
				tree->code_to_sorted[bits] = INT16_MAX;
			}
			prev_count = count;
		}

		// All codes should be used if there's more than one symbol
		if (nonzero_sym_count > 1 && num_codes_left != 0) {
			return -2;
		}
	}

	tree->end_of_block_bits = 0;

	// Generate per-length sorted-to-symbol and fast lookup tables
	uint32_t bits_index[UFBXI_HUFF_MAX_BITS] = { 0 };
	memset(tree->sorted_to_sym, 0xff, sizeof(tree->sorted_to_sym));
	memset(tree->fast_sym, 0, sizeof(tree->fast_sym));
	for (uint32_t i = 0; i < sym_count; i++) {
		uint32_t bits = sym_bits[i];
		if (bits == 0) continue;

		uint32_t index = bits_index[bits]++;
		uint32_t sorted = total_syms[bits - 1] + index;
		tree->sorted_to_sym[sorted] = (uint16_t)i;

		// Reverse the code and fill all fast lookups with the reversed prefix
		uint32_t code = first_code[bits] + index;
		uint32_t rev_code = ufbxi_bit_reverse(code, bits);
		if (bits <= UFBXI_HUFF_FAST_BITS) {
			uint16_t fast_sym = (uint16_t)(i | bits << 12);
			uint32_t hi_max = 1 << (UFBXI_HUFF_FAST_BITS - bits);
			for (uint32_t hi = 0; hi < hi_max; hi++) {
				ufbx_assert(tree->fast_sym[rev_code | hi << bits] == 0);
				tree->fast_sym[rev_code | hi << bits] = fast_sym;
			}
		}

		// Store the end-of-block code so we can interrupt decoding
		if (i == 256) {
			tree->end_of_block_bits = rev_code;
		}
	}

	return 0;
}

static ufbxi_forceinline uint32_t
ufbxi_huff_decode_bits(const ufbxi_huff_tree *tree, uint64_t *p_bits, size_t *p_left)
{
	// If the code length is less than or equal UFBXI_HUFF_FAST_BITS we can
	// resolve the symbol and bit length directly from a lookup table.
	uint32_t fast_sym_bits = tree->fast_sym[*p_bits & UFBXI_HUFF_FAST_MASK];
	if (fast_sym_bits != 0) {
		uint32_t bits = fast_sym_bits >> 12;
		*p_bits >>= bits;
		*p_left -= bits;
		return fast_sym_bits & 0x3ff;
	}

	// The code length must be longer than UFBXI_HUFF_FAST_BITS, reverse the prefix
	// and build the code one bit at a time until we are in range for the bit length.
	uint32_t code = ufbxi_bit_reverse((uint32_t)*p_bits, UFBXI_HUFF_FAST_BITS + 1);
	*p_bits >>= UFBXI_HUFF_FAST_BITS + 1;
	*p_left -= UFBXI_HUFF_FAST_BITS + 1;
	for (uint32_t bits = UFBXI_HUFF_FAST_BITS + 1; bits < UFBXI_HUFF_MAX_BITS; bits++) {
		if (code < tree->past_max_code[bits]) {
			uint32_t sorted = code + tree->code_to_sorted[bits];
			if (sorted >= tree->num_symbols) return ~0u;
			return tree->sorted_to_sym[sorted];
		}
		code = code << 1 | (uint32_t)(*p_bits & 1);
		*p_bits >>= 1;
		*p_left -= 1;
	}

	// We shouldn't get here unless the tree is underfull _or_ has only
	// one symbol where the code `1` is invalid.
	return ~0u;
}

static void ufbxi_init_static_huff(ufbxi_trees *trees)
{
	ptrdiff_t err = 0;

	// 0-143: 8 bits, 144-255: 9 bits, 256-279: 7 bits, 280-287: 8 bits
	uint8_t lit_length_bits[288];
	memset(lit_length_bits +   0, 8, 144 -   0);
	memset(lit_length_bits + 144, 9, 256 - 144);
	memset(lit_length_bits + 256, 7, 280 - 256);
	memset(lit_length_bits + 280, 8, 288 - 280);
	err |= ufbxi_huff_build(&trees->lit_length, lit_length_bits, sizeof(lit_length_bits));

	// "Distance codes 0-31 are represented by (fixed-length) 5-bit codes"
	uint8_t dist_bits[32];
	memset(dist_bits + 0, 5, 32 - 0);
	err |= ufbxi_huff_build(&trees->dist, dist_bits, sizeof(dist_bits));

	// Building the static trees cannot fail as we use pre-defined code lengths.
	ufbx_assert(err == 0);
}

// 0: Success
// -1: Huffman Overfull
// -2: Huffman Underfull
// -3: Code 16 repeat overflow
// -4: Code 17 repeat overflow
// -5: Code 18 repeat overflow
// -6: Bad length code
// -7: Cancelled
static ufbxi_noinline ptrdiff_t
ufbxi_init_dynamic_huff_tree(ufbxi_deflate_context *dc, const ufbxi_huff_tree *huff_code_length,
	ufbxi_huff_tree *tree, uint32_t num_symbols)
{
	uint8_t code_lengths[UFBXI_HUFF_MAX_VALUE];
	ufbx_assert(num_symbols <= UFBXI_HUFF_MAX_VALUE);

	uint64_t bits = dc->stream.bits;
	size_t left = dc->stream.left;
	const char *data = dc->stream.chunk_ptr;

	uint32_t symbol_index = 0;
	uint8_t prev = 0;
	while (symbol_index < num_symbols) {
		ufbxi_bit_refill(&bits, &left, &data, &dc->stream);
		if (dc->stream.cancelled) return -7;

		uint32_t inst = ufbxi_huff_decode_bits(huff_code_length, &bits, &left);
		if (inst <= 15) {
			// "0 - 15: Represent code lengths of 0 - 15"
			prev = (uint8_t)inst;
			code_lengths[symbol_index++] = (uint8_t)inst;
		} else if (inst == 16) {
			// "16: Copy the previous code length 3 - 6 times. The next 2 bits indicate repeat length."
			uint32_t num = 3 + ((uint32_t)bits & 0x3);
			bits >>= 2;
			left -= 2;
			if (symbol_index + num > num_symbols) return -3;
			memset(code_lengths + symbol_index, prev, num);
			symbol_index += num;
		} else if (inst == 17) {
			// "17: Repeat a code length of 0 for 3 - 10 times. (3 bits of length)"
			uint32_t num = 3 + ((uint32_t)bits & 0x7);
			bits >>= 3;
			left -= 3;
			if (symbol_index + num > num_symbols) return -4;
			memset(code_lengths + symbol_index, 0, num);
			symbol_index += num;
			prev = 0;
		} else if (inst == 18) {
			// "18: Repeat a code length of 0 for 11 - 138 times (7 bits of length)"
			uint32_t num = 11 + ((uint32_t)bits & 0x7f);
			bits >>= 7;
			left -= 7;
			if (symbol_index + num > num_symbols) return -5;
			memset(code_lengths + symbol_index, 0, num);
			symbol_index += num;
			prev = 0;
		} else {
			return -6;
		}
	}

	ptrdiff_t err = ufbxi_huff_build(tree, code_lengths, num_symbols);
	if (err != 0) return err;

	dc->stream.bits = bits;
	dc->stream.left = left;
	dc->stream.chunk_ptr = data;

	return 0;
}

static ptrdiff_t
ufbxi_init_dynamic_huff(ufbxi_deflate_context *dc, ufbxi_trees *trees)
{
	uint64_t bits = dc->stream.bits;
	size_t left = dc->stream.left;
	const char *data = dc->stream.chunk_ptr;
	ufbxi_bit_refill(&bits, &left, &data, &dc->stream);
	if (dc->stream.cancelled) return -28;

	// The header contains the number of Huffman codes in each of the three trees.
	uint32_t num_lit_lengths = 257 + (bits & 0x1f);
	uint32_t num_dists = 1 + (bits >> 5 & 0x1f);
	uint32_t num_code_lengths = 4 + (bits >> 10 & 0xf);
	bits >>= 14;
	left -= 14;

	// Code lengths for the "code length" Huffman tree are represented literally
	// 3 bits in order of: 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 up to
	// `num_code_lengths`, rest of the code lengths are 0 (unused)
	uint8_t code_lengths[19];
	memset(code_lengths, 0, sizeof(code_lengths));
	for (size_t len_i = 0; len_i < num_code_lengths; len_i++) {
		if (len_i == 14) {
			ufbxi_bit_refill(&bits, &left, &data, &dc->stream);
			if (dc->stream.cancelled) return -28;
		}
		code_lengths[ufbxi_deflate_code_length_permutation[len_i]] = (uint32_t)bits & 0x7;
		bits >>= 3;
		left -= 3;
	}

	dc->stream.bits = bits;
	dc->stream.left = left;
	dc->stream.chunk_ptr = data;

	ufbxi_huff_tree huff_code_length;
	ptrdiff_t err;

	// Build the temporary "code length" Huffman tree used to encode the actual
	// trees used to compress the data. Use that to build the literal/length and
	// distance trees.
	err = ufbxi_huff_build(&huff_code_length, code_lengths, ufbxi_arraycount(code_lengths));
	if (err) return -14 + 1 + err;
	err = ufbxi_init_dynamic_huff_tree(dc, &huff_code_length, &trees->lit_length, num_lit_lengths);
	if (err) return err == -7 ? -28 : -16 + 1 + err;
	err = ufbxi_init_dynamic_huff_tree(dc, &huff_code_length, &trees->dist, num_dists);
	if (err) return err == -7 ? -28 : -22 + 1 + err;

	return 0;
}

static uint32_t ufbxi_adler32(const void *data, size_t size)
{
	size_t a = 1, b = 0;
	const char *p = (const char*)data;

	// Adler-32 consists of two running sums modulo 65521. As an optimization
	// we can accumulate N sums before applying the modulo, where N depends on
	// the size of the type holding the sum.
	const size_t num_before_wrap = sizeof(size_t) == 8 ? 380368439u : 5552u;

	size_t size_left = size;
	while (size_left > 0) {
		size_t num = size_left <= num_before_wrap ? size_left : num_before_wrap;
		size_left -= num;
		const char *end = p + num;

		while (end - p >= 8) {
			a += (size_t)(uint8_t)p[0]; b += a;
			a += (size_t)(uint8_t)p[1]; b += a;
			a += (size_t)(uint8_t)p[2]; b += a;
			a += (size_t)(uint8_t)p[3]; b += a;
			a += (size_t)(uint8_t)p[4]; b += a;
			a += (size_t)(uint8_t)p[5]; b += a;
			a += (size_t)(uint8_t)p[6]; b += a;
			a += (size_t)(uint8_t)p[7]; b += a;
			p += 8;
		}

		while (p != end) {
			a += (size_t)(uint8_t)p[0]; b += a;
			p++;
		}

		a %= 65521u;
		b %= 65521u;
	}

	return (uint32_t)((b << 16) | (a & 0xffff));
}

static int
ufbxi_inflate_block(ufbxi_deflate_context *dc, ufbxi_trees *trees)
{
	char *out_ptr = dc->out_ptr;
	char *const out_begin = dc->out_begin;
	char *const out_end = dc->out_end;

	uint64_t bits = dc->stream.bits;
	size_t left = dc->stream.left;
	const char *data = dc->stream.chunk_ptr;

	// Make the stream return the lit/len end of block Huffman code on cancellation
	dc->stream.cancel_bits = trees->lit_length.end_of_block_bits;

	for (;;) {
		// NOTE: Cancellation handled implicitly by forcing an end-of-chunk symbol
		ufbxi_bit_refill(&bits, &left, &data, &dc->stream);

		// Decode literal/length value from input stream
		uint32_t lit_length = ufbxi_huff_decode_bits(&trees->lit_length, &bits, &left);

		// If value < 256: copy value (literal byte) to output stream
		if (lit_length < 256) {
			if (out_ptr == out_end) {
				return -10;
			}
			*out_ptr++ = (char)lit_length;
		} else if (lit_length - 257 <= 285 - 257) {
			// If value = 257..285: Decode extra length and distance and copy `length` bytes
			// from `distance` bytes before in the buffer.
			uint32_t length, distance;

			// Length: Look up base length and add optional additional bits
			{
				uint32_t lut = ufbxi_deflate_length_lut[lit_length - 257];
				uint32_t base = lut >> 17;
				uint32_t offset = ((uint32_t)bits & lut & 0x1fff);
				uint32_t offset_bits = (lut >> 13) & 0xf;
				bits >>= offset_bits;
				left -= offset_bits;
				length = base + offset;
			}

			// Distance: Decode as a Huffman code and add optional additional bits
			{
				uint32_t dist = ufbxi_huff_decode_bits(&trees->dist, &bits, &left);
				if (dist >= 30) {
					return -11;
				}
				uint32_t lut = ufbxi_deflate_dist_lut[dist];
				uint32_t base = lut >> 17;
				uint32_t offset = ((uint32_t)bits & lut & 0x1fff);
				uint32_t offset_bits = (lut >> 13) & 0xf;
				bits >>= offset_bits;
				left -= offset_bits;
				distance = base + offset;
			}

			if ((ptrdiff_t)distance > out_ptr - out_begin || (ptrdiff_t)length > out_end - out_ptr) {
				return -12;
			}

			ufbx_assert(length > 0);
			const char *src = out_ptr - distance;
			char *dst = out_ptr;
			out_ptr += length;
			{
				// TODO: Do something better than per-byte copy
				char *end = dst + length;

				while (end - dst >= 4) {
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = src[3];
					dst += 4;
					src += 4;
				}

				while (dst != end) {
					*dst++ = *src++;
				}
			}
		} else if (lit_length == 256) {
			break;
		} else {
			return -13;
		}
	}

	dc->out_ptr = out_ptr;
	dc->stream.bits = bits;
	dc->stream.left = left;
	dc->stream.chunk_ptr = data;

	return 0;
}

// TODO: Error codes should have a quick test if the destination buffer overflowed
// Returns actual number of decompressed bytes or negative error:
// -1: Bad compression method (ZLIB header)
// -2: Requires dictionary (ZLIB header)
// -3: Bad FCHECK (ZLIB header)
// -4: Bad NLEN (Uncompressed LEN != ~NLEN)
// -5: Uncompressed source overflow
// -6: Uncompressed destination overflow
// -7: Bad block type
// -8: Truncated checksum (deprecated, reported as -9)
// -9: Checksum mismatch
// -10: Literal destination overflow
// -11: Bad distance code or distance of (30..31)
// -12: Match out of bounds
// -13: Bad lit/length code
// -14: Codelen Huffman Overfull
// -15: Codelen Huffman Underfull
// -16 - -21: Litlen Huffman: Overfull / Underfull / Repeat 16/17/18 overflow / Bad length code
// -22 - -27: Distance Huffman: Overfull / Underfull / Repeat 16/17/18 overflow / Bad length code
// -28: Cancelled
ptrdiff_t ufbx_inflate(void *dst, size_t dst_size, const ufbx_inflate_input *input, ufbx_inflate_retain *retain)
{
	ufbxi_inflate_retain_imp *ret_imp = (ufbxi_inflate_retain_imp*)retain;

	ptrdiff_t err;
	ufbxi_deflate_context dc;
	ufbxi_bit_stream_init(&dc.stream, input);
	dc.out_begin = (char*)dst;
	dc.out_ptr = (char*)dst;
	dc.out_end = (char*)dst + dst_size;

	uint64_t bits = dc.stream.bits;
	size_t left = dc.stream.left;
	const char *data = dc.stream.chunk_ptr;

	ufbxi_bit_refill(&bits, &left, &data, &dc.stream);
	if (dc.stream.cancelled) return -28;

	// Zlib header
	{
		size_t cmf = (size_t)(bits & 0xff);
		size_t flg = (size_t)(bits >> 8) & 0xff;
		bits >>= 16;
		left -= 16;

		if ((cmf & 0xf) != 0x8) return -1;
		if ((flg & 0x20) != 0) return -2;
		if ((cmf << 8 | flg) % 31u != 0) return -3;
	}

	for (;;) { 
		ufbxi_bit_refill(&bits, &left, &data, &dc.stream);
		if (dc.stream.cancelled) return -28;

		// Block header: [0:1] BFINAL [1:3] BTYPE
		size_t header = (size_t)bits & 0x7;
		bits >>= 3;
		left -= 3;

		size_t type = header >> 1;
		if (type == 0) {

			// Round up to the next byte
			size_t align_bits = left & 0x7;
			bits >>= align_bits;
			left -= align_bits;

			size_t len = (size_t)(bits & 0xffff);
			size_t nlen = (size_t)((bits >> 16) & 0xffff);
			if ((len ^ nlen) != 0xffff) return -4;
			if (dc.out_end - dc.out_ptr < (ptrdiff_t)len) return -6;
			bits >>= 32;
			left -= 32;

			dc.stream.bits = bits;
			dc.stream.left = left;
			dc.stream.chunk_ptr = data;

			// Copy `len` bytes of literal data
			if (!ufbxi_bit_copy_bytes(dc.out_ptr, &dc.stream, len)) return -5;

			dc.out_ptr += len;

		} else if (type <= 2) {

			dc.stream.bits = bits;
			dc.stream.left = left;
			dc.stream.chunk_ptr = data;

			ufbxi_trees tree_data;
			ufbxi_trees *trees;
			if (type == 1) {
				// Static Huffman: Initialize the trees once and cache them in `retain`.
				if (!ret_imp->initialized) {
					ufbxi_init_static_huff(&ret_imp->static_trees);
					ret_imp->initialized = true;
				}
				trees = &ret_imp->static_trees;
			} else { 
				// Dynamic Huffman
				err = ufbxi_init_dynamic_huff(&dc, &tree_data);
				if (err) return err;
				trees = &tree_data;
			}

			err = ufbxi_inflate_block(&dc, trees);
			if (err) return err;

			// `ufbxi_inflate_block()` returns normally on cancel so check it here
			if (dc.stream.cancelled) return -28;

		} else {
			// 0b11 - reserved (error)
			return -7;
		}

		bits = dc.stream.bits;
		left = dc.stream.left;
		data = dc.stream.chunk_ptr;

		// BFINAL: End of stream
		if (header & 1) break;
	}

	// Check Adler-32
	{
		// Round up to the next byte
		size_t align_bits = left & 0x7;
		bits >>= align_bits;
		left -= align_bits;
		ufbxi_bit_refill(&bits, &left, &data, &dc.stream);
		if (dc.stream.cancelled) return -28;

		uint32_t ref = (uint32_t)bits;
		ref = (ref>>24) | ((ref>>8)&0xff00) | ((ref<<8)&0xff0000) | (ref<<24);

		uint32_t checksum = ufbxi_adler32(dc.out_begin, dc.out_ptr - dc.out_begin);
		if (ref != checksum) {
			return -9;
		}
	}

	return dc.out_ptr - dc.out_begin;
}

#endif // !defined(ufbx_inflate)

// -- Errors

// Prefix the error condition with $Description\0 for a human readable description
#define ufbxi_error_msg(cond, msg) "$" msg "\0" cond

static ufbxi_noinline int ufbxi_fail_imp_err(ufbx_error *err, const char *cond, const char *func, uint32_t line)
{
	if (cond[0] == '$') {
		if (!err->description) {
			err->description = cond + 1;
		}
		cond = cond + strlen(cond) + 1;
	}

	// NOTE: This is the base function all fails boil down to, place a breakpoint here to
	// break at the first error
	if (err->stack_size < UFBX_ERROR_STACK_MAX_DEPTH) {
		ufbx_error_frame *frame = &err->stack[err->stack_size++];
		frame->description = cond;
		frame->function = func;
		frame->source_line = line;
	}
	return 0;
}

#define ufbxi_cond_str(cond) #cond

#define ufbxi_check_err(err, cond) do { if (!(cond)) { ufbxi_fail_imp_err((err), ufbxi_cond_str(cond), __FUNCTION__, __LINE__); return 0; } } while (0)
#define ufbxi_check_return_err(err, cond, ret) do { if (!(cond)) { ufbxi_fail_imp_err((err), ufbxi_cond_str(cond), __FUNCTION__, __LINE__); return ret; } } while (0)
#define ufbxi_fail_err(err, desc) return ufbxi_fail_imp_err(err, desc, __FUNCTION__, __LINE__)

#define ufbxi_check_err_msg(err, cond, msg) do { if (!(cond)) { ufbxi_fail_imp_err((err), ufbxi_error_msg(ufbxi_cond_str(cond), msg), __FUNCTION__, __LINE__); return 0; } } while (0)
#define ufbxi_check_return_err_msg(err, cond, ret, msg) do { if (!(cond)) { ufbxi_fail_imp_err((err), ufbxi_error_msg(ufbxi_cond_str(cond), msg), __FUNCTION__, __LINE__); return ret; } } while (0)
#define ufbxi_fail_err_msg(err, desc, msg) return ufbxi_fail_imp_err(err, ufbxi_error_msg(desc, msg), __FUNCTION__, __LINE__)

static void ufbxi_fix_error_type(ufbx_error *error, const char *default_desc)
{
	if (!error->description) error->description = default_desc;
	error->type = UFBX_ERROR_UNKNOWN;
	if (!strcmp(error->description, "Out of memory")) {
		error->type = UFBX_ERROR_OUT_OF_MEMORY;
	} else if (!strcmp(error->description, "Memory limit exceeded")) {
		error->type = UFBX_ERROR_MEMORY_LIMIT;
	} else if (!strcmp(error->description, "Allocation limit exceeded")) {
		error->type = UFBX_ERROR_ALLOCATION_LIMIT;
	} else if (!strcmp(error->description, "Truncated file")) {
		error->type = UFBX_ERROR_TRUNCATED_FILE;
	} else if (!strcmp(error->description, "IO error")) {
		error->type = UFBX_ERROR_IO;
	} else if (!strcmp(error->description, "Cancelled")) {
		error->type = UFBX_ERROR_CANCELLED;
	} else if (!strcmp(error->description, "Unsupported version")) {
		error->type = UFBX_ERROR_UNSUPPORTED_VERSION;
	} else if (!strcmp(error->description, "Not an FBX file")) {
		error->type = UFBX_ERROR_NOT_FBX;
	} else if (!strcmp(error->description, "File not found")) {
		error->type = UFBX_ERROR_FILE_NOT_FOUND;
	} else if (!strcmp(error->description, "Uninitialized options")) {
		error->type = UFBX_ERROR_FILE_NOT_FOUND;
	}
}

// -- Allocator

// Returned for zero size allocations, place in the constant data
// to catch writes to bad allocations.
#if defined(UFBX_REGRESSION)
static const char ufbxi_zero_size_buffer[4096] = { 0 };
#else
static const char ufbxi_zero_size_buffer[64] = { 0 };
#endif

typedef struct {
	ufbx_error *error;
	size_t current_size;
	size_t max_size;
	size_t num_allocs;
	size_t max_allocs;
	size_t huge_size;
	size_t chunk_max;
	ufbx_allocator ator;
} ufbxi_allocator;

static ufbxi_forceinline bool ufbxi_does_overflow(size_t total, size_t a, size_t b)
{
	// If `a` and `b` have at most 4 bits per `size_t` byte, the product can't overflow.
	if (((a | b) >> sizeof(size_t)*4) != 0) {
		if (a != 0 && total / a != b) return true;
	}
	return false;
}

static void *ufbxi_alloc_size(ufbxi_allocator *ator, size_t size, size_t n)
{
	// Always succeed with an emtpy non-NULL buffer for empty allocations
	ufbx_assert(size > 0);
	if (n == 0) return (void*)ufbxi_zero_size_buffer;

	size_t total = size * n;
	ufbxi_check_return_err(ator->error, !ufbxi_does_overflow(total, size, n), NULL);
	ufbxi_check_return_err_msg(ator->error, total <= ator->max_size - ator->current_size, NULL, "Memory limit exceeded");
	ufbxi_check_return_err_msg(ator->error, ator->num_allocs < ator->max_allocs, NULL, "Allocation limit exceeded");
	ator->num_allocs++;

	ator->current_size += total;

	void *ptr;
	if (ator->ator.alloc_fn) {
		ptr = ator->ator.alloc_fn(ator->ator.user, total);
	} else if (ator->ator.realloc_fn) {
		ptr = ator->ator.realloc_fn(ator->ator.user, NULL, 0, total);
	} else {
		ptr = malloc(total);
	}

	ufbxi_check_return_err_msg(ator->error, ptr, NULL, "Out of memory");
	return ptr;
}

static void ufbxi_free_size(ufbxi_allocator *ator, size_t size, void *ptr, size_t n);
static void *ufbxi_realloc_size(ufbxi_allocator *ator, size_t size, void *old_ptr, size_t old_n, size_t n)
{
	ufbx_assert(size > 0);
	// realloc() with zero old/new size is equivalent to alloc()/free()
	if (old_n == 0) return ufbxi_alloc_size(ator, size, n);
	if (n == 0) { ufbxi_free_size(ator, size, old_ptr, old_n); return NULL; }

	size_t old_total = size * old_n;
	size_t total = size * n;

	// The old values have been checked by a previous allocate call
	ufbx_assert(!ufbxi_does_overflow(old_total, size, old_n));
	ufbx_assert(old_total <= ator->current_size);

	ufbxi_check_return_err(ator->error, !ufbxi_does_overflow(total, size, n), NULL);
	ufbxi_check_return_err_msg(ator->error, total <= ator->max_size - ator->current_size, NULL, "Memory limit exceeded");
	ufbxi_check_return_err_msg(ator->error, ator->num_allocs < ator->max_allocs, NULL, "Allocation limit exceeded");
	ator->num_allocs++;

	ator->current_size += total;
	ator->current_size -= old_total;

	void *ptr;
	if (ator->ator.realloc_fn) {
		ptr = ator->ator.realloc_fn(ator->ator.user, old_ptr, old_total, total);
	} else if (ator->ator.alloc_fn) {
		// Use user-provided alloc_fn() and free_fn()
		ptr = ator->ator.alloc_fn(ator->ator.user, total);
		if (ptr) memcpy(ptr, old_ptr, old_total);
		if (ator->ator.free_fn) {
			ator->ator.free_fn(ator->ator.user, old_ptr, old_total);
		}
	} else {
		ptr = realloc(old_ptr, total);
	}

	ufbxi_check_return_err_msg(ator->error, ptr, NULL, "Out of memory");
	return ptr;
}

static void ufbxi_free_size(ufbxi_allocator *ator, size_t size, void *ptr, size_t n)
{
	ufbx_assert(size > 0);
	if (n == 0) return;
	ufbx_assert(ptr);

	size_t total = size * n;

	// The old values have been checked by a previous allocate call
	ufbx_assert(!ufbxi_does_overflow(total, size, n));
	ufbx_assert(total <= ator->current_size);

	ator->current_size -= total;

	if (ator->ator.alloc_fn || ator->ator.realloc_fn) {
		// Don't call default free() if there is an user-provided `alloc_fn()`
		if (ator->ator.free_fn) {
			ator->ator.free_fn(ator->ator.user, ptr, total);
		} else if (ator->ator.realloc_fn) {
			ator->ator.realloc_fn(ator->ator.user, ptr, total, 0);
		}
	} else {
		free(ptr);
	}
}

ufbxi_nodiscard static bool ufbxi_grow_array_size(ufbxi_allocator *ator, size_t size, void *p_ptr, size_t *p_cap, size_t n)
{
	if (n <= *p_cap) return true;
	void *ptr = *(void**)p_ptr;
	size_t old_n = *p_cap;
	if (old_n >= n) return true;
	size_t new_n = ufbxi_max_sz(old_n * 2, n);
	void *new_ptr = ufbxi_realloc_size(ator, size, ptr, old_n, new_n);
	if (!new_ptr) return false;
	*(void**)p_ptr = new_ptr;
	*p_cap = new_n;
	return true;
}

static ufbxi_noinline void ufbxi_free_ator(ufbxi_allocator *ator)
{
	ufbx_assert(ator->current_size == 0);

	ufbx_free_allocator_fn *free_fn = ator->ator.free_allocator_fn;
	if (free_fn) {
		void *user = ator->ator.user;
		free_fn(user);
	}
}

#define ufbxi_alloc(ator, type, n) (type*)ufbxi_alloc_size((ator), sizeof(type), (n))
#define ufbxi_alloc_zero(ator, type, n) (type*)ufbxi_alloc_zero_size((ator), sizeof(type), (n))
#define ufbxi_realloc(ator, type, old_ptr, old_n, n) (type*)ufbxi_realloc_size((ator), sizeof(type), (old_ptr), (old_n), (n))
#define ufbxi_realloc_zero(ator, type, old_ptr, old_n, n) (type*)ufbxi_realloc_zero_size((ator), sizeof(type), (old_ptr), (old_n), (n))
#define ufbxi_free(ator, type, ptr, n) ufbxi_free_size((ator), sizeof(type), (ptr), (n))

#define ufbxi_grow_array(ator, p_ptr, p_cap, n) ufbxi_grow_array_size((ator), sizeof(**(p_ptr)), (p_ptr), (p_cap), (n))

// -- Memory buffer
//
// General purpose memory buffer that can be used either as a chunked linear memory
// allocator or a non-contiguous stack. You can convert the contents of `ufbxi_buf`
// to a contiguous range of memory by calling `ufbxi_make_array[_all]()`

typedef struct ufbxi_buf_padding ufbxi_buf_padding;
typedef struct ufbxi_buf_chunk ufbxi_buf_chunk;

struct ufbxi_buf_padding {
	size_t original_pos; // < Original position before aligning
	size_t prev_padding; // < Starting offset of the previous `ufbxi_buf_padding`
};

struct ufbxi_buf_chunk {

	// Linked list of nodes
	ufbxi_buf_chunk *root;
	ufbxi_buf_chunk *prev;
	ufbxi_buf_chunk *next;

	void *align_0; // < Align to 4x pointer size (16/32 bytes)

	size_t size;         // < Size of the chunk `data`, excluding this header
	size_t pushed_pos;   // < Size of valid data when pushed to the list
	size_t next_size;    // < Next geometrically growing chunk size to allocate
	size_t padding_pos;  // < One past the offset of the most recent `ufbxi_buf_padding`

	char data[]; // < Must be aligned to 8 bytes
};

ufbx_static_assert(buf_chunk_align, offsetof(ufbxi_buf_chunk, data) % 8 == 0);

typedef struct {
	ufbxi_allocator *ator;
	ufbxi_buf_chunk *chunk;

	size_t pos;       // < Next offset to allocate from
	size_t size;      // < Size of the current chunk ie. `chunk->size` (or 0 if `chunk == NULL`)
	size_t num_items; // < Number of individual items pushed to the buffer

	bool unordered;  // < Does not support popping from the buffer
} ufbxi_buf;

typedef struct {
	ufbxi_buf_chunk *chunk;
	size_t pos;
	size_t num_items;
} ufbxi_buf_state;

static ufbxi_forceinline size_t ufbxi_align_to_mask(size_t value, size_t align_mask)
{
	return value + (((size_t)0 - value) & align_mask);
}

static ufbxi_forceinline size_t ufbxi_size_align_mask(size_t size)
{
	// Align to the all bits below the lowest set one in `size`
	// up to a maximum of 0x7 (align to 8 bytes).
	return ((size ^ (size - 1)) >> 1) & 0x7;
}

static void *ufbxi_push_size_new_block(ufbxi_buf *b, size_t size)
{
	bool huge = size >= b->ator->huge_size;

	ufbxi_buf_chunk *chunk = b->chunk;
	if (chunk) {
		// Store the final position for the retired chunk
		chunk->pushed_pos = b->pos;

		size_t align_mask = ufbxi_size_align_mask(size);

		ufbxi_buf_chunk *next;
		while ((next = chunk->next) != NULL) {
			chunk = next;
			size_t pos = ufbxi_align_to_mask(chunk->pushed_pos, align_mask);
			if (size <= chunk->size - pos) {
				b->chunk = chunk;
				b->pos = pos + (uint32_t)size;
				b->size = chunk->size;
				return chunk->data + pos;
			}
		}
	}

	// Allocate a new chunk, grow `next_size` geometrically but don't double
	// the current or previous user sizes if they are larger.
	size_t chunk_size, next_size;

	// If `size` is larger than `huge_size` don't grow `next_size` geometrically,
	// but use a dedicated allocation.
	if (huge) {
		next_size = chunk ? chunk->next_size : 4096;
		if (next_size > b->ator->chunk_max) next_size = b->ator->chunk_max;
		chunk_size = size;
	} else {
		next_size = chunk ? chunk->next_size * 2 : 4096;
		if (next_size > b->ator->chunk_max) next_size = b->ator->chunk_max;
		chunk_size = next_size - sizeof(ufbxi_buf_chunk);
		if (chunk_size < size) chunk_size = size;
	}

	// Align chunk sizes to 16 bytes
	chunk_size = ufbxi_align_to_mask(chunk_size, 0xf);

	ufbxi_buf_chunk *new_chunk = (ufbxi_buf_chunk*)ufbxi_alloc_size(b->ator, 1, sizeof(ufbxi_buf_chunk) + chunk_size);
	if (!new_chunk) return NULL;

	new_chunk->prev = chunk;
	new_chunk->size = chunk_size;
	new_chunk->next_size = next_size;
	new_chunk->align_0 = NULL;
	new_chunk->padding_pos = 0;
	new_chunk->pushed_pos = 0;

	if (b->unordered && huge && chunk) {
		// If the buffer is unordered and we pushed a huge chunk keep using the current one
		new_chunk->next = chunk->next;
		new_chunk->root = chunk->root;
		new_chunk->pushed_pos = size;
		if (chunk->next) chunk->next->prev = new_chunk;
		chunk->next = new_chunk;
	} else {
		new_chunk->next = NULL;

		// Link the chunk to the list and set it as the active one
		if (chunk) {
			chunk->next = new_chunk;
			new_chunk->root = chunk->root;
		} else {
			new_chunk->root = new_chunk;
		}

		b->chunk = new_chunk;
		b->pos = size;
		b->size = chunk_size;
	}

	return new_chunk->data;
}

static void *ufbxi_push_size(ufbxi_buf *b, size_t size, size_t n)
{
	// Always succeed with an emtpy non-NULL buffer for empty allocations
	ufbx_assert(size > 0);
	if (n == 0) return (void*)ufbxi_zero_size_buffer;

	b->num_items += n;

	size_t total = size * n;
	if (ufbxi_does_overflow(total, size, n)) return NULL;

	// Align to the natural alignment based on the size
	size_t align_mask = ufbxi_size_align_mask(size);
	size_t pos = ufbxi_align_to_mask(b->pos, align_mask);

	if (!b->unordered && pos != b->pos) {
		// Alignment mismatch in an unordered block. Align to 16 bytes to guarantee
		// sufficient alignment for anything afterwards and mark the padding.
		// If we overflow the current block we don't need to care as the block
		// boundaries are not contiguous.
		pos = ufbxi_align_to_mask(b->pos, 0xf);
		if (total < SIZE_MAX - 16 && total + 16 <= b->size - pos) {
			ufbxi_buf_padding *padding = (ufbxi_buf_padding*)(b->chunk->data + pos);
			padding->original_pos = b->pos;
			padding->prev_padding = b->chunk->padding_pos;
			b->chunk->padding_pos = pos + 16 + 1;
			b->pos = pos + 16 + total;
			return (char*)padding + 16;
		} else {
			return ufbxi_push_size_new_block(b, total);
		}
	} else {
		// Try to push to the current block. Allocate a new block
		// if the aligned size doesn't fit.
		if (total <= b->size - pos) {
			b->pos = pos + total;
			return b->chunk->data + pos;
		} else {
			return ufbxi_push_size_new_block(b, total);
		}
	}
}

static ufbxi_forceinline void *ufbxi_push_size_zero(ufbxi_buf *b, size_t size, size_t n)
{
	void *ptr = ufbxi_push_size(b, size, n);
	if (ptr) memset(ptr, 0, size * n);
	return ptr;
}

ufbxi_nodiscard static ufbxi_forceinline void *ufbxi_push_size_copy(ufbxi_buf *b, size_t size, size_t n, const void *data)
{
	// Always succeed with an emtpy non-NULL buffer for empty allocations, even if `data == NULL`
	ufbx_assert(size > 0);
	if (n == 0) return (void*)ufbxi_zero_size_buffer;

	ufbx_assert(data);
	void *ptr = ufbxi_push_size(b, size, n);
	if (ptr) memcpy(ptr, data, size * n);
	return ptr;
}

static ufbxi_noinline void ufbxi_buf_free_unused(ufbxi_buf *b)
{
	ufbx_assert(!b->unordered);

	ufbxi_buf_chunk *chunk = b->chunk;
	if (!chunk) return;

	ufbxi_buf_chunk *next = chunk->next;
	while (next) {
		ufbxi_buf_chunk *to_free = next;
		next = next->next;
		ufbxi_free_size(b->ator, 1, to_free, sizeof(ufbxi_buf_chunk) + to_free->size);
	}
	chunk->next = NULL;

	while (b->pos == 0 && chunk) {
		ufbxi_buf_chunk *prev = chunk->prev;
		ufbxi_free_size(b->ator, 1, chunk, sizeof(ufbxi_buf_chunk) + chunk->size);
		chunk = prev;
		b->chunk = prev;
		if (prev) {
			prev->next = NULL;
			b->pos = prev->pushed_pos;
			b->size = prev->size;
		} else {
			b->pos = 0;
			b->size = 0;
		}
	}
}

static void ufbxi_pop_size(ufbxi_buf *b, size_t size, size_t n, void *dst)
{
	ufbx_assert(!b->unordered);
	ufbx_assert(size > 0);
	ufbx_assert(b->num_items >= n);
	b->num_items -= n;

	char *ptr = (char*)dst;
	size_t bytes_left = size * n;

	// We've already pushed this, it better not overflow
	ufbx_assert(!ufbxi_does_overflow(bytes_left, size, n));

	if (ptr) {
		ptr += bytes_left;
		size_t pos = b->pos;
		for (;;) {
			ufbxi_buf_chunk *chunk = b->chunk;
			if (bytes_left <= pos) {
				// Rest of the data is in this single chunk
				pos -= bytes_left;
				b->pos = pos;
				ptr -= bytes_left;
				memcpy(ptr, chunk->data + pos, bytes_left);
				break;
			} else {
				// Pop the whole chunk
				ptr -= pos;
				bytes_left -= pos;
				memcpy(ptr, chunk->data, pos);
				chunk->pushed_pos = 0;
				chunk = chunk->prev;
				b->chunk = chunk;
				b->size = chunk->size;
				pos = chunk->pushed_pos;
			}
		}
	} else {
		size_t pos = b->pos;
		for (;;) {
			ufbxi_buf_chunk *chunk = b->chunk;
			if (bytes_left <= pos) {
				// Rest of the data is in this single chunk
				pos -= bytes_left;
				b->pos = pos;
				break;
			} else {
				// Pop the whole chunk
				bytes_left -= pos;
				chunk->pushed_pos = 0;
				chunk = chunk->prev;
				b->chunk = chunk;
				b->size = chunk->size;
				pos = chunk->pushed_pos;
			}
		}
	}

	// Check if we need to rewind past some alignment padding
	if (b->chunk) {
		size_t pos = b->pos, padding_pos = b->chunk->padding_pos;
		if (pos < padding_pos) {
			ufbx_assert(pos + 1 == padding_pos);
			ufbxi_buf_padding *padding = (ufbxi_buf_padding*)(b->chunk->data + padding_pos - 1 - 16);
			b->pos = padding->original_pos;
			b->chunk->padding_pos = padding->prev_padding;
		}
	}

	// Immediately free popped items if all the allocations are huge
	// as it means we want to have dedicated allocations for each push.
	if (b->ator->huge_size <= 1) {
		ufbxi_buf_free_unused(b);
	}
}

static void *ufbxi_push_pop_size(ufbxi_buf *dst, ufbxi_buf *src, size_t size, size_t n)
{
	void *data = ufbxi_push_size(dst, size, n);
	if (!data) return NULL;
	ufbxi_pop_size(src, size, n, data);
	return data;
}

static void ufbxi_buf_free(ufbxi_buf *buf)
{
	ufbxi_buf_chunk *chunk = buf->chunk;
	if (chunk) {
		chunk = chunk->root;
		while (chunk) {
			ufbxi_buf_chunk *next = chunk->next;
			ufbxi_free_size(buf->ator, 1, chunk, sizeof(ufbxi_buf_chunk) + chunk->size);
			chunk = next;
		}
	}
	buf->chunk = NULL;
	buf->pos = 0;
	buf->size = 0;
	buf->num_items = 0;
}

static void ufbxi_buf_clear(ufbxi_buf *buf)
{
	// Free the memory if using ASAN
	if (buf->ator->huge_size <= 1) {
		ufbxi_buf_free(buf);
		return;
	}

	ufbxi_buf_chunk *chunk = buf->chunk;
	if (chunk) {
		buf->chunk = chunk->root;
		buf->pos = 0;
		buf->size = buf->chunk->size;
		buf->num_items = 0;

		// Reset all the chunks
		while (chunk) {
			chunk->pushed_pos = 0;
			chunk->padding_pos = 0;
			chunk = chunk->next;
		}
	}
}

#define ufbxi_push(b, type, n) (type*)ufbxi_push_size((b), sizeof(type), (n))
#define ufbxi_push_zero(b, type, n) (type*)ufbxi_push_size_zero((b), sizeof(type), (n))
#define ufbxi_push_copy(b, type, n, data) (type*)ufbxi_push_size_copy((b), sizeof(type), (n), (data))
#define ufbxi_pop(b, type, n, dst) ufbxi_pop_size((b), sizeof(type), (n), (dst))
#define ufbxi_push_pop(dst, src, type, n) (type*)ufbxi_push_pop_size((dst), (src), sizeof(type), (n))

// -- Hash map
//
// The actual element comparison is left to the user of `ufbxi_map`, see usage below.
//
// NOTES:
//   ufbxi_map_insert() inserts duplicates, use ufbxi_map_find() before if necessary!

typedef struct ufbxi_aa_node ufbxi_aa_node;

struct ufbxi_aa_node {
	ufbxi_aa_node *left, *right;
	uint32_t level;
	uint32_t index;
};

typedef int ufbxi_cmp_fn(void *user, const void *a, const void *b);

typedef struct {
	ufbxi_allocator *ator;
	size_t data_size;

	void *items;
	uint64_t *entries;
	uint32_t mask;

	uint32_t capacity;
	uint32_t size;

	ufbxi_cmp_fn *cmp_fn;
	void *cmp_user;

	ufbxi_buf aa_buf;
	ufbxi_aa_node *aa_root;
} ufbxi_map;

static ufbxi_noinline void ufbxi_map_init(ufbxi_map *map, ufbxi_allocator *ator, ufbxi_cmp_fn *cmp_fn, void *cmp_user)
{
	map->ator = ator;
	map->aa_buf.ator = ator;
	map->cmp_fn = cmp_fn;
	map->cmp_user = cmp_user;
}

static void ufbxi_map_free(ufbxi_map *map)
{
	ufbxi_buf_free(&map->aa_buf);
	ufbxi_free(map->ator, char, map->entries, map->data_size);
	map->entries = NULL;
	map->items = NULL;
	map->aa_root = NULL;
	map->mask = map->capacity = map->size = 0;
}

static ufbxi_noinline ufbxi_aa_node *ufbxi_aa_tree_insert(ufbxi_map *map, ufbxi_aa_node *node, const void *value, uint32_t index, size_t item_size)
{
	if (!node) {
		ufbxi_aa_node *new_node = ufbxi_push(&map->aa_buf, ufbxi_aa_node, 1);
		if (!new_node) return NULL;
		new_node->left = NULL;
		new_node->right = NULL;
		new_node->level = 1;
		new_node->index = index;
		return new_node;
	}

	void *entry = (char*)map->items + node->index * item_size;
	int cmp = map->cmp_fn(map->cmp_user, value, entry);
	if (cmp < 0) {
		node->left = ufbxi_aa_tree_insert(map, node->left, value, index, item_size);
	} else if (cmp >= 0) {
		node->right = ufbxi_aa_tree_insert(map, node->right, value, index, item_size);
	}

	if (node->left && node->left->level == node->level) {
		ufbxi_aa_node *left = node->left;
		node->left = left->right;
		left->right = node;
		node = left;
	}

	if (node->right && node->right->right && node->right->right->level == node->level) {
		ufbxi_aa_node *right = node->right;
		node->right = right->left;
		right->left = node;
		right->level += 1;
		node = right;
	}

	return node;
}

static ufbxi_noinline void *ufbxi_aa_tree_find(ufbxi_map *map, const void *value, size_t item_size)
{
	ufbxi_aa_node *node = map->aa_root;
	while (node) {
		void *entry = (char*)map->items + node->index * item_size;
		int cmp = map->cmp_fn(map->cmp_user, value, entry);
		if (cmp < 0) {
			node = node->left;
		} else if (cmp > 0) {
			node = node->right;
		} else {
			return entry;
		}
	}
	return NULL;
}

static ufbxi_noinline bool ufbxi_map_grow_size_imp(ufbxi_map *map, size_t item_size, size_t min_size)
{
	ufbx_assert(min_size > 0);
	const double load_factor = 0.7;

	// Find the lowest power of two size that fits `min_size` within `load_factor`
	size_t num_entries = map->mask + 1;
	size_t new_size = (size_t)((double)num_entries * load_factor);
	if (min_size < map->capacity + 1) min_size = map->capacity + 1;
	while (new_size < min_size) {
		num_entries *= 2;
		new_size = (size_t)((double)num_entries * load_factor);
	}

	// Check for overflow
	ufbxi_check_return_err(map->ator->error, SIZE_MAX / num_entries > sizeof(uint64_t), false);
	size_t alloc_size = num_entries * sizeof(uint64_t);

	// Allocate a combined entry/item memory block
	size_t data_size = alloc_size + new_size * item_size;
	char *data = ufbxi_alloc(map->ator, char, data_size);
	ufbxi_check_return_err(map->ator->error, data, false);

	// Copy the previous user items over
	uint64_t *old_entries = map->entries;
	uint64_t *new_entries = (uint64_t*)data;
	void *new_items = data + alloc_size;
	if (map->size > 0) {
		memcpy(new_items, map->items, item_size * map->size);
	}

	// Re-hash the entries
	uint32_t old_mask = map->mask;
	uint32_t new_mask = (uint32_t)(num_entries) - 1;
	memset(new_entries, 0, sizeof(uint64_t) * num_entries);
	if (old_mask) {
		for (uint32_t i = 0; i <= old_mask; i++) {
			uint64_t entry, new_entry = old_entries[i];
			if (!new_entry) continue;

			// Reconstruct the hash of the old entry at `i`
			uint32_t old_scan = (uint32_t)(new_entry & old_mask) - 1;
			uint32_t hash = ((uint32_t)new_entry & ~old_mask) | ((i - old_scan) & old_mask);
			uint32_t slot = hash & new_mask;
			new_entry &= ~(uint64_t)new_mask;

			// Scan forward until we find an empty slot, potentially swapping
			// `new_element` if it has a shorter scan distance (Robin Hood).
			uint32_t scan = 1;
			while ((entry = new_entries[slot]) != 0) {
				uint32_t entry_scan = (entry & new_mask);
				if (entry_scan < scan) {
					new_entries[slot] = new_entry + scan;
					new_entry = (entry & ~(uint64_t)new_mask);
					scan = entry_scan;
				}
				scan += 1;
				slot = (slot + 1) & new_mask;
			}
			new_entries[slot] = new_entry + scan;
		}
	}

	// And finally free the previous allocation
	ufbxi_free(map->ator, char, (char*)old_entries, map->data_size);
	map->items = new_items;
	map->data_size = data_size;
	map->entries = new_entries;
	map->mask = new_mask;
	map->capacity = (uint32_t)new_size;

	return true;
}

static ufbxi_forceinline bool ufbxi_map_grow_size(ufbxi_map *map, size_t size, size_t min_size)
{
	if (map->size < map->capacity && map->capacity >= min_size) return true;
	return ufbxi_map_grow_size_imp(map, size, min_size);
}

static ufbxi_forceinline void *ufbxi_map_find_size(ufbxi_map *map, size_t size, uint32_t hash, const void *value)
{
	uint64_t *entries = map->entries;
	uint32_t mask = map->mask, scan = 0;

	uint32_t ref = hash & ~mask;
	if (!mask || scan == UINT32_MAX) return 0;

	// Scan entries until we find an exact match of the hash or until we hit
	// an element that has lower scan distance than our search (Robin Hood).
	// The encoding guarantees that zero slots also terminate with the same test.
	for (;;) {
		uint64_t entry = entries[(hash + scan) & mask];
		scan += 1;
		if ((uint32_t)entry == ref + scan) {
			uint32_t index = (uint32_t)(entry >> 32u);
			void *data = (char*)map->items + size * index;
			int cmp = map->cmp_fn(map->cmp_user, value, data);
			if (cmp == 0) return data;
		} else if ((entry & mask) < scan) {
			if (map->aa_root) {
				return ufbxi_aa_tree_find(map, value, size);
			} else {
				return NULL;
			}
		}
	}
}

static ufbxi_forceinline void *ufbxi_map_insert_size(ufbxi_map *map, size_t size, uint32_t hash, const void *value)
{
	if (!ufbxi_map_grow_size(map, size, 64)) return NULL;

	uint32_t index = map->size++;

	uint64_t *entries = map->entries;
	uint32_t mask = map->mask;

	// Scan forward until we find an empty slot, potentially swapping
	// `new_element` if it has a shorter scan distance (Robin Hood).
	uint32_t slot = hash & mask;
	uint64_t entry, new_entry = (uint64_t)index << 32u | (hash & ~mask);
	uint32_t scan = 1;
	while ((entry = entries[slot]) != 0) {
		uint32_t entry_scan = (entry & mask);
		if (entry_scan < scan) {
			entries[slot] = new_entry + scan;
			new_entry = (entry & ~(uint64_t)mask);
			scan = entry_scan;
		}
		scan += 1;
		slot = (slot + 1) & mask;

		if (scan > UFBXI_MAP_MAX_SCAN) {
			uint32_t new_index = (uint32_t)(new_entry >> 32u);
			const void *new_value = new_index == index ? value : (const void*)((char*)map->items + size * new_index);
			map->aa_root = ufbxi_aa_tree_insert(map, map->aa_root, new_value, new_index, size);
			return (char*)map->items + size * index;
		}
	}
	entries[slot] = new_entry + scan;

	return (char*)map->items + size * index;
}

#define ufbxi_map_grow(map, type, min_size) ufbxi_map_grow_size((map), sizeof(type), (min_size))
#define ufbxi_map_find(map, type, hash, value) (type*)ufbxi_map_find_size((map), sizeof(type), (hash), (value))
#define ufbxi_map_insert(map, type, hash, value) (type*)ufbxi_map_insert_size((map), sizeof(type), (hash), (value))

static int ufbxi_map_cmp_uint64(void *user, const void *va, const void *vb)
{
	(void)user;
	uint64_t a = *(const uint64_t*)va, b = *(const uint64_t*)vb;
	if (a < b) return -1;
	if (a > b) return +1;
	return 0;
}

static int ufbxi_map_cmp_const_char_ptr(void *user, const void *va, const void *vb)
{
	(void)user;
	const char *a = *(const char **)va, *b = *(const char **)vb;
	if (a < b) return -1;
	if (a > b) return +1;
	return 0;
}

// -- Hash functions

static uint32_t ufbxi_hash_string(const char *str, size_t length)
{
	uint32_t hash = (uint32_t)length;
	uint32_t seed = UINT32_C(0x9e3779b9);
	if (length >= 4) {
		do {
			uint32_t word = ufbxi_read_u32(str);
			hash = ((hash << 5u | hash >> 27u) ^ word) * seed;
			str += 4;
			length -= 4;
		} while (length >= 4);

		uint32_t word = ufbxi_read_u32(str + length - 4);
		hash = ((hash << 5u | hash >> 27u) ^ word) * seed;
	} else {
		uint32_t word = 0;
		if (length >= 1) word |= (uint32_t)(uint8_t)str[0] << 0;
		if (length >= 2) word |= (uint32_t)(uint8_t)str[1] << 8;
		if (length >= 3) word |= (uint32_t)(uint8_t)str[2] << 16;
		hash = ((hash << 5u | hash >> 27u) ^ word) * seed;
	}
	hash ^= hash >> 16;
	hash *= UINT32_C(0x7feb352d);
	hash ^= hash >> 15;
	return hash;
}

static ufbxi_forceinline uint32_t ufbxi_hash32(uint32_t x)
{
	x ^= x >> 16;
	x *= UINT32_C(0x7feb352d);
	x ^= x >> 15;
	x *= UINT32_C(0x846ca68b);
	x ^= x >> 16;
	return x;	
}

static ufbxi_forceinline uint32_t ufbxi_hash64(uint64_t x)
{
	x ^= x >> 32;
	x *= UINT64_C(0xd6e8feb86659fd93);
	x ^= x >> 32;
	x *= UINT64_C(0xd6e8feb86659fd93);
	x ^= x >> 32;
	return (uint32_t)x;
}

static ufbxi_forceinline uint32_t ufbxi_hash_uptr(uintptr_t ptr)
{
	return sizeof(ptr) == 8 ? ufbxi_hash64((uint64_t)ptr) : ufbxi_hash32((uint32_t)ptr);
}

#define ufbxi_hash_ptr(ptr) ufbxi_hash_uptr((uintptr_t)(ptr))

// -- String pool

// All strings found in FBX files are interned for deduplication and fast
// comparison. Our fixed internal strings (`ufbxi_String`) are considered the
// canonical pointers for said strings so we can compare them by address.

typedef struct {
	ufbx_error *error;
	ufbxi_buf buf; // < Buffer for the actual string data
	ufbxi_map map; // < Map of `ufbxi_string`
	size_t initial_size; // < Number of initial entries
} ufbxi_string_pool;

static ufbxi_forceinline bool ufbxi_str_equal(ufbx_string a, ufbx_string b)
{
	return a.length == b.length && !memcmp(a.data, b.data, a.length);
}

static ufbxi_forceinline bool ufbxi_str_less(ufbx_string a, ufbx_string b)
{
	size_t len = ufbxi_min_sz(a.length, b.length);
	int cmp = memcmp(a.data, b.data, len);
	if (cmp != 0) return cmp < 0;
	return a.length < b.length;
}

static ufbxi_forceinline int ufbxi_str_cmp(ufbx_string a, ufbx_string b)
{
	size_t len = ufbxi_min_sz(a.length, b.length);
	int cmp = memcmp(a.data, b.data, len);
	if (cmp != 0) return cmp;
	if (a.length != b.length) return a.length < b.length ? -1 : 1;
	return 0;
}

static ufbxi_forceinline ufbx_string ufbxi_str_c(const char *str)
{
	ufbx_string s = { str, strlen(str) };
	return s;
}

static int ufbxi_map_cmp_string(void *user, const void *va, const void *vb)
{
	(void)user;
	const ufbx_string *a = (const ufbx_string*)va, *b = (const ufbx_string*)vb;
	return ufbxi_str_cmp(*a, *b);
}

const char ufbxi_empty_char[1] = { '\0' };

ufbxi_nodiscard static const char *ufbxi_push_string_imp(ufbxi_string_pool *pool, const char *str, size_t length, bool copy)
{
	if (length == 0) return ufbxi_empty_char;

	ufbxi_check_return_err(pool->error, ufbxi_map_grow(&pool->map, ufbx_string, pool->initial_size), NULL);

	ufbx_string ref = { str, length };

	uint32_t hash = ufbxi_hash_string(str, length);
	ufbx_string *entry = ufbxi_map_find(&pool->map, ufbx_string, hash, &ref);
	if (entry) return entry->data;
	entry = ufbxi_map_insert(&pool->map, ufbx_string, hash, &ref);
	ufbxi_check_return_err(pool->error, entry, NULL);
	entry->length = length;
	if (copy) {
		char *dst = ufbxi_push(&pool->buf, char, length + 1);
		ufbxi_check_return_err(pool->error, dst, NULL);
		memcpy(dst, str, length);
		dst[length] = '\0';
		entry->data = dst;
	} else {
		entry->data = str;
	}
	return entry->data;
}

ufbxi_nodiscard static ufbxi_forceinline const char *ufbxi_push_string(ufbxi_string_pool *pool, const char *str, size_t length)
{
	return ufbxi_push_string_imp(pool, str, length, true);
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_push_string_place(ufbxi_string_pool *pool, const char **p_str, size_t length)
{
	const char *str = *p_str;
	ufbxi_check_err(pool->error, str || length == 0);
	str = ufbxi_push_string(pool, str, length);
	ufbxi_check_err(pool->error, str);
	*p_str = str;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_push_string_place_str(ufbxi_string_pool *pool, ufbx_string *p_str)
{
	ufbxi_check_err(pool->error, p_str);
	return ufbxi_push_string_place(pool, &p_str->data, p_str->length);
}

// -- String constants
//
// All strings in FBX files are pooled so by having canonical string constant
// addresses we can compare strings to these constants by comparing pointers.
// Keep the list alphabetically sorted!

static const char ufbxi_AllSame[] = "AllSame";
static const char ufbxi_Alphas[] = "Alphas";
static const char ufbxi_AmbientColor[] = "AmbientColor";
static const char ufbxi_AnimationCurveNode[] = "AnimationCurveNode";
static const char ufbxi_AnimationCurve[] = "AnimationCurve";
static const char ufbxi_AnimationLayer[] = "AnimationLayer";
static const char ufbxi_AnimationStack[] = "AnimationStack";
static const char ufbxi_ApertureFormat[] = "ApertureFormat";
static const char ufbxi_ApertureMode[] = "ApertureMode";
static const char ufbxi_AreaLightShape[] = "AreaLightShape";
static const char ufbxi_AspectH[] = "AspectH";
static const char ufbxi_AspectHeight[] = "AspectHeight";
static const char ufbxi_AspectRatioMode[] = "AspectRatioMode";
static const char ufbxi_AspectW[] = "AspectW";
static const char ufbxi_AspectWidth[] = "AspectWidth";
static const char ufbxi_BaseLayer[] = "BaseLayer";
static const char ufbxi_BindPose[] = "BindPose";
static const char ufbxi_BindingTable[] = "BindingTable";
static const char ufbxi_BinormalsIndex[] = "BinormalsIndex";
static const char ufbxi_Binormals[] = "Binormals";
static const char ufbxi_BlendMode[] = "BlendMode";
static const char ufbxi_BlendModes[] = "BlendModes";
static const char ufbxi_BlendShapeChannel[] = "BlendShapeChannel";
static const char ufbxi_BlendShape[] = "BlendShape";
static const char ufbxi_BlendWeights[] = "BlendWeights";
static const char ufbxi_BoundaryRule[] = "BoundaryRule";
static const char ufbxi_Boundary[] = "Boundary";
static const char ufbxi_ByEdge[] = "ByEdge";
static const char ufbxi_ByPolygonVertex[] = "ByPolygonVertex";
static const char ufbxi_ByPolygon[] = "ByPolygon";
static const char ufbxi_ByVertex[] = "ByVertex";
static const char ufbxi_ByVertice[] = "ByVertice";
static const char ufbxi_Cache[] = "Cache";
static const char ufbxi_CameraStereo[] = "CameraStereo";
static const char ufbxi_CameraSwitcher[] = "CameraSwitcher";
static const char ufbxi_Camera[] = "Camera";
static const char ufbxi_CastLight[] = "CastLight";
static const char ufbxi_CastShadows[] = "CastShadows";
static const char ufbxi_Channel[] = "Channel";
static const char ufbxi_Character[] = "Character";
static const char ufbxi_Children[] = "Children";
static const char ufbxi_Closed[] = "Closed";
static const char ufbxi_Cluster[] = "Cluster";
static const char ufbxi_CollectionExclusive[] = "CollectionExclusive";
static const char ufbxi_Collection[] = "Collection";
static const char ufbxi_ColorIndex[] = "ColorIndex";
static const char ufbxi_Color[] = "Color";
static const char ufbxi_Colors[] = "Colors";
static const char ufbxi_Cone_angle[] = "Cone angle";
static const char ufbxi_ConeAngle[] = "ConeAngle";
static const char ufbxi_Connections[] = "Connections";
static const char ufbxi_Constraint[] = "Constraint";
static const char ufbxi_Content[] = "Content";
static const char ufbxi_CoordAxisSign[] = "CoordAxisSign";
static const char ufbxi_CoordAxis[] = "CoordAxis";
static const char ufbxi_Count[] = "Count";
static const char ufbxi_Creator[] = "Creator";
static const char ufbxi_CurrentTextureBlendMode[] = "CurrentTextureBlendMode";
static const char ufbxi_CurrentTimeMarker[] = "CurrentTimeMarker";
static const char ufbxi_CustomFrameRate[] = "CustomFrameRate";
static const char ufbxi_DecayType[] = "DecayType";
static const char ufbxi_DefaultCamera[] = "DefaultCamera";
static const char ufbxi_Default[] = "Default";
static const char ufbxi_Definitions[] = "Definitions";
static const char ufbxi_DeformPercent[] = "DeformPercent";
static const char ufbxi_Deformer[] = "Deformer";
static const char ufbxi_DiffuseColor[] = "DiffuseColor";
static const char ufbxi_Dimension[] = "Dimension";
static const char ufbxi_Dimensions[] = "Dimensions";
static const char ufbxi_DisplayLayer[] = "DisplayLayer";
static const char ufbxi_Document[] = "Document";
static const char ufbxi_Documents[] = "Documents";
static const char ufbxi_EdgeCrease[] = "EdgeCrease";
static const char ufbxi_EdgeIndexArray[] = "EdgeIndexArray";
static const char ufbxi_Edges[] = "Edges";
static const char ufbxi_EmissiveColor[] = "EmissiveColor";
static const char ufbxi_Entry[] = "Entry";
static const char ufbxi_FBXHeaderExtension[] = "FBXHeaderExtension";
static const char ufbxi_FBXVersion[] = "FBXVersion";
static const char ufbxi_FbxPropertyEntry[] = "FbxPropertyEntry";
static const char ufbxi_FbxSemanticEntry[] = "FbxSemanticEntry";
static const char ufbxi_FieldOfViewX[] = "FieldOfViewX";
static const char ufbxi_FieldOfViewY[] = "FieldOfViewY";
static const char ufbxi_FieldOfView[] = "FieldOfView";
static const char ufbxi_FileName[] = "FileName";
static const char ufbxi_Filename[] = "Filename";
static const char ufbxi_FilmHeight[] = "FilmHeight";
static const char ufbxi_FilmSqueezeRatio[] = "FilmSqueezeRatio";
static const char ufbxi_FilmWidth[] = "FilmWidth";
static const char ufbxi_FlipNormals[] = "FlipNormals";
static const char ufbxi_FocalLength[] = "FocalLength";
static const char ufbxi_Form[] = "Form";
static const char ufbxi_Freeze[] = "Freeze";
static const char ufbxi_FrontAxisSign[] = "FrontAxisSign";
static const char ufbxi_FrontAxis[] = "FrontAxis";
static const char ufbxi_FullWeights[] = "FullWeights";
static const char ufbxi_GateFit[] = "GateFit";
static const char ufbxi_GeometricRotation[] = "GeometricRotation";
static const char ufbxi_GeometricScaling[] = "GeometricScaling";
static const char ufbxi_GeometricTranslation[] = "GeometricTranslation";
static const char ufbxi_GeometryUVInfo[] = "GeometryUVInfo";
static const char ufbxi_Geometry[] = "Geometry";
static const char ufbxi_GlobalSettings[] = "GlobalSettings";
static const char ufbxi_HotSpot[] = "HotSpot";
static const char ufbxi_Implementation[] = "Implementation";
static const char ufbxi_Indexes[] = "Indexes";
static const char ufbxi_InheritType[] = "InheritType";
static const char ufbxi_InnerAngle[] = "InnerAngle";
static const char ufbxi_Intensity[] = "Intensity";
static const char ufbxi_IsTheNodeInSet[] = "IsTheNodeInSet";
static const char ufbxi_KeyAttrDataFloat[] = "KeyAttrDataFloat";
static const char ufbxi_KeyAttrFlags[] = "KeyAttrFlags";
static const char ufbxi_KeyAttrRefCount[] = "KeyAttrRefCount";
static const char ufbxi_KeyCount[] = "KeyCount";
static const char ufbxi_KeyTime[] = "KeyTime";
static const char ufbxi_KeyValueFloat[] = "KeyValueFloat";
static const char ufbxi_Key[] = "Key";
static const char ufbxi_KnotVectorU[] = "KnotVectorU";
static const char ufbxi_KnotVectorV[] = "KnotVectorV";
static const char ufbxi_KnotVector[] = "KnotVector";
static const char ufbxi_LayerElementBinormal[] = "LayerElementBinormal";
static const char ufbxi_LayerElementColor[] = "LayerElementColor";
static const char ufbxi_LayerElementEdgeCrease[] = "LayerElementEdgeCrease";
static const char ufbxi_LayerElementMaterial[] = "LayerElementMaterial";
static const char ufbxi_LayerElementNormal[] = "LayerElementNormal";
static const char ufbxi_LayerElementSmoothing[] = "LayerElementSmoothing";
static const char ufbxi_LayerElementTangent[] = "LayerElementTangent";
static const char ufbxi_LayerElementUV[] = "LayerElementUV";
static const char ufbxi_LayerElementVertexCrease[] = "LayerElementVertexCrease";
static const char ufbxi_LayerElement[] = "LayerElement";
static const char ufbxi_Layer[] = "Layer";
static const char ufbxi_LayeredTexture[] = "LayeredTexture";
static const char ufbxi_Lcl_Rotation[] = "Lcl Rotation";
static const char ufbxi_Lcl_Scaling[] = "Lcl Scaling";
static const char ufbxi_Lcl_Translation[] = "Lcl Translation";
static const char ufbxi_LeftCamera[] = "LeftCamera";
static const char ufbxi_LightType[] = "LightType";
static const char ufbxi_Light[] = "Light";
static const char ufbxi_LimbLength[] = "LimbLength";
static const char ufbxi_LimbNode[] = "LimbNode";
static const char ufbxi_Limb[] = "Limb";
static const char ufbxi_Line[] = "Line";
static const char ufbxi_Link[] = "Link";
static const char ufbxi_LocalStart[] = "LocalStart";
static const char ufbxi_LocalStop[] = "LocalStop";
static const char ufbxi_LocalTime[] = "LocalTime";
static const char ufbxi_LodGroup[] = "LodGroup";
static const char ufbxi_MappingInformationType[] = "MappingInformationType";
static const char ufbxi_Marker[] = "Marker";
static const char ufbxi_MaterialAssignation[] = "MaterialAssignation";
static const char ufbxi_Material[] = "Material";
static const char ufbxi_Materials[] = "Materials";
static const char ufbxi_Matrix[] = "Matrix";
static const char ufbxi_Mesh[] = "Mesh";
static const char ufbxi_Model[] = "Model";
static const char ufbxi_Name[] = "Name";
static const char ufbxi_NodeAttributeName[] = "NodeAttributeName";
static const char ufbxi_NodeAttribute[] = "NodeAttribute";
static const char ufbxi_Node[] = "Node";
static const char ufbxi_NormalsIndex[] = "NormalsIndex";
static const char ufbxi_Normals[] = "Normals";
static const char ufbxi_Null[] = "Null";
static const char ufbxi_NurbsCurve[] = "NurbsCurve";
static const char ufbxi_NurbsSurfaceOrder[] = "NurbsSurfaceOrder";
static const char ufbxi_NurbsSurface[] = "NurbsSurface";
static const char ufbxi_Nurbs[] = "Nurbs";
static const char ufbxi_OO[] = "OO\0";
static const char ufbxi_OP[] = "OP\0";
static const char ufbxi_ObjectMetaData[] = "ObjectMetaData";
static const char ufbxi_ObjectType[] = "ObjectType";
static const char ufbxi_Objects[] = "Objects";
static const char ufbxi_Open[] = "Open";
static const char ufbxi_Order[] = "Order";
static const char ufbxi_OriginalUnitScaleFactor[] = "OriginalUnitScaleFactor";
static const char ufbxi_OriginalUpAxisSign[] = "OriginalUpAxisSign";
static const char ufbxi_OriginalUpAxis[] = "OriginalUpAxis";
static const char ufbxi_OuterAngle[] = "OuterAngle";
static const char ufbxi_PO[] = "PO\0";
static const char ufbxi_PP[] = "PP\0";
static const char ufbxi_Periodic[] = "Periodic";
static const char ufbxi_PointsIndex[] = "PointsIndex";
static const char ufbxi_Points[] = "Points";
static const char ufbxi_PolygonIndexArray[] = "PolygonIndexArray";
static const char ufbxi_PolygonVertexIndex[] = "PolygonVertexIndex";
static const char ufbxi_PoseNode[] = "PoseNode";
static const char ufbxi_Pose[] = "Pose";
static const char ufbxi_PostRotation[] = "PostRotation";
static const char ufbxi_PreRotation[] = "PreRotation";
static const char ufbxi_PreviewDivisionLevels[] = "PreviewDivisionLevels";
static const char ufbxi_Properties60[] = "Properties60";
static const char ufbxi_Properties70[] = "Properties70";
static const char ufbxi_PropertyTemplate[] = "PropertyTemplate";
static const char ufbxi_R[] = "R\0\0";
static const char ufbxi_ReferenceStart[] = "ReferenceStart";
static const char ufbxi_ReferenceStop[] = "ReferenceStop";
static const char ufbxi_ReferenceTime[] = "ReferenceTime";
static const char ufbxi_RelativeFileName[] = "RelativeFileName";
static const char ufbxi_RelativeFilename[] = "RelativeFilename";
static const char ufbxi_RenderDivisionLevels[] = "RenderDivisionLevels";
static const char ufbxi_RightCamera[] = "RightCamera";
static const char ufbxi_RootNode[] = "RootNode";
static const char ufbxi_Root[] = "Root";
static const char ufbxi_RotationAccumulationMode[] = "RotationAccumulationMode";
static const char ufbxi_RotationOffset[] = "RotationOffset";
static const char ufbxi_RotationOrder[] = "RotationOrder";
static const char ufbxi_RotationPivot[] = "RotationPivot";
static const char ufbxi_Rotation[] = "Rotation";
static const char ufbxi_S[] = "S\0\0";
static const char ufbxi_ScaleAccumulationMode[] = "ScaleAccumulationMode";
static const char ufbxi_ScalingOffset[] = "ScalingOffset";
static const char ufbxi_ScalingPivot[] = "ScalingPivot";
static const char ufbxi_Scaling[] = "Scaling";
static const char ufbxi_SceneInfo[] = "SceneInfo";
static const char ufbxi_SelectionNode[] = "SelectionNode";
static const char ufbxi_SelectionSet[] = "SelectionSet";
static const char ufbxi_ShadingModel[] = "ShadingModel";
static const char ufbxi_Shape[] = "Shape";
static const char ufbxi_Shininess[] = "Shininess";
static const char ufbxi_Show[] = "Show";
static const char ufbxi_Size[] = "Size";
static const char ufbxi_Skin[] = "Skin";
static const char ufbxi_SkinningType[] = "SkinningType";
static const char ufbxi_Smoothing[] = "Smoothing";
static const char ufbxi_Smoothness[] = "Smoothness";
static const char ufbxi_SnapOnFrameMode[] = "SnapOnFrameMode";
static const char ufbxi_SpecularColor[] = "SpecularColor";
static const char ufbxi_Step[] = "Step";
static const char ufbxi_SubDeformer[] = "SubDeformer";
static const char ufbxi_T[] = "T\0\0";
static const char ufbxi_Take[] = "Take";
static const char ufbxi_Takes[] = "Takes";
static const char ufbxi_TangentsIndex[] = "TangentsIndex";
static const char ufbxi_Tangents[] = "Tangents";
static const char ufbxi_Texture[] = "Texture";
static const char ufbxi_Texture_alpha[] = "Texture alpha";
static const char ufbxi_TextureId[] = "TextureId";
static const char ufbxi_TextureRotationPivot[] = "TextureRotationPivot";
static const char ufbxi_TextureScalingPivot[] = "TextureScalingPivot";
static const char ufbxi_TextureUV[] = "TextureUV";
static const char ufbxi_TextureUVVerticeIndex[] = "TextureUVVerticeIndex";
static const char ufbxi_TimeMarker[] = "TimeMarker";
static const char ufbxi_TimeMode[] = "TimeMode";
static const char ufbxi_TimeProtocol[] = "TimeProtocol";
static const char ufbxi_TimeSpanStart[] = "TimeSpanStart";
static const char ufbxi_TimeSpanStop[] = "TimeSpanStop";
static const char ufbxi_TransformLink[] = "TransformLink";
static const char ufbxi_Transform[] = "Transform";
static const char ufbxi_Translation[] = "Translation";
static const char ufbxi_TrimNurbsSurface[] = "TrimNurbsSurface";
static const char ufbxi_Type[] = "Type";
static const char ufbxi_TypedIndex[] = "TypedIndex";
static const char ufbxi_UVIndex[] = "UVIndex";
static const char ufbxi_UVSet[] = "UVSet";
static const char ufbxi_UVSwap[] = "UVSwap";
static const char ufbxi_UV[] = "UV\0";
static const char ufbxi_UnitScaleFactor[] = "UnitScaleFactor";
static const char ufbxi_UpAxisSign[] = "UpAxisSign";
static const char ufbxi_UpAxis[] = "UpAxis";
static const char ufbxi_VertexCacheDeformer[] = "VertexCacheDeformer";
static const char ufbxi_VertexCrease[] = "VertexCrease";
static const char ufbxi_VertexCreaseIndex[] = "VertexCreaseIndex";
static const char ufbxi_VertexIndexArray[] = "VertexIndexArray";
static const char ufbxi_Vertices[] = "Vertices";
static const char ufbxi_Video[] = "Video";
static const char ufbxi_Visibility[] = "Visibility";
static const char ufbxi_Weight[] = "Weight";
static const char ufbxi_Weights[] = "Weights";
static const char ufbxi_WrapModeU[] = "WrapModeU";
static const char ufbxi_WrapModeV[] = "WrapModeV";
static const char ufbxi_X[] = "X\0\0";
static const char ufbxi_Y[] = "Y\0\0";
static const char ufbxi_Z[] = "Z\0\0";
static const char ufbxi_d_X[] = "d|X";
static const char ufbxi_d_Y[] = "d|Y";
static const char ufbxi_d_Z[] = "d|Z";

static ufbx_string ufbxi_strings[] = {
	{ ufbxi_AllSame, 7 },
	{ ufbxi_Alphas, 6 },
	{ ufbxi_AmbientColor, 12 },
	{ ufbxi_AnimationCurve, 14 },
	{ ufbxi_AnimationCurveNode, 18 },
	{ ufbxi_AnimationLayer, 14 },
	{ ufbxi_AnimationStack, 14 },
	{ ufbxi_ApertureFormat, 14 },
	{ ufbxi_ApertureMode, 12 },
	{ ufbxi_AreaLightShape, 14 },
	{ ufbxi_AspectH, 7 },
	{ ufbxi_AspectHeight, 12 },
	{ ufbxi_AspectRatioMode, 15 },
	{ ufbxi_AspectW, 7 },
	{ ufbxi_AspectWidth, 11 },
	{ ufbxi_BaseLayer, 9 },
	{ ufbxi_BindPose, 8 },
	{ ufbxi_BindingTable, 12 },
	{ ufbxi_Binormals, 9 },
	{ ufbxi_BinormalsIndex, 14 },
	{ ufbxi_BlendMode, 9 },
	{ ufbxi_BlendModes, 10 },
	{ ufbxi_BlendShape, 10 },
	{ ufbxi_BlendShapeChannel, 17 },
	{ ufbxi_BlendWeights, 12 },
	{ ufbxi_Boundary, 8 },
	{ ufbxi_BoundaryRule, 12 },
	{ ufbxi_ByEdge, 6 },
	{ ufbxi_ByPolygon, 9 },
	{ ufbxi_ByPolygonVertex, 15 },
	{ ufbxi_ByVertex, 8 },
	{ ufbxi_ByVertice, 9 },
	{ ufbxi_Cache, 5 },
	{ ufbxi_Camera, 6 },
	{ ufbxi_CameraStereo, 12 },
	{ ufbxi_CameraSwitcher, 14 },
	{ ufbxi_CastLight, 9 },
	{ ufbxi_CastShadows, 11 },
	{ ufbxi_Channel, 7 },
	{ ufbxi_Character, sizeof(ufbxi_Character) - 1 },
	{ ufbxi_Children, 8 },
	{ ufbxi_Closed, 6 },
	{ ufbxi_Cluster, 7 },
	{ ufbxi_Collection, 10 },
	{ ufbxi_CollectionExclusive, 19 },
	{ ufbxi_Color, 5 },
	{ ufbxi_ColorIndex, 10 },
	{ ufbxi_Colors, 6 },
	{ ufbxi_Cone_angle, 10 },
	{ ufbxi_ConeAngle, 9 },
	{ ufbxi_Connections, 11 },
	{ ufbxi_Constraint, sizeof(ufbxi_Constraint) - 1 },
	{ ufbxi_Content, 7 },
	{ ufbxi_CoordAxis, 9 },
	{ ufbxi_CoordAxisSign, 13 },
	{ ufbxi_Count, 5 },
	{ ufbxi_Creator, 7 },
	{ ufbxi_CurrentTextureBlendMode, 23 },
	{ ufbxi_CurrentTimeMarker, 17 },
	{ ufbxi_CustomFrameRate, 15 },
	{ ufbxi_DecayType, 9 },
	{ ufbxi_Default, 7 },
	{ ufbxi_DefaultCamera, 13 },
	{ ufbxi_Definitions, 11 },
	{ ufbxi_DeformPercent, 13 },
	{ ufbxi_Deformer, 8 },
	{ ufbxi_DiffuseColor, 12 },
	{ ufbxi_Dimension, 9 },
	{ ufbxi_Dimensions, 10 },
	{ ufbxi_DisplayLayer, 12 },
	{ ufbxi_Document, 8 },
	{ ufbxi_Documents, 9 },
	{ ufbxi_EdgeCrease, 10 },
	{ ufbxi_EdgeIndexArray, 14 },
	{ ufbxi_Edges, 5 },
	{ ufbxi_EmissiveColor, 13 },
	{ ufbxi_Entry, 5 },
	{ ufbxi_FBXHeaderExtension, 18 },
	{ ufbxi_FBXVersion, 10 },
	{ ufbxi_FbxPropertyEntry, 16 },
	{ ufbxi_FbxSemanticEntry, 16 },
	{ ufbxi_FieldOfView, 11 },
	{ ufbxi_FieldOfViewX, 12 },
	{ ufbxi_FieldOfViewY, 12 },
	{ ufbxi_FileName, 8 },
	{ ufbxi_Filename, 8 },
	{ ufbxi_FilmHeight, 10 },
	{ ufbxi_FilmSqueezeRatio, 16 },
	{ ufbxi_FilmWidth, 9 },
	{ ufbxi_FlipNormals, 11 },
	{ ufbxi_FocalLength, 11 },
	{ ufbxi_Form, 4 },
	{ ufbxi_Freeze, 6 },
	{ ufbxi_FrontAxis, 9 },
	{ ufbxi_FrontAxisSign, 13 },
	{ ufbxi_FullWeights, 11 },
	{ ufbxi_GateFit, 7 },
	{ ufbxi_GeometricRotation, 17 },
	{ ufbxi_GeometricScaling, 16 },
	{ ufbxi_GeometricTranslation, 20 },
	{ ufbxi_Geometry, 8 },
	{ ufbxi_GeometryUVInfo, 14 },
	{ ufbxi_GlobalSettings, 14 },
	{ ufbxi_HotSpot, 7 },
	{ ufbxi_Implementation, 14 },
	{ ufbxi_Indexes, 7 },
	{ ufbxi_InheritType, 11 },
	{ ufbxi_InnerAngle, 10 },
	{ ufbxi_Intensity, 9 },
	{ ufbxi_IsTheNodeInSet, 14 },
	{ ufbxi_Key, 3 },
	{ ufbxi_KeyAttrDataFloat, 16 },
	{ ufbxi_KeyAttrFlags, 12 },
	{ ufbxi_KeyAttrRefCount, 15 },
	{ ufbxi_KeyCount, 8 },
	{ ufbxi_KeyTime, 7 },
	{ ufbxi_KeyValueFloat, 13 },
	{ ufbxi_KnotVector, 10 },
	{ ufbxi_KnotVectorU, 11 },
	{ ufbxi_KnotVectorV, 11 },
	{ ufbxi_Layer, 5 },
	{ ufbxi_LayerElement, 12 },
	{ ufbxi_LayerElementBinormal, 20 },
	{ ufbxi_LayerElementColor, 17 },
	{ ufbxi_LayerElementEdgeCrease, 22 },
	{ ufbxi_LayerElementMaterial, 20 },
	{ ufbxi_LayerElementNormal, 18 },
	{ ufbxi_LayerElementSmoothing, 21 },
	{ ufbxi_LayerElementTangent, 19 },
	{ ufbxi_LayerElementUV, 14 },
	{ ufbxi_LayerElementVertexCrease, 24 },
	{ ufbxi_LayeredTexture, 14 },
	{ ufbxi_Lcl_Rotation, 12 },
	{ ufbxi_Lcl_Scaling, 11 },
	{ ufbxi_Lcl_Translation, 15 },
	{ ufbxi_LeftCamera, 10 },
	{ ufbxi_Light, 5 },
	{ ufbxi_LightType, 9 },
	{ ufbxi_Limb, 4 },
	{ ufbxi_LimbLength, 10 },
	{ ufbxi_LimbNode, 8 },
	{ ufbxi_Line, 4 },
	{ ufbxi_Link, 4 },
	{ ufbxi_LocalStart, 10 },
	{ ufbxi_LocalStop, 9 },
	{ ufbxi_LocalTime, 9 },
	{ ufbxi_LodGroup, 8 },
	{ ufbxi_MappingInformationType, 22 },
	{ ufbxi_Marker, 6 },
	{ ufbxi_Material, 8 },
	{ ufbxi_MaterialAssignation, 19 },
	{ ufbxi_Materials, 9 },
	{ ufbxi_Matrix, 6 },
	{ ufbxi_Mesh, 4 },
	{ ufbxi_Model, 5 },
	{ ufbxi_Name, 4 },
	{ ufbxi_Node, 4 },
	{ ufbxi_NodeAttribute, 13 },
	{ ufbxi_NodeAttributeName, 17 },
	{ ufbxi_Normals, 7 },
	{ ufbxi_NormalsIndex, 12 },
	{ ufbxi_Null, 4 },
	{ ufbxi_Nurbs, 5 },
	{ ufbxi_NurbsCurve, 10 },
	{ ufbxi_NurbsSurface, 12 },
	{ ufbxi_NurbsSurfaceOrder, 17 },
	{ ufbxi_OO, 2 },
	{ ufbxi_OP, 2 },
	{ ufbxi_ObjectMetaData, 14 },
	{ ufbxi_ObjectType, 10 },
	{ ufbxi_Objects, 7 },
	{ ufbxi_Open, 4 },
	{ ufbxi_Order, 5 },
	{ ufbxi_OriginalUnitScaleFactor, 23 },
	{ ufbxi_OriginalUpAxis, 14 },
	{ ufbxi_OriginalUpAxisSign, 18 },
	{ ufbxi_OuterAngle, 10 },
	{ ufbxi_PO, 2 },
	{ ufbxi_PP, 2 },
	{ ufbxi_Periodic, 8 },
	{ ufbxi_Points, 6 },
	{ ufbxi_PointsIndex, 11 },
	{ ufbxi_PolygonIndexArray, 17 },
	{ ufbxi_PolygonVertexIndex, 18 },
	{ ufbxi_Pose, 4 },
	{ ufbxi_PoseNode, 8 },
	{ ufbxi_PostRotation, 12 },
	{ ufbxi_PreRotation, 11 },
	{ ufbxi_PreviewDivisionLevels, 21 },
	{ ufbxi_Properties60, 12 },
	{ ufbxi_Properties70, 12 },
	{ ufbxi_PropertyTemplate, 16 },
	{ ufbxi_R, 1 },
	{ ufbxi_ReferenceStart, 14 },
	{ ufbxi_ReferenceStop, 13 },
	{ ufbxi_ReferenceTime, 13 },
	{ ufbxi_RelativeFileName, 16 },
	{ ufbxi_RelativeFilename, 16 },
	{ ufbxi_RenderDivisionLevels, 20 },
	{ ufbxi_RightCamera, 11 },
	{ ufbxi_Root, 4 },
	{ ufbxi_RootNode, 8 },
	{ ufbxi_Rotation, 8 },
	{ ufbxi_RotationAccumulationMode, 24 },
	{ ufbxi_RotationOffset, 14 },
	{ ufbxi_RotationOrder, 13 },
	{ ufbxi_RotationPivot, 13 },
	{ ufbxi_S, 1 },
	{ ufbxi_ScaleAccumulationMode, 21 },
	{ ufbxi_Scaling, 7 },
	{ ufbxi_ScalingOffset, 13 },
	{ ufbxi_ScalingPivot, 12 },
	{ ufbxi_SceneInfo, 9 },
	{ ufbxi_SelectionNode, 13 },
	{ ufbxi_SelectionSet, 12 },
	{ ufbxi_ShadingModel, 12 },
	{ ufbxi_Shape, 5 },
	{ ufbxi_Shininess, 9 },
	{ ufbxi_Show, 4 },
	{ ufbxi_Size, 4 },
	{ ufbxi_Skin, 4 },
	{ ufbxi_SkinningType, 12 },
	{ ufbxi_Smoothing, 9 },
	{ ufbxi_Smoothness, 10 },
	{ ufbxi_SnapOnFrameMode, 15 },
	{ ufbxi_SpecularColor, 13 },
	{ ufbxi_Step, 4 },
	{ ufbxi_SubDeformer, 11 },
	{ ufbxi_T, 1 },
	{ ufbxi_Take, 4 },
	{ ufbxi_Takes, 5 },
	{ ufbxi_Tangents, 8 },
	{ ufbxi_TangentsIndex, 13 },
	{ ufbxi_Texture, 7 },
	{ ufbxi_Texture_alpha, 13 },
	{ ufbxi_TextureId, 9 },
	{ ufbxi_TextureRotationPivot, 20 },
	{ ufbxi_TextureScalingPivot, 19 },
	{ ufbxi_TextureUV, 9 },
	{ ufbxi_TextureUVVerticeIndex, 21 },
	{ ufbxi_TimeMarker, 10 },
	{ ufbxi_TimeMode, 8 },
	{ ufbxi_TimeProtocol, 12 },
	{ ufbxi_TimeSpanStart, 13 },
	{ ufbxi_TimeSpanStop, 12 },
	{ ufbxi_Transform, 9 },
	{ ufbxi_TransformLink, 13 },
	{ ufbxi_Translation, 11 },
	{ ufbxi_TrimNurbsSurface, 16 },
	{ ufbxi_Type, 4 },
	{ ufbxi_TypedIndex, 10 },
	{ ufbxi_UV, 2 },
	{ ufbxi_UVIndex, 7 },
	{ ufbxi_UVSet, 5 },
	{ ufbxi_UVSwap, 6 },
	{ ufbxi_UnitScaleFactor, 15 },
	{ ufbxi_UpAxis, 6 },
	{ ufbxi_UpAxisSign, 10 },
	{ ufbxi_VertexCacheDeformer, 19 },
	{ ufbxi_VertexCrease, 12 },
	{ ufbxi_VertexCreaseIndex, 17 },
	{ ufbxi_VertexIndexArray, 16 },
	{ ufbxi_Vertices, 8 },
	{ ufbxi_Video, 5 },
	{ ufbxi_Visibility, 10 },
	{ ufbxi_Weight, 6 },
	{ ufbxi_Weights, 7 },
	{ ufbxi_WrapModeU, 9 },
	{ ufbxi_WrapModeV, 9 },
	{ ufbxi_X, 1 },
	{ ufbxi_Y, 1 },
	{ ufbxi_Z, 1 },
	{ ufbxi_d_X, 3 },
	{ ufbxi_d_Y, 3 },
	{ ufbxi_d_Z, 3 },
};

static ufbxi_noinline const char *ufbxi_find_canonical_string(const char *data, size_t length)
{
	ufbx_string str = { data, length };

	size_t ix = SIZE_MAX;
	ufbxi_macro_lower_bound_eq(ufbx_string, 8, &ix, ufbxi_strings, 0, ufbxi_arraycount(ufbxi_strings),
		( ufbxi_str_less(*a, str) ), ( ufbxi_str_equal(*a, str) ));

	if (ix < SIZE_MAX) {
		return ufbxi_strings[ix].data;
	} else {
		return data;
	}
}

// -- Type definitions

typedef struct ufbxi_node ufbxi_node;

typedef enum {
	UFBXI_VALUE_NONE,
	UFBXI_VALUE_NUMBER,
	UFBXI_VALUE_STRING,
	UFBXI_VALUE_ARRAY,
} ufbxi_value_type;

typedef union {
	struct { double f; int64_t i; }; // if `UFBXI_PROP_NUMBER`
	ufbx_string s;                   // if `UFBXI_PROP_STRING`
} ufbxi_value;

typedef struct {
	void *data;  // < Pointer to `size` bool/int32_t/int64_t/float/double elements
	size_t size; // < Number of elements
	char type;   // < FBX type code: b/i/l/f/d
} ufbxi_value_array;

struct ufbxi_node {
	const char *name;      // < Name of the node (pooled, comapre with == to ufbxi_* strings)
	uint32_t num_children; // < Number of child nodes
	uint8_t name_len;      // < Length of `name` in bytes

	// If `value_type_mask == UFBXI_PROP_ARRAY` then the node is an array
	// (`array` field is valid) otherwise the node has N values in `vals`
	// where the type of each value is stored in 2 bits per value from LSB.
	// ie. `vals[ix]` type is `(value_type_mask >> (ix*2)) & 0x3`
	uint16_t value_type_mask;

	ufbxi_node *children;
	union {
		ufbxi_value_array *array; // if `prop_type_mask == UFBXI_PROP_ARRAY`
		ufbxi_value *vals;        // otherwise
	};
};

#define UFBXI_SCENE_IMP_MAGIC 0x58424655
#define UFBXI_MESH_IMP_MAGIC 0x48534d55
#define UFBXI_CACHE_IMP_MAGIC 0x48434355
#define UFBXI_SEGMENT_IMP_MAGIC 0x4d474553

typedef struct {
	ufbx_scene scene;
	uint32_t magic;

	ufbxi_allocator ator;
	ufbxi_buf result_buf;
	ufbxi_buf string_buf;
} ufbxi_scene_imp;

typedef struct {
	ufbx_mesh mesh;
	uint32_t magic;

	ufbxi_allocator ator;
	ufbxi_buf result_buf;
} ufbxi_mesh_imp;

typedef struct {
	// Semantic string data and length eg. for a string token
	// this string doesn't include the quotes.
	char *str_data;
	size_t str_len;
	size_t str_cap;

	// Type of the token, either single character such as '{' or ':'
	// or one of UFBXI_ASCII_* defines.
	char type;

	// Parsed semantic value
	union {
		double f64;
		int64_t i64;
		size_t name_len;
	} value;
} ufbxi_ascii_token;

typedef struct {
	size_t max_token_length;

	const char *src;
	const char *src_yield;
	const char *src_end;

	bool read_first_comment;
	bool found_version;
	bool parse_as_f32;

	ufbxi_ascii_token prev_token;
	ufbxi_ascii_token token;
} ufbxi_ascii;

typedef struct {
	const char *type;
	ufbx_string sub_type;
	ufbx_props props;
} ufbxi_template;

typedef struct {
	uint64_t fbx_id;
	uint32_t element_id;
} ufbxi_fbx_id_entry;

typedef struct {
	uint64_t node_fbx_id;
	uint64_t attr_fbx_id;
} ufbxi_fbx_attr_entry;

// Temporary connection before we resolve the element pointers
typedef struct {
	uint64_t src, dst;
	ufbx_string src_prop;
	ufbx_string dst_prop;
} ufbxi_tmp_connection;

typedef struct {
	uint64_t fbx_id;
	ufbx_string name;
	ufbx_props props;
} ufbxi_element_info;

typedef struct {
	uint64_t bone_fbx_id;
	ufbx_matrix bone_to_world;
} ufbxi_tmp_bone_pose;

typedef struct {
	ufbx_string prop_name;
	int32_t *face_texture;
	size_t num_faces;
	bool all_same;
} ufbxi_tmp_mesh_texture;

typedef struct {
	ufbxi_tmp_mesh_texture *texture_arr;
	size_t texture_count;
} ufbxi_mesh_extra;

typedef struct {
	int32_t material_id;
	int32_t texture_id;
	ufbx_string prop_name;
} ufbxi_tmp_material_texture;

typedef struct {
	int32_t *blend_modes;
	size_t num_blend_modes;

	ufbx_real *alphas;
	size_t num_alphas;
} ufbxi_texture_extra;

typedef struct {

	ufbx_error error;
	uint32_t version;
	ufbx_exporter exporter;
	uint32_t exporter_version;
	bool from_ascii;
	bool local_big_endian;
	bool file_big_endian;
	bool sure_fbx;

	ufbx_load_opts opts;

	// IO
	uint64_t data_offset;

	ufbx_read_fn *read_fn;
	ufbx_skip_fn *skip_fn;
	ufbx_close_fn *close_fn;
	void *read_user;

	char *read_buffer;
	size_t read_buffer_size;

	const char *data_begin;
	const char *data;
	size_t yield_size;
	size_t data_size;

	// Allocators
	ufbxi_allocator ator_result;
	ufbxi_allocator ator_tmp;

	// Temporary maps
	ufbxi_map prop_type_map;  // < `ufbxi_prop_type_name` Property type to enum
	ufbxi_map fbx_id_map;     // < `ufbxi_fbx_id_entry` FBX ID to local ID

	// 6x00 specific maps
	ufbxi_map fbx_attr_map;   // < `ufbxi_fbx_attr_entry` Node ID to attrib ID
	ufbxi_map node_prop_set;  // < `const char*` Node property names

	// Temporary array
	char *tmp_arr;
	size_t tmp_arr_size;
	char *swap_arr;
	size_t swap_arr_size;

	// Generated index buffers
	size_t max_zero_indices;
	size_t max_consecutive_indices;

	// Temporary buffers
	ufbxi_buf tmp;
	ufbxi_buf tmp_parse;
	ufbxi_buf tmp_stack;
	ufbxi_buf tmp_connections;
	ufbxi_buf tmp_node_ids;
	ufbxi_buf tmp_elements;
	ufbxi_buf tmp_element_offsets;
	ufbxi_buf tmp_typed_element_offsets[UFBX_NUM_ELEMENT_TYPES];
	ufbxi_buf tmp_mesh_textures;
	ufbxi_buf tmp_full_weights;
	size_t tmp_element_byte_offset;

	ufbxi_template *templates;
	size_t num_templates;

	// String pool
	ufbxi_string_pool string_pool;

	// Result buffers, these are retained in `ufbx_scene` returned to user.
	ufbxi_buf result;

	// Top-level state
	ufbxi_node *top_nodes;
	size_t top_nodes_len, top_nodes_cap;
	bool parsed_to_end;

	// "Focused" top-level node and child index, if `top_child_index == SIZE_MAX`
	// the children are parsed on demand.
	ufbxi_node *top_node;
	size_t top_child_index;
	ufbxi_node top_child;
	bool has_next_child;

	// Shared consecutive and all-zero index buffers
	int32_t *zero_indices;
	int32_t *consecutive_indices;

	// Call progress function periodically
	ptrdiff_t progress_timer;
	uint64_t progress_bytes_total;
	size_t progress_interval;

	// Extra data on the side of elements
	void **element_extra_arr;
	size_t element_extra_cap;

	ufbxi_ascii ascii;

	ufbxi_node root;

	ufbx_scene scene;
	ufbxi_scene_imp *scene_imp;

	ufbx_inflate_retain *inflate_retain;

	uint64_t root_id;
	uint32_t num_elements;

	ufbxi_node legacy_node;
	uint64_t legacy_implicit_anim_layer_id;

	double ktime_to_sec;

} ufbxi_context;

static ufbxi_noinline int ufbxi_fail_imp(ufbxi_context *uc, const char *cond, const char *func, uint32_t line)
{
	return ufbxi_fail_imp_err(&uc->error, cond, func, line);
}

#define ufbxi_check(cond) if (!(cond)) return ufbxi_fail_imp(uc, ufbxi_cond_str(cond), __FUNCTION__, __LINE__)
#define ufbxi_check_return(cond, ret) do { if (!(cond)) { ufbxi_fail_imp(uc, ufbxi_cond_str(cond), __FUNCTION__, __LINE__); return ret; } } while (0)
#define ufbxi_fail(desc) return ufbxi_fail_imp(uc, desc, __FUNCTION__, __LINE__)

#define ufbxi_check_msg(cond, msg) if (!(cond)) return ufbxi_fail_imp(uc, ufbxi_error_msg(ufbxi_cond_str(cond), msg), __FUNCTION__, __LINE__)
#define ufbxi_check_return_msg(cond, ret, msg) do { if (!(cond)) { ufbxi_fail_imp(uc, ufbxi_error_msg(ufbxi_cond_str(cond), msg), __FUNCTION__, __LINE__); return ret; } } while (0)
#define ufbxi_fail_msg(desc, msg) return ufbxi_fail_imp(uc, ufbxi_error_msg(desc, msg), __FUNCTION__, __LINE__)

// -- Progress

static ufbxi_forceinline uint64_t ufbxi_get_read_offset(ufbxi_context *uc)
{
	return uc->data_offset + (uc->data - uc->data_begin);
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_report_progress(ufbxi_context *uc)
{
	if (!uc->opts.progress_fn) return 1;
	ufbx_progress progress;
	progress.bytes_read = ufbxi_get_read_offset(uc);
	progress.bytes_total = uc->progress_bytes_total;

	uc->progress_timer = 1024;
	ufbxi_check_msg(uc->opts.progress_fn(uc->opts.progress_user, &progress), "Cancelled");
	return 1;
}

// TODO: Remove `ufbxi_unused` when it's not needed anymore
ufbxi_unused ufbxi_nodiscard static ufbxi_forceinline int ufbxi_progress(ufbxi_context *uc, size_t work_units)
{
	if (!uc->opts.progress_fn) return 1;
	ptrdiff_t left = uc->progress_timer - (ptrdiff_t)work_units;
	uc->progress_timer = left;
	if (left > 0) return 1;
	return ufbxi_report_progress(uc);
}

// -- IO

static ufbxi_noinline const char *ufbxi_refill(ufbxi_context *uc, size_t size)
{
	ufbx_assert(uc->data_size < size);
	ufbxi_check_return_msg(uc->read_fn, NULL, "Truncated file");

	void *data_to_free = NULL;
	size_t size_to_free = 0;

	// Grow the read buffer if necessary, data is copied over below with the
	// usual path so the free is deferred (`size_to_free`, `data_to_free`)
	if (size > uc->read_buffer_size) {
		size_t new_size = ufbxi_max_sz(size, uc->opts.read_buffer_size);
		new_size = ufbxi_max_sz(new_size, uc->read_buffer_size * 2);
		size_to_free = uc->read_buffer_size;
		data_to_free = uc->read_buffer;
		char *new_buffer = ufbxi_alloc(&uc->ator_tmp, char, new_size);
		ufbxi_check_return(new_buffer, NULL);
		uc->read_buffer = new_buffer;
		uc->read_buffer_size = new_size;
	}

	// Copy the remains of the previous buffer to the beginning of the new one
	size_t num_read = uc->data_size;
	if (num_read > 0) {
		memmove(uc->read_buffer, uc->data, num_read);
	}

	if (size_to_free) {
		ufbxi_free(&uc->ator_tmp, char, data_to_free, size_to_free);
	}

	// Fill the rest of the buffer with user data
	size_t to_read = uc->read_buffer_size - num_read;
	size_t read_result = uc->read_fn(uc->read_user, uc->read_buffer + num_read, to_read);
	ufbxi_check_return_msg(read_result != SIZE_MAX, NULL, "IO error");
	ufbxi_check_return(read_result <= to_read, NULL);

	num_read += read_result;
	ufbxi_check_return_msg(num_read >= size, NULL, "Truncated file");

	uc->data_offset += uc->data - uc->data_begin;
	uc->data_begin = uc->data = uc->read_buffer;
	uc->data_size = num_read;

	return uc->read_buffer;
}

static ufbxi_noinline const char *ufbxi_yield(ufbxi_context *uc, size_t size)
{
	const char *ret;
	uc->data_size += uc->yield_size;
	if (uc->data_size >= size) {
		ret = uc->data;
	} else {
		ret = ufbxi_refill(uc, size);
	}
	uc->yield_size = ufbxi_min_sz(uc->data_size, ufbxi_max_sz(size, uc->progress_interval));
	uc->data_size -= uc->yield_size;

	ufbxi_check_return(ufbxi_report_progress(uc), NULL);
	return ret;
}

static ufbxi_forceinline const char *ufbxi_peek_bytes(ufbxi_context *uc, size_t size)
{
	if (uc->yield_size >= size) {
		return uc->data;
	} else {
		return ufbxi_yield(uc, size);
	}
}

static ufbxi_forceinline const char *ufbxi_read_bytes(ufbxi_context *uc, size_t size)
{
	// Refill the current buffer if necessary
	const char *ret;
	if (uc->yield_size >= size) {
		ret = uc->data;
	} else {
		ret = ufbxi_yield(uc, size);
		if (!ret) return NULL;
	}

	// Advance the read position inside the current buffer
	uc->yield_size -= size;
	uc->data = ret + size;
	return ret;
}

static ufbxi_forceinline void ufbxi_consume_bytes(ufbxi_context *uc, size_t size)
{
	// Bytes must have been checked first with `ufbxi_peek_bytes()`
	ufbx_assert(size <= uc->yield_size);
	uc->yield_size -= size;
	uc->data += size;
}

ufbxi_nodiscard static int ufbxi_skip_bytes(ufbxi_context *uc, uint64_t size)
{
	if (uc->skip_fn) {
		uc->data_size += uc->yield_size;
		uc->yield_size = 0;

		if (size > uc->data_size) {
			size -= uc->data_size;
			uc->data += uc->data_size;
			uc->data_size = 0;

			uc->data_offset += size;
			while (size >= UFBXI_MAX_SKIP_SIZE) {
				size -= UFBXI_MAX_SKIP_SIZE;
				ufbxi_check_msg(uc->skip_fn(uc->read_user, UFBXI_MAX_SKIP_SIZE - 1), "Truncated file");

				// Check that we can read at least one byte in case the file is broken
				// and causes us to seek indefinitely forwards as `fseek()` does not
				// report if we hit EOF...
				char single_byte[1];
				size_t num_read = uc->read_fn(uc->read_user, single_byte, 1);
				ufbxi_check_msg(num_read <= 1, "IO error");
				ufbxi_check_msg(num_read == 1, "Truncated file");
			}

			if (size > 0) {
				ufbxi_check_msg(uc->skip_fn(uc->read_user, (size_t)size), "Truncated file");
			}

		} else {
			uc->data += (size_t)size;
			uc->data_size -= (size_t)size;
		}

		uc->yield_size = ufbxi_min_sz(uc->data_size, uc->progress_interval);
		uc->data_size -= uc->yield_size;
	} else {
		// Read and discard bytes in reasonable chunks
		uint64_t skip_size = ufbxi_max64(uc->read_buffer_size, uc->opts.read_buffer_size);
		while (size > 0) {
			uint64_t to_skip = ufbxi_min64(size, skip_size);
			ufbxi_check(ufbxi_read_bytes(uc, (size_t)to_skip));
			size -= to_skip;
		}
	}

	return 1;
}

static int ufbxi_read_to(ufbxi_context *uc, void *dst, size_t size)
{
	char *ptr = (char*)dst;

	uc->data_size += uc->yield_size;
	uc->yield_size = 0;

	// Copy data from the current buffer first
	size_t len = ufbxi_min_sz(uc->data_size, size);
	memcpy(ptr, uc->data, len);
	uc->data += len;
	uc->data_size -= len;
	ptr += len;
	size -= len;

	// If there's data left to copy try to read from user IO
	// TODO: Progress reporting here...
	if (size > 0) {
		uc->data_offset += uc->data - uc->data_begin;

		uc->data_begin = uc->data = NULL;
		uc->data_size = 0;
		ufbxi_check(uc->read_fn);
		len = uc->read_fn(uc->read_user, ptr, size);
		ufbxi_check_msg(len != SIZE_MAX, "IO error");
		ufbxi_check(len == size);

		uc->data_offset += size;
	}

	uc->yield_size = ufbxi_min_sz(uc->data_size, uc->progress_interval);
	uc->data_size -= uc->yield_size;

	return 1;
}

// -- File IO

static void ufbxi_init_ator(ufbx_error *error, ufbxi_allocator *ator, const ufbx_allocator *desc)
{
	ufbx_allocator zero_ator;
	if (!desc) {
		memset(&zero_ator, 0, sizeof(zero_ator));
		desc = &zero_ator;
	}

	ator->error = error;
	ator->ator = *desc;
	ator->max_size = desc->memory_limit ? desc->memory_limit : SIZE_MAX;
	ator->max_allocs = desc->allocation_limit ? desc->allocation_limit : SIZE_MAX;
	ator->huge_size = desc->huge_threshold ? desc->huge_threshold : 0x100000;
	ator->chunk_max = desc->max_chunk_size ? desc->max_chunk_size : 0x1000000;
}

static FILE *ufbxi_fopen(const char *path, size_t path_len, ufbxi_allocator *tmp_ator)
{
#if defined(_WIN32)
	wchar_t wpath_buf[256];
	wchar_t *wpath = NULL;

	if (path_len == SIZE_MAX) {
		path_len = strlen(path);
	}
	if (path_len < ufbxi_arraycount(wpath_buf) - 1) {
		wpath = wpath_buf;
	} else {
		wpath = ufbxi_alloc(tmp_ator, wchar_t, path_len + 1);
		if (!wpath) return NULL;
	}

	// Convert UTF-8 to UTF-16 but allow stray surrogate pairs as the Windows
	// file system encoding allows them as well..
	size_t wlen = 0;
	for (size_t i = 0; i < path_len; ) {
		uint32_t code = UINT32_MAX;
		char c = path[i++];
		if ((c & 0x80) == 0) {
			code = (uint32_t)c;
		} else if ((c & 0xe0) == 0xc0) {
			code = (uint32_t)(c & 0x1f);
			if (i < path_len) code = code << 6 | (uint32_t)(path[i++] & 0x3f);
		} else if ((c & 0xf0) == 0xe0) {
			code = (uint32_t)(c & 0x0f);
			if (i < path_len) code = code << 6 | (uint32_t)(path[i++] & 0x3f);
			if (i < path_len) code = code << 6 | (uint32_t)(path[i++] & 0x3f);
		} else if ((c & 0xf8) == 0xf0) {
			code = (uint32_t)(c & 0x07);
			if (i < path_len) code = code << 6 | (uint32_t)(path[i++] & 0x3f);
			if (i < path_len) code = code << 6 | (uint32_t)(path[i++] & 0x3f);
			if (i < path_len) code = code << 6 | (uint32_t)(path[i++] & 0x3f);
		}
		if (code < 0x10000) {
			wpath[wlen++] = (wchar_t)code;
		} else {
			code -= 0x10000;
			wpath[wlen++] = (wchar_t)(0xd800 + (code >> 10));
			wpath[wlen++] = (wchar_t)(0xdc00 + (code & 0x3ff));
		}
	}
	wpath[wlen] = 0;

	FILE *file = NULL;
#if defined(_MSC_VER) && _MSC_VER >= 1400
	if (_wfopen_s(&file, wpath, L"rb") != 0) {
		file = NULL;
	}
#else
	file = _wfopen(wpath, L"rb");
#endif

	if (wpath != wpath_buf) {
		ufbxi_free(tmp_ator, wchar_t, wpath, path_len + 1);
	}

	return file;
#else
	if (path_len == SIZE_MAX) {
		return fopen(path, "rb");
	}

	char copy_buf[256];
	char *copy = NULL;

	if (path_len < ufbxi_arraycount(copy_buf) - 1) {
		copy = copy_buf;
	} else {
		copy = ufbxi_alloc(tmp_ator, char, path_len + 1);
		if (!copy) return NULL;
	}
	memcpy(copy, path, path_len);
	copy[path_len] = '\0';

	FILE *file = fopen(copy, "rb");

	if (copy != copy_buf) {
		ufbxi_free(tmp_ator, char, copy, path_len + 1);
	}

	return file;
#endif
}

static uint64_t ufbxi_ftell(FILE *file)
{
#if defined(_POSIX_VERSION)
	off_t result = ftello(file);
	if (result >= 0) return (uint64_t)result;
#elif defined(_MSC_VER)
	int64_t result = _ftelli64(file);
	if (result >= 0) return (uint64_t)result;
#else
	long result = ftell(file);
	if (result >= 0) return (uint64_t)result;
#endif
	return UINT64_MAX;
}

static size_t ufbxi_file_read(void *user, void *data, size_t max_size)
{
	FILE *file = (FILE*)user;
	if (ferror(file)) return SIZE_MAX;
	return fread(data, 1, max_size, file);
}

static bool ufbxi_file_skip(void *user, size_t size)
{
	FILE *file = (FILE*)user;
	ufbx_assert(size <= UFBXI_MAX_SKIP_SIZE);
	if (fseek(file, (long)size, SEEK_CUR) != 0) return false;
	if (ferror(file)) return false;
	return true;
}

static void ufbxi_file_close(void *user)
{
	FILE *file = (FILE*)user;
	fclose(file);
}

// -- XML

typedef struct ufbxi_xml_tag ufbxi_xml_tag;
typedef struct ufbxi_xml_attrib ufbxi_xml_attrib;
typedef struct ufbxi_xml_document ufbxi_xml_document;

struct ufbxi_xml_attrib {
	ufbx_string name;
	ufbx_string value;
};

struct ufbxi_xml_tag {
	ufbx_string name;
	ufbx_string text;

	ufbxi_xml_attrib *attribs;
	size_t num_attribs;

	ufbxi_xml_tag *children;
	size_t num_children;
};

struct ufbxi_xml_document {
	ufbxi_xml_tag *root;
	ufbxi_buf buf;
};

typedef struct {
	ufbx_error error;

	ufbxi_allocator *ator;

	ufbxi_buf tmp_stack;
	ufbxi_buf result;

	ufbxi_xml_document *doc;

	ufbx_read_fn *read_fn;
	void *read_user;

	char *tok;
	size_t tok_cap;
	size_t tok_len;

	const char *pos, *pos_end;
	char data[4096];

	bool io_error;

	size_t depth;
} ufbxi_xml_context;

enum {
	UFBXI_XML_CTYPE_WHITESPACE = 0x1,
	UFBXI_XML_CTYPE_SINGLE_QUOTE = 0x2,
	UFBXI_XML_CTYPE_DOUBLE_QUOTE = 0x4,
	UFBXI_XML_CTYPE_NAME_END = 0x8,
	UFBXI_XML_CTYPE_TAG_START = 0x10,
	UFBXI_XML_CTYPE_END_OF_FILE = 0x20,
};

// Generated by `misc/gen_xml_ctype.py`
static const uint8_t ufbxi_xml_ctype[256] = {
	32,0,0,0,0,0,0,0,0,9,9,0,0,9,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	9,0,12,0,0,0,0,10,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,16,8,8,8,
};

static ufbxi_noinline void ufbxi_xml_refill(ufbxi_xml_context *xc)
{
	size_t num = xc->read_fn(xc->read_user, xc->data, sizeof(xc->data));
	if (num == SIZE_MAX || num < sizeof(xc->data)) xc->io_error = true;
	if (num < sizeof(xc->data)) {
		xc->data[num++] = '\0';
	}
	xc->pos = xc->data;
	xc->pos_end = xc->data + num;
}

static ufbxi_forceinline void ufbxi_xml_advance(ufbxi_xml_context *xc)
{
	if (++xc->pos == xc->pos_end) ufbxi_xml_refill(xc);
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_xml_push_token_char(ufbxi_xml_context *xc, char c)
{
	if (xc->tok_len == xc->tok_cap) {
		ufbxi_check_err(&xc->error, ufbxi_grow_array(xc->ator, &xc->tok, &xc->tok_cap, xc->tok_len + 1));
	}
	xc->tok[xc->tok_len++] = c;
	return 1;
}

static ufbxi_noinline int ufbxi_xml_accept(ufbxi_xml_context *xc, char ch)
{
	if (*xc->pos == ch) {
		ufbxi_xml_advance(xc);
		return 1;
	} else {
		return 0;
	}
}

static ufbxi_noinline void ufbxi_xml_skip_while(ufbxi_xml_context *xc, uint32_t ctypes)
{
	while (ufbxi_xml_ctype[(uint8_t)*xc->pos] & ctypes) {
		ufbxi_xml_advance(xc);
	}
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_xml_skip_until_string(ufbxi_xml_context *xc, ufbx_string *dst, const char *suffix)
{
	xc->tok_len = 0;
	size_t match_len = 0, ix = 0, suffix_len = strlen(suffix);
	char buf[16] = { 0 };
	size_t wrap_mask = sizeof(buf) - 1;
	ufbx_assert(suffix_len < sizeof(buf));
	for (;;) {
		char c = *xc->pos;
		ufbxi_check_err_msg(&xc->error, c != 0, "Truncated file");
		ufbxi_xml_advance(xc);
		if (ix >= suffix_len) {
			ufbxi_check_err(&xc->error, ufbxi_xml_push_token_char(xc, buf[(ix - suffix_len) & wrap_mask]));
		}

		buf[ix++ & wrap_mask] = c;
		for (match_len = 0; match_len < suffix_len; match_len++) {
			if (buf[(ix - suffix_len + match_len) & wrap_mask] != suffix[match_len]) {
				break;
			}
		}
		if (match_len == suffix_len) break;
	}

	ufbxi_check_err(&xc->error, ufbxi_xml_push_token_char(xc, '\0'));
	if (dst) {
		dst->length = xc->tok_len - 1;
		dst->data = ufbxi_push_copy(&xc->result, char, xc->tok_len, xc->tok);
		ufbxi_check_err(&xc->error, dst->data);
	}

	return 1;
}

static ufbxi_noinline int ufbxi_xml_read_until(ufbxi_xml_context *xc, ufbx_string *dst, uint32_t ctypes)
{
	xc->tok_len = 0;
	for (;;) {
		char c = *xc->pos;

		if (c == '&') {
			size_t entity_begin = xc->tok_len;
			for (;;) {
				ufbxi_xml_advance(xc);
				c = *xc->pos;
				ufbxi_check_err(&xc->error, c != '\0');
				if (c == ';') break;
				ufbxi_check_err(&xc->error, ufbxi_xml_push_token_char(xc, c));
			}
			ufbxi_xml_advance(xc);
			ufbxi_check_err(&xc->error, ufbxi_xml_push_token_char(xc, '\0'));

			char *entity = xc->tok + entity_begin;
			xc->tok_len = entity_begin;

			if (entity[0] == '#') {
				unsigned long code = 0;
				if (entity[1] == 'x') {
					code = strtoul(entity + 2, NULL, 16);
				} else {
					code = strtoul(entity + 1, NULL, 10);
				}

				char bytes[5] = { 0 };
				if (code < 0x80) {
					bytes[0] = (char)code;
				} else if (code < 0x800) {
					bytes[0] = (char)(0xc0 | (code>>6));
					bytes[1] = (char)(0x80 | (code & 0x3f));
				} else if (code < 0x10000) {
					bytes[0] = (char)(0xe0 | (code>>12));
					bytes[1] = (char)(0x80 | ((code>>6) & 0x3f));
					bytes[2] = (char)(0x80 | (code & 0x3f));
				} else {
					bytes[0] = (char)(0xf0 | (code>>18));
					bytes[1] = (char)(0x80 | ((code>>12) & 0x3f));
					bytes[2] = (char)(0x80 | ((code>>6) & 0x3f));
					bytes[3] = (char)(0x80 | (code & 0x3f));
				}
				for (char *b = bytes; *b; b++) {
					ufbxi_check_err(&xc->error, ufbxi_xml_push_token_char(xc, *b));
				}
			} else {
				char ch = '\0';
				if (!strcmp(entity, "lt")) ch = '<';
				else if (!strcmp(entity, "quot")) ch = '"';
				else if (!strcmp(entity, "amp")) ch = '&';
				else if (!strcmp(entity, "apos")) ch = '\'';
				else if (!strcmp(entity, "gt")) ch = '>';
				if (ch) {
					ufbxi_check_err(&xc->error, ufbxi_xml_push_token_char(xc, ch));
				}
			}
		} else {
			if ((ufbxi_xml_ctype[(uint8_t)c] & ctypes) != 0) break;
			ufbxi_check_err_msg(&xc->error, c != 0, "Truncated file");
			ufbxi_check_err(&xc->error, ufbxi_xml_push_token_char(xc, c));
			ufbxi_xml_advance(xc);
		}
	}

	ufbxi_check_err(&xc->error, ufbxi_xml_push_token_char(xc, '\0'));
	if (dst) {
		dst->length = xc->tok_len - 1;
		dst->data = ufbxi_push_copy(&xc->result, char, xc->tok_len, xc->tok);
		ufbxi_check_err(&xc->error, dst->data);
	}

	return 1;
}

static ufbxi_noinline int ufbxi_xml_parse_tag(ufbxi_xml_context *xc, bool *p_closing, const char *opening)
{
	if (!ufbxi_xml_accept(xc, '<')) {
		if (*xc->pos == '\0') {
			*p_closing = true;
		} else {
			ufbxi_check_err(&xc->error, ufbxi_xml_read_until(xc, NULL, UFBXI_XML_CTYPE_TAG_START | UFBXI_XML_CTYPE_END_OF_FILE));
			bool has_text = false;
			for (size_t i = 0; i < xc->tok_len; i++) {
				if ((ufbxi_xml_ctype[(uint8_t)xc->tok[i]] & UFBXI_XML_CTYPE_WHITESPACE) == 0) {
					has_text = true;
					break;
				}
			}

			if (has_text) {
				ufbxi_xml_tag *tag = ufbxi_push_zero(&xc->tmp_stack, ufbxi_xml_tag, 1);
				ufbxi_check_err(&xc->error, tag);
				tag->name.data = ufbxi_empty_char;

				tag->text.length = xc->tok_len - 1;
				tag->text.data = ufbxi_push_copy(&xc->result, char, xc->tok_len, xc->tok);
				ufbxi_check_err(&xc->error, tag->text.data);
			}
		}
		return 1;
	}

	if (ufbxi_xml_accept(xc, '/')) {
		ufbxi_check_err(&xc->error, ufbxi_xml_read_until(xc, NULL, UFBXI_XML_CTYPE_NAME_END));
		ufbxi_check_err(&xc->error, opening && !strcmp(xc->tok, opening));
		ufbxi_xml_skip_while(xc, UFBXI_XML_CTYPE_WHITESPACE);
		if (!ufbxi_xml_accept(xc, '>')) return 0;
		*p_closing = true;
		return 1;
	} else if (ufbxi_xml_accept(xc, '!')) {
		if (ufbxi_xml_accept(xc, '[')) {
			for (const char *ch = "CDATA["; *ch; ch++) {
				if (!ufbxi_xml_accept(xc, *ch)) return 0;
			}

			ufbxi_xml_tag *tag = ufbxi_push_zero(&xc->tmp_stack, ufbxi_xml_tag, 1);
			ufbxi_check_err(&xc->error, tag);
			ufbxi_check_err(&xc->error, ufbxi_xml_skip_until_string(xc, &tag->text, "]]>"));
			tag->name.data = ufbxi_empty_char;

		} else if (ufbxi_xml_accept(xc, '-')) {
			if (!ufbxi_xml_accept(xc, '-')) return 0;
			ufbxi_check_err(&xc->error, ufbxi_xml_skip_until_string(xc, NULL, "-->"));
		} else {
			// TODO: !DOCTYPE
			ufbxi_check_err(&xc->error, ufbxi_xml_skip_until_string(xc, NULL, ">"));
		}
		return 1;
	} else if (ufbxi_xml_accept(xc, '?')) {
		ufbxi_check_err(&xc->error, ufbxi_xml_skip_until_string(xc, NULL, "?>"));
		return 1;
	}

	ufbxi_xml_tag *tag = ufbxi_push_zero(&xc->tmp_stack, ufbxi_xml_tag, 1);
	ufbxi_check_err(&xc->error, tag);
	ufbxi_check_err(&xc->error, ufbxi_xml_read_until(xc, &tag->name, UFBXI_XML_CTYPE_NAME_END));
	tag->text.data = ufbxi_empty_char;

	bool has_children = false;

	size_t num_attribs = 0;
	for (;;) {
		ufbxi_xml_skip_while(xc, UFBXI_XML_CTYPE_WHITESPACE);
		if (ufbxi_xml_accept(xc, '/')) {
			if (!ufbxi_xml_accept(xc, '>')) return 0;
			break;
		} else if (ufbxi_xml_accept(xc, '>')) {
			has_children = true;
			break;
		} else {
			ufbxi_xml_attrib *attrib = ufbxi_push_zero(&xc->tmp_stack, ufbxi_xml_attrib, 1);
			ufbxi_check_err(&xc->error, attrib);
			ufbxi_check_err(&xc->error, ufbxi_xml_read_until(xc, &attrib->name, UFBXI_XML_CTYPE_NAME_END));
			ufbxi_xml_skip_while(xc, UFBXI_XML_CTYPE_WHITESPACE);
			if (!ufbxi_xml_accept(xc, '=')) return 0;
			ufbxi_xml_skip_while(xc, UFBXI_XML_CTYPE_WHITESPACE);
			uint32_t quote_ctype = 0;
			if (ufbxi_xml_accept(xc, '"')) {
				quote_ctype = UFBXI_XML_CTYPE_DOUBLE_QUOTE;
			} else if (ufbxi_xml_accept(xc, '\'')) {
				quote_ctype = UFBXI_XML_CTYPE_SINGLE_QUOTE;
			} else {
				ufbxi_fail_err(&xc->error, "Bad attrib value");
			}
			ufbxi_check_err(&xc->error, ufbxi_xml_read_until(xc, &attrib->value, quote_ctype));
			ufbxi_xml_advance(xc);
			num_attribs++;
		}
	}

	tag->num_attribs = num_attribs;
	tag->attribs = ufbxi_push_pop(&xc->result, &xc->tmp_stack, ufbxi_xml_attrib, num_attribs);
	ufbxi_check_err(&xc->error, tag->attribs);

	if (has_children) {
		size_t children_begin = xc->tmp_stack.num_items;
		for (;;) {
			bool closing = false;
			ufbxi_check_err(&xc->error, ufbxi_xml_parse_tag(xc, &closing, tag->name.data));
			if (closing) break;
		}

		tag->num_children = xc->tmp_stack.num_items - children_begin;
		tag->children = ufbxi_push_pop(&xc->result, &xc->tmp_stack, ufbxi_xml_tag, tag->num_children);
		ufbxi_check_err(&xc->error, tag->children);
	}

	return 1;
}

static ufbxi_noinline int ufbxi_xml_parse_root(ufbxi_xml_context *xc)
{
	ufbxi_xml_tag *tag = ufbxi_push_zero(&xc->result, ufbxi_xml_tag, 1);
	ufbxi_check_err(&xc->error, tag);
	tag->name.data = ufbxi_empty_char;
	tag->text.data = ufbxi_empty_char;

	for (;;) {
		bool closing = false;
		ufbxi_check_err(&xc->error, ufbxi_xml_parse_tag(xc, &closing, NULL));
		if (closing) break;
	}

	tag->num_children = xc->tmp_stack.num_items;
	tag->children = ufbxi_push_pop(&xc->result, &xc->tmp_stack, ufbxi_xml_tag, tag->num_children);
	ufbxi_check_err(&xc->error, tag->children);

	xc->doc = ufbxi_push(&xc->result, ufbxi_xml_document, 1);
	ufbxi_check_err(&xc->error, xc->doc);

	xc->doc->root = tag;
	xc->doc->buf = xc->result;

	return 1;
}

typedef struct {
	ufbxi_allocator *ator;
	ufbx_read_fn *read_fn;
	void *read_user;
	const char *prefix;
	size_t prefix_length;
} ufbxi_xml_load_opts;

static ufbxi_noinline ufbxi_xml_document *ufbxi_load_xml(ufbxi_xml_load_opts *opts, ufbx_error *error)
{
	ufbxi_xml_context xc = { UFBX_ERROR_NONE };
	xc.ator = opts->ator;
	xc.read_fn = opts->read_fn;
	xc.read_user = opts->read_user;

	xc.tmp_stack.ator = xc.ator;
	xc.result.ator = xc.ator;

	xc.result.unordered = true;

	if (opts->prefix_length > 0) {
		xc.pos = opts->prefix;
		xc.pos_end = opts->prefix + opts->prefix_length;
	} else {
		ufbxi_xml_refill(&xc);
	}

	int ok = ufbxi_xml_parse_root(&xc);

	ufbxi_buf_free(&xc.tmp_stack);
	ufbxi_free(xc.ator, char, xc.tok, xc.tok_cap);

	if (ok) {
		return xc.doc;
	} else {
		ufbxi_buf_free(&xc.result);
		if (error) {
			*error = xc.error;
		}

		return NULL;
	}
}

static ufbxi_noinline void ufbxi_free_xml(ufbxi_xml_document *doc)
{
	ufbxi_buf buf = doc->buf;
	ufbxi_buf_free(&buf);
}

static ufbxi_noinline ufbxi_xml_tag *ufbxi_xml_find_child(ufbxi_xml_tag *tag, const char *name)
{
	ufbxi_for(ufbxi_xml_tag, child, tag->children, tag->num_children) {
		if (!strcmp(child->name.data, name)) {
			return child;
		}
	}
	return NULL;
}

static ufbxi_noinline ufbxi_xml_attrib *ufbxi_xml_find_attrib(ufbxi_xml_tag *tag, const char *name)
{
	ufbxi_for(ufbxi_xml_attrib, attrib, tag->attribs, tag->num_attribs) {
		if (!strcmp(attrib->name.data, name)) {
			return attrib;
		}
	}
	return NULL;
}

// -- FBX value type information

static char ufbxi_normalize_array_type(char type) {
	switch (type) {
	case 'r': return sizeof(ufbx_real) == sizeof(float) ? 'f' : 'd';
	case 'c': return 'b';
	default: return type;
	}
}

size_t ufbxi_array_type_size(char type)
{
	switch (type) {
	case 'r': return sizeof(ufbx_real);
	case 'b': return sizeof(bool);
	case 'i': return sizeof(int32_t);
	case 'l': return sizeof(int64_t);
	case 'f': return sizeof(float);
	case 'd': return sizeof(double);
	case 's': return sizeof(ufbx_string);
	default: return 1;
	}
}

// -- Node operations

static ufbxi_noinline ufbxi_node *ufbxi_find_child(ufbxi_node *node, const char *name)
{
	ufbxi_for(ufbxi_node, c, node->children, node->num_children) {
		if (c->name == name) return c;
	}
	return NULL;
}

static ufbxi_forceinline bool ufbxi_check_string(ufbx_string s)
{
	return memchr(s.data, 0, s.length) == NULL;
}

// Retrieve values from nodes with type codes:
// Any: '_' (ignore)
// NUMBER: 'I' int32_t 'L' int64_t 'F' float 'D' double 'R' ufbxi_real 'B' bool 'Z' size_t
// STRING: 'S' ufbx_string 'C' const char* (checked) 's' ufbx_string 'c' const char * (unchecked)

ufbxi_nodiscard ufbxi_forceinline static int ufbxi_get_val_at(ufbxi_node *node, size_t ix, char fmt, void *v)
{
	ufbxi_value_type type = (ufbxi_value_type)((node->value_type_mask >> (ix*2)) & 0x3);
	switch (fmt) {
	case '_': return 1;
	case 'I': if (type == UFBXI_VALUE_NUMBER) { *(int32_t*)v = (int32_t)node->vals[ix].i; return 1; } else return 0;
	case 'L': if (type == UFBXI_VALUE_NUMBER) { *(int64_t*)v = (int64_t)node->vals[ix].i; return 1; } else return 0;
	case 'F': if (type == UFBXI_VALUE_NUMBER) { *(float*)v = (float)node->vals[ix].f; return 1; } else return 0;
	case 'D': if (type == UFBXI_VALUE_NUMBER) { *(double*)v = (double)node->vals[ix].f; return 1; } else return 0;
	case 'R': if (type == UFBXI_VALUE_NUMBER) { *(ufbx_real*)v = (ufbx_real)node->vals[ix].f; return 1; } else return 0;
	case 'B': if (type == UFBXI_VALUE_NUMBER) { *(bool*)v = node->vals[ix].i != 0; return 1; } else return 0;
	case 'Z': if (type == UFBXI_VALUE_NUMBER) { if (node->vals[ix].i < 0) return 0; *(size_t*)v = (size_t)node->vals[ix].i; return 1; } else return 0;
	case 'S': if (type == UFBXI_VALUE_STRING && ufbxi_check_string(node->vals[ix].s)) { *(ufbx_string*)v = node->vals[ix].s; return 1; } else return 0;
	case 'C': if (type == UFBXI_VALUE_STRING && ufbxi_check_string(node->vals[ix].s)) { *(const char**)v = node->vals[ix].s.data; return 1; } else return 0;
	case 's': if (type == UFBXI_VALUE_STRING) { *(ufbx_string*)v = node->vals[ix].s; return 1; } else return 0;
	case 'c': if (type == UFBXI_VALUE_STRING) { *(const char**)v = node->vals[ix].s.data; return 1; } else return 0;
	default:
		ufbx_assert(0 && "Bad format char");
		return 0;
	}
}

ufbxi_nodiscard ufbxi_noinline static ufbxi_value_array *ufbxi_get_array(ufbxi_node *node, char fmt)
{
	if (node->value_type_mask != UFBXI_VALUE_ARRAY) return NULL;
	ufbxi_value_array *array = node->array;
	if (fmt != '?') {
		fmt = ufbxi_normalize_array_type(fmt);
		if (array->type != fmt) return NULL;
	}
	return array;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_get_val1(ufbxi_node *node, const char *fmt, void *v0)
{
	if (!ufbxi_get_val_at(node, 0, fmt[0], v0)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_get_val2(ufbxi_node *node, const char *fmt, void *v0, void *v1)
{
	if (!ufbxi_get_val_at(node, 0, fmt[0], v0)) return 0;
	if (!ufbxi_get_val_at(node, 1, fmt[1], v1)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_get_val3(ufbxi_node *node, const char *fmt, void *v0, void *v1, void *v2)
{
	if (!ufbxi_get_val_at(node, 0, fmt[0], v0)) return 0;
	if (!ufbxi_get_val_at(node, 1, fmt[1], v1)) return 0;
	if (!ufbxi_get_val_at(node, 2, fmt[2], v2)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_get_val4(ufbxi_node *node, const char *fmt, void *v0, void *v1, void *v2, void *v3)
{
	if (!ufbxi_get_val_at(node, 0, fmt[0], v0)) return 0;
	if (!ufbxi_get_val_at(node, 1, fmt[1], v1)) return 0;
	if (!ufbxi_get_val_at(node, 2, fmt[2], v2)) return 0;
	if (!ufbxi_get_val_at(node, 3, fmt[3], v3)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_get_val5(ufbxi_node *node, const char *fmt, void *v0, void *v1, void *v2, void *v3, void *v4)
{
	if (!ufbxi_get_val_at(node, 0, fmt[0], v0)) return 0;
	if (!ufbxi_get_val_at(node, 1, fmt[1], v1)) return 0;
	if (!ufbxi_get_val_at(node, 2, fmt[2], v2)) return 0;
	if (!ufbxi_get_val_at(node, 3, fmt[3], v3)) return 0;
	if (!ufbxi_get_val_at(node, 4, fmt[4], v4)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_find_val1(ufbxi_node *node, const char *name, const char *fmt, void *v0)
{
	ufbxi_node *child = ufbxi_find_child(node, name);
	if (!child) return 0;
	if (!ufbxi_get_val_at(child, 0, fmt[0], v0)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_find_val2(ufbxi_node *node, const char *name, const char *fmt, void *v0, void *v1)
{
	ufbxi_node *child = ufbxi_find_child(node, name);
	if (!child) return 0;
	if (!ufbxi_get_val_at(child, 0, fmt[0], v0)) return 0;
	if (!ufbxi_get_val_at(child, 1, fmt[1], v1)) return 0;
	return 1;
}

ufbxi_nodiscard static ufbxi_noinline ufbxi_value_array *ufbxi_find_array(ufbxi_node *node, const char *name, char fmt)
{
	ufbxi_node *child = ufbxi_find_child(node, name);
	if (!child) return NULL;
	return ufbxi_get_array(child, fmt);
}

static ufbxi_node *ufbxi_find_child_strcmp(ufbxi_node *node, const char *name)
{
	ufbxi_for(ufbxi_node, c, node->children, node->num_children) {
		if (!strcmp(c->name, name)) return c;
	}
	return NULL;
}

// -- Element extra data allocation

ufbxi_nodiscard static ufbxi_noinline void *ufbxi_push_element_extra_size(ufbxi_context *uc, uint32_t id, size_t size)
{
	void *extra = ufbxi_push_size_zero(&uc->tmp, size, 1);
	ufbxi_check_return(extra, NULL);

	if (uc->element_extra_cap <= id) {
		size_t old_cap = uc->element_extra_cap;
		ufbxi_check_return(ufbxi_grow_array(&uc->ator_tmp, &uc->element_extra_arr, &uc->element_extra_cap, id + 1), NULL);
		memset(uc->element_extra_arr + old_cap, 0, (uc->element_extra_cap - old_cap) * sizeof(void*));
	}
	ufbx_assert(uc->element_extra_arr[id] == NULL);
	uc->element_extra_arr[id] = extra;

	return extra;
}

static ufbxi_noinline void *ufbxi_get_element_extra(ufbxi_context *uc, uint32_t id)
{
	if (id < uc->element_extra_cap) {
		return uc->element_extra_arr[id];
	} else {
		return NULL;
	}
}

#define ufbxi_push_element_extra(uc, id, type) (type*)ufbxi_push_element_extra_size((uc), (id), sizeof(type))

// -- Parsing state machine
//
// When reading the file we maintain a coarse representation of the structure so
// that we can resolve array info (type, included in result, etc). Using this info
// we can often read/decompress the contents directly into the right memory area.

typedef enum {
	UFBXI_PARSE_ROOT,
	UFBXI_PARSE_FBX_HEADER_EXTENSION,
	UFBXI_PARSE_DEFINITIONS,
	UFBXI_PARSE_OBJECTS,
	UFBXI_PARSE_TAKES,
	UFBXI_PARSE_FBX_VERSION,
	UFBXI_PARSE_MODEL,
	UFBXI_PARSE_GEOMETRY,
	UFBXI_PARSE_LEGACY_MODEL,
	UFBXI_PARSE_ANIMATION_CURVE,
	UFBXI_PARSE_DEFORMER,
	UFBXI_PARSE_LEGACY_LINK,
	UFBXI_PARSE_POSE,
	UFBXI_PARSE_POSE_NODE,
	UFBXI_PARSE_VIDEO,
	UFBXI_PARSE_LAYERED_TEXTURE,
	UFBXI_PARSE_SELECTION_NODE,
	UFBXI_PARSE_LAYER_ELEMENT_NORMAL,
	UFBXI_PARSE_LAYER_ELEMENT_BINORMAL,
	UFBXI_PARSE_LAYER_ELEMENT_TANGENT,
	UFBXI_PARSE_LAYER_ELEMENT_UV,
	UFBXI_PARSE_LAYER_ELEMENT_COLOR,
	UFBXI_PARSE_LAYER_ELEMENT_VERTEX_CREASE,
	UFBXI_PARSE_LAYER_ELEMENT_EDGE_CREASE,
	UFBXI_PARSE_LAYER_ELEMENT_SMOOTHING,
	UFBXI_PARSE_LAYER_ELEMENT_MATERIAL,
	UFBXI_PARSE_LAYER_ELEMENT_OTHER,
	UFBXI_PARSE_GEOMETRY_UV_INFO,
	UFBXI_PARSE_SHAPE,
	UFBXI_PARSE_TAKE,
	UFBXI_PARSE_TAKE_OBJECT,
	UFBXI_PARSE_CHANNEL,
	UFBXI_PARSE_UNKNOWN,
} ufbxi_parse_state;

typedef struct {
	char type;      // < FBX type code of the array: b,i,l,f,d (or 'r' meaning ufbx_real '-' ignore)
	bool result;    // < Alloacte the array from the result buffer
	bool tmp_buf;   // < Alloacte the array from the global temporary buffer
	bool pad_begin; // < Pad the begin of the array with 4 zero elements to guard from invalid -1 index accesses
} ufbxi_array_info;

static ufbxi_parse_state ufbxi_update_parse_state(ufbxi_parse_state parent, const char *name)
{
	switch (parent) {

	case UFBXI_PARSE_ROOT:
		if (name == ufbxi_FBXHeaderExtension) return UFBXI_PARSE_FBX_HEADER_EXTENSION;
		if (name == ufbxi_Definitions) return UFBXI_PARSE_DEFINITIONS;
		if (name == ufbxi_Objects) return UFBXI_PARSE_OBJECTS;
		if (name == ufbxi_Takes) return UFBXI_PARSE_TAKES;
		if (name == ufbxi_Model) return UFBXI_PARSE_LEGACY_MODEL;
		break;

	case UFBXI_PARSE_FBX_HEADER_EXTENSION:
		if (name == ufbxi_FBXVersion) return UFBXI_PARSE_FBX_VERSION;
		break;

	case UFBXI_PARSE_OBJECTS:
		if (name == ufbxi_Model) return UFBXI_PARSE_MODEL;
		if (name == ufbxi_Geometry) return UFBXI_PARSE_GEOMETRY;
		if (name == ufbxi_AnimationCurve) return UFBXI_PARSE_ANIMATION_CURVE;
		if (name == ufbxi_Deformer) return UFBXI_PARSE_DEFORMER;
		if (name == ufbxi_Pose) return UFBXI_PARSE_POSE;
		if (name == ufbxi_Video) return UFBXI_PARSE_VIDEO;
		if (name == ufbxi_LayeredTexture) return UFBXI_PARSE_LAYERED_TEXTURE;
		if (name == ufbxi_SelectionNode) return UFBXI_PARSE_SELECTION_NODE;
		break;

	case UFBXI_PARSE_MODEL:
	case UFBXI_PARSE_GEOMETRY:
		if (name == ufbxi_LayerElementNormal) return UFBXI_PARSE_LAYER_ELEMENT_NORMAL;
		if (name == ufbxi_LayerElementBinormal) return UFBXI_PARSE_LAYER_ELEMENT_BINORMAL;
		if (name == ufbxi_LayerElementTangent) return UFBXI_PARSE_LAYER_ELEMENT_TANGENT;
		if (name == ufbxi_LayerElementUV) return UFBXI_PARSE_LAYER_ELEMENT_UV;
		if (name == ufbxi_LayerElementColor) return UFBXI_PARSE_LAYER_ELEMENT_COLOR;
		if (name == ufbxi_LayerElementVertexCrease) return UFBXI_PARSE_LAYER_ELEMENT_VERTEX_CREASE;
		if (name == ufbxi_LayerElementEdgeCrease) return UFBXI_PARSE_LAYER_ELEMENT_EDGE_CREASE;
		if (name == ufbxi_LayerElementSmoothing) return UFBXI_PARSE_LAYER_ELEMENT_SMOOTHING;
		if (name == ufbxi_LayerElementMaterial) return UFBXI_PARSE_LAYER_ELEMENT_MATERIAL;
		if (!strncmp(name, "LayerElement", 12)) return UFBXI_PARSE_LAYER_ELEMENT_OTHER;
		if (name == ufbxi_Shape) return UFBXI_PARSE_SHAPE;
		break;

	case UFBXI_PARSE_LEGACY_MODEL:
		if (name == ufbxi_GeometryUVInfo) return UFBXI_PARSE_GEOMETRY_UV_INFO;
		if (name == ufbxi_Link) return UFBXI_PARSE_LEGACY_LINK;
		if (name == ufbxi_Channel) return UFBXI_PARSE_CHANNEL;
		if (name == ufbxi_Shape) return UFBXI_PARSE_SHAPE;
		break;

	case UFBXI_PARSE_POSE:
		if (name == ufbxi_PoseNode) return UFBXI_PARSE_POSE_NODE;
		break;

	case UFBXI_PARSE_TAKES:
		if (name == ufbxi_Take) return UFBXI_PARSE_TAKE;
		break;

	case UFBXI_PARSE_TAKE:
		return UFBXI_PARSE_TAKE_OBJECT;

	case UFBXI_PARSE_TAKE_OBJECT:
		if (name == ufbxi_Channel) return UFBXI_PARSE_CHANNEL;
		break;

	case UFBXI_PARSE_CHANNEL:
		if (name == ufbxi_Channel) return UFBXI_PARSE_CHANNEL;
		break;

	default:
		break;

	}

	return UFBXI_PARSE_UNKNOWN;
}

static bool ufbxi_is_array_node(ufbxi_context *uc, ufbxi_parse_state parent, const char *name, ufbxi_array_info *info)
{
	info->result = false;
	info->tmp_buf = false;
	info->pad_begin = false;
	switch (parent) {

	case UFBXI_PARSE_GEOMETRY:
	case UFBXI_PARSE_MODEL:
		if (name == ufbxi_Vertices && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_PolygonVertexIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_Edges && !uc->opts.ignore_geometry) {
			info->type = 'i';
			return true;
		} else if (name == ufbxi_Indexes && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_Points && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		} else if (name == ufbxi_KnotVector && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		} else if (name == ufbxi_KnotVectorU && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		} else if (name == ufbxi_KnotVectorV && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		} else if (name == ufbxi_PointsIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LEGACY_MODEL:
		if (name == ufbxi_Vertices && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_Normals && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_Materials && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_PolygonVertexIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_Children) {
			info->type = 's';
			return true;
		}
		break;

	case UFBXI_PARSE_ANIMATION_CURVE:
		if (name == ufbxi_KeyTime && !uc->opts.ignore_animation) {
			info->type = 'l';
			return true;
		} else if (name == ufbxi_KeyValueFloat && !uc->opts.ignore_animation) {
			info->type = 'r';
			return true;
		} else if (name == ufbxi_KeyAttrFlags && !uc->opts.ignore_animation) {
			info->type = 'i';
			return true;
		} else if (name == ufbxi_KeyAttrDataFloat && !uc->opts.ignore_animation) {
			// The float data in a keyframe attribute array is represented as integers
			// in versions >= 7200 as some of the elements aren't actually floats (!)
			info->type = uc->from_ascii && uc->version >= 7200 ? 'i' : 'f';
			return true;
		} else if (name == ufbxi_KeyAttrRefCount && !uc->opts.ignore_animation) {
			info->type = 'i';
			return true;
		}
		break;

	case UFBXI_PARSE_VIDEO:
		if (name == ufbxi_Content && uc->opts.ignore_embedded) {
			info->type = '-';
			return true;
		}
		break;


	case UFBXI_PARSE_LAYERED_TEXTURE:
		if (name == ufbxi_BlendModes) {
			info->type = 'i';
			info->tmp_buf = true;
			return true;
		} else if (name == ufbxi_Alphas) {
			info->type = 'r';
			info->tmp_buf = true;
			return true;
		}
		break;

	case UFBXI_PARSE_SELECTION_NODE:
		if (name == ufbxi_VertexIndexArray) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_EdgeIndexArray) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_PolygonIndexArray) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_NORMAL:
		if (name == ufbxi_Normals && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_NormalsIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_BINORMAL:
		if (name == ufbxi_Binormals && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_BinormalsIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_TANGENT:
		if (name == ufbxi_Tangents && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_TangentsIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_UV:
		if (name == ufbxi_UV && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_UVIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_COLOR:
		if (name == ufbxi_Colors && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_ColorIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_VERTEX_CREASE:
		if (name == ufbxi_VertexCrease && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_VertexCreaseIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_EDGE_CREASE:
		if (name == ufbxi_EdgeCrease && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_SMOOTHING:
		if (name == ufbxi_Smoothing && !uc->opts.ignore_geometry) {
			info->type = 'b';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_MATERIAL:
		if (name == ufbxi_Materials && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LAYER_ELEMENT_OTHER:
		if (name == ufbxi_TextureId && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->tmp_buf = true;
			return true;
		}
		break;

	case UFBXI_PARSE_GEOMETRY_UV_INFO:
		if (name == ufbxi_TextureUV && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		} else if (name == ufbxi_TextureUVVerticeIndex && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			info->pad_begin = true;
			return true;
		}
		break;

	case UFBXI_PARSE_SHAPE:
		if (name == ufbxi_Indexes && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		}
		if (name == ufbxi_Vertices && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			info->pad_begin = true;
			return true;
		}
		break;

	case UFBXI_PARSE_DEFORMER:
		if (name == ufbxi_Transform) {
			info->type = 'r';
			info->result = false;
			return true;
		} else if (name == ufbxi_TransformLink) {
			info->type = 'r';
			info->result = false;
			return true;
		} else if (name == ufbxi_Indexes && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_Weights && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		} else if (name == ufbxi_BlendWeights && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		} else if (name == ufbxi_FullWeights) {
			// Ignore blend shape FullWeights as it's used in Blender for vertex groups
			// which we don't currently handle. https://developer.blender.org/T90382
			// TODO: Should we present this to users anyway somehow?
			if (!uc->opts.disable_quirks && uc->exporter == UFBX_EXPORTER_BLENDER_BINARY) {
				return false;
			}
			info->type = 'd';
			info->tmp_buf = true;
			return true;
		}
		break;

	case UFBXI_PARSE_LEGACY_LINK:
		if (name == ufbxi_Transform) {
			info->type = 'r';
			info->result = false;
			return true;
		} else if (name == ufbxi_TransformLink) {
			info->type = 'r';
			info->result = false;
			return true;
		} else if (name == ufbxi_Indexes && !uc->opts.ignore_geometry) {
			info->type = 'i';
			info->result = true;
			return true;
		} else if (name == ufbxi_Weights && !uc->opts.ignore_geometry) {
			info->type = 'r';
			info->result = true;
			return true;
		}
		break;

	case UFBXI_PARSE_POSE_NODE:
		if (name == ufbxi_Matrix) {
			info->type = 'r';
			info->result = false;
			return true;
		}
		break;

	case UFBXI_PARSE_CHANNEL:
		if (name == ufbxi_Key && !uc->opts.ignore_animation) {
			info->type = 'd';
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}

// -- Binary parsing

ufbxi_nodiscard static ufbxi_noinline char *ufbxi_swap_endian(ufbxi_context *uc, const void *src, size_t count, size_t elem_size)
{
	size_t total_size = count * elem_size;
	ufbxi_check_return(!ufbxi_does_overflow(total_size, count, elem_size), NULL);
	if (uc->swap_arr_size < total_size) {
		ufbxi_check_return(ufbxi_grow_array(&uc->ator_tmp, &uc->swap_arr, &uc->swap_arr_size, total_size), NULL);
	}
	char *dst = uc->swap_arr, *d = dst;

	const char *s = (const char*)src;
	switch (elem_size) {
	case 1:
		for (size_t i = 0; i < count; i++) {
			d[0] = s[0];
			d += 1; s += 1;
		}
		break;
	case 2:
		for (size_t i = 0; i < count; i++) {
			d[0] = s[1]; d[1] = s[0];
			d += 2; s += 2;
		}
		break;
	case 4:
		for (size_t i = 0; i < count; i++) {
			d[0] = s[3]; d[1] = s[2]; d[2] = s[1]; d[3] = s[0];
			d += 4; s += 4;
		}
		break;
	case 8:
		for (size_t i = 0; i < count; i++) {
			d[0] = s[7]; d[1] = s[6]; d[2] = s[5]; d[3] = s[4];
			d[4] = s[3]; d[5] = s[2]; d[6] = s[1]; d[7] = s[0];
			d += 8; s += 8;
		}
		break;
	default:
		ufbx_assert(0 && "Bad endian swap size");
	}

	return dst;
}

// Swap the endianness of an array typed with a lowercase letter
ufbxi_nodiscard static ufbxi_noinline const char *ufbxi_swap_endian_array(ufbxi_context *uc, const void *src, size_t count, char type)
{
	switch (type) {
	case 'i': case 'f': return ufbxi_swap_endian(uc, src, count, 4); break;
	case 'l': case 'd': return ufbxi_swap_endian(uc, src, count, 8); break;
	default: return (const char*)src;
	}
}

// Swap the endianness of a single value (shallow, swaps string/array header words)
ufbxi_nodiscard static ufbxi_noinline const char *ufbxi_swap_endian_value(ufbxi_context *uc, const void *src, char type)
{
	switch (type) {
	case 'Y': return ufbxi_swap_endian(uc, src, 1, 2); break;
	case 'I': case 'F': return ufbxi_swap_endian(uc, src, 1, 4); break;
	case 'L': case 'D': return ufbxi_swap_endian(uc, src, 1, 8); break;
	case 'S': case 'R': return ufbxi_swap_endian(uc, src, 1, 4); break;
	case 'i': case 'l': case 'f': case 'd': case 'b': return ufbxi_swap_endian(uc, src, 3, 4); break;
	default: return (const char*)src;
	}
}

// Read and convert a post-7000 FBX data array into a different format. `src_type` may be equal to `dst_type`
// if the platform is not binary compatible with the FBX data representation.
ufbxi_nodiscard static ufbxi_noinline int ufbxi_binary_convert_array(ufbxi_context *uc, char src_type, char dst_type, const void *src, void *dst, size_t size)
{
	if (uc->file_big_endian) {
		src = ufbxi_swap_endian_array(uc, src, size, src_type);
		ufbxi_check(src);
	}

	switch (dst_type)
	{
	#define ufbxi_convert_loop(m_dst, m_size, m_expr) { \
		const char *val = (const char*)src, *val_end = val + size*m_size; \
		m_dst *d = (m_dst*)dst; \
		while (val != val_end) { *d++ = (m_dst)(m_expr); val += m_size; } }

	#define ufbxi_convert_switch(m_dst) \
		switch (src_type) { \
		case 'b': ufbxi_convert_loop(m_dst, 1, *val != 0); break; \
		case 'i': ufbxi_convert_loop(m_dst, 4, ufbxi_read_i32(val)); break; \
		case 'l': ufbxi_convert_loop(m_dst, 8, ufbxi_read_i64(val)); break; \
		case 'f': ufbxi_convert_loop(m_dst, 4, ufbxi_read_f32(val)); break; \
		case 'd': ufbxi_convert_loop(m_dst, 8, ufbxi_read_f64(val)); break; \
		default: ufbxi_fail("Bad array source type"); \
		} \
		break; \

	case 'b':
		switch (src_type) {
		case 'b': ufbxi_convert_loop(char, 1, *val != 0); break;
		case 'i': ufbxi_convert_loop(char, 4, ufbxi_read_i32(val) != 0); break;
		case 'l': ufbxi_convert_loop(char, 8, ufbxi_read_i64(val) != 0); break;
		case 'f': ufbxi_convert_loop(char, 4, ufbxi_read_f32(val) != 0); break;
		case 'd': ufbxi_convert_loop(char, 8, ufbxi_read_f64(val) != 0); break;
		default: ufbxi_fail("Bad array source type");
		}
		break;

	case 'i': ufbxi_convert_switch(int32_t); break;
	case 'l': ufbxi_convert_switch(int64_t); break;
	case 'f': ufbxi_convert_switch(float); break;
	case 'd': ufbxi_convert_switch(double); break;

	default: return 0;

	}

	return 1;
}

// Read pre-7000 separate properties as an array.
ufbxi_nodiscard static ufbxi_noinline int ufbxi_binary_parse_multivalue_array(ufbxi_context *uc, char dst_type, void *dst, size_t size)
{
	if (size == 0) return 1;
	const char *val;
	size_t val_size;

	bool file_big_endian = uc->file_big_endian;

	#define ufbxi_convert_parse_fast(m_dst, m_type, m_expr) { \
		m_dst *d = (m_dst*)dst; \
		for (; base < size; base++) { \
			val = ufbxi_peek_bytes(uc, 13); \
			ufbxi_check(val); \
			if (*val != m_type) break; \
			val++; \
			*d++ = (m_dst)(m_expr); \
			ufbxi_consume_bytes(uc, 1 + sizeof(m_dst)); \
		} \
	}

	// String array special case
	if (dst_type == 's') {
		ufbx_string *d = (ufbx_string*)dst;
		for (size_t i = 0; i < size; i++) {
			val = ufbxi_peek_bytes(uc, 13);
			ufbxi_check(val);
			char type = *val++;
			ufbxi_check(type == 'S' || type == 'R');
			if (file_big_endian) {
				val = ufbxi_swap_endian_value(uc, val, type);
				ufbxi_check(val);
			}
			size_t len = ufbxi_read_u32(val);
			ufbxi_consume_bytes(uc, 5);
			d->data = ufbxi_read_bytes(uc, len);
			d->length = len;
			ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, d));
			d++;
		}
		return 1;
	}

	// Optimize a couple of common cases
	size_t base = 0;
	if (!file_big_endian) {
		switch (dst_type) {
		case 'i': ufbxi_convert_parse_fast(int32_t, 'I', ufbxi_read_i32(val)); break;
		case 'l': ufbxi_convert_parse_fast(int64_t, 'L', ufbxi_read_i64(val)); break;
		case 'f': ufbxi_convert_parse_fast(float, 'F', ufbxi_read_f32(val)); break;
		case 'd': ufbxi_convert_parse_fast(double, 'D', ufbxi_read_f64(val)); break;
		}

		// Early return if we handled everything
		if (base == size) return 1;
	}

	switch (dst_type)
	{

	#define ufbxi_convert_parse(m_dst, m_size, m_expr) \
		*d++ = (m_dst)(m_expr); val_size = m_size + 1; \

	#define ufbxi_convert_parse_switch(m_dst) { \
		m_dst *d = (m_dst*)dst + base; \
		for (size_t i = base; i < size; i++) { \
			val = ufbxi_peek_bytes(uc, 13); \
			ufbxi_check(val); \
			char type = *val++; \
			if (file_big_endian) { \
				val = ufbxi_swap_endian_value(uc, val, type); \
				ufbxi_check(val); \
			} \
			switch (type) { \
				case 'C': \
				case 'B': ufbxi_convert_parse(m_dst, 1, *val); break; \
				case 'Y': ufbxi_convert_parse(m_dst, 2, ufbxi_read_i16(val)); break; \
				case 'I': ufbxi_convert_parse(m_dst, 4, ufbxi_read_i32(val)); break; \
				case 'L': ufbxi_convert_parse(m_dst, 8, ufbxi_read_i64(val)); break; \
				case 'F': ufbxi_convert_parse(m_dst, 4, ufbxi_read_f32(val)); break; \
				case 'D': ufbxi_convert_parse(m_dst, 8, ufbxi_read_f64(val)); break; \
				default: ufbxi_fail("Bad multivalue array type"); \
			} \
			ufbxi_consume_bytes(uc, val_size); \
		} \
	} \

	case 'b':
	{
		char *d = (char*)dst;
		for (size_t i = base; i < size; i++) {
			val = ufbxi_peek_bytes(uc, 13);
			ufbxi_check(val);
			char type = *val++; \
			if (file_big_endian) { \
				val = ufbxi_swap_endian_value(uc, val, type); \
				ufbxi_check(val); \
			} \
			switch (type) {
				case 'C':
				case 'B': ufbxi_convert_parse(char, 1, *val != 0); break;
				case 'Y': ufbxi_convert_parse(char, 2, ufbxi_read_i16(val) != 0); break;
				case 'I': ufbxi_convert_parse(char, 4, ufbxi_read_i32(val) != 0); break;
				case 'L': ufbxi_convert_parse(char, 8, ufbxi_read_i64(val) != 0); break;
				case 'F': ufbxi_convert_parse(char, 4, ufbxi_read_f32(val) != 0); break;
				case 'D': ufbxi_convert_parse(char, 8, ufbxi_read_f64(val) != 0); break;
				default: ufbxi_fail("Bad multivalue array type");
			}
			ufbxi_consume_bytes(uc, val_size);
		}
	}
	break;

	case 'i': ufbxi_convert_parse_switch(int32_t); break;
	case 'l': ufbxi_convert_parse_switch(int64_t); break;
	case 'f': ufbxi_convert_parse_switch(float); break;
	case 'd': ufbxi_convert_parse_switch(double); break;

	default: return 0;

	}

	return 1;
}

ufbxi_nodiscard static void *ufbxi_push_array_data(ufbxi_context *uc, const ufbxi_array_info *info, size_t size, ufbxi_buf *tmp_buf)
{
	char type = ufbxi_normalize_array_type(info->type);
	size_t elem_size = ufbxi_array_type_size(type);
	if (info->pad_begin) size += 4;

	// The array may be pushed either to the result or temporary buffer depending
	// if it's already in the right format
	ufbxi_buf *arr_buf = tmp_buf;
	if (info->result) arr_buf = &uc->result;
	else if (info->tmp_buf) arr_buf = &uc->tmp;
	char *data = (char*)ufbxi_push_size(arr_buf, elem_size, size);
	ufbxi_check_return(data, NULL);

	if (info->pad_begin) {
		memset(data, 0, elem_size * 4);
		data += elem_size * 4;
	}

	return data;
}

ufbxi_nodiscard static int ufbxi_binary_parse_node(ufbxi_context *uc, uint32_t depth, ufbxi_parse_state parent_state, bool *p_end, ufbxi_buf *tmp_buf, bool recursive)
{
	// https://code.blender.org/2013/08/fbx-binary-file-format-specification
	// Parse an FBX document node in the binary format
	ufbxi_check(depth < UFBXI_MAX_NODE_DEPTH);

	// Parse the node header, post-7500 versions use 64-bit values for most
	// header fields. 
	uint64_t end_offset, num_values64, values_len;
	uint8_t name_len;
	size_t header_size = (uc->version >= 7500) ? 25 : 13;
	const char *header = ufbxi_read_bytes(uc, header_size), *header_words = header;
	ufbxi_check(header);
	if (uc->version >= 7500) {
		if (uc->file_big_endian) {
			header_words = ufbxi_swap_endian(uc, header_words, 3, 8);
			ufbxi_check(header_words);
		}
		end_offset = ufbxi_read_u64(header_words + 0);
		num_values64 = ufbxi_read_u64(header_words + 8);
		values_len = ufbxi_read_u64(header_words + 16);
		name_len = ufbxi_read_u8(header + 24);
	} else {
		if (uc->file_big_endian) {
			header_words = ufbxi_swap_endian(uc, header_words, 3, 4);
			ufbxi_check(header_words);
		}
		end_offset = ufbxi_read_u32(header_words + 0);
		num_values64 = ufbxi_read_u32(header_words + 4);
		values_len = ufbxi_read_u32(header_words + 8);
		name_len = ufbxi_read_u8(header + 12);
	}

	ufbxi_check(num_values64 <= UINT32_MAX);
	uint32_t num_values = (uint32_t)num_values64;

	// If `end_offset` and `name_len` is zero we treat as the node as a NULL-sentinel
	// that terminates a node list.
	if (end_offset == 0 && name_len == 0) {
		*p_end = true;
		return 1;
	}

	// Update estimated end offset if possible
	if (end_offset > uc->progress_bytes_total) {
		uc->progress_bytes_total = end_offset;
	}

	// Push the parsed node into the `tmp_stack` buffer, the nodes will be popped by
	// calling code after its done parsing all of it's children.
	ufbxi_node *node = ufbxi_push_zero(&uc->tmp_stack, ufbxi_node, 1);
	ufbxi_check(node);

	// Parse and intern the name to the string pool.
	const char *name = ufbxi_read_bytes(uc, name_len);
	ufbxi_check(name);
	name = ufbxi_push_string(&uc->string_pool, name, name_len);
	ufbxi_check(name);
	node->name_len = name_len;
	node->name = name;

	uint64_t values_end_offset = ufbxi_get_read_offset(uc) + values_len;

	// Check if the values of the node we're parsing currently should be
	// treated as an array.
	ufbxi_array_info arr_info;
	if (ufbxi_is_array_node(uc, parent_state, name, &arr_info)) {

		// Normalize the array type (eg. 'r' to 'f'/'d' depending on the build)
		// and get the per-element size of the array.
		char dst_type = ufbxi_normalize_array_type(arr_info.type);

		ufbxi_value_array *arr = ufbxi_push(tmp_buf, ufbxi_value_array, 1);
		ufbxi_check(arr);

		node->value_type_mask = UFBXI_VALUE_ARRAY;
		node->array = arr;
		arr->type = dst_type;

		// Peek the first bytes of the array. We can always look at least 13 bytes
		// ahead safely as valid FBX files must end in a 13/25 byte NULL record.
		const char *data = ufbxi_peek_bytes(uc, 13);
		ufbxi_check(data);

		// Check if the data type is one of the explicit array types (post-7000).
		// Otherwise we form the array by concatenating all the normal values of the
		// node (pre-7000)
		char c = data[0];

		// HACK: Override the "type" if either the array is empty or we want to
		// specifically ignore the contents.
		if (num_values == 0) c = '0';
		if (dst_type == '-') c = '-';

		if (c=='c' || c=='b' || c=='i' || c=='l' || c =='f' || c=='d') {

			const char *arr_words = data + 1;
			if (uc->file_big_endian) {
				arr_words = ufbxi_swap_endian(uc, arr_words, 3, 4);
				ufbxi_check(arr_words);
			}

			// Parse the array header from the prefix we already peeked above.
			char src_type = data[0];
			uint32_t size = ufbxi_read_u32(arr_words + 0); 
			uint32_t encoding = ufbxi_read_u32(arr_words + 4); 
			uint32_t encoded_size = ufbxi_read_u32(arr_words + 8); 
			ufbxi_consume_bytes(uc, 13);

			// Normalize the source type as well, but don't convert UFBX-specific
			// 'r' to 'f'/'d', but fail later instead.
			if (src_type != 'r') src_type = ufbxi_normalize_array_type(src_type);
			size_t src_elem_size = ufbxi_array_type_size(src_type);
			size_t decoded_data_size = src_elem_size * size;

			// Allocate `size` elements for the array.
			char *arr_data = (char*)ufbxi_push_array_data(uc, &arr_info, size, tmp_buf);
			ufbxi_check(arr_data);

			// If the source and destination types are equal and our build is binary-compatible
			// with the FBX format we can read the decoded data directly into the array buffer.
			// Otherwise we need a temporary buffer to decode the array into before conversion.
			void *decoded_data = arr_data;
			if (src_type != dst_type || uc->local_big_endian != uc->file_big_endian) {
				ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, decoded_data_size));
				decoded_data = uc->tmp_arr;
			}

			uint64_t arr_begin = ufbxi_get_read_offset(uc);
			ufbxi_check(UINT64_MAX - encoded_size > arr_begin);
			uint64_t arr_end = arr_begin + encoded_size;
			if (arr_end > uc->progress_bytes_total) {
				uc->progress_bytes_total = arr_end;
			}

			if (encoding == 0) {
				// Encoding 0: Plain binary data.
				ufbxi_check(encoded_size == decoded_data_size);

				// If the array is contained in the current read buffer and we need to convert
				// the data anyway we can use the read buffer as the decoded array source, otherwise
				// do a plain byte copy to the array/conversion buffer.
				if (uc->yield_size + uc->data_size >= encoded_size && decoded_data != arr_data) {
					// Yield right after this if we crossed the yield threshold
					if (encoded_size > uc->yield_size) {
						uc->data_size += uc->yield_size;
						uc->yield_size = encoded_size;
						uc->data_size -= uc->yield_size;
					}

					decoded_data = (void*)uc->data;
					ufbxi_consume_bytes(uc, encoded_size);
				} else {
					ufbxi_check(ufbxi_read_to(uc, decoded_data, encoded_size));
				}
			} else if (encoding == 1) {
				// Encoding 1: DEFLATE

				uc->data_size += uc->yield_size;
				uc->yield_size = 0;

				// Inflate the data from the user-provided IO buffer / read callbacks
				ufbx_inflate_input input;
				input.total_size = encoded_size;
				input.data = uc->data;
				input.data_size = uc->data_size;

				if (uc->opts.progress_fn) {
					input.progress_fn = uc->opts.progress_fn;
					input.progress_user = uc->opts.progress_user;
					input.progress_size_before = arr_begin;
					input.progress_size_after = uc->progress_bytes_total - arr_end;
					input.progress_interval_hint = uc->progress_interval;
				} else {
					input.progress_fn = NULL;
					input.progress_user = NULL;
					input.progress_size_before = 0;
					input.progress_size_after = 0;
					input.progress_interval_hint = 0;
				}

				// If the encoded array is larger than the data we have currently buffered
				// we need to allow `ufbxi_inflate()` to read from the IO callback. We can
				// let `ufbxi_inflate()` freely clobber our `read_buffer` as all the data
				// in the buffer will be consumed. `ufbxi_inflate()` always reads exactly
				// the amount of bytes needed so we can continue reading from `read_fn` as
				// usual (given that we clear the `uc->data/_size` buffer below).
				// NOTE: We _cannot_ share `read_buffer` if we plan to read later from it
				// as `ufbxi_inflate()` overwrites parts of it with zeroes.
				if (encoded_size > input.data_size) {
					input.buffer = uc->read_buffer;
					input.buffer_size = uc->read_buffer_size;
					input.read_fn = uc->read_fn;
					input.read_user = uc->read_user;
					uc->data_offset += encoded_size - input.data_size;
					uc->data += input.data_size;
					uc->data_size = 0;
				} else {
					input.buffer = NULL;
					input.buffer_size = 0;
					input.read_fn = NULL;
					input.read_user = 0;
					uc->data += encoded_size;
					uc->data_size -= encoded_size;
					uc->yield_size = ufbxi_min_sz(uc->data_size, uc->progress_interval);
					uc->data_size -= uc->yield_size;
				}

				ptrdiff_t res = ufbx_inflate(decoded_data, decoded_data_size, &input, uc->inflate_retain);
				ufbxi_check_msg(res != -28, "Cancelled");
				ufbxi_check_msg(res == (ptrdiff_t)decoded_data_size, "Bad DEFLATE data");

			} else {
				ufbxi_fail("Bad array encoding");
			}

			// Convert the decoded array if necessary. If we didn't perform conversion but use the
			// "bool" type we need to normalize the array contents afterwards.
			if (decoded_data != arr_data) {
				ufbxi_check(ufbxi_binary_convert_array(uc, src_type, dst_type, decoded_data, arr_data, size));
			} else if (dst_type == 'b') {
				ufbxi_for(char, b, (char*)arr_data, size) {
					*b = (char)(b != 0);
				}
			}

			arr->data = arr_data;
			arr->size = size;

		} else if (c == '0' || c == '-') {
			// Ignore the array
			arr->type = c == '-' ? '-' : dst_type;
			arr->data = (char*)ufbxi_zero_size_buffer + 32;
			arr->size = 0;
		} else {
			// Allocate `num_values` elements for the array and parse single values into it.
			char *arr_data = (char*)ufbxi_push_array_data(uc, &arr_info, num_values, tmp_buf);
			ufbxi_check(arr_data);
			ufbxi_check(ufbxi_binary_parse_multivalue_array(uc, dst_type, arr_data, num_values));
			arr->data = arr_data;
			arr->size = num_values;
		}

	} else {
		// Parse up to UFBXI_MAX_NON_ARRAY_VALUES as plain values
		num_values = ufbxi_min32(num_values, UFBXI_MAX_NON_ARRAY_VALUES);
		ufbxi_value *vals = ufbxi_push(tmp_buf, ufbxi_value, num_values);
		ufbxi_check(vals);
		node->vals = vals;

		uint32_t type_mask = 0;
		for (size_t i = 0; i < (size_t)num_values; i++) {
			// The file must end in a 13/25 byte NULL record, so we can peek
			// up to 13 bytes safely here.
			const char *data = ufbxi_peek_bytes(uc, 13);
			ufbxi_check(data);

			const char *value = data + 1;

			char type = data[0];
			if (uc->file_big_endian) {
				value = ufbxi_swap_endian_value(uc, value, type);
				ufbxi_check(value);
			}

			switch (type) {

			case 'C': case 'B':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].f = (double)(vals[i].i = (int64_t)value[0]);
				ufbxi_consume_bytes(uc, 2);
				break;

			case 'Y':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].f = (double)(vals[i].i = ufbxi_read_i16(value));
				ufbxi_consume_bytes(uc, 3);
				break;

			case 'I':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].f = (double)(vals[i].i = ufbxi_read_i32(value));
				ufbxi_consume_bytes(uc, 5);
				break;

			case 'L':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].f = (double)(vals[i].i = ufbxi_read_i64(value));
				ufbxi_consume_bytes(uc, 9);
				break;

			case 'F':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].i = (int64_t)(vals[i].f = ufbxi_read_f32(value));
				ufbxi_consume_bytes(uc, 5);
				break;

			case 'D':
				type_mask |= UFBXI_VALUE_NUMBER << (i*2);
				vals[i].i = (int64_t)(vals[i].f = ufbxi_read_f64(value));
				ufbxi_consume_bytes(uc, 9);
				break;

			case 'S': case 'R':
			{
				size_t len = ufbxi_read_u32(value);
				ufbxi_consume_bytes(uc, 5);
				vals[i].s.data = ufbxi_read_bytes(uc, len);
				vals[i].s.length = len;
				ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, &vals[i].s));
				type_mask |= UFBXI_VALUE_STRING << (i*2);
			}
			break;

			// Treat arrays as non-values and skip them
			case 'c': case 'b': case 'i': case 'l': case 'f': case 'd':
			{
				uint32_t encoded_size = ufbxi_read_u32(value + 8);
				ufbxi_consume_bytes(uc, 13);
				ufbxi_check(ufbxi_skip_bytes(uc, encoded_size));
			}
			break;

			default:
				ufbxi_fail("Bad value type");

			}
		}

		node->value_type_mask = (uint16_t)type_mask;
	}

	// Skip over remaining values if necessary if we for example truncated
	// the list of values or if there are values after an array
	uint64_t offset = ufbxi_get_read_offset(uc);
	ufbxi_check(offset <= values_end_offset);
	if (offset < values_end_offset) {
		ufbxi_check(ufbxi_skip_bytes(uc, values_end_offset - offset));
	}

	if (recursive) {
		// Recursively parse the children of this node. Update the parse state
		// to provide context for child node parsing.
		ufbxi_parse_state parse_state = ufbxi_update_parse_state(parent_state, node->name);
		uint32_t num_children = 0;
		for (;;) {
			// Stop at end offset
			uint64_t current_offset = ufbxi_get_read_offset(uc);
			if (current_offset >= end_offset) {
				ufbxi_check(current_offset == end_offset || end_offset == 0);
				break;
			}

			bool end = false;
			ufbxi_check(ufbxi_binary_parse_node(uc, depth + 1, parse_state, &end, tmp_buf, true));
			if (end) break;
			num_children++;
		}

		// Pop children from `tmp_stack` to a contiguous array
		node->num_children = num_children;
		if (num_children > 0) {
			node->children = ufbxi_push_pop(tmp_buf, &uc->tmp_stack, ufbxi_node, num_children);
			ufbxi_check(node->children);
		}
	} else {
		uint64_t current_offset = ufbxi_get_read_offset(uc);
		uc->has_next_child = (current_offset < end_offset);
	}

	return 1;
}

#define UFBXI_BINARY_MAGIC_SIZE 22
#define UFBXI_BINARY_HEADER_SIZE 27
static const char ufbxi_binary_magic[] = "Kaydara FBX Binary  \x00\x1a";

// -- ASCII parsing

#define UFBXI_ASCII_END '\0'
#define UFBXI_ASCII_NAME 'N'
#define UFBXI_ASCII_BARE_WORD 'B'
#define UFBXI_ASCII_INT 'I'
#define UFBXI_ASCII_FLOAT 'F'
#define UFBXI_ASCII_STRING 'S'

static ufbxi_noinline char ufbxi_ascii_refill(ufbxi_context *uc)
{
	ufbxi_ascii *ua = &uc->ascii;
	uc->data_offset += ua->src - uc->data_begin;
	if (uc->read_fn) {
		// Grow the read buffer if necessary
		if (uc->read_buffer_size < uc->opts.read_buffer_size) {
			size_t new_size = uc->opts.read_buffer_size;
			ufbxi_check_return(ufbxi_grow_array(&uc->ator_tmp, &uc->read_buffer, &uc->read_buffer_size, new_size), '\0');
		}

		// Read user data, return '\0' on EOF
		size_t num_read = uc->read_fn(uc->read_user, uc->read_buffer, uc->read_buffer_size);
		ufbxi_check_return_msg(num_read != SIZE_MAX, '\0', "IO error");
		ufbxi_check_return(num_read <= uc->read_buffer_size, '\0');
		if (num_read == 0) return '\0';

		uc->data = uc->data_begin = ua->src = uc->read_buffer;
		ua->src_end = uc->read_buffer + num_read;
		return *ua->src;
	} else {
		// If the user didn't specify a `read_fn()` treat anything
		// past the initial data buffer as EOF.
		uc->data = uc->data_begin = ua->src = "";
		ua->src_end = ua->src + 1;
		return '\0';
	}
}

static ufbxi_noinline char ufbxi_ascii_yield(ufbxi_context *uc)
{
	ufbxi_ascii *ua = &uc->ascii;

	char ret;
	if (ua->src == ua->src_end) {
		ret = ufbxi_ascii_refill(uc);
	} else {
		ret = *ua->src;
	}

	if ((size_t)(ua->src_end - ua->src) < uc->progress_interval) {
		ua->src_yield = ua->src_end;
	} else {
		ua->src_yield = ua->src + uc->progress_interval;
	}

	// TODO: Unify these properly
	uc->data = ua->src;
	ufbxi_check_return(ufbxi_report_progress(uc), '\0');
	return ret;
}

static ufbxi_forceinline char ufbxi_ascii_peek(ufbxi_context *uc)
{
	ufbxi_ascii *ua = &uc->ascii;
	if (ua->src == ua->src_yield) return ufbxi_ascii_yield(uc);
	return *ua->src;
}

static ufbxi_forceinline char ufbxi_ascii_next(ufbxi_context *uc)
{
	ufbxi_ascii *ua = &uc->ascii;
	if (ua->src == ua->src_yield) return ufbxi_ascii_yield(uc);
	ua->src++;
	if (ua->src == ua->src_yield) return ufbxi_ascii_yield(uc);
	return *ua->src;
}

static uint32_t ufbxi_ascii_parse_version(ufbxi_context *uc)
{
	uint8_t digits[3];
	uint32_t num_digits = 0;

	char c = ufbxi_ascii_next(uc);

	const char fmt[] = " FBX ?.?.?";
	uint32_t ix = 0;
	while (num_digits < 3) {
		char ref = fmt[ix++];
		switch (ref) {

		// Digit
		case '?':
			if (c < '0' || c > '9') return 0;
			digits[num_digits++] = (uint8_t)(c - '0');
			c = ufbxi_ascii_next(uc);
			break;

		// Whitespace 
		case ' ':
			while (c == ' ' || c == '\t') {
				c = ufbxi_ascii_next(uc);
			}
			break;

		// Literal character
		default:
			if (c != ref) return 0;
			c = ufbxi_ascii_next(uc);
			break;
		}
	}

	return 1000*digits[0] + 100*digits[1] + 10*digits[2];
}

static char ufbxi_ascii_skip_whitespace(ufbxi_context *uc)
{
	ufbxi_ascii *ua = &uc->ascii;

	// Ignore whitespace
	char c = ufbxi_ascii_peek(uc);
	for (;;) {
		while (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			c = ufbxi_ascii_next(uc);
		}

		// Line comment
		if (c == ';') {

			bool read_magic = false;
			// FBX ASCII files begin with a magic comment of form "; FBX 7.7.0 project file"
			// Try to extract the version number from the magic comment
			if (!ua->read_first_comment) {
				ua->read_first_comment = true;
				uint32_t version = ufbxi_ascii_parse_version(uc);
				if (version) {
					uc->version = version;
					ua->found_version = true;
					read_magic = true;
				}
			}

			c = ufbxi_ascii_next(uc);
			while (c != '\n' && c != '\0') {
				c = ufbxi_ascii_next(uc);
			}
			c = ufbxi_ascii_next(uc);

			// Try to determine if this is a Blender 6100 ASCII file
			if (read_magic) {
				if (c == ';') {
					char line[32];
					size_t line_len = 0;

					c = ufbxi_ascii_next(uc);
					while (c != '\n' && c != '\0') {
						if (line_len < sizeof(line)) {
							line[line_len++] = c;
						}
						c = ufbxi_ascii_next(uc);
					}

					if (line_len >= 19 && !memcmp(line, " Created by Blender", 19)) {
						uc->exporter = UFBX_EXPORTER_BLENDER_ASCII;
					}
				}
			}

		} else {
			break;
		}
	}
	return c;
}

ufbxi_nodiscard static ufbxi_forceinline int ufbxi_ascii_push_token_char(ufbxi_context *uc, ufbxi_ascii_token *token, char c)
{
	// Grow the string data buffer if necessary
	if (token->str_len == token->str_cap) {
		size_t len = ufbxi_max_sz(token->str_len + 1, 256);
		ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &token->str_data, &token->str_cap, len));
	}

	token->str_data[token->str_len++] = c;

	return 1;
}

ufbxi_nodiscard static int ufbxi_ascii_try_ignore_string(ufbxi_context *uc, ufbxi_ascii_token *token)
{
	ufbxi_ascii *ua = &uc->ascii;

	char c = ufbxi_ascii_skip_whitespace(uc);
	token->str_len = 0;

	if (c == '"') {
		// Replace `prev_token` with `token` but swap the buffers so `token` uses
		// the now-unused string buffer of the old `prev_token`.
		char *swap_data = ua->prev_token.str_data;
		size_t swap_cap = ua->prev_token.str_cap;
		ua->prev_token = ua->token;
		ua->token.str_data = swap_data;
		ua->token.str_cap = swap_cap;

		token->type = UFBXI_ASCII_STRING;
		c = ufbxi_ascii_next(uc);
		while (c != '"') {
			c = ufbxi_ascii_next(uc);
			ufbxi_check(c != '\0');
		}
		// Skip closing quote
		ufbxi_ascii_next(uc);
		return true;
	}

	return false;
}

ufbxi_nodiscard static int ufbxi_ascii_next_token(ufbxi_context *uc, ufbxi_ascii_token *token)
{
	ufbxi_ascii *ua = &uc->ascii;

	// Replace `prev_token` with `token` but swap the buffers so `token` uses
	// the now-unused string buffer of the old `prev_token`.
	char *swap_data = ua->prev_token.str_data;
	size_t swap_cap = ua->prev_token.str_cap;
	ua->prev_token = ua->token;
	ua->token.str_data = swap_data;
	ua->token.str_cap = swap_cap;

	char c = ufbxi_ascii_skip_whitespace(uc);
	token->str_len = 0;

	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
		token->type = UFBXI_ASCII_BARE_WORD;
		while ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '_') {
			ufbxi_check(ufbxi_ascii_push_token_char(uc, token, c));
			c = ufbxi_ascii_next(uc);
		}

		// Skip whitespace to find if there's a following ':'
		c = ufbxi_ascii_skip_whitespace(uc);
		if (c == ':') {
			token->value.name_len = token->str_len;
			token->type = UFBXI_ASCII_NAME;
			ufbxi_ascii_next(uc);
		}
	} else if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
		token->type = UFBXI_ASCII_INT;

		while ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
			if (c == '.' || c == 'e' || c == 'E') {
				token->type = UFBXI_ASCII_FLOAT;
			}
			ufbxi_check(ufbxi_ascii_push_token_char(uc, token, c));
			c = ufbxi_ascii_next(uc);
		}

		if (c == '#') {
			ufbxi_check(token->type == UFBXI_ASCII_FLOAT);
			ufbxi_check(ufbxi_ascii_push_token_char(uc, token, c));
			c = ufbxi_ascii_next(uc);

			bool is_inf = c == 'I' || c == 'i';
			while ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
				ufbxi_check(ufbxi_ascii_push_token_char(uc, token, c));
				c = ufbxi_ascii_next(uc);
			}
			ufbxi_check(ufbxi_ascii_push_token_char(uc, token, '\0'));

			if (is_inf) {
				token->value.f64 = token->str_data[0] == '-' ? -INFINITY : INFINITY;
			} else {
				token->value.f64 = NAN;
			}

		} else {
			ufbxi_check(ufbxi_ascii_push_token_char(uc, token, '\0'));
			char *end;
			if (token->type == UFBXI_ASCII_INT) {
				token->value.i64 = strtoll(token->str_data, &end, 10);
				ufbxi_check(end == token->str_data + token->str_len - 1);
			} else if (token->type == UFBXI_ASCII_FLOAT) {
				if (ua->parse_as_f32) {
					token->value.f64 = strtof(token->str_data, &end);
				} else {
					token->value.f64 = strtod(token->str_data, &end);
				}
				ufbxi_check(end == token->str_data + token->str_len - 1);
			}
		}
	} else if (c == '"') {
		token->type = UFBXI_ASCII_STRING;
		c = ufbxi_ascii_next(uc);
		while (c != '"') {
			ufbxi_check(ufbxi_ascii_push_token_char(uc, token, c));
			c = ufbxi_ascii_next(uc);
			ufbxi_check(c != '\0');
		}
		// Skip closing quote
		ufbxi_ascii_next(uc);
	} else {
		// Single character token
		token->type = c;
		ufbxi_ascii_next(uc);
	}

	return 1;
}


ufbxi_nodiscard static int ufbxi_ascii_accept(ufbxi_context *uc, char type)
{
	ufbxi_ascii *ua = &uc->ascii;

	if (ua->token.type == type) {
		ufbxi_check(ufbxi_ascii_next_token(uc, &ua->token));
		return 1;
	} else {
		return 0;
	}
}

ufbxi_nodiscard static int ufbxi_ascii_parse_node(ufbxi_context *uc, uint32_t depth, ufbxi_parse_state parent_state, bool *p_end, ufbxi_buf *tmp_buf, bool recursive)
{
	ufbxi_ascii *ua = &uc->ascii;

	if (ua->token.type == '}') {
		ufbxi_check(ufbxi_ascii_next_token(uc, &ua->token));
		*p_end = true;
		return 1;
	}

	if (ua->token.type == UFBXI_ASCII_END) {
		ufbxi_check_msg(depth == 0, "Truncated file");
		*p_end = true;
		return 1;
	}

	// Parse the name eg. "Node:" token and intern the name
	ufbxi_check(depth < UFBXI_MAX_NODE_DEPTH);
	if (!uc->sure_fbx && depth == 0 && ua->token.type != UFBXI_ASCII_NAME) {
		ufbxi_fail_msg("Expected a 'Name:' token", "Not an FBX file");
	}
	ufbxi_check(ufbxi_ascii_accept(uc, UFBXI_ASCII_NAME));
	size_t name_len = ua->prev_token.value.name_len;
	ufbxi_check(name_len <= 0xff);
	const char *name = ufbxi_push_string(&uc->string_pool, ua->prev_token.str_data, ua->prev_token.str_len);
	ufbxi_check(name);

	// Push the parsed node into the `tmp_stack` buffer, the nodes will be popped by
	// calling code after its done parsing all of it's children.
	ufbxi_node *node = ufbxi_push_zero(&uc->tmp_stack, ufbxi_node, 1);
	ufbxi_check(node);
	node->name = name;
	node->name_len = (uint8_t)name_len;

	bool in_ascii_array = false;

	uint32_t num_values = 0;
	uint32_t type_mask = 0;

	int arr_type = 0;
	ufbxi_buf *arr_buf = NULL;
	size_t arr_elem_size = 0;

	// Check if the values of the node we're parsing currently should be
	// treated as an array.
	ufbxi_array_info arr_info;
	if (ufbxi_is_array_node(uc, parent_state, name, &arr_info)) {
		arr_type = ufbxi_normalize_array_type(arr_info.type);
		arr_buf = tmp_buf;
		if (arr_info.result) arr_buf = &uc->result;
		else if (arr_info.tmp_buf) arr_buf = &uc->tmp;

		ufbxi_value_array *arr = ufbxi_push(tmp_buf, ufbxi_value_array, 1);
		ufbxi_check(arr);
		node->value_type_mask = UFBXI_VALUE_ARRAY;
		node->array = arr;
		arr->type = (char)arr_type;

		// Parse array values using strtof() if the array destination is 32-bit float
		// since KeyAttrDataFloat packs integer data (!) into floating point values so we
		// should try to be as exact as possible.
		if (arr->type == 'f') {
			ua->parse_as_f32 = true;
		}

		arr_elem_size = ufbxi_array_type_size((char)arr_type);

		// Pad with 4 zero elements to make indexing with `-1` safe.
		if (arr_info.pad_begin) {
			ufbxi_check(ufbxi_push_size_zero(&uc->tmp_stack, arr_elem_size, 4));
			num_values += 4;
		}
	}

	// Some fields in ASCII may have leading commas eg. `Content: , "base64-string"`
	if (ua->token.type == ',') {
		// HACK: If we are parsing an "array" that should be ignored, ie. `Content` when
		// `opts.ignore_embedded == true` try to skip the next token string if possible.
		if (arr_type == '-') {
			if (!ufbxi_ascii_try_ignore_string(uc, &ua->token)) {
				ufbxi_check(ufbxi_ascii_next_token(uc, &ua->token));
			}
		} else {
			ufbxi_check(ufbxi_ascii_next_token(uc, &ua->token));
		}
	}

	ufbxi_parse_state parse_state = ufbxi_update_parse_state(parent_state, node->name);
	ufbxi_value vals[UFBXI_MAX_NON_ARRAY_VALUES];

	// NOTE: Infinite loop to allow skipping the comma parsing via `continue`.
	for (;;) {
		ufbxi_ascii_token *tok = &ua->prev_token;
		if (ufbxi_ascii_accept(uc, UFBXI_ASCII_STRING)) {

			if (arr_type) {

				if (arr_type == 's') {
					ufbx_string *v = ufbxi_push(&uc->tmp_stack, ufbx_string, 1);
					ufbxi_check(v);
					v->data = tok->str_data;
					v->length = tok->str_len;
					ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, v));
				} else {
					// Ignore strings in non-string arrays, decrement `num_values` as it will be
					// incremented after the loop iteration is done to ignore it.
					num_values--;
				}

			} else if (num_values < UFBXI_MAX_NON_ARRAY_VALUES) {
				type_mask |= UFBXI_VALUE_STRING << (num_values*2);
				ufbxi_value *v = &vals[num_values];
				v->s.data = tok->str_data;
				v->s.length = tok->str_len;
				ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, &v->s));
			}

		} else if (ufbxi_ascii_accept(uc, UFBXI_ASCII_INT)) {
			int64_t val = tok->value.i64;

			switch (arr_type) {

			case 0:
				// Parse version from comment if there was no magic comment
				if (!ua->found_version && parse_state == UFBXI_PARSE_FBX_VERSION && num_values == 0) {
					if (val >= 6000 && val <= 10000) {
						ua->found_version = true;
						uc->version = (uint32_t)val;
					}
				}

				if (num_values < UFBXI_MAX_NON_ARRAY_VALUES) {
					type_mask |= UFBXI_VALUE_NUMBER << (num_values*2);
					ufbxi_value *v = &vals[num_values];
					v->f = (double)(v->i = val);
				}
				break;

			case 'b': { bool *v = ufbxi_push(&uc->tmp_stack, bool, 1); ufbxi_check(v); *v = val != 0; } break;
			case 'i': { int32_t *v = ufbxi_push(&uc->tmp_stack, int32_t, 1); ufbxi_check(v); *v = (int32_t)val; } break;
			case 'l': { int64_t *v = ufbxi_push(&uc->tmp_stack, int64_t, 1); ufbxi_check(v); *v = (int64_t)val; } break;
			case 'f': { float *v = ufbxi_push(&uc->tmp_stack, float, 1); ufbxi_check(v); *v = (float)val; } break;
			case 'd': { double *v = ufbxi_push(&uc->tmp_stack, double, 1); ufbxi_check(v); *v = (double)val; } break;

			default:
				ufbxi_fail("Bad array dst type");

			}

		} else if (ufbxi_ascii_accept(uc, UFBXI_ASCII_FLOAT)) {
			double val = tok->value.f64;

			switch (arr_type) {

			case 0:
				if (num_values < UFBXI_MAX_NON_ARRAY_VALUES) {
					type_mask |= UFBXI_VALUE_NUMBER << (num_values*2);
					ufbxi_value *v = &vals[num_values];
					v->i = (int64_t)(v->f = val);
				}
				break;

			case 'b': { bool *v = ufbxi_push(&uc->tmp_stack, bool, 1); ufbxi_check(v); *v = val != 0; } break;
			case 'i': { int32_t *v = ufbxi_push(&uc->tmp_stack, int32_t, 1); ufbxi_check(v); *v = (int32_t)val; } break;
			case 'l': { int64_t *v = ufbxi_push(&uc->tmp_stack, int64_t, 1); ufbxi_check(v); *v = (int64_t)val; } break;
			case 'f': { float *v = ufbxi_push(&uc->tmp_stack, float, 1); ufbxi_check(v); *v = (float)val; } break;
			case 'd': { double *v = ufbxi_push(&uc->tmp_stack, double, 1); ufbxi_check(v); *v = (double)val; } break;

			default:
				ufbxi_fail("Bad array dst type");

			}

		} else if (ufbxi_ascii_accept(uc, UFBXI_ASCII_BARE_WORD)) {

			int64_t val = 0;
			if (tok->str_len >= 1) {
				val = (int64_t)tok->str_data[0];
			}

			switch (arr_type) {

			case 0:
				if (num_values < UFBXI_MAX_NON_ARRAY_VALUES) {
					type_mask |= UFBXI_VALUE_NUMBER << (num_values*2);
					ufbxi_value *v = &vals[num_values];
					v->f = (double)(v->i = val);
				}
				break;

			case 'b': { bool *v = ufbxi_push(&uc->tmp_stack, bool, 1); ufbxi_check(v); *v = val != 0; } break;
			case 'i': { int32_t *v = ufbxi_push(&uc->tmp_stack, int32_t, 1); ufbxi_check(v); *v = (int32_t)val; } break;
			case 'l': { int64_t *v = ufbxi_push(&uc->tmp_stack, int64_t, 1); ufbxi_check(v); *v = (int64_t)val; } break;
			case 'f': { float *v = ufbxi_push(&uc->tmp_stack, float, 1); ufbxi_check(v); *v = (float)val; } break;
			case 'd': { double *v = ufbxi_push(&uc->tmp_stack, double, 1); ufbxi_check(v); *v = (double)val; } break;

			}

		} else if (ufbxi_ascii_accept(uc, '*')) {
			// Parse a post-7000 ASCII array eg. "*3 { 1,2,3 }"
			ufbxi_check(!in_ascii_array);
			ufbxi_check(ufbxi_ascii_accept(uc, UFBXI_ASCII_INT));

			if (ufbxi_ascii_accept(uc, '{')) {
				ufbxi_check(ufbxi_ascii_accept(uc, UFBXI_ASCII_NAME));

				// NOTE: This `continue` skips incrementing `num_values` and parsing
				// a comma, continuing to parse the values in the array.
				in_ascii_array = true;
			}
			continue;
		} else {
			break;
		}

		// Add value and keep parsing if there's a comma. This part may be
		// skipped if we enter an array block.
		num_values++;
		ufbxi_check(num_values < UINT32_MAX);
		if (!ufbxi_ascii_accept(uc, ',')) break;
	}

	// Close the ASCII array if we are in one
	if (in_ascii_array) {
		ufbxi_check(ufbxi_ascii_accept(uc, '}'));
	}

	ua->parse_as_f32 = false;

	if (arr_type) {
		if (arr_type == '-') {
			node->array->data = NULL;
			node->array->size = 0;
		} else {
			void *arr_data = ufbxi_push_pop_size(arr_buf, &uc->tmp_stack, arr_elem_size, num_values);
			ufbxi_check(arr_data);
			if (arr_info.pad_begin) {
				node->array->data = (char*)arr_data + 4*arr_elem_size;
				node->array->size = num_values - 4;
			} else {
				node->array->data = arr_data;
				node->array->size = num_values;
			}
		}
	} else {
		num_values = ufbxi_min32(num_values, UFBXI_MAX_NON_ARRAY_VALUES);
		node->value_type_mask = (uint16_t)type_mask;
		node->vals = ufbxi_push_copy(tmp_buf, ufbxi_value, num_values, vals);
		ufbxi_check(node->vals);
	}

	// Recursively parse the children of this node. Update the parse state
	// to provide context for child node parsing.
	if (ufbxi_ascii_accept(uc, '{')) {
		if (recursive) {
			size_t num_children = 0;
			for (;;) {
				bool end = false;
				ufbxi_check(ufbxi_ascii_parse_node(uc, depth + 1, parse_state, &end, tmp_buf, recursive));
				if (end) break;
				num_children++;
			}

			// Pop children from `tmp_stack` to a contiguous array
			node->children = ufbxi_push_pop(tmp_buf, &uc->tmp_stack, ufbxi_node, num_children);
			ufbxi_check(node->children);
			node->num_children = (uint32_t)num_children;
		}

		uc->has_next_child = true;
	} else {
		uc->has_next_child = false;
	}

	return 1;
}

// -- General parsing

ufbxi_nodiscard static int ufbxi_begin_parse(ufbxi_context *uc)
{
	const char *header = ufbxi_peek_bytes(uc, UFBXI_BINARY_HEADER_SIZE);
	ufbxi_check(header);

	// If the file starts with the binary magic parse it as binary, otherwise
	// treat it as an ASCII file.
	if (!memcmp(header, ufbxi_binary_magic, UFBXI_BINARY_MAGIC_SIZE)) {

		// The byte after the magic indicates endianness
		char endian = header[UFBXI_BINARY_MAGIC_SIZE + 0];
		uc->file_big_endian = endian != 0;

		// Read the version directly from the header
		const char *version_word = header + UFBXI_BINARY_MAGIC_SIZE + 1;
		if (uc->file_big_endian) {
			version_word = ufbxi_swap_endian(uc, version_word, 1, 4);
			ufbxi_check(version_word);
		}
		uc->version = ufbxi_read_u32(version_word);

		// This is quite probably an FBX file..
		uc->sure_fbx = true;
		ufbxi_consume_bytes(uc, UFBXI_BINARY_HEADER_SIZE);

	} else {
		uc->from_ascii = true;

		// Use the current read buffer as the initial parse buffer
		memset(&uc->ascii, 0, sizeof(uc->ascii));
		uc->ascii.src = uc->data;
		uc->ascii.src_yield = uc->data + uc->yield_size;
		uc->ascii.src_end = uc->data + uc->data_size + uc->yield_size;

		// Initialize the first token
		ufbxi_check(ufbxi_ascii_next_token(uc, &uc->ascii.token));

		// Default to version 7400 if not found in header
		if (uc->version > 0) {
			uc->sure_fbx = true;
		} else {
			if (!uc->opts.strict) uc->version = 7400;
			ufbxi_check_msg(uc->version > 0, "Not an FBX file");
		}
	}

	// Initialize the scene
	uc->scene.metadata.creator = ufbx_empty_string;

	return 1;
}

ufbxi_nodiscard static int ufbxi_parse_toplevel_child_imp(ufbxi_context *uc, ufbxi_parse_state state, ufbxi_buf *buf, bool *p_end)
{
	if (uc->from_ascii) {
		ufbxi_check(ufbxi_ascii_parse_node(uc, 0, state, p_end, buf, true));
	} else {
		ufbxi_check(ufbxi_binary_parse_node(uc, 0, state, p_end, buf, true));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_parse_toplevel(ufbxi_context *uc, const char *name)
{
	// Check if the top-level node has already been parsed
	ufbxi_for(ufbxi_node, node, uc->top_nodes, uc->top_nodes_len) {
		if (node->name == name) {
			uc->top_node = node;
			uc->top_child_index = 0;
			return 1;
		}
	}

	// Reached end and not found in cache
	if (uc->parsed_to_end) {
		uc->top_node = NULL;
		uc->top_child_index = 0;
		return 1;
	}

	for (;;) {
		// Parse the next top-level node
		bool end = false;
		if (uc->from_ascii) {
			ufbxi_check(ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &uc->tmp, false));
		} else {
			ufbxi_check(ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &uc->tmp, false));
		}

		// Top-level node not found
		if (end) {
			uc->top_node = NULL;
			uc->top_child_index = 0;
			uc->parsed_to_end = true;
			return 1;
		}

		uc->top_nodes_len++;
		ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->top_nodes, &uc->top_nodes_cap, uc->top_nodes_len));
		ufbxi_node *node = &uc->top_nodes[uc->top_nodes_len - 1];
		ufbxi_pop(&uc->tmp_stack, ufbxi_node, 1, node);

		// Return if we parsed the right one
		if (node->name == name) {
			uc->top_node = node;
			uc->top_child_index = SIZE_MAX;
			return 1;
		}

		// If not we need to parse all the children of the node for later
		uint32_t num_children = 0;
		ufbxi_parse_state state = ufbxi_update_parse_state(UFBXI_PARSE_ROOT, node->name);
		if (uc->has_next_child) {
			for (;;) {
				ufbxi_check(ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp, &end));
				if (end) break;
				num_children++;
			}
		}

		node->num_children = num_children;
		node->children = ufbxi_push_pop(&uc->tmp, &uc->tmp_stack, ufbxi_node, num_children);
		ufbxi_check(node->children);
	}
}

ufbxi_nodiscard static int ufbxi_parse_toplevel_child(ufbxi_context *uc, ufbxi_node **p_node)
{
	// Top-level node not found
	if (!uc->top_node) {
		*p_node = NULL;
		return 1;
	}

	if (uc->top_child_index == SIZE_MAX) {
		// Parse children on demand
		ufbxi_buf_clear(&uc->tmp_parse);
		bool end = false;
		ufbxi_parse_state state = ufbxi_update_parse_state(UFBXI_PARSE_ROOT, uc->top_node->name);
		ufbxi_check(ufbxi_parse_toplevel_child_imp(uc, state, &uc->tmp_parse, &end));
		if (end) {
			*p_node = NULL;
		} else {
			ufbxi_pop(&uc->tmp_stack, ufbxi_node, 1, &uc->top_child);
			*p_node = &uc->top_child;
		}
	} else {
		// Iterate already parsed nodes
		if (uc->top_child_index == uc->top_node->num_children) {
			*p_node = NULL;
		} else {
			*p_node = &uc->top_node->children[uc->top_child_index++];
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_parse_legacy_toplevel(ufbxi_context *uc)
{
	ufbx_assert(uc->top_nodes_len == 0);

	bool end = false;
	if (uc->from_ascii) {
		ufbxi_check(ufbxi_ascii_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &uc->tmp, true));
	} else {
		ufbxi_check(ufbxi_binary_parse_node(uc, 0, UFBXI_PARSE_ROOT, &end, &uc->tmp, true));
	}

	// Top-level node not found
	if (end) {
		uc->top_node = NULL;
		uc->top_child_index = 0;
		uc->parsed_to_end = true;
		return 1;
	}

	ufbxi_pop(&uc->tmp_stack, ufbxi_node, 1, &uc->legacy_node);
	uc->top_child_index = 0;
	uc->top_node = &uc->legacy_node;

	return 1;
}

// -- Setup

ufbxi_nodiscard static int ufbxi_load_strings(ufbxi_context *uc)
{
#if defined(UFBX_REGRESSION)
	ufbx_string reg_prev = ufbx_empty_string;
#endif

	// Push all the global 'ufbxi_*' strings into the pool without copying them
	// This allows us to compare name pointers to the global values
	ufbxi_for(ufbx_string, str, ufbxi_strings, ufbxi_arraycount(ufbxi_strings)) {
#if defined(UFBX_REGRESSION)
		ufbx_assert(strlen(str->data) == str->length);
		ufbx_assert(ufbxi_str_less(reg_prev, *str));
		reg_prev = *str;
#endif
		ufbxi_check(ufbxi_push_string_imp(&uc->string_pool, str->data, str->length, false));
	}

	return 1;
}

typedef struct {
	const char *name;
	ufbx_prop_type type;
} ufbxi_prop_type_name;

const ufbxi_prop_type_name ufbxi_prop_type_names[] = {
	{ "Boolean", UFBX_PROP_BOOLEAN },
	{ "bool", UFBX_PROP_BOOLEAN },
	{ "Bool", UFBX_PROP_BOOLEAN },
	{ "Integer", UFBX_PROP_INTEGER },
	{ "int", UFBX_PROP_INTEGER },
	{ "enum", UFBX_PROP_INTEGER },
	{ "Visibility", UFBX_PROP_INTEGER },
	{ "Visibility Inheritance", UFBX_PROP_INTEGER },
	{ "KTime", UFBX_PROP_INTEGER },
	{ "Number", UFBX_PROP_NUMBER },
	{ "double", UFBX_PROP_NUMBER },
	{ "Real", UFBX_PROP_NUMBER },
	{ "Float", UFBX_PROP_NUMBER },
	{ "Intensity", UFBX_PROP_NUMBER },
	{ "Vector", UFBX_PROP_VECTOR },
	{ "Vector3D", UFBX_PROP_VECTOR },
	{ "Color", UFBX_PROP_COLOR },
	{ "ColorRGB", UFBX_PROP_COLOR },
	{ "String", UFBX_PROP_STRING },
	{ "KString", UFBX_PROP_STRING },
	{ "object", UFBX_PROP_STRING },
	{ "DateTime", UFBX_PROP_DATE_TIME },
	{ "Lcl Translation", UFBX_PROP_TRANSLATION },
	{ "Lcl Rotation", UFBX_PROP_ROTATION },
	{ "Lcl Scaling", UFBX_PROP_SCALING },
	{ "Distance", UFBX_PROP_DISTANCE },
	{ "Compound", UFBX_PROP_COMPOUND },
};

static ufbx_prop_type ufbxi_get_prop_type(ufbxi_context *uc, const char *name)
{
	uint32_t hash = ufbxi_hash_ptr(name);
	ufbxi_prop_type_name *entry = ufbxi_map_find(&uc->prop_type_map, ufbxi_prop_type_name, hash, &name);
	if (entry) {
		return entry->type;
	}
	return UFBX_PROP_UNKNOWN;
}

static ufbxi_noinline ufbx_prop *ufbxi_find_prop_with_key(const ufbx_props *props, const char *name, uint32_t key)
{
	do {
		ufbx_prop *prop_data = props->props;
		size_t begin = 0;
		size_t end = props->num_props;
		while (end - begin >= 16) {
			size_t mid = (begin + end) >> 1;
			const ufbx_prop *p = &prop_data[mid];
			if (p->internal_key < key) {
				begin = mid + 1;
			} else { 
				end = mid;
			}
		}

		end = props->num_props;
		for (; begin < end; begin++) {
			const ufbx_prop *p = &prop_data[begin];
			if (p->internal_key > key) break;
			if (p->name.data == name && (p->flags & UFBX_PROP_FLAG_NO_VALUE) == 0) {
				return (ufbx_prop*)p;
			}
		}

		props = props->defaults;
	} while (props);

	return NULL;
}

#define ufbxi_find_prop(props, name) ufbxi_find_prop_with_key((props), (name), \
	(name[0] << 24) | (name[1] << 16) | (name[2] << 8) | name[3])

static ufbxi_forceinline uint32_t ufbxi_get_name_key(const char *name, size_t len)
{
	uint32_t key = 0;
	if (len >= 4) {
		key = (uint32_t)(uint8_t)name[0]<<24 | (uint32_t)(uint8_t)name[1]<<16
			| (uint32_t)(uint8_t)name[2]<<8 | (uint32_t)(uint8_t)name[3];
	} else {
		for (size_t i = 0; i < 4; i++) {
			key <<= 8;
			if (i < len) key |= (uint8_t)name[i];
		}
	}
	return key;
}

static ufbxi_forceinline uint32_t ufbxi_get_name_key_c(const char *name)
{
	if (name[0] == '\0') return 0;
	if (name[1] == '\0') return (uint32_t)(uint8_t)name[0]<<24;
	if (name[2] == '\0') return (uint32_t)(uint8_t)name[0]<<24 | (uint8_t)name[1]<<16;
	return (uint32_t)(uint8_t)name[0]<<24 | (uint32_t)(uint8_t)name[1]<<16
		| (uint32_t)(uint8_t)name[2]<<8 | (uint32_t)(uint8_t)name[3];
}

static ufbxi_forceinline bool ufbxi_name_key_less(ufbx_prop *prop, const char *data, size_t name_len, uint32_t key)
{
	if (prop->internal_key < key) return true;
	if (prop->internal_key > key) return false;

	size_t prop_len = prop->name.length;
	size_t len = ufbxi_min_sz(prop_len, name_len);
	int cmp = memcmp(prop->name.data, data, len);
	if (cmp != 0) return cmp < 0;
	return prop_len < name_len;
}

static const char *ufbxi_node_prop_names[] = {
	"AxisLen",
	"DefaultAttributeIndex",
	"Freeze",
	"GeometricRotation",
	"GeometricScaling",
	"GeometricTranslation",
	"InheritType",
	"LODBox",
	"Lcl Rotation",
	"Lcl Scaling",
	"Lcl Translation",
	"LookAtProperty",
	"MaxDampRangeX",
	"MaxDampRangeY",
	"MaxDampRangeZ",
	"MaxDampStrengthX",
	"MaxDampStrengthY",
	"MaxDampStrengthZ",
	"MinDampRangeX",
	"MinDampRangeY",
	"MinDampRangeZ",
	"MinDampStrengthX",
	"MinDampStrengthY",
	"MinDampStrengthZ",
	"NegativePercentShapeSupport",
	"PostRotation",
	"PreRotation",
	"PreferedAngleX",
	"PreferedAngleY",
	"PreferedAngleZ",
	"QuaternionInterpolate",
	"RotationActive",
	"RotationMax",
	"RotationMaxX",
	"RotationMaxY",
	"RotationMaxZ",
	"RotationMin",
	"RotationMinX",
	"RotationMinY",
	"RotationMinZ",
	"RotationOffset",
	"RotationOrder",
	"RotationPivot",
	"RotationSpaceForLimitOnly",
	"RotationStiffnessX",
	"RotationStiffnessY",
	"RotationStiffnessZ",
	"ScalingActive",
	"ScalingMax",
	"ScalingMaxX",
	"ScalingMaxY",
	"ScalingMaxZ",
	"ScalingMin",
	"ScalingMinX",
	"ScalingMinY",
	"ScalingMinZ",
	"ScalingOffset",
	"ScalingPivot",
	"Show",
	"TranslationActive",
	"TranslationMax",
	"TranslationMaxX",
	"TranslationMaxY",
	"TranslationMaxZ",
	"TranslationMin",
	"TranslationMinX",
	"TranslationMinY",
	"TranslationMinZ",
	"UpVectorProperty",
	"Visibility Inheritance",
	"Visibility",
};

ufbxi_nodiscard static int ufbxi_init_node_prop_names(ufbxi_context *uc)
{
	ufbxi_check(ufbxi_map_grow(&uc->node_prop_set, const char*, ufbxi_arraycount(ufbxi_node_prop_names)));
	ufbxi_for_ptr(const char, p_name, ufbxi_node_prop_names, ufbxi_arraycount(ufbxi_node_prop_names)) {
		const char *name = *p_name;
		const char *pooled = ufbxi_push_string_imp(&uc->string_pool, name, strlen(name), false);
		ufbxi_check(pooled);
		uint32_t hash = ufbxi_hash_ptr(pooled);
		const char **entry = ufbxi_map_insert(&uc->node_prop_set, const char*, hash, &pooled);
		ufbxi_check(entry);
		*entry = pooled;
	}

	return 1;
}

static bool ufbxi_is_node_property(ufbxi_context *uc, const char *name)
{
	// You need to call `ufbxi_init_node_prop_names()` before calling this
	ufbx_assert(uc->node_prop_set.size > 0);

	uint32_t hash = ufbxi_hash_ptr(name);
	const char **entry = ufbxi_map_find(&uc->node_prop_set, const char*, hash, &name);
	return entry != NULL;
}

ufbxi_nodiscard static int ufbxi_load_maps(ufbxi_context *uc)
{
	ufbxi_check(ufbxi_map_grow(&uc->prop_type_map, ufbxi_prop_type_name, ufbxi_arraycount(ufbxi_prop_type_names)));
	ufbxi_for(const ufbxi_prop_type_name, name, ufbxi_prop_type_names, ufbxi_arraycount(ufbxi_prop_type_names)) {
		const char *pooled = ufbxi_push_string_imp(&uc->string_pool, name->name, strlen(name->name), false);
		ufbxi_check(pooled);
		uint32_t hash = ufbxi_hash_ptr(pooled);
		ufbxi_prop_type_name *entry = ufbxi_map_insert(&uc->prop_type_map, ufbxi_prop_type_name, hash, &pooled);
		ufbxi_check(entry);
		entry->type = name->type;
		entry->name = pooled;
	}

	return 1;
}

// -- Reading the parsed data

ufbxi_nodiscard static int ufbxi_read_property(ufbxi_context *uc, ufbxi_node *node, ufbx_prop *prop, int version)
{
	const char *type_str = NULL, *subtype_str = NULL;
	ufbxi_check(ufbxi_get_val2(node, "SC", &prop->name, (char**)&type_str));
	uint32_t val_ix = 2;
	if (version == 70) {
		ufbxi_check(ufbxi_get_val_at(node, val_ix++, 'C', (char**)&subtype_str));
	}

	prop->internal_key = ufbxi_get_name_key(prop->name.data, prop->name.length);

	ufbx_string flags_str;
	if (ufbxi_get_val_at(node, val_ix++, 'S', &flags_str)) {
		uint32_t flags = 0;
		for (size_t i = 0; i < flags_str.length; i++) {
			char next = i + 1 < flags_str.length ? flags_str.data[i + 1] : '0';
			switch (flags_str.data[i]) {
			case 'A': flags |= UFBX_PROP_FLAG_ANIMATABLE; break;
			case 'U': flags |= UFBX_PROP_FLAG_USER_DEFINED; break;
			case 'H': flags |= UFBX_PROP_FLAG_HIDDEN; break;
			case 'L': flags |= ((uint32_t)(next - '0') & 0xf) << 4; break; // UFBX_PROP_FLAG_LOCK_*
			case 'M': flags |= ((uint32_t)(next - '0') & 0xf) << 8; break; // UFBX_PROP_FLAG_MUTE_*
			}
		}
		prop->flags = (ufbx_prop_flags)flags;
	}

	prop->type = ufbxi_get_prop_type(uc, type_str);
	if (prop->type == UFBX_PROP_UNKNOWN && subtype_str) {
		prop->type = ufbxi_get_prop_type(uc, subtype_str);
	}

	ufbxi_ignore(ufbxi_get_val_at(node, val_ix, 'L', &prop->value_int));
	for (size_t i = 0; i < 3; i++) {
		if (!ufbxi_get_val_at(node, val_ix + i, 'R', &prop->value_real_arr[i])) break;
	}

	// Distance properties have a string unit _after_ the real value, eg. `10, "cm"`
	if (prop->type == UFBX_PROP_DISTANCE) {
		val_ix++;
	}

	if (!ufbxi_get_val_at(node, val_ix, 'S', &prop->value_str)) {
		prop->value_str = ufbx_empty_string;
	}
	
	return 1;
}

static ufbxi_forceinline bool ufbxi_prop_less(ufbx_prop *a, ufbx_prop *b)
{
	if (a->internal_key < b->internal_key) return true;
	if (a->internal_key > b->internal_key) return false;
	return strcmp(a->name.data, b->name.data) < 0;
}

ufbxi_nodiscard static int ufbxi_sort_properties(ufbxi_context *uc, ufbx_prop *props, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_prop)));
	ufbxi_macro_stable_sort(ufbx_prop, 32, props, uc->tmp_arr, count, ( ufbxi_prop_less(a, b) ));
	return 1;
}

ufbxi_nodiscard static int ufbxi_read_properties(ufbxi_context *uc, ufbxi_node *parent, ufbx_props *props)
{
	props->defaults = NULL;

	int version = 70;
	ufbxi_node *node = ufbxi_find_child(parent, ufbxi_Properties70);
	if (!node) {
		node = ufbxi_find_child(parent, ufbxi_Properties60);
		if (!node) {
			// No properties found, not an error
			props->props = NULL;
			props->num_props = 0;
			return 1;
		}
		version = 60;
	}

	props->props = ufbxi_push_zero(&uc->result, ufbx_prop, node->num_children);
	props->num_props = node->num_children;
	ufbxi_check(props->props);

	for (size_t i = 0; i < props->num_props; i++) {
		ufbxi_check(ufbxi_read_property(uc, &node->children[i], &props->props[i], version));
	}

	// Sort the properties
	ufbxi_check(ufbxi_sort_properties(uc, props->props, props->num_props));

	// Remove duplicates, the last one wins
	if (props->num_props >= 2) {
		ufbx_prop *ps = props->props;
		size_t dst = 0, src = 0, end = props->num_props;
		while (src < end) {
			if (src + 1 < end && ps[src].name.data == ps[src + 1].name.data) {
				src++;
			} else if (dst != src) {
				ps[dst++] = ps[src++];
			} else {
				dst++; src++;
			}
		}
		props->num_props = dst;
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_scene_info(ufbxi_context *uc, ufbxi_node *node)
{
	ufbxi_check(ufbxi_read_properties(uc, node, &uc->scene.metadata.scene_props));

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_header_extension(ufbxi_context *uc)
{
	// TODO: Read TCDefinition and adjust timestamps
	uc->ktime_to_sec = (1.0 / 46186158000.0);

	for (;;) {
		ufbxi_node *child;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &child));
		if (!child) break;

		if (child->name == ufbxi_Creator) {
			ufbxi_ignore(ufbxi_get_val1(child, "S", &uc->scene.metadata.creator));
		}

		if (uc->version < 6000 && child->name == ufbxi_FBXVersion) {
			int32_t version;
			if (ufbxi_get_val1(child, "I", &version)) {
				if (version > 0 && (uint32_t)version > uc->version) {
					uc->version = (uint32_t)version;
				}
			}
		}

		if (child->name == ufbxi_SceneInfo) {
			ufbxi_check(ufbxi_read_scene_info(uc, child));
		}

	}

	return 1;
}

static bool ufbxi_match_version_string(const char *fmt, ufbx_string str, uint32_t *p_version)
{
	size_t num_ix = 0;
	size_t pos = 0;
	while (*fmt) {
		char c = *fmt++;
		if (c >= 'a' && c <= 'z') {
			if (pos >= str.length) return false;
			char s = str.data[pos];
			if (s != c && (int)s + (int)('a' - 'A') != (int)c) return false;
			pos++;
		} else if (c == ' ') {
			while (pos < str.length) {
				char s = str.data[pos];
				if (s != ' ' && s != '\t') break;
				pos++;
			}
		} else if (c == '-') {
			while (pos < str.length) {
				char s = str.data[pos];
				if (s == '-') break;
				pos++;
			}
			if (pos >= str.length) return false;
			pos++;
		} else if (c == '/' || c == '.' || c == '(' || c == ')') {
			if (pos >= str.length) return false;
			if (str.data[pos] != c) return false;
			pos++;
		} else if (c == '?') {
			uint32_t num = 0;
			size_t len = 0;
			while (pos < str.length) {
				char s = str.data[pos];
				if (!(s >= '0' && s <= '9')) break;
				num = num*10 + (uint32_t)(s - '0');
				pos++;
				len++;
			}
			if (len == 0) return false;
			p_version[num_ix++] = num;
		} else {
			ufbx_assert(0 && "Unhandled match character");
		}
	}

	return true;
}

ufbxi_nodiscard static int ufbxi_match_exporter(ufbxi_context *uc)
{
	ufbx_string creator = uc->scene.metadata.creator;
	uint32_t version[3] = { 0 };
	if (ufbxi_match_version_string("blender-- ?.?.?", creator, version)) {
		uc->exporter = UFBX_EXPORTER_BLENDER_BINARY;
		uc->exporter_version = ufbx_pack_version(version[0], version[1], version[2]);
	} else if (ufbxi_match_version_string("blender- ?.?", creator, version)) {
		uc->exporter = UFBX_EXPORTER_BLENDER_BINARY;
		uc->exporter_version = ufbx_pack_version(version[0], version[1], 0);
	} else if (ufbxi_match_version_string("blender version ?.?", creator, version)) {
		uc->exporter = UFBX_EXPORTER_BLENDER_ASCII;
		uc->exporter_version = ufbx_pack_version(version[0], version[1], 0);
	} else if (ufbxi_match_version_string("fbx sdk/fbx plugins version ?.?", creator, version)) {
		uc->exporter = UFBX_EXPORTER_FBX_SDK;
		uc->exporter_version = ufbx_pack_version(version[0], version[1], 0);
	} else if (ufbxi_match_version_string("fbx sdk/fbx plugins build ?", creator, version)) {
		uc->exporter = UFBX_EXPORTER_FBX_SDK;
		uc->exporter_version = ufbx_pack_version(version[0]/10000u, version[0]/100u%100u, version[0]%100u);
	} else if (ufbxi_match_version_string("motionbuilder version ?.?", creator, version)) {
		uc->exporter = UFBX_EXPORTER_MOTION_BUILDER;
		uc->exporter_version = ufbx_pack_version(version[0], version[1], 0);
	} else if (ufbxi_match_version_string("motionbuilder/mocap/online version ?.?", creator, version)) {
		uc->exporter = UFBX_EXPORTER_MOTION_BUILDER;
		uc->exporter_version = ufbx_pack_version(version[0], version[1], 0);
	} else if (ufbxi_match_version_string("fbx unity export version ?.?", creator, version)) {
		uc->exporter = UFBX_EXPORTER_BC_UNITY_EXPORTER;
		uc->exporter_version = ufbx_pack_version(version[0], version[1], 0);
	} else if (ufbxi_match_version_string("fbx unity export version ?.?.?", creator, version)) {
		uc->exporter = UFBX_EXPORTER_BC_UNITY_EXPORTER;
		uc->exporter_version = ufbx_pack_version(version[0], version[1], version[2]);
	} else if (ufbxi_match_version_string("made using asset forge", creator, version)) {
		uc->exporter = UFBX_EXPORTER_BC_UNITY_EXPORTER;
	} else if (ufbxi_match_version_string("model created by kenney", creator, version)) {
		uc->exporter = UFBX_EXPORTER_BC_UNITY_EXPORTER;
	}

	uc->scene.metadata.exporter = uc->exporter;
	uc->scene.metadata.exporter_version = uc->exporter_version;

	// Un-detect the exporter in `ufbxi_context` to disable special cases
	if (uc->opts.disable_quirks) {
		uc->exporter = UFBX_EXPORTER_UNKNOWN;
		uc->exporter_version = 0;
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_document(ufbxi_context *uc)
{
	bool found_root_id = 0;

	for (;;) {
		ufbxi_node *child;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &child));
		if (!child) break;

		if (child->name == ufbxi_Document && !found_root_id) {
			// Post-7000: Try to find the first document node and root ID.
			// TODO: Multiple documents / roots?
			if (ufbxi_find_val1(child, ufbxi_RootNode, "L", &uc->root_id)) {
				found_root_id = true;
			}
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_definitions(ufbxi_context *uc)
{
	for (;;) {
		ufbxi_node *object;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &object));
		if (!object) break;

		if (object->name != ufbxi_ObjectType) continue;

		ufbxi_template *tmpl = ufbxi_push_zero(&uc->tmp_stack, ufbxi_template, 1);
		uc->num_templates++;
		ufbxi_check(tmpl);
		ufbxi_check(ufbxi_get_val1(object, "C", (char**)&tmpl->type));

		// Pre-7000 FBX versions don't have property templates, they just have
		// the object counts by themselves.
		ufbxi_node *props = ufbxi_find_child(object, ufbxi_PropertyTemplate);
		if (props) {
			ufbxi_check(ufbxi_get_val1(props, "S", &tmpl->sub_type));

			// Remove the "Fbx" prefix from sub-types, remember to re-intern!
			if (tmpl->sub_type.length > 3 && !strncmp(tmpl->sub_type.data, "Fbx", 3)) {
				tmpl->sub_type.data += 3;
				tmpl->sub_type.length -= 3;

				// HACK: LOD groups use LODGroup for Template, LodGroup for Object?
				if (tmpl->sub_type.length == 8 && !memcmp(tmpl->sub_type.data, "LODGroup", 8)) {
					tmpl->sub_type.data = "LodGroup";
				}

				ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, &tmpl->sub_type));
			}

			ufbxi_check(ufbxi_read_properties(uc, props, &tmpl->props));
		}
	}

	// TODO: Preserve only the `props` part of the templates
	uc->templates = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbxi_template, uc->num_templates);
	ufbxi_check(uc->templates);

	return 1;
}

ufbxi_nodiscard static ufbx_props *ufbxi_find_template(ufbxi_context *uc, const char *name, const char *sub_type)
{
	// TODO: Binary search
	ufbxi_for(ufbxi_template, tmpl, uc->templates, uc->num_templates) {
		if (tmpl->type == name) {

			// Check that sub_type matches unless the type is Material, Model, AnimationStack, AnimationLayer.
			// Those match to all sub-types.
			if (tmpl->type != ufbxi_Material && tmpl->type != ufbxi_Model
				&& tmpl->type != ufbxi_AnimationStack && tmpl->type != ufbxi_AnimationLayer) {
				if (tmpl->sub_type.data != sub_type) {
					return NULL;
				}
			}

			if (tmpl->props.num_props > 0) {
				return &tmpl->props;
			} else {
				return NULL;
			}
		}
	}
	return NULL;
}

// Name ID categories
#define UFBXI_SYNTHETIC_ID_BIT UINT64_C(0x8000000000000000)

ufbxi_nodiscard static int ufbxi_split_type_and_name(ufbxi_context *uc, ufbx_string type_and_name, ufbx_string *type, ufbx_string *name)
{
	// Name and type are packed in a single property as Type::Name (in ASCII)
	// or Name\x00\x01Type (in binary)
	const char *sep = uc->from_ascii ? "::" : "\x00\x01";
	size_t type_end = 2;
	for (; type_end <= type_and_name.length; type_end++) {
		const char *ch = type_and_name.data + type_end - 2;
		if (ch[0] == sep[0] && ch[1] == sep[1]) break;
	}

	// ???: ASCII and binary store type and name in different order
	if (type_end <= type_and_name.length) {
		if (uc->from_ascii) {
			name->data = type_and_name.data + type_end;
			name->length = type_and_name.length - type_end;
			type->data = type_and_name.data;
			type->length = type_end - 2;
		} else {
			name->data = type_and_name.data;
			name->length = type_end - 2;
			type->data = type_and_name.data + type_end;
			type->length = type_and_name.length - type_end;
		}
	} else {
		*name = type_and_name;
		type->data = ufbxi_empty_char;
		type->length = 0;
	}

	ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, type));
	ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, name));
	ufbxi_check(ufbxi_check_string(*type));
	ufbxi_check(ufbxi_check_string(*name));

	return 1;
}

ufbxi_nodiscard static ufbx_element *ufbxi_push_element_size(ufbxi_context *uc, ufbxi_element_info *info, size_t size, ufbx_element_type type)
{
	size_t aligned_size = (size + 7) & ~0x7;

	uint32_t typed_id = (uint32_t)uc->tmp_typed_element_offsets[type].num_items;

	ufbxi_check_return(ufbxi_push_copy(&uc->tmp_typed_element_offsets[type], size_t, 1, &uc->tmp_element_byte_offset), NULL);
	ufbxi_check_return(ufbxi_push_copy(&uc->tmp_element_offsets, size_t, 1, &uc->tmp_element_byte_offset), NULL);
	uc->tmp_element_byte_offset += aligned_size;

	ufbx_element *elem = (ufbx_element*)ufbxi_push_zero(&uc->tmp_elements, uint64_t, aligned_size/8);
	ufbxi_check_return(elem, NULL);
	elem->type = type;
	elem->element_id = uc->num_elements++;
	elem->typed_id = typed_id;
	elem->name = info->name;
	elem->props = info->props;

	uint32_t hash = ufbxi_hash64(info->fbx_id);
	ufbxi_fbx_id_entry *entry = ufbxi_map_insert(&uc->fbx_id_map, ufbxi_fbx_id_entry, hash, &info->fbx_id);
	ufbxi_check_return(entry, NULL);
	entry->fbx_id = info->fbx_id;
	entry->element_id = elem->element_id;

	return elem;
}

ufbxi_nodiscard ufbxi_noinline static ufbx_element *ufbxi_push_synthetic_element_size(ufbxi_context *uc, uint64_t *p_fbx_id, const char *name, size_t size, ufbx_element_type type)
{
	size_t aligned_size = (size + 7) & ~0x7;

	uint32_t typed_id = (uint32_t)uc->tmp_typed_element_offsets[type].num_items;

	ufbxi_check_return(ufbxi_push_copy(&uc->tmp_typed_element_offsets[type], size_t, 1, &uc->tmp_element_byte_offset), NULL);
	ufbxi_check_return(ufbxi_push_copy(&uc->tmp_element_offsets, size_t, 1, &uc->tmp_element_byte_offset), NULL);
	uc->tmp_element_byte_offset += aligned_size;

	ufbx_element *elem = (ufbx_element*)ufbxi_push_zero(&uc->tmp_elements, uint64_t, aligned_size/8);
	ufbxi_check_return(elem, NULL);
	elem->type = type;
	elem->element_id = uc->num_elements++;
	elem->typed_id = typed_id;
	if (name) {
		elem->name.data = name;
		elem->name.length = strlen(name);
	}

	uint64_t fbx_id = (uintptr_t)elem | UFBXI_SYNTHETIC_ID_BIT;
	*p_fbx_id = fbx_id;
	uint32_t hash = ufbxi_hash64(fbx_id);
	ufbxi_fbx_id_entry *entry = ufbxi_map_insert(&uc->fbx_id_map, ufbxi_fbx_id_entry, hash, &fbx_id);
	ufbxi_check_return(entry, NULL);
	entry->fbx_id = fbx_id;
	entry->element_id = elem->element_id;

	return elem;
}

#define ufbxi_push_element(uc, info, type_name, type_enum) (type_name*)ufbxi_push_element_size((uc), (info), sizeof(type_name), (type_enum))
#define ufbxi_push_synthetic_element(uc, p_fbx_id, name, type_name, type_enum) (type_name*)ufbxi_push_synthetic_element_size((uc), (p_fbx_id), (name), sizeof(type_name), (type_enum))

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_model(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	(void)node;
	ufbx_node *elem_node = ufbxi_push_element(uc, info, ufbx_node, UFBX_ELEMENT_NODE);
	ufbxi_check(elem_node);
	ufbxi_check(ufbxi_push_copy(&uc->tmp_node_ids, uint32_t, 1, &elem_node->element.element_id));
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_element(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info, size_t size, ufbx_element_type type)
{
	(void)node;
	ufbx_element *elem = ufbxi_push_element_size(uc, info, size, type);
	ufbxi_check(elem);
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_connect_oo(ufbxi_context *uc, uint64_t src, uint64_t dst)
{
	ufbxi_tmp_connection *conn = ufbxi_push(&uc->tmp_connections, ufbxi_tmp_connection, 1);
	ufbxi_check(conn);
	conn->src = src;
	conn->dst = dst;
	conn->src_prop = conn->dst_prop = ufbx_empty_string;
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_connect_op(ufbxi_context *uc, uint64_t src, uint64_t dst, ufbx_string prop)
{
	ufbxi_tmp_connection *conn = ufbxi_push(&uc->tmp_connections, ufbxi_tmp_connection, 1);
	ufbxi_check(conn);
	conn->src = src;
	conn->dst = dst;
	conn->src_prop = ufbx_empty_string;
	conn->dst_prop = prop;
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_connect_pp(ufbxi_context *uc, uint64_t src, uint64_t dst, ufbx_string src_prop, ufbx_string dst_prop)
{
	ufbxi_tmp_connection *conn = ufbxi_push(&uc->tmp_connections, ufbxi_tmp_connection, 1);
	ufbxi_check(conn);
	conn->src = src;
	conn->dst = dst;
	conn->src_prop = src_prop;
	conn->dst_prop = dst_prop;
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_unknown(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *element, ufbx_string type, ufbx_string sub_type, const char *node_name)
{
	(void)node;
	ufbx_unknown *unknown = ufbxi_push_element(uc, element, ufbx_unknown, UFBX_ELEMENT_UNKNOWN);
	ufbxi_check(unknown);
	unknown->type = type;
	unknown->sub_type = sub_type;
	unknown->super_type.data = node_name;
	unknown->super_type.length = strlen(node_name);
	return 1;
}

typedef struct {
	ufbx_vertex_vec3 elem;
	int32_t index;
} ufbxi_tangent_layer;

static ufbx_real ufbxi_zero_element[8] = { 0 };

// Sentinel pointers used for zero/sequential index buffers
static const int32_t ufbxi_sentinel_index_zero[1] = { 100000000 };
static const int32_t ufbxi_sentinel_index_consecutive[1] = { 123456789 };

ufbxi_nodiscard ufbxi_noinline static int ufbxi_check_indices(ufbxi_context *uc, int32_t **p_dst, int32_t *indices, bool owns_indices, size_t num_indices, size_t num_indexers, size_t num_elems)
{
	ufbxi_check(num_elems > 0 && num_elems < INT32_MAX);

	// If the indices are truncated extend them with `invalid_index`
	if (num_indices < num_indexers) {
		int32_t *new_indices = ufbxi_push(&uc->result, int32_t, num_indexers);
		ufbxi_check(new_indices);

		memcpy(new_indices, indices, sizeof(int32_t) * num_indices);
		for (size_t i = num_indices; i < num_indexers; i++) {
			new_indices[i] = (int32_t)num_elems - 1;
		}

		indices = new_indices;
		num_indices = num_indexers;
		owns_indices = true;
	}

	if (!uc->opts.allow_out_of_bounds_vertex_indices) {
		// Normalize out-of-bounds indices to `invalid_index`
		for (size_t i = 0; i < num_indices; i++) {
			int32_t ix = indices[i];
			if (ix < 0 || ix >= (int32_t)num_elems) {
				// If the indices refer to an external buffer we need to
				// allocate a separate buffer for them
				if (!owns_indices) {
					int32_t *new_indices = ufbxi_push(&uc->result, int32_t, num_indices);
					ufbxi_check(new_indices);
					memcpy(new_indices, indices, sizeof(int32_t) * num_indices);
					indices = new_indices;
					owns_indices = true;
				}
				indices[i] = (int32_t)num_elems - 1;
			}
		}
	}

	*p_dst = indices;

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_vertex_element(ufbxi_context *uc, ufbx_mesh *mesh, ufbxi_node *node,
	void *p_dst_data_void, int32_t **p_dst_index, bool *p_dst_unique_per_vertex, size_t *p_num_elems,
	const char *data_name, const char *index_name, char data_type, size_t num_components)
{
	ufbx_real **p_dst_data = (ufbx_real**)p_dst_data_void;

	ufbxi_value_array *data = ufbxi_find_array(node, data_name, data_type);
	ufbxi_value_array *indices = ufbxi_find_array(node, index_name, 'i');

	if (!uc->opts.strict) {
		if (!data) return 1;
	}

	ufbxi_check(data);
	ufbxi_check(data->size % num_components == 0);

	size_t num_elems = data->size / num_components;

	// HACK: If there's no elements at all keep the attribute as NULL
	// TODO: Strict mode for this?
	if (num_elems == 0) {
		return 1;
	}

	const char *mapping;
	ufbxi_check(ufbxi_find_val1(node, ufbxi_MappingInformationType, "C", (char**)&mapping));

	if (num_elems > mesh->num_indices) {
		num_elems = mesh->num_indices;
	}
	*p_num_elems = num_elems ? num_elems : 1;

	// Data array is always used as-is, if empty set the data to a global
	// zero buffer so invalid zero index can point to some valid data.
	// The zero data is offset by 4 elements to accomodate for invalid index (-1)
	if (num_elems > 0) {
		*p_dst_data = (ufbx_real*)data->data;
	} else {
		*p_dst_data = ufbxi_zero_element + 4;
	}

	if (indices) {
		size_t num_indices = indices->size;
		int32_t *index_data = (int32_t*)indices->data;

		if (mapping == ufbxi_ByPolygonVertex || mapping == ufbxi_ByPolygon) {

			// Indexed by polygon vertex: We can use the provided indices directly.
			ufbxi_check(ufbxi_check_indices(uc, p_dst_index, index_data, true, num_indices, mesh->num_indices, num_elems));
			*p_dst_unique_per_vertex = true;

		} else if (mapping == ufbxi_ByVertex || mapping == ufbxi_ByVertice) {

			// Indexed by vertex: Follow through the position index mapping to get the final indices.
			int32_t *new_index_data = ufbxi_push(&uc->result, int32_t, mesh->num_indices);
			ufbxi_check(new_index_data);

			int32_t *vert_ix = mesh->vertex_indices;
			for (size_t i = 0; i < mesh->num_indices; i++) {
				int32_t ix = vert_ix[i];
				if (ix >= 0 && (uint32_t)ix < num_indices) {
					new_index_data[i] = index_data[ix];
				} else {
					new_index_data[i] = -1;
				}
			}

			ufbxi_check(ufbxi_check_indices(uc, p_dst_index, new_index_data, true, mesh->num_indices, mesh->num_indices, num_elems));
			*p_dst_unique_per_vertex = true;

		} else if (mapping == ufbxi_AllSame) {

			// Indexed by all same: ??? This could be possibly used for making
			// holes with invalid indices, but that seems really fringe.
			// Just use the shared zero index buffer for this.
			uc->max_zero_indices = ufbxi_max_sz(uc->max_zero_indices, mesh->num_indices);
			*p_dst_index = (int32_t*)ufbxi_sentinel_index_zero;
			*p_dst_unique_per_vertex = true;

		} else {
			ufbxi_fail("Invalid mapping");
		}

	} else {

		if (mapping == ufbxi_ByPolygonVertex || mapping == ufbxi_ByPolygon) {

			// Direct by polygon index: Use shared consecutive array if there's enough
			// elements, otherwise use a unique truncated consecutive index array.
			if (num_elems >= mesh->num_indices) {
				uc->max_consecutive_indices = ufbxi_max_sz(uc->max_consecutive_indices, mesh->num_indices);
				*p_dst_index = (int32_t*)ufbxi_sentinel_index_consecutive;
			} else {
				int32_t *index_data = ufbxi_push(&uc->result, int32_t, mesh->num_indices);
				ufbxi_check(index_data);
				for (size_t i = 0; i < mesh->num_indices; i++) {
					index_data[i] = (int32_t)i;
				}
				ufbxi_check(ufbxi_check_indices(uc, p_dst_index, index_data, true, mesh->num_indices, mesh->num_indices, num_elems));
			}

		} else if (mapping == ufbxi_ByVertex || mapping == ufbxi_ByVertice) {

			// Direct by vertex: We can re-use the position indices..
			ufbxi_check(ufbxi_check_indices(uc, p_dst_index, mesh->vertex_position.indices, false, mesh->num_indices, mesh->num_indices, num_elems));
			*p_dst_unique_per_vertex = true;

		} else if (mapping == ufbxi_AllSame) {

			// Direct by all same: This cannot fail as the index list is just zero.
			uc->max_zero_indices = ufbxi_max_sz(uc->max_zero_indices, mesh->num_indices);
			*p_dst_index = (int32_t*)ufbxi_sentinel_index_zero;
			*p_dst_unique_per_vertex = true;

		} else {
			ufbxi_fail("Invalid mapping");
		}
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_truncated_array(ufbxi_context *uc, void *p_data, ufbxi_node *node, const char *name, char fmt, size_t size)
{
	ufbxi_value_array *arr = ufbxi_find_array(node, name, fmt);
	ufbxi_check(arr);

	void *data = arr->data;
	if (arr->size < size) {
		size_t elem_size = ufbxi_array_type_size(fmt);
		void *new_data = ufbxi_push_size(&uc->result, elem_size, size);
		ufbxi_check(new_data);
		memcpy(new_data, data, arr->size * elem_size);
		// Extend the array with the last element if possible
		if (arr->size > 0) {
			char *first_elem = (char*)data + (arr->size - 1) * elem_size;
			for (size_t i = arr->size; i < size; i++) {
				memcpy((char*)new_data + i * elem_size, first_elem, elem_size);
			}
		} else {
			memset((char*)new_data + arr->size * elem_size, 0, (size - arr->size) * elem_size);
		}
		data = new_data;
	}

	*(void**)p_data = data;
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_uv_sets(ufbxi_context *uc, ufbx_uv_set *sets, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_uv_set)));
	ufbxi_macro_stable_sort(ufbx_uv_set, 32, sets, uc->tmp_arr, count, ( a->index < b->index ));
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_color_sets(ufbxi_context *uc, ufbx_color_set *sets, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_color_set)));
	ufbxi_macro_stable_sort(ufbx_color_set, 32, sets, uc->tmp_arr, count, ( a->index < b->index ));
	return 1;
}

typedef struct ufbxi_blend_offset {
	int32_t vertex;
	ufbx_vec3 position_offset;
	ufbx_vec3 normal_offset;
} ufbxi_blend_offset;

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_blend_offsets(ufbxi_context *uc, ufbxi_blend_offset *offsets, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbxi_blend_offset)));
	ufbxi_macro_stable_sort(ufbxi_blend_offset, 16, offsets, uc->tmp_arr, count, ( a->vertex < b->vertex ));
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_shape(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbxi_node *node_vertices = ufbxi_find_child(node, ufbxi_Vertices);
	ufbxi_node *node_indices = ufbxi_find_child(node, ufbxi_Indexes);
	if (!node_vertices || !node_indices) return 1;

	ufbx_blend_shape *shape = ufbxi_push_element(uc, info, ufbx_blend_shape, UFBX_ELEMENT_BLEND_SHAPE);
	ufbxi_check(shape);

	if (uc->opts.ignore_geometry) return 1;

	// TODO: Normals

	ufbxi_value_array *vertices = ufbxi_get_array(node_vertices, 'r');
	ufbxi_value_array *indices = ufbxi_get_array(node_indices, 'i');

	ufbxi_check(vertices && indices);
	ufbxi_check(vertices->size % 3 == 0);
	ufbxi_check(indices->size == vertices->size / 3);

	size_t num_offsets = indices->size;
	int32_t *vertex_indices = (int32_t*)indices->data;

	shape->num_offsets = num_offsets;
	shape->position_offsets = (ufbx_vec3*)vertices->data;
	shape->offset_vertices = vertex_indices;

	// Sort the blend shape vertices only if absolutely necessary
	bool sorted = true;
	for (size_t i = 1; i < num_offsets; i++) {
		if (vertex_indices[i - 1] > vertex_indices[i]) {
			sorted = false;
			break;
		}
	}

	if (!sorted) {
		ufbxi_blend_offset *offsets = ufbxi_push(&uc->tmp_stack, ufbxi_blend_offset, num_offsets);
		ufbxi_check(offsets);

		for (size_t i = 0; i < num_offsets; i++) {
			offsets[i].vertex = shape->offset_vertices[i];
			offsets[i].position_offset = shape->position_offsets[i];
			if (shape->normal_offsets) offsets[i].normal_offset = shape->normal_offsets[i];
		}

		ufbxi_check(ufbxi_sort_blend_offsets(uc, offsets, num_offsets));

		for (size_t i = 0; i < num_offsets; i++) {
			shape->offset_vertices[i] = offsets[i].vertex;
			shape->position_offsets[i] = offsets[i].position_offset;
			if (shape->normal_offsets) shape->normal_offsets[i] = offsets[i].normal_offset;
		}
		ufbxi_pop(&uc->tmp_stack, ufbxi_blend_offset, num_offsets, NULL);
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_synthetic_blend_shapes(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_blend_deformer *deformer = NULL;
	uint64_t deformer_fbx_id = 0;

	ufbxi_for (ufbxi_node, n, node->children, node->num_children) {
		if (n->name != ufbxi_Shape) continue;

		ufbx_string name;
		ufbxi_check(ufbxi_get_val1(n, "S", &name));

		if (deformer == NULL) {
			deformer = ufbxi_push_synthetic_element(uc, &deformer_fbx_id, name.data, ufbx_blend_deformer, UFBX_ELEMENT_BLEND_DEFORMER);
			ufbxi_check(deformer);
			ufbxi_check(ufbxi_connect_oo(uc, deformer_fbx_id, info->fbx_id));
		}

		uint64_t channel_fbx_id = 0;
		ufbx_blend_channel *channel = ufbxi_push_synthetic_element(uc, &channel_fbx_id, name.data, ufbx_blend_channel, UFBX_ELEMENT_BLEND_CHANNEL);
		ufbxi_check(channel);

		ufbx_real_list weight_list = { NULL, 0 };
		ufbxi_check(ufbxi_push_copy(&uc->tmp_full_weights, ufbx_real_list, 1, &weight_list));

		size_t num_shape_props = 1;
		ufbx_prop *shape_props = ufbxi_push_zero(&uc->result, ufbx_prop, num_shape_props);
		ufbxi_check(shape_props);
		shape_props[0].name.data = ufbxi_DeformPercent;
		shape_props[0].name.length = sizeof(ufbxi_DeformPercent) - 1;
		shape_props[0].internal_key = ufbxi_get_name_key_c(ufbxi_DeformPercent);
		shape_props[0].type = UFBX_PROP_NUMBER;
		shape_props[0].value_real = (ufbx_real)0.0;
		shape_props[0].value_str = ufbx_empty_string;

		ufbx_prop *self_prop = ufbx_find_prop_len(&info->props, name.data, name.length);
		if (self_prop && (self_prop->type == UFBX_PROP_NUMBER || self_prop->type == UFBX_PROP_INTEGER)) {
			shape_props[0].value_real = self_prop->value_real;
			ufbxi_check(ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name, shape_props[0].name));
		} else if (uc->version < 6000) {
			ufbxi_check(ufbxi_connect_pp(uc, info->fbx_id, channel_fbx_id, name, shape_props[0].name));
		}

		channel->name = name;
		channel->props.props = shape_props;
		channel->props.num_props = num_shape_props;

		ufbxi_element_info shape_info = { 0 };

		// HACK: Derive an FBX ID for the shape from the channel. Synthetic IDs are
		// equivalent to resulting object pointers (as `uintptr_t`) so incrementing
		// it gives us an unique ID (as long as `sizeof(ufbx_blend_channel) > 0`...)
		shape_info.fbx_id = channel_fbx_id + 1;
		shape_info.name = name;

		ufbxi_check(ufbxi_read_shape(uc, n, &shape_info));

		ufbxi_check(ufbxi_connect_oo(uc, channel_fbx_id, deformer_fbx_id));
		ufbxi_check(ufbxi_connect_oo(uc, shape_info.fbx_id, channel_fbx_id));
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_mesh(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_mesh *ufbxi_restrict mesh = ufbxi_push_element(uc, info, ufbx_mesh, UFBX_ELEMENT_MESH);
	ufbxi_check(mesh);

	// In up to version 7100 FBX files blend shapes are contained within the same geometry node
	if (uc->version <= 7100) {
		ufbxi_check(ufbxi_read_synthetic_blend_shapes(uc, node, info));
	}

	mesh->vertex_position.value_reals = 3;
	mesh->vertex_normal.value_reals = 3;
	mesh->vertex_uv.value_reals = 2;
	mesh->vertex_tangent.value_reals = 3;
	mesh->vertex_bitangent.value_reals = 3;
	mesh->vertex_color.value_reals = 4;
	mesh->vertex_crease.value_reals = 1;
	mesh->skinned_position.value_reals = 3;
	mesh->skinned_normal.value_reals = 3;

	if (uc->opts.ignore_geometry) return 1;

	ufbxi_node *node_vertices = ufbxi_find_child(node, ufbxi_Vertices);
	ufbxi_node *node_indices = ufbxi_find_child(node, ufbxi_PolygonVertexIndex);

	ufbxi_check(node_vertices);
	ufbxi_check(node_indices);

	ufbxi_value_array *vertices = ufbxi_get_array(node_vertices, 'r');
	ufbxi_value_array *indices = ufbxi_get_array(node_indices, 'i');
	ufbxi_value_array *edge_indices = ufbxi_find_array(node, ufbxi_Edges, 'i');
	ufbxi_check(vertices && indices);
	ufbxi_check(vertices->size % 3 == 0);

	mesh->num_vertices = vertices->size / 3;
	mesh->num_indices = indices->size;

	int32_t *index_data = (int32_t*)indices->data;

	mesh->vertices = (ufbx_vec3*)vertices->data;
	mesh->vertex_indices = index_data;

	mesh->vertex_position.data = (ufbx_vec3*)vertices->data;
	mesh->vertex_position.indices = index_data;
	mesh->vertex_position.num_values = mesh->num_vertices;


	// Check/make sure that the last index is negated (last of polygon)
	if (mesh->num_indices > 0) {
		if (index_data[mesh->num_indices - 1] > 0) {
			if (uc->opts.strict) ufbxi_fail("Non-negated last index");
			index_data[mesh->num_indices - 1] = ~index_data[mesh->num_indices - 1];
		}
	}

	// Read edges before un-negating the indices
	if (edge_indices) {
		size_t num_edges = edge_indices->size;
		ufbx_edge *edges = ufbxi_push(&uc->result, ufbx_edge, num_edges);
		ufbxi_check(edges);

		size_t dst_ix = 0;

		// Edges are represented using a single index into PolygonVertexIndex.
		// The edge is between two consecutive vertices in the polygon.
		int32_t *edge_data = (int32_t*)edge_indices->data;
		for (size_t i = 0; i < num_edges; i++) {
			int32_t index_ix = edge_data[i];
			if (index_ix < 0 || (size_t)index_ix >= mesh->num_indices) {
				if (uc->opts.strict) ufbxi_fail("Edge index out of bounds");
				continue;
			}
			edges[dst_ix].indices[0] = index_ix;
			if (index_data[index_ix] < 0) {
				// Previous index is the last one of this polygon, rewind to first index.
				while (index_ix > 0 && index_data[index_ix - 1] >= 0) {
					index_ix--;
				}
			} else {
				// Connect to the next index in the same polygon
				index_ix++;
			}
			ufbxi_check(index_ix >= 0 && (size_t)index_ix < mesh->num_indices);
			edges[dst_ix].indices[1] = index_ix;
			dst_ix++;
		}

		mesh->edges = edges;
		mesh->num_edges = dst_ix;
	}

	// Count the number of faces and allocate the index list
	// Indices less than zero (~actual_index) ends a polygon
	size_t num_total_faces = 0;
	ufbxi_for (int32_t, p_ix, index_data, mesh->num_indices) {
		if (*p_ix < 0) num_total_faces++;
	}
	mesh->faces = ufbxi_push(&uc->result, ufbx_face, num_total_faces);
	ufbxi_check(mesh->faces);

	size_t num_triangles = 0;
	size_t num_bad_faces = 0;
	size_t max_face_triangles = 0;

	ufbx_face *dst_face = mesh->faces;
	int32_t *p_face_begin = index_data;
	ufbxi_for (int32_t, p_ix, index_data, mesh->num_indices) {
		int32_t ix = *p_ix;
		// Un-negate final indices of polygons
		if (ix < 0) {
			ix = ~ix;
			*p_ix =  ix;
			uint32_t num_indices = (uint32_t)((p_ix - p_face_begin) + 1);
			dst_face->index_begin = (uint32_t)(p_face_begin - index_data);
			dst_face->num_indices = num_indices;
			if (num_indices >= 3) {
				num_triangles += num_indices - 2;
				max_face_triangles = ufbxi_max_sz(max_face_triangles, num_indices - 2);
			} else {
				num_bad_faces++;
			}
			dst_face++;
			p_face_begin = p_ix + 1;
		}
		ufbxi_check((size_t)ix < mesh->num_vertices);
	}

	mesh->vertex_position.indices = index_data;
	mesh->num_faces = dst_face - mesh->faces;
	mesh->num_triangles = num_triangles;
	mesh->max_face_triangles = max_face_triangles;
	mesh->num_bad_faces = num_bad_faces;

	mesh->vertex_first_index = ufbxi_push(&uc->result, int32_t, mesh->num_vertices);
	ufbxi_check(mesh->vertex_first_index);

	ufbxi_for(int32_t, p_vx_ix, mesh->vertex_first_index, mesh->num_vertices) {
		*p_vx_ix = -1;
	}

	for (size_t ix = 0; ix < mesh->num_indices; ix++) {
		int32_t vx = mesh->vertex_indices[ix];
		if (vx >= 0 && (size_t)vx < mesh->num_vertices) {
			if (mesh->vertex_first_index[vx] < 0) {
				mesh->vertex_first_index[vx] = (int32_t)ix;
			}
		} else if (!uc->opts.allow_out_of_bounds_vertex_indices) {
			if (uc->opts.strict) ufbxi_fail("Index out of range");
			mesh->vertex_indices[ix] = 0;
		}
	}

	// HACK(consecutive-faces): Prepare for finalize to re-use a consecutive/zero
	// index buffer for face materials..
	uc->max_zero_indices = ufbxi_max_sz(uc->max_zero_indices, mesh->num_faces);
	uc->max_consecutive_indices = ufbxi_max_sz(uc->max_consecutive_indices, mesh->num_faces);

	// Count the number of UV/color sets
	size_t num_uv = 0, num_color = 0, num_bitangents = 0, num_tangents = 0;
	ufbxi_for (ufbxi_node, n, node->children, node->num_children) {
		if (n->name == ufbxi_LayerElementUV) num_uv++;
		if (n->name == ufbxi_LayerElementColor) num_color++;
		if (n->name == ufbxi_LayerElementBinormal) num_bitangents++;
		if (n->name == ufbxi_LayerElementTangent) num_tangents++;
	}

	size_t num_textures = 0;

	ufbxi_tangent_layer *bitangents = ufbxi_push_zero(&uc->tmp_stack, ufbxi_tangent_layer, num_bitangents);
	ufbxi_tangent_layer *tangents = ufbxi_push_zero(&uc->tmp_stack, ufbxi_tangent_layer, num_tangents);
	ufbxi_check(bitangents);
	ufbxi_check(tangents);

	mesh->uv_sets.data = ufbxi_push_zero(&uc->result, ufbx_uv_set, num_uv);
	mesh->color_sets.data = ufbxi_push_zero(&uc->result, ufbx_color_set, num_color);
	ufbxi_check(mesh->uv_sets.data);
	ufbxi_check(mesh->color_sets.data);

	size_t num_bitangents_read = 0, num_tangents_read = 0;
	ufbxi_for (ufbxi_node, n, node->children, node->num_children) {
		if (n->name[0] != 'L') continue; // All names start with 'LayerElement*'

		if (n->name == ufbxi_LayerElementNormal) {
			if (mesh->vertex_normal.data) continue;
			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_normal.data, &mesh->vertex_normal.indices, 
				&mesh->vertex_normal.unique_per_vertex, &mesh->vertex_normal.num_values, ufbxi_Normals, ufbxi_NormalsIndex, 'r', 3));
		} else if (n->name == ufbxi_LayerElementBinormal) {
			ufbxi_tangent_layer *layer = &bitangents[num_bitangents_read++];
			layer->elem.value_reals = 3;

			ufbxi_ignore(ufbxi_get_val1(n, "I", &layer->index));
			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.data, &layer->elem.indices,
				&layer->elem.unique_per_vertex, &layer->elem.num_values, ufbxi_Binormals, ufbxi_BinormalsIndex, 'r', 3));
			if (!layer->elem.data) num_bitangents_read--;

		} else if (n->name == ufbxi_LayerElementTangent) {
			ufbxi_tangent_layer *layer = &tangents[num_tangents_read++];
			layer->elem.value_reals = 3;

			ufbxi_ignore(ufbxi_get_val1(n, "I", &layer->index));
			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &layer->elem.data, &layer->elem.indices,
				&layer->elem.unique_per_vertex, &layer->elem.num_values, ufbxi_Tangents, ufbxi_TangentsIndex, 'r', 3));
			if (!layer->elem.data) num_tangents_read--;

		} else if (n->name == ufbxi_LayerElementUV) {
			ufbx_uv_set *set = &mesh->uv_sets.data[mesh->uv_sets.count++];
			set->vertex_uv.value_reals = 2;
			set->vertex_tangent.value_reals = 3;
			set->vertex_bitangent.value_reals = 3;

			ufbxi_ignore(ufbxi_get_val1(n, "I", &set->index));
			if (!ufbxi_find_val1(n, ufbxi_Name, "S", &set->name)) {
				set->name = ufbx_empty_string;
			}

			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_uv.data, &set->vertex_uv.indices,
				&set->vertex_uv.unique_per_vertex, &set->vertex_uv.num_values, ufbxi_UV, ufbxi_UVIndex, 'r', 2));
			if (!set->vertex_uv.data) mesh->uv_sets.count--;

		} else if (n->name == ufbxi_LayerElementColor) {
			ufbx_color_set *set = &mesh->color_sets.data[mesh->color_sets.count++];
			set->vertex_color.value_reals = 4;

			ufbxi_ignore(ufbxi_get_val1(n, "I", &set->index));
			if (!ufbxi_find_val1(n, ufbxi_Name, "S", &set->name)) {
				set->name = ufbx_empty_string;
			}

			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &set->vertex_color.data, &set->vertex_color.indices,
				&set->vertex_color.unique_per_vertex, &set->vertex_color.num_values, ufbxi_Colors, ufbxi_ColorIndex, 'r', 4));
			if (!set->vertex_color.data) mesh->color_sets.count--;

		} else if (n->name == ufbxi_LayerElementVertexCrease) {
			ufbxi_check(ufbxi_read_vertex_element(uc, mesh, n, &mesh->vertex_crease.data, &mesh->vertex_crease.indices,
				&mesh->vertex_crease.unique_per_vertex, &mesh->vertex_crease.num_values, ufbxi_VertexCrease, ufbxi_VertexCreaseIndex, 'r', 1));
		} else if (n->name == ufbxi_LayerElementEdgeCrease) {
			const char *mapping;
			ufbxi_check(ufbxi_find_val1(n, ufbxi_MappingInformationType, "C", (char**)&mapping));
			if (mapping == ufbxi_ByEdge) {
				if (mesh->edge_crease) continue;
				ufbxi_check(ufbxi_read_truncated_array(uc, &mesh->edge_crease, n, ufbxi_EdgeCrease, 'r', mesh->num_edges));
			}
		} else if (n->name == ufbxi_LayerElementSmoothing) {
			const char *mapping;
			ufbxi_check(ufbxi_find_val1(n, ufbxi_MappingInformationType, "C", (char**)&mapping));
			if (mapping == ufbxi_ByEdge) {
				if (mesh->edge_smoothing) continue;
				ufbxi_check(ufbxi_read_truncated_array(uc, &mesh->edge_smoothing, n, ufbxi_Smoothing, 'b', mesh->num_edges));
			} else if (mapping == ufbxi_ByPolygon) {
				if (mesh->face_smoothing) continue;
				ufbxi_check(ufbxi_read_truncated_array(uc, &mesh->face_smoothing, n, ufbxi_Smoothing, 'b', mesh->num_faces));
			}
		} else if (n->name == ufbxi_LayerElementMaterial) {
			if (mesh->face_material) continue;
			const char *mapping;
			ufbxi_check(ufbxi_find_val1(n, ufbxi_MappingInformationType, "C", (char**)&mapping));
			if (mapping == ufbxi_ByPolygon) {
				ufbxi_check(ufbxi_read_truncated_array(uc, &mesh->face_material, n, ufbxi_Materials, 'i', mesh->num_faces));
			} else if (mapping == ufbxi_AllSame) {
				ufbxi_value_array *arr = ufbxi_find_array(n, ufbxi_Materials, 'i');
				ufbxi_check(arr && arr->size >= 1);
				int32_t material = *(int32_t*)arr->data;
				if (material == 0) {
					mesh->face_material = (int32_t*)ufbxi_sentinel_index_zero;
				} else {
					mesh->face_material = ufbxi_push(&uc->result, int32_t, mesh->num_faces);
					ufbxi_check(mesh->face_material);
					ufbxi_for(int32_t, p_mat, mesh->face_material, mesh->num_faces) {
						*p_mat = material;
					}
				}
			}
		} else if (!strncmp(n->name, "LayerElement", 12)) {

			// Make sure the name has no internal zero bytes
			ufbxi_check(!memchr(n->name, '\0', n->name_len));

			// What?! 6x00 stores textures in mesh geometry, eg. "LayerElementTexture",
			// "LayerElementDiffuseFactorTextures", "LayerElementEmissive_Textures"...
			ufbx_string prop_name = ufbx_empty_string;
			if (n->name_len > 20 && !strcmp(n->name + n->name_len - 8, "Textures")) {
				prop_name.data = n->name + 12;
				prop_name.length = n->name_len - 20;
				if (prop_name.data[prop_name.length - 1] == '_') {
					prop_name.length -= 1;
				}
			} else if (!strcmp(n->name, "LayerElementTexture")) {
				prop_name.data = "Diffuse";
				prop_name.length = 7;
			}

			if (prop_name.length > 0) {
				ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, &prop_name));
				const char *mapping;
				if (ufbxi_find_val1(n, ufbxi_MappingInformationType, "C", (char**)&mapping)) {
					ufbxi_value_array *arr = ufbxi_find_array(n, ufbxi_TextureId, 'i');

					ufbxi_tmp_mesh_texture *tex = ufbxi_push_zero(&uc->tmp_mesh_textures, ufbxi_tmp_mesh_texture, 1);
					ufbxi_check(tex);
					if (arr) {
						tex->face_texture = (int32_t*)arr->data;
						tex->num_faces = arr->size;
					}
					tex->prop_name = prop_name;
					tex->all_same = (mapping == ufbxi_AllSame);
					num_textures++;
				}
			}
		}
	}

	// Always use a default zero material, this will be removed if no materials are found
	if (!mesh->face_material) {
		uc->max_zero_indices = ufbxi_max_sz(uc->max_zero_indices, mesh->num_faces);
		mesh->face_material = (int32_t*)ufbxi_sentinel_index_zero;
	}

	if (uc->opts.strict) {
		ufbxi_check(mesh->uv_sets.count == num_uv);
		ufbxi_check(mesh->color_sets.count == num_color);
		ufbxi_check(num_bitangents_read == num_bitangents);
		ufbxi_check(num_tangents_read == num_tangents);
	}

	// Connect bitangents/tangents to UV sets
	ufbxi_for (ufbxi_node, n, node->children, node->num_children) {
		if (n->name != ufbxi_Layer) continue;
		ufbx_uv_set *uv_set = NULL;
		ufbxi_tangent_layer *bitangent_layer = NULL;
		ufbxi_tangent_layer *tangent_layer = NULL;

		ufbxi_for (ufbxi_node, c, n->children, n->num_children) {
			int32_t index;
			const char *type;
			if (c->name != ufbxi_LayerElement) continue;
			if (!ufbxi_find_val1(c, ufbxi_TypedIndex, "I", &index)) continue;
			if (!ufbxi_find_val1(c, ufbxi_Type, "C", (char**)&type)) continue;

			if (type == ufbxi_LayerElementUV) {
				ufbxi_for(ufbx_uv_set, set, mesh->uv_sets.data, mesh->uv_sets.count) {
					if (set->index == index) {
						uv_set = set;
						break;
					}
				}
			} else if (type == ufbxi_LayerElementBinormal) {
				ufbxi_for(ufbxi_tangent_layer, layer, bitangents, num_bitangents_read) {
					if (layer->index == index) {
						bitangent_layer = layer;
						break;
					}
				}
			} else if (type == ufbxi_LayerElementTangent) {
				ufbxi_for(ufbxi_tangent_layer, layer, tangents, num_tangents_read) {
					if (layer->index == index) {
						tangent_layer = layer;
						break;
					}
				}
			}
		}

		if (uv_set) {
			if (bitangent_layer) {
				uv_set->vertex_bitangent = bitangent_layer->elem;
			}
			if (tangent_layer) {
				uv_set->vertex_tangent = tangent_layer->elem;
			}
		}
	}

	mesh->skinned_is_local = true;
	mesh->skinned_position = mesh->vertex_position;
	mesh->skinned_normal = mesh->vertex_normal;

	// Sort UV and color sets by set index
	ufbxi_check(ufbxi_sort_uv_sets(uc, mesh->uv_sets.data, mesh->uv_sets.count));
	ufbxi_check(ufbxi_sort_color_sets(uc, mesh->color_sets.data, mesh->color_sets.count));

	if (num_textures > 0) {
		ufbxi_mesh_extra *extra = ufbxi_push_element_extra(uc, mesh->element.element_id, ufbxi_mesh_extra);
		ufbxi_check(extra);
		extra->texture_count = num_textures;
		extra->texture_arr = ufbxi_push_pop(&uc->tmp, &uc->tmp_mesh_textures, ufbxi_tmp_mesh_texture, num_textures);
		ufbxi_check(extra->texture_arr);
	}

	// Subdivision

	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_PreviewDivisionLevels, "I", &mesh->subdivision_preview_levels));
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_RenderDivisionLevels, "I", &mesh->subdivision_render_levels));

	int32_t smoothness, boundary;
	if (ufbxi_find_val1(node, ufbxi_Smoothness, "I", &smoothness)) {
		if (smoothness >= 0 && smoothness <= UFBX_SUBDIVISION_DISPLAY_SMOOTH) {
			mesh->subdivision_display_mode = (ufbx_subdivision_display_mode)smoothness;
		}
	}
	if (ufbxi_find_val1(node, ufbxi_BoundaryRule, "I", &boundary)) {
		if (boundary >= 0 && boundary <= UFBX_SUBDIVISION_BOUNDARY_SHARP_CORNERS - 1) {
			mesh->subdivision_boundary = (ufbx_subdivision_boundary)(boundary + 1);
		}
	}

	return 1;
}

ufbxi_noinline static ufbx_nurbs_topology ufbxi_read_nurbs_topology(const char *form)
{
	if (form == ufbxi_Open) {
		return UFBX_NURBS_TOPOLOGY_OPEN;
	} else if (form == ufbxi_Closed) {
		return UFBX_NURBS_TOPOLOGY_CLOSED;
	} else if (form == ufbxi_Periodic) {
		return UFBX_NURBS_TOPOLOGY_PERIODIC;
	}
	return UFBX_NURBS_TOPOLOGY_OPEN;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_nurbs_curve(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_nurbs_curve *nurbs = ufbxi_push_element(uc, info, ufbx_nurbs_curve, UFBX_ELEMENT_NURBS_CURVE);
	ufbxi_check(nurbs);

	int32_t dimension = 3;

	const char *form = NULL;
	ufbxi_check(ufbxi_find_val1(node, ufbxi_Order, "I", &nurbs->basis.order));
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_Dimension, "I", &dimension));
	ufbxi_check(ufbxi_find_val1(node, ufbxi_Form, "C", (char**)&form));
	nurbs->basis.topology = ufbxi_read_nurbs_topology(form);
	nurbs->basis.is_2d = dimension == 2;

	if (!uc->opts.ignore_geometry) {
		ufbxi_value_array *points = ufbxi_find_array(node, ufbxi_Points, 'r');
		ufbxi_value_array *knot = ufbxi_find_array(node, ufbxi_KnotVector, 'r');
		ufbxi_check(points);
		ufbxi_check(knot);
		ufbxi_check(points->size % 4 == 0);

		nurbs->control_points.count = points->size / 4;
		nurbs->control_points.data = (ufbx_vec4*)points->data;
		nurbs->basis.knot_vector.data = (ufbx_real*)knot->data;
		nurbs->basis.knot_vector.count = knot->size;
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_nurbs_surface(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_nurbs_surface *nurbs = ufbxi_push_element(uc, info, ufbx_nurbs_surface, UFBX_ELEMENT_NURBS_SURFACE);
	ufbxi_check(nurbs);

	const char *form_u = NULL, *form_v = NULL;
	size_t dimension_u = 0, dimension_v = 0;
	int32_t step_u = 0, step_v = 0;
	ufbxi_check(ufbxi_find_val2(node, ufbxi_NurbsSurfaceOrder, "II", &nurbs->basis_u.order, &nurbs->basis_v.order));
	ufbxi_check(ufbxi_find_val2(node, ufbxi_Dimensions, "ZZ", &dimension_u, &dimension_v));
	ufbxi_check(ufbxi_find_val2(node, ufbxi_Step, "II", &step_u, &step_v));
	ufbxi_check(ufbxi_find_val2(node, ufbxi_Form, "CC", (char**)&form_u, (char**)&form_v));
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_FlipNormals, "B", &nurbs->flip_normals));
	nurbs->basis_u.topology = ufbxi_read_nurbs_topology(form_u);
	nurbs->basis_v.topology = ufbxi_read_nurbs_topology(form_v);
	nurbs->num_control_points_u = dimension_u;
	nurbs->num_control_points_v = dimension_v;
	nurbs->span_subdivision_u = step_u > 0 ? step_u : 4;
	nurbs->span_subdivision_v = step_v > 0 ? step_v : 4;

	if (!uc->opts.ignore_geometry) {
		ufbxi_value_array *points = ufbxi_find_array(node, ufbxi_Points, 'r');
		ufbxi_value_array *knot_u = ufbxi_find_array(node, ufbxi_KnotVectorU, 'r');
		ufbxi_value_array *knot_v = ufbxi_find_array(node, ufbxi_KnotVectorV, 'r');
		ufbxi_check(points);
		ufbxi_check(knot_u);
		ufbxi_check(knot_v);
		ufbxi_check(points->size % 4 == 0);
		ufbxi_check(points->size / 4 == (size_t)dimension_u * (size_t)dimension_v);

		nurbs->control_points.count = points->size / 4;
		nurbs->control_points.data = (ufbx_vec4*)points->data;
		nurbs->basis_u.knot_vector.data = (ufbx_real*)knot_u->data;
		nurbs->basis_u.knot_vector.count = knot_u->size;
		nurbs->basis_v.knot_vector.data = (ufbx_real*)knot_v->data;
		nurbs->basis_v.knot_vector.count = knot_v->size;
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_line(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_line_curve *line = ufbxi_push_element(uc, info, ufbx_line_curve, UFBX_ELEMENT_LINE_CURVE);
	ufbxi_check(line);

	if (!uc->opts.ignore_geometry) {
		ufbxi_value_array *points = ufbxi_find_array(node, ufbxi_Points, 'r');
		ufbxi_value_array *points_index = ufbxi_find_array(node, ufbxi_PointsIndex, 'i');
		ufbxi_check(points);
		ufbxi_check(points_index);
		ufbxi_check(points->size % 3 == 0);

		if (points->size > 0) {
			line->control_points.count = points->size / 3;
			line->control_points.data = (ufbx_vec3*)points->data;
			line->point_indices.count = points_index->size;
			line->point_indices.data = (int32_t*)points_index->data;

			// Count end points
			size_t num_segments = 1;
			if (line->point_indices.count > 0) {
				for (size_t i = 0; i < line->point_indices.count - 1; i++) {
					int32_t ix = line->point_indices.data[i];
					num_segments += ix < 0 ? 1 : 0;
				}
			}

			size_t prev_end = 0;
			line->segments.data = ufbxi_push(&uc->result, ufbx_line_segment, num_segments);
			ufbxi_check(line->segments.data);
			for (size_t i = 0; i < line->point_indices.count; i++) {
				int32_t ix = line->point_indices.data[i];
				if (ix < 0) {
					ix = ~ix;
					if (i + 1 < line->point_indices.count) {
						ufbx_line_segment *segment = &line->segments.data[line->segments.count++];
						segment->index_begin = (uint32_t)prev_end;
						segment->num_indices = (uint32_t)(i - prev_end);
						prev_end = i;
					}
				}

				if (!uc->opts.allow_out_of_bounds_vertex_indices && (size_t)ix >= line->control_points.count) {
					ix = (int32_t)line->control_points.count - 1;
				}

				line->point_indices.data[i] = ix;
			}

			ufbx_line_segment *segment = &line->segments.data[line->segments.count++];
			segment->index_begin = (uint32_t)prev_end;
			segment->num_indices = (uint32_t)(line->point_indices.count - prev_end);
			ufbx_assert(line->segments.count == num_segments);
		}
	}

	return 1;
}

ufbxi_noinline static void ufbxi_read_transform_matrix(ufbx_matrix *m, ufbx_real *data)
{
	m->m00 = data[ 0]; m->m10 = data[ 1]; m->m20 = data[ 2];
	m->m01 = data[ 4]; m->m11 = data[ 5]; m->m21 = data[ 6];
	m->m02 = data[ 8]; m->m12 = data[ 9]; m->m22 = data[10];
	m->m03 = data[12]; m->m13 = data[13]; m->m23 = data[14];
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_bone(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info, const char *sub_type)
{
	(void)node;

	ufbx_bone *bone = ufbxi_push_element(uc, info, ufbx_bone, UFBX_ELEMENT_BONE);
	ufbxi_check(bone);

	if (sub_type == ufbxi_Root) {
		bone->is_root = true;
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_skin(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_skin_deformer *skin = ufbxi_push_element(uc, info, ufbx_skin_deformer, UFBX_ELEMENT_SKIN_DEFORMER);
	ufbxi_check(skin);

	const char *skinning_type = NULL;
	if (ufbxi_find_val1(node, ufbxi_SkinningType, "C", (char**)&skinning_type)) {
		if (!strcmp(skinning_type, "Rigid")) {
			skin->skinning_method = UFBX_SKINNING_RIGID;
		} else if (!strcmp(skinning_type, "Linear")) {
			skin->skinning_method = UFBX_SKINNING_LINEAR;
		} else if (!strcmp(skinning_type, "DualQuaternion")) {
			skin->skinning_method = UFBX_SKINNING_DUAL_QUATERNION;
		} else if (!strcmp(skinning_type, "Blend")) {
			skin->skinning_method = UFBX_SKINNING_BLENDED_DQ_LINEAR;
		}
	}

	ufbxi_value_array *indices = ufbxi_find_array(node, ufbxi_Indexes, 'i');
	ufbxi_value_array *weights = ufbxi_find_array(node, ufbxi_BlendWeights, 'r');
	if (indices && weights) {
		// TODO strict: ufbxi_check(indices->size == weights->size);
		skin->num_dq_weights = ufbxi_min_sz(indices->size, weights->size);
		skin->dq_vertices = (int32_t*)indices->data;
		skin->dq_weights = (ufbx_real*)weights->data;
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_skin_cluster(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_skin_cluster *cluster = ufbxi_push_element(uc, info, ufbx_skin_cluster, UFBX_ELEMENT_SKIN_CLUSTER);
	ufbxi_check(cluster);

	ufbxi_value_array *indices = ufbxi_find_array(node, ufbxi_Indexes, 'i');
	ufbxi_value_array *weights = ufbxi_find_array(node, ufbxi_Weights, 'r');

	if (indices && weights) {
		ufbxi_check(indices->size == weights->size);
		cluster->num_weights = indices->size;
		cluster->vertices = (int32_t*)indices->data;
		cluster->weights = (ufbx_real*)weights->data;
	}

	ufbxi_value_array *transform = ufbxi_find_array(node, ufbxi_Transform, 'r');
	ufbxi_value_array *transform_link = ufbxi_find_array(node, ufbxi_TransformLink, 'r');
	if (transform && transform_link) {
		ufbxi_check(transform->size >= 16);
		ufbxi_check(transform_link->size >= 16);

		ufbxi_read_transform_matrix(&cluster->mesh_node_to_bone, (ufbx_real*)transform->data);
		ufbxi_read_transform_matrix(&cluster->bind_to_world, (ufbx_real*)transform_link->data);
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_blend_channel(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_blend_channel *channel = ufbxi_push_element(uc, info, ufbx_blend_channel, UFBX_ELEMENT_BLEND_CHANNEL);
	ufbxi_check(channel);

	ufbx_real_list list = { NULL, 0 };
	ufbxi_value_array *full_weights = ufbxi_find_array(node, ufbxi_FullWeights, 'd');
	if (full_weights) {
		list.data = (ufbx_real*)full_weights->data;
		list.count = full_weights->size;
	}
	ufbxi_check(ufbxi_push_copy(&uc->tmp_full_weights, ufbx_real_list, 1, &list));

	return 1;
}

static ufbxi_forceinline float ufbxi_solve_auto_tangent(double prev_time, double time, double next_time, ufbx_real prev_value, ufbx_real value, ufbx_real next_value, float weight_left, float weight_right)
{
	// In between two keyframes: Set the initial slope to be the difference between
	// the two keyframes. Prevent overshooting by clamping the slope in case either
	// tangent goes above/below the endpoints.
	double slope = (next_value - prev_value) / (next_time - prev_time);

	// Split the slope to sign and a non-negative absolute value
	double slope_sign = slope >= 0.0 ? 1.0 : -1.0;
	double abs_slope = slope_sign * slope;

	// Find limits for the absolute value of the slope
	double range_left = weight_left * (time - prev_time);
	double range_right = weight_right * (next_time - time);
	double max_left = range_left > 0.0 ? slope_sign * (value - prev_value) / range_left : 0.0;
	double max_right = range_right > 0.0 ? slope_sign * (next_value - value) / range_right : 0.0;

	// Clamp negative values and NaNs to zero 
	if (!(max_left > 0.0)) max_left = 0.0;
	if (!(max_right > 0.0)) max_right = 0.0;

	// Clamp the absolute slope from both sides
	if (abs_slope > max_left) abs_slope = max_left;
	if (abs_slope > max_right) abs_slope = max_right;

	return (float)(slope_sign * abs_slope);
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_animation_curve(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_anim_curve *curve = ufbxi_push_element(uc, info, ufbx_anim_curve, UFBX_ELEMENT_ANIM_CURVE);
	ufbxi_check(curve);

	if (uc->opts.ignore_animation) return 1;

	ufbxi_value_array *times, *values, *attr_flags, *attrs, *refs;
	ufbxi_check(times = ufbxi_find_array(node, ufbxi_KeyTime, 'l'));
	ufbxi_check(values = ufbxi_find_array(node, ufbxi_KeyValueFloat, 'r'));
	ufbxi_check(attr_flags = ufbxi_find_array(node, ufbxi_KeyAttrFlags, 'i'));
	ufbxi_check(attrs = ufbxi_find_array(node, ufbxi_KeyAttrDataFloat, '?'));
	ufbxi_check(refs = ufbxi_find_array(node, ufbxi_KeyAttrRefCount, 'i'));

	// Time and value arrays that define the keyframes should be parallel
	ufbxi_check(times->size == values->size);

	// Flags and attributes are run-length encoded where KeyAttrRefCount (refs)
	// is an array that describes how many times to repeat a given flag/attribute.
	// Attributes consist of 4 32-bit floating point values per key.
	ufbxi_check(attr_flags->size == refs->size);
	ufbxi_check(attrs->size == refs->size * 4u);

	size_t num_keys = times->size;
	ufbx_keyframe *keys = ufbxi_push(&uc->result, ufbx_keyframe, num_keys);
	ufbxi_check(keys);

	curve->keyframes.data = keys;
	curve->keyframes.count = num_keys;

	int64_t *p_time = (int64_t*)times->data;
	ufbx_real *p_value = (ufbx_real*)values->data;
	int32_t *p_flag = (int32_t*)attr_flags->data;
	float *p_attr = (float*)attrs->data;
	int32_t *p_ref = (int32_t*)refs->data, *p_ref_end = p_ref + refs->size;

	// The previous key defines the weight/slope of the left tangent
	float slope_left = 0.0f;
	float weight_left = 0.333333f;

	double prev_time = 0.0;
	double next_time = 0.0;

	if (num_keys > 0) {
		next_time = (double)p_time[0] * uc->ktime_to_sec;
	}

	for (size_t i = 0; i < num_keys; i++) {
		ufbx_keyframe *key = &keys[i];
		ufbxi_check(p_ref < p_ref_end);

		key->time = next_time;
		key->value = *p_value;

		if (i + 1 < num_keys) {
			next_time = (double)p_time[1] * uc->ktime_to_sec;
		}

		uint32_t flags = (uint32_t)*p_flag;

		float slope_right = p_attr[0];
		float weight_right = 0.333333f;
		float next_slope_left = p_attr[1];
		float next_weight_left = 0.333333f;

		if (flags & 0x3000000) {
			// At least one of the tangents is weighted. The weights are encoded as
			// two 0.4 _decimal_ fixed point values that are packed into 32 bits and
			// interpreted as a 32-bit float.
			uint32_t packed_weights;
			memcpy(&packed_weights, &p_attr[2], sizeof(uint32_t));

			if (flags & 0x1000000) {
				// Right tangent is weighted
				weight_right = (float)(packed_weights & 0xffff) * 0.0001f;
			}

			if (flags & 0x2000000) {
				// Next left tangent is weighted
				next_weight_left = (float)(packed_weights >> 16) * 0.0001f;
			}
		}

		if (flags & 0x2) {
			// Constant interpolation: Set cubic tangents to flat.

			if (flags & 0x100) {
				// Take constant value from next key
				key->interpolation = UFBX_INTERPOLATION_CONSTANT_NEXT;

			} else {
				// Take constant value from the previous key
				key->interpolation = UFBX_INTERPOLATION_CONSTANT_PREV;
			}

			weight_right = next_weight_left = 0.333333f;
			slope_right = next_slope_left = 0.0f;

		} else if (flags & 0x8) {
			// Cubic interpolation
			key->interpolation = UFBX_INTERPOLATION_CUBIC;

			if (flags & 0x400) {
				// User tangents

				if (flags & 0x800) {
					// Broken tangents: No need to modify slopes
				} else {
					// Unified tangents: Use right slope for both sides
					// TODO: ??? slope_left = slope_right;
				}

			} else {
				// Automatic (0x100) or unknown tangents
				// TODO: TCB tangents (0x200)
				// TODO: Auto break (0x800)

				if (i > 0 && i + 1 < num_keys && key->time > prev_time && next_time > key->time) {
					slope_left = slope_right = ufbxi_solve_auto_tangent(
						prev_time, key->time, next_time,
						p_value[-1], key->value, p_value[1],
						weight_left, weight_right);
				} else {
					// Endpoint / invalid keyframe: Set both slopes to zero
					slope_left = slope_right = 0.0f;
				}
			}

		} else {
			// Linear (0x4) or unknown interpolation: Set cubic tangents to match
			// the linear interpolation with weights of 1/3.
			key->interpolation = UFBX_INTERPOLATION_LINEAR;

			weight_right = 0.333333f;
			next_weight_left = 0.333333f;

			if (next_time > key->time) {
				double delta_time = next_time - key->time;
				if (delta_time > 0.0) {
					double slope = (p_value[1] - key->value) / delta_time;
					slope_right = next_slope_left = (float)slope;
				} else {
					slope_right = next_slope_left = 0.0f;
				}
			} else {
				slope_right = next_slope_left = 0.0f;
			}
		}

		// Set the tangents based on weights (dx relative to the time difference
		// between the previous/next key) and slope (simply d = slope * dx)

		if (key->time > prev_time) {
			double delta = key->time - prev_time;
			key->left.dx = (float)(weight_left * delta);
			key->left.dy = key->left.dx * slope_left;
		} else {
			key->left.dx = 0.0f;
			key->left.dy = 0.0f;
		}

		if (next_time > key->time) {
			double delta = next_time - key->time;
			key->right.dx = (float)(weight_right * delta);
			key->right.dy = key->right.dx * slope_right;
		} else {
			key->right.dx = 0.0f;
			key->right.dy = 0.0f;
		}

		slope_left = next_slope_left;
		weight_left = next_weight_left;
		prev_time = key->time;

		// Decrement attribute refcount and potentially move to the next one.
		int32_t refs_left = --*p_ref;
		ufbxi_check(refs_left >= 0);
		if (refs_left == 0) {
			p_flag++;
			p_attr += 4;
			p_ref++;
		}
		p_time++;
		p_value++;
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_material(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_material *material = ufbxi_push_element(uc, info, ufbx_material, UFBX_ELEMENT_MATERIAL);
	ufbxi_check(material);

	if (!ufbxi_find_val1(node, ufbxi_ShadingModel, "S", &material->shading_model_name)) {
		material->shading_model_name = ufbx_empty_string;
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_texture(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_texture *texture = ufbxi_push_element(uc, info, ufbx_texture, UFBX_ELEMENT_TEXTURE);
	ufbxi_check(texture);

	texture->type = UFBX_TEXTURE_FILE;

	texture->filename = ufbx_empty_string;
	texture->relative_filename = ufbx_empty_string;

	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_FileName, "S", &texture->absolute_filename));
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_Filename, "S", &texture->absolute_filename));
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_RelativeFileName, "S", &texture->relative_filename));
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_RelativeFilename, "S", &texture->relative_filename));

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_layered_texture(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_texture *texture = ufbxi_push_element(uc, info, ufbx_texture, UFBX_ELEMENT_TEXTURE);
	ufbxi_check(texture);

	texture->type = UFBX_TEXTURE_LAYERED;

	texture->filename = ufbx_empty_string;
	texture->relative_filename = ufbx_empty_string;

	ufbxi_texture_extra *extra = ufbxi_push_element_extra(uc, texture->element.element_id, ufbxi_texture_extra);
	ufbxi_check(extra);

	ufbxi_value_array *alphas = ufbxi_find_array(node, ufbxi_Alphas, 'r');
	if (alphas) {
		extra->alphas = (ufbx_real*)alphas->data;
		extra->num_alphas = alphas->size;
	}

	ufbxi_value_array *blend_modes = ufbxi_find_array(node, ufbxi_BlendModes, 'i');
	if (blend_modes) {
		extra->blend_modes = (int32_t*)blend_modes->data;
		extra->num_blend_modes = blend_modes->size;
	}

	return 1;
}

ufbxi_noinline static void ufbxi_decode_base64(char *dst, const char *src, size_t src_length)
{
	uint8_t table[256] = { 0 };
	for (char c = 'A'; c <= 'Z'; c++) table[(size_t)c] = (uint8_t)(c - 'A');
	for (char c = 'a'; c <= 'z'; c++) table[(size_t)c] = (uint8_t)(26 + (c - 'a'));
	for (char c = '0'; c <= '9'; c++) table[(size_t)c] = (uint8_t)(52 + (c - '0'));
	table[(size_t)'+'] = 62;
	table[(size_t)'/'] = 63;

	for (size_t i = 0; i + 4 <= src_length; i += 4) {
		uint32_t a = table[(size_t)(uint8_t)src[i + 0]];
		uint32_t b = table[(size_t)(uint8_t)src[i + 1]];
		uint32_t c = table[(size_t)(uint8_t)src[i + 2]];
		uint32_t d = table[(size_t)(uint8_t)src[i + 3]];

		dst[0] = (char)(uint8_t)(a << 2 | b >> 4);
		dst[1] = (char)(uint8_t)(b << 4 | c >> 2);
		dst[2] = (char)(uint8_t)(c << 6 | d);
		dst += 3;
	}
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_video(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_video *video = ufbxi_push_element(uc, info, ufbx_video, UFBX_ELEMENT_VIDEO);
	ufbxi_check(video);

	video->absolute_filename = ufbx_empty_string;
	video->relative_filename = ufbx_empty_string;

	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_FileName, "S", &video->absolute_filename));
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_Filename, "S", &video->absolute_filename));
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_RelativeFileName, "S", &video->relative_filename));
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_RelativeFilename, "S", &video->relative_filename));

	ufbx_string content;
	if (ufbxi_find_val1(node, ufbxi_Content, "s", &content)) {
		if (uc->from_ascii) {
			if (content.length % 4 == 0) {
				size_t padding = 0;
				while (padding < 2 && padding < content.length && content.data[content.length - 1 - padding] == '=') {
					padding++;
				}

				video->content_size = content.length / 4 * 3 - padding;
				video->content = ufbxi_push(&uc->result, char, video->content_size + 3);
				ufbxi_check(video->content);

				ufbxi_decode_base64((char*)video->content, content.data, content.length);
			}
		} else {
			video->content = content.data;
			video->content_size = content.length;
		}
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_pose(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info, const char *sub_type)
{
	ufbx_pose *pose = ufbxi_push_element(uc, info, ufbx_pose, UFBX_ELEMENT_POSE);
	ufbxi_check(pose);

	// TODO: What are the actual other types?
	pose->bind_pose = sub_type == ufbxi_BindPose;

	size_t num_bones = 0;
	ufbxi_for(ufbxi_node, n, node->children, node->num_children) {
		if (n->name != ufbxi_PoseNode) continue;

		// Bones are linked with FBX names/IDs bypassing the connection system (!?)
		uint64_t fbx_id = 0;
		if (uc->version < 7000) {
			char *name = NULL;
			if (!ufbxi_find_val1(n, ufbxi_Node, "c", &name)) continue;
			fbx_id = (uintptr_t)name | UFBXI_SYNTHETIC_ID_BIT;
		} else {
			if (!ufbxi_find_val1(n, ufbxi_Node, "L", &fbx_id)) continue;
		}

		ufbxi_value_array *matrix = ufbxi_find_array(n, ufbxi_Matrix, 'r');
		if (!matrix) continue;
		ufbxi_check(matrix->size >= 16);

		ufbxi_tmp_bone_pose *tmp_pose = ufbxi_push(&uc->tmp_stack, ufbxi_tmp_bone_pose, 1);
		ufbxi_check(tmp_pose);

		num_bones++;
		tmp_pose->bone_fbx_id = fbx_id;
		ufbxi_read_transform_matrix(&tmp_pose->bone_to_world, (ufbx_real*)matrix->data);
	}

	// HACK: Transport `ufbxi_tmp_bone_pose` array through the `ufbx_bone_pose` pointer
	pose->bone_poses.count = num_bones;
	pose->bone_poses.data = (ufbx_bone_pose*)ufbxi_push_pop(&uc->tmp, &uc->tmp_stack, ufbxi_tmp_bone_pose, num_bones);
	ufbxi_check(pose->bone_poses.data);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_shader_prop_bindings(ufbxi_context *uc, ufbx_shader_prop_binding *bindings, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_shader_prop_binding)));
	ufbxi_macro_stable_sort(ufbx_shader_prop_binding, 32, bindings, uc->tmp_arr, count,
		( ufbxi_str_less(a->shader_prop, b->shader_prop) ) );
	return 1;
}


ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_binding_table(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_shader_binding *bindings = ufbxi_push_element(uc, info, ufbx_shader_binding, UFBX_ELEMENT_SHADER_BINDING);
	ufbxi_check(bindings);

	size_t num_entries = 0;
	ufbxi_for (ufbxi_node, n, node->children, node->num_children) {
		if (n->name != ufbxi_Entry) continue;

		ufbx_string src, dst;
		const char *src_type = NULL, *dst_type = NULL;
		if (!ufbxi_get_val4(n, "SCSC", &src, (char**)&src_type, &dst, (char**)&dst_type)) {
			continue;
		}

		if (src_type == ufbxi_FbxPropertyEntry && dst_type == ufbxi_FbxSemanticEntry) {
			ufbx_shader_prop_binding *bind = ufbxi_push(&uc->tmp_stack, ufbx_shader_prop_binding, 1);
			ufbxi_check(bind);
			bind->material_prop = src;
			bind->shader_prop = dst;
			num_entries++;
		} else if (src_type == ufbxi_FbxSemanticEntry && dst_type == ufbxi_FbxPropertyEntry) {
			ufbx_shader_prop_binding *bind = ufbxi_push(&uc->tmp_stack, ufbx_shader_prop_binding, 1);
			ufbxi_check(bind);
			bind->material_prop = dst;
			bind->shader_prop = src;
			num_entries++;
		}
	}

	bindings->prop_bindings.count = num_entries;
	bindings->prop_bindings.data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_shader_prop_binding, num_entries);
	ufbxi_check(bindings->prop_bindings.data);

	ufbxi_check(ufbxi_sort_shader_prop_bindings(uc, bindings->prop_bindings.data, bindings->prop_bindings.count));

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_selection_set(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	(void)node;

	ufbx_selection_set *set = ufbxi_push_element(uc, info, ufbx_selection_set, UFBX_ELEMENT_SELECTION_SET);
	ufbxi_check(set);

	return 1;
}

ufbxi_noinline static void ufbxi_find_int32_list(ufbx_int32_list *dst, ufbxi_node *node, const char *name)
{
	ufbxi_value_array *arr = ufbxi_find_array(node, name, 'i');
	if (arr) {
		dst->data = (int32_t*)arr->data;
		dst->count = arr->size;
	}
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_selection_node(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_selection_node *sel = ufbxi_push_element(uc, info, ufbx_selection_node, UFBX_ELEMENT_SELECTION_NODE);
	ufbxi_check(sel);

	int32_t in_set = 0;
	if (ufbxi_find_val1(node, ufbxi_IsTheNodeInSet, "I", &in_set) && in_set) {
		sel->include_node = true;
	}

	ufbxi_find_int32_list(&sel->vertices, node, ufbxi_VertexIndexArray);
	ufbxi_find_int32_list(&sel->edges, node, ufbxi_EdgeIndexArray);
	ufbxi_find_int32_list(&sel->faces, node, ufbxi_PolygonIndexArray);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_character(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	(void)node;

	ufbx_character *character = ufbxi_push_element(uc, info, ufbx_character, UFBX_ELEMENT_CHARACTER);
	ufbxi_check(character);

	// TODO: There's some extremely cursed all-caps data in characters

	return 1;
}

typedef struct {
	ufbx_constraint_type type;
	const char *name;
} ufbxi_constraint_type;

static const ufbxi_constraint_type ufbxi_constraint_types[] = {
	{ UFBX_CONSTRAINT_AIM, "Aim" },
	{ UFBX_CONSTRAINT_PARENT, "Parent-Child" },
	{ UFBX_CONSTRAINT_POSITION, "Position From Positions" },
	{ UFBX_CONSTRAINT_ROTATION, "Rotation From Rotations" },
	{ UFBX_CONSTRAINT_SCALE, "Scale From Scales" },
	{ UFBX_CONSTRAINT_SINGLE_CHAIN_IK, "Single Chain IK" },
};

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_constraint(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	(void)node;

	ufbx_constraint *constraint = ufbxi_push_element(uc, info, ufbx_constraint, UFBX_ELEMENT_CONSTRAINT);
	ufbxi_check(constraint);

	if (!ufbxi_find_val1(node, ufbxi_Type, "S", &constraint->type_name)) {
		constraint->type_name = ufbx_empty_string;
	}

	ufbxi_for(const ufbxi_constraint_type, ctype, ufbxi_constraint_types, ufbxi_arraycount(ufbxi_constraint_types)) {
		if (!strcmp(constraint->type_name.data, ctype->name)) {
			constraint->type = ctype->type;
			break;
		}
	}

	// TODO: There's some extremely cursed all-caps data in characters

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_synthetic_attribute(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info, ufbx_string type_str, const char *sub_type, const char *super_type)
{
	if ((sub_type == ufbxi_empty_char || sub_type == ufbxi_Model) && type_str.data == ufbxi_Model) {
		// Plain model
		return 1;
	}

	ufbxi_element_info attrib_info = *info;

	// HACK: We can create an unique FBX ID from the existing node ID as the
	// IDs are derived from NULL-terminated string pool pointers so bumping
	// the ID value by one can never cross to the next string...
	attrib_info.fbx_id = info->fbx_id + 1;

	// Use type and name from NodeAttributeName if it exists
	ufbx_string type_and_name;
	if (ufbxi_find_val1(node, ufbxi_NodeAttributeName, "s", &type_and_name)) {
		ufbx_string attrib_type_str, attrib_name_str;
		ufbxi_check(ufbxi_split_type_and_name(uc, type_and_name, &attrib_type_str, &attrib_name_str));
		if (attrib_name_str.length > 0) {
			attrib_info.name = attrib_name_str;
			uint64_t attrib_id = (uintptr_t)type_and_name.data | UFBXI_SYNTHETIC_ID_BIT;
			if (info->fbx_id != attrib_id) {
				attrib_info.fbx_id = attrib_id;
			}
		}
	}

	// 6x00: Link the node to the node attribute so property connections can be
	// redirected from connections if necessary.
	if (uc->version < 7000) {
		uint32_t hash = ufbxi_hash64(info->fbx_id);
		ufbxi_fbx_attr_entry *entry = ufbxi_map_insert(&uc->fbx_attr_map, ufbxi_fbx_attr_entry, hash, &info->fbx_id);
		ufbxi_check(entry);
		entry->node_fbx_id = info->fbx_id;
		entry->attr_fbx_id = attrib_info.fbx_id;

		// Split properties between the node and the attribute
		ufbx_prop *ps = info->props.props;
		size_t dst = 0, src = 0, end = info->props.num_props;
		while (src < end) {
			if (!ufbxi_is_node_property(uc, ps[src].name.data)) {
				ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_prop, 1, &ps[src]));
				src++;
			} else if (dst != src) {
				ps[dst++] = ps[src++];
			} else {
				dst++; src++;
			}
		}
		attrib_info.props.num_props = end - dst;
		attrib_info.props.props = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_prop, attrib_info.props.num_props);
		ufbxi_check(attrib_info.props.props);
		info->props.num_props = dst;
	}

	if (sub_type == ufbxi_Mesh) {
		ufbxi_check(ufbxi_read_mesh(uc, node, &attrib_info));
	} else if (sub_type == ufbxi_Light) {
		ufbxi_check(ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_light), UFBX_ELEMENT_LIGHT));
	} else if (sub_type == ufbxi_Camera) {
		ufbxi_check(ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_camera), UFBX_ELEMENT_CAMERA));
	} else if (sub_type == ufbxi_LimbNode || sub_type == ufbxi_Limb || sub_type == ufbxi_Root) {
		ufbxi_check(ufbxi_read_bone(uc, node, &attrib_info, sub_type));
	} else if (sub_type == ufbxi_Null || sub_type == ufbxi_Marker) {
		ufbxi_check(ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_empty), UFBX_ELEMENT_EMPTY));
	} else if (sub_type == ufbxi_NurbsCurve) {
		if (!ufbxi_find_child(node, ufbxi_KnotVector)) return 1;
		ufbxi_check(ufbxi_read_nurbs_curve(uc, node, &attrib_info));
	} else if (sub_type == ufbxi_NurbsSurface) {
		if (!ufbxi_find_child(node, ufbxi_KnotVectorU)) return 1;
		if (!ufbxi_find_child(node, ufbxi_KnotVectorV)) return 1;
		ufbxi_check(ufbxi_read_nurbs_surface(uc, node, &attrib_info));
	} else if (sub_type == ufbxi_Line) {
		if (!ufbxi_find_child(node, ufbxi_Points)) return 1;
		if (!ufbxi_find_child(node, ufbxi_PointsIndex)) return 1;
		ufbxi_check(ufbxi_read_line(uc, node, &attrib_info));
	} else if (sub_type == ufbxi_TrimNurbsSurface) {
		ufbxi_check(ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_nurbs_trim_surface), UFBX_ELEMENT_NURBS_TRIM_SURFACE));
	} else if (sub_type == ufbxi_Boundary) {
		ufbxi_check(ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_nurbs_trim_boundary), UFBX_ELEMENT_NURBS_TRIM_BOUNDARY));
	} else if (sub_type == ufbxi_CameraStereo) {
		ufbxi_check(ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_stereo_camera), UFBX_ELEMENT_STEREO_CAMERA));
	} else if (sub_type == ufbxi_CameraSwitcher) {
		ufbxi_check(ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_camera_switcher), UFBX_ELEMENT_CAMERA_SWITCHER));
	} else if (sub_type == ufbxi_LodGroup) {
		ufbxi_check(ufbxi_read_element(uc, node, &attrib_info, sizeof(ufbx_lod_group), UFBX_ELEMENT_LOD_GROUP));
	} else {
		ufbx_string sub_type_str = { sub_type, strlen(sub_type) };
		ufbxi_check(ufbxi_read_unknown(uc, node, &attrib_info, type_str, sub_type_str, super_type));
	}

	ufbxi_check(ufbxi_connect_oo(uc, attrib_info.fbx_id, info->fbx_id));
	return 1;
}

ufbxi_nodiscard static int ufbxi_read_global_settings(ufbxi_context *uc, ufbxi_node *node)
{
	ufbxi_check(ufbxi_read_properties(uc, node, &uc->scene.settings.props));
	return 1;
}

ufbxi_nodiscard static int ufbxi_read_objects(ufbxi_context *uc)
{
	ufbxi_element_info info = { 0 };
	for (;;) {
		ufbxi_node *node;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &node));
		if (!node) break;

		if (node->name == ufbxi_GlobalSettings) {
			ufbxi_check(ufbxi_read_global_settings(uc, node));
			continue;
		}

		ufbx_string type_and_name, sub_type_str;

		// Failing to parse the object properties is not an error since
		// there's some weird objects mixed in every now and then.
		// FBX version 7000 and up uses 64-bit unique IDs per object,
		// older FBX versions just use name/type pairs, which we can
		// use as IDs since all strings are interned into a string pool.
		if (uc->version >= 7000) {
			if (!ufbxi_get_val3(node, "LsS", &info.fbx_id, &type_and_name, &sub_type_str)) continue;
			ufbxi_check((info.fbx_id & UFBXI_SYNTHETIC_ID_BIT) == 0);
		} else {
			if (!ufbxi_get_val2(node, "sS", &type_and_name, &sub_type_str)) continue;
			info.fbx_id = (uintptr_t)type_and_name.data | UFBXI_SYNTHETIC_ID_BIT;
		}

		// Remove the "Fbx" prefix from sub-types, remember to re-intern!
		if (sub_type_str.length > 3 && !memcmp(sub_type_str.data, "Fbx", 3)) {
			sub_type_str.data += 3;
			sub_type_str.length -= 3;
			ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, &sub_type_str));
		}

		ufbx_string type_str;
		ufbxi_check(ufbxi_split_type_and_name(uc, type_and_name, &type_str, &info.name));

		const char *name = node->name, *sub_type = sub_type_str.data;
		ufbxi_check(ufbxi_read_properties(uc, node, &info.props));
		info.props.defaults = ufbxi_find_template(uc, name, sub_type);

		if (name == ufbxi_Model) {
			if (uc->version < 7000) {
				ufbxi_check(ufbxi_read_synthetic_attribute(uc, node, &info, type_str, sub_type, name));
			}
			ufbxi_check(ufbxi_read_model(uc, node, &info));
		} else if (name == ufbxi_NodeAttribute) {
			if (sub_type == ufbxi_Light) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_light), UFBX_ELEMENT_LIGHT));
			} else if (sub_type == ufbxi_Camera) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera), UFBX_ELEMENT_CAMERA));
			} else if (sub_type == ufbxi_LimbNode || sub_type == ufbxi_Limb || sub_type == ufbxi_Root) {
				ufbxi_check(ufbxi_read_bone(uc, node, &info, sub_type));
			} else if (sub_type == ufbxi_Null || sub_type == ufbxi_Marker) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_empty), UFBX_ELEMENT_EMPTY));
			} else if (sub_type == ufbxi_CameraStereo) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_stereo_camera), UFBX_ELEMENT_STEREO_CAMERA));
			} else if (sub_type == ufbxi_CameraSwitcher) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_camera_switcher), UFBX_ELEMENT_CAMERA_SWITCHER));
			} else if (sub_type == ufbxi_LodGroup) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_lod_group), UFBX_ELEMENT_LOD_GROUP));
			} else {
				ufbxi_check(ufbxi_read_unknown(uc, node, &info, type_str, sub_type_str, name));
			}
		} else if (name == ufbxi_Geometry) {
			if (sub_type == ufbxi_Mesh) {
				ufbxi_check(ufbxi_read_mesh(uc, node, &info));
			} else if (sub_type == ufbxi_Shape) {
				ufbxi_check(ufbxi_read_shape(uc, node, &info));
			} else if (sub_type == ufbxi_NurbsCurve) {
				ufbxi_check(ufbxi_read_nurbs_curve(uc, node, &info));
			} else if (sub_type == ufbxi_NurbsSurface) {
				ufbxi_check(ufbxi_read_nurbs_surface(uc, node, &info));
			} else if (sub_type == ufbxi_Line) {
				ufbxi_check(ufbxi_read_line(uc, node, &info));
			} else if (sub_type == ufbxi_TrimNurbsSurface) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_trim_surface), UFBX_ELEMENT_NURBS_TRIM_SURFACE));
			} else if (sub_type == ufbxi_Boundary) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_nurbs_trim_boundary), UFBX_ELEMENT_NURBS_TRIM_BOUNDARY));
			} else {
				ufbxi_check(ufbxi_read_unknown(uc, node, &info, type_str, sub_type_str, name));
			}
		} else if (name == ufbxi_Deformer) {
			if (sub_type == ufbxi_Skin) {
				ufbxi_check(ufbxi_read_skin(uc, node, &info));
			} else if (sub_type == ufbxi_Cluster) {
				ufbxi_check(ufbxi_read_skin_cluster(uc, node, &info));
			} else if (sub_type == ufbxi_BlendShape) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_blend_deformer), UFBX_ELEMENT_BLEND_DEFORMER));
			} else if (sub_type == ufbxi_BlendShapeChannel) {
				ufbxi_check(ufbxi_read_blend_channel(uc, node, &info));
			} else if (sub_type == ufbxi_VertexCacheDeformer) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_deformer), UFBX_ELEMENT_CACHE_DEFORMER));
			} else {
				ufbxi_check(ufbxi_read_unknown(uc, node, &info, type_str, sub_type_str, name));
			}
		} else if (name == ufbxi_Material) {
			ufbxi_check(ufbxi_read_material(uc, node, &info));
		} else if (name == ufbxi_Texture) {
			ufbxi_check(ufbxi_read_texture(uc, node, &info));
		} else if (name == ufbxi_LayeredTexture) {
			ufbxi_check(ufbxi_read_layered_texture(uc, node, &info));
		} else if (name == ufbxi_Video) {
			ufbxi_check(ufbxi_read_video(uc, node, &info));
		} else if (name == ufbxi_AnimationStack) {
			ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_stack), UFBX_ELEMENT_ANIM_STACK));
		} else if (name == ufbxi_AnimationLayer) {
			ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_layer), UFBX_ELEMENT_ANIM_LAYER));
		} else if (name == ufbxi_AnimationCurveNode) {
			ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_anim_value), UFBX_ELEMENT_ANIM_VALUE));
		} else if (name == ufbxi_AnimationCurve) {
			ufbxi_check(ufbxi_read_animation_curve(uc, node, &info));
		} else if (name == ufbxi_Pose) {
			ufbxi_check(ufbxi_read_pose(uc, node, &info, sub_type));
		} else if (name == ufbxi_Implementation) {
			ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_shader), UFBX_ELEMENT_SHADER));
		} else if (name == ufbxi_BindingTable) {
			ufbxi_check(ufbxi_read_binding_table(uc, node, &info));
		} else if (name == ufbxi_Collection) {
			if (sub_type == ufbxi_SelectionSet) {
				ufbxi_check(ufbxi_read_selection_set(uc, node, &info));
			}
		} else if (name == ufbxi_CollectionExclusive) {
			if (sub_type == ufbxi_DisplayLayer) {
				ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_display_layer), UFBX_ELEMENT_DISPLAY_LAYER));
			}
		} else if (name == ufbxi_SelectionNode) {
			ufbxi_check(ufbxi_read_selection_node(uc, node, &info));
		} else if (name == ufbxi_Constraint) {
			if (sub_type == ufbxi_Character) {
				ufbxi_check(ufbxi_read_character(uc, node, &info));
			} else {
				ufbxi_check(ufbxi_read_constraint(uc, node, &info));
			}
		} else if (name == ufbxi_SceneInfo) {
			ufbxi_check(ufbxi_read_scene_info(uc, node));
		} else if (name == ufbxi_Cache) {
			ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_cache_file), UFBX_ELEMENT_CACHE_FILE));
		} else if (name == ufbxi_ObjectMetaData) {
			ufbxi_check(ufbxi_read_element(uc, node, &info, sizeof(ufbx_metadata_object), UFBX_ELEMENT_METADATA_OBJECT));
		} else {
			ufbxi_check(ufbxi_read_unknown(uc, node, &info, type_str, sub_type_str, name));
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_connections(ufbxi_context *uc)
{
	// Read the connections to the list first
	for (;;) {
		ufbxi_node *node;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &node));
		if (!node) break;

		char *type;
		// TODO: Strict mode?
		if (!ufbxi_get_val1(node, "C", &type)) continue;

		uint64_t src_id = 0, dst_id = 0;
		ufbx_string src_prop = ufbx_empty_string, dst_prop = ufbx_empty_string;

		if (uc->version < 7000) {
			char *src_name = NULL, *dst_name = NULL;
			// Pre-7000 versions use Type::Name pairs as identifiers

			if (type == ufbxi_OO) {
				if (!ufbxi_get_val3(node, "_cc", NULL, &src_name, &dst_name)) continue;
			} else if (type == ufbxi_OP) {
				if (!ufbxi_get_val4(node, "_ccS", NULL, &src_name, &dst_name, &dst_prop)) continue;
			} else if (type == ufbxi_PO) {
				if (!ufbxi_get_val4(node, "_cSc", NULL, &src_name, &src_prop, &dst_name)) continue;
			} else if (type == ufbxi_PP) {
				if (!ufbxi_get_val5(node, "_cScS", NULL, &src_name, &src_prop, &dst_name, &dst_prop)) continue;
			} else {
				continue;
			}

			src_id = (uintptr_t)src_name | UFBXI_SYNTHETIC_ID_BIT;
			dst_id = (uintptr_t)dst_name | UFBXI_SYNTHETIC_ID_BIT;

		} else {
			// Post-7000 versions use proper unique 64-bit IDs

			if (type == ufbxi_OO) {
				if (!ufbxi_get_val3(node, "_LL", NULL, &src_id, &dst_id)) continue;
			} else if (type == ufbxi_OP) {
				if (!ufbxi_get_val4(node, "_LLS", NULL, &src_id, &dst_id, &dst_prop)) continue;
			} else if (type == ufbxi_PO) {
				if (!ufbxi_get_val4(node, "_LSL", NULL, &src_id, &src_prop, &dst_id)) continue;
			} else if (type == ufbxi_PP) {
				if (!ufbxi_get_val5(node, "_LSLS", NULL, &src_id, &src_prop, &dst_id, &dst_prop)) continue;
			} else {
				continue;
			}
		}

		ufbxi_tmp_connection *conn = ufbxi_push(&uc->tmp_connections, ufbxi_tmp_connection, 1);
		ufbxi_check(conn);
		conn->src = src_id;
		conn->dst = dst_id;
		conn->src_prop = src_prop;
		conn->dst_prop = dst_prop;
	}

	return 1;
}

// -- Pre-7000 "Take" based animation

ufbxi_nodiscard static int ufbxi_read_take_anim_channel(ufbxi_context *uc, ufbxi_node *node, uint64_t value_fbx_id, const char *name, ufbx_real *p_default)
{
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_Default, "R", p_default));

	// Find the key array, early return with success if not found as we may have only a default
	ufbxi_value_array *keys = ufbxi_find_array(node, ufbxi_Key, 'd');
	if (!keys) return 1;

	uint64_t curve_fbx_id = 0;
	ufbx_anim_curve *curve = ufbxi_push_synthetic_element(uc, &curve_fbx_id, name, ufbx_anim_curve, UFBX_ELEMENT_ANIM_CURVE);
	ufbxi_check(curve);

	ufbxi_check(ufbxi_connect_op(uc, curve_fbx_id, value_fbx_id, curve->name));

	if (uc->opts.ignore_animation) return 1;

	size_t num_keys;
	ufbxi_check(ufbxi_find_val1(node, ufbxi_KeyCount, "Z", &num_keys));
	curve->keyframes.data = ufbxi_push(&uc->result, ufbx_keyframe, num_keys);
	curve->keyframes.count = num_keys;
	ufbxi_check(curve->keyframes.data);

	float slope_left = 0.0f;
	float weight_left = 0.333333f;

	double next_time = 0.0;
	double next_value = 0.0;
	double prev_time = 0.0;

	// The pre-7000 keyframe data is stored as a _heterogenous_ array containing 64-bit integers,
	// floating point values, and _bare characters_. We cast all values to double and interpret them.
	double *data = (double*)keys->data, *data_end = data + keys->size;

	if (num_keys > 0) {
		ufbxi_check(data_end - data >= 2);
		// TODO: This could break with large times...
		next_time = data[0] * uc->ktime_to_sec;
		next_value = data[1];
	}

	for (size_t i = 0; i < num_keys; i++) {
		ufbx_keyframe *key = &curve->keyframes.data[i];

		// First three values: Time, Value, InterpolationMode
		ufbxi_check(data_end - data >= 3);
		key->time = next_time;
		key->value = (ufbx_real)next_value;
		char mode = (char)data[2];
		data += 3;

		float slope_right = 0.0f;
		float weight_right = 0.333333f;
		float next_slope_left = 0.0f;
		float next_weight_left = 0.333333f;
		bool auto_slope = false;

		if (mode == 'U') {
			// Cubic interpolation
			key->interpolation = UFBX_INTERPOLATION_CUBIC;

			ufbxi_check(data_end - data >= 1);
			char slope_mode = (char)data[0];
			data += 1;

			size_t num_weights = 1;
			if (slope_mode == 's' || slope_mode == 'b') {
				// Slope mode 's'/'b' (standard? broken?) always have two explicit slopes
				// TODO: `b` might actually be some kind of TCB curve
				ufbxi_check(data_end - data >= 2);
				slope_right = (float)data[0];
				next_slope_left = (float)data[1];
				data += 2;
			} else if (slope_mode == 'a') {
				// Parameterless slope mode 'a' seems to appear in baked animations. Let's just assume
				// automatic tangents for now as they're the least likely to break with
				// objectionable artifacts. We need to defer the automatic tangent resolve
				// until we have read the next time/value.
				// TODO: Solve what this is more throroughly
				auto_slope = true;
				if (uc->version == 5000) {
					num_weights = 0;
				}
			} else if (slope_mode == 'p') {
				// TODO: What is this mode? It seems to have negative values sometimes?
				// Also it seems to have _two_ trailing weights values, currently observed:
				// `n,n` and `a,X,Y,n`...
				// Ignore unknown values for now
				ufbxi_check(data_end - data >= 2);
				data += 2;
				num_weights = 2;
			} else if (slope_mode == 't') {
				// TODO: What is this mode? It seems that it does not have any weights and the
				// third value seems _tiny_ (around 1e-30?)
				ufbxi_check(data_end - data >= 3);
				data += 3;
				num_weights = 0;
			} else {
				ufbxi_fail("Unknown slope mode");
			}

			for (; num_weights > 0; num_weights--) {
				ufbxi_check(data_end - data >= 1);
				char weight_mode = (char)data[0];
				data += 1;

				if (weight_mode == 'n') {
					// Automatic weights (0.3333...)
				} else if (weight_mode == 'a') {
					// Manual weights: RightWeight, NextLeftWeight
					ufbxi_check(data_end - data >= 2);
					weight_right = (float)data[0];
					next_weight_left = (float)data[1];
					data += 2;
				} else if (weight_mode == 'l') {
					// Next left tangent is weighted
					ufbxi_check(data_end - data >= 1);
					next_weight_left = (float)data[0];
					data += 1;
				} else if (weight_mode == 'r') {
					// Right tangent is weighted
					ufbxi_check(data_end - data >= 1);
					weight_right = (float)data[0];
					data += 1;
				} else if (weight_mode == 'c') {
					// TODO: What is this mode? At least it has no parameters so let's
					// just assume automatic weights for the time being (0.3333...)
				} else {
					ufbxi_fail("Unknown weight mode");
				}
			}

		} else if (mode == 'L') {
			// Linear interpolation: No parameters
			key->interpolation = UFBX_INTERPOLATION_LINEAR;
		} else if (mode == 'C') {
			// Constant interpolation: Single parameter (use prev/next)
			ufbxi_check(data_end - data >= 1);
			key->interpolation = (char)data[0] == 'n' ? UFBX_INTERPOLATION_CONSTANT_NEXT : UFBX_INTERPOLATION_CONSTANT_PREV;
			data += 1;
		} else {
			ufbxi_fail("Unknown key mode");
		}

		// Retrieve next key and value
		if (i + 1 < num_keys) {
			ufbxi_check(data_end - data >= 2);
			next_time = data[0] * uc->ktime_to_sec;
			next_value = data[1];
		}

		if (auto_slope) {
			if (i > 0) {
				slope_left = slope_right = ufbxi_solve_auto_tangent(
					prev_time, key->time, next_time,
					key[-1].value, key->value, next_value,
					weight_left, weight_right);
			} else {
				slope_left = slope_right = 0.0f;
			}
		}

		// Set up linear cubic tangents if necessary
		if (key->interpolation == UFBX_INTERPOLATION_LINEAR) {
			if (next_time > key->time) {
				double slope = (next_value - key->value) / (next_time - key->time);
				slope_right = next_slope_left = (float)slope;
			} else {
				slope_right = next_slope_left = 0.0f;
			}
		}

		if (key->time > prev_time) {
			double delta = key->time - prev_time;
			key->left.dx = (float)(weight_left * delta);
			key->left.dy = key->left.dx * slope_left;
		} else {
			key->left.dx = 0.0f;
			key->left.dy = 0.0f;
		}

		if (next_time > key->time) {
			double delta = next_time - key->time;
			key->right.dx = (float)(weight_right * delta);
			key->right.dy = key->right.dx * slope_right;
		} else {
			key->right.dx = 0.0f;
			key->right.dy = 0.0f;
		}

		slope_left = next_slope_left;
		weight_left = next_weight_left;
		prev_time = key->time;
	}

	ufbxi_check(data == data_end);

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_take_prop_channel(ufbxi_context *uc, ufbxi_node *node, uint64_t target_fbx_id, uint64_t layer_fbx_id, ufbx_string name)
{
	if (name.data == ufbxi_Transform) {
		// Pre-7000 have transform keyframes in a deeply nested structure,
		// flatten it to make it resemble post-7000 structure a bit closer:
		// old: Model: { Channel: "Transform" { Channel: "T" { Channel "X": { ... } } } }
		// new: Model: { Channel: "Lcl Translation" { Channel "X": { ... } } }

		ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
			if (child->name != ufbxi_Channel) continue;

			const char *old_name;
			ufbxi_check(ufbxi_get_val1(child, "C", (char**)&old_name));

			ufbx_string new_name;
			if (old_name == ufbxi_T) { new_name.data = ufbxi_Lcl_Translation; new_name.length = sizeof(ufbxi_Lcl_Translation) - 1; }
			else if (old_name == ufbxi_R) { new_name.data = ufbxi_Lcl_Rotation; new_name.length = sizeof(ufbxi_Lcl_Rotation) - 1; }
			else if (old_name == ufbxi_S) { new_name.data = ufbxi_Lcl_Scaling; new_name.length = sizeof(ufbxi_Lcl_Scaling) - 1; }
			else {
				continue;
			}

			// Read child as a top-level property channel
			ufbxi_check(ufbxi_read_take_prop_channel(uc, child, target_fbx_id, layer_fbx_id, new_name));
		}

	} else {

		// Pre-6000 FBX files store blend shape keys with a " (Shape)" suffix
		if (uc->version < 6000) {
			const char *const suffix = " (Shape)";
			size_t suffix_len = strlen(suffix);
			if (name.length > suffix_len && !memcmp(name.data + name.length - suffix_len, suffix, suffix_len)) {
				name.length -= suffix_len;
				ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, &name));
			}
		}

		// Find 1-3 channel nodes thast contain a `Key:` node
		ufbxi_node *channel_nodes[3] = { 0 };
		const char *channel_names[3] = { 0 };
		size_t num_channel_nodes = 0;

		if (ufbxi_find_child(node, ufbxi_Key) || ufbxi_find_child(node, ufbxi_Default)) {
			// Channel has only a single curve
			channel_nodes[0] = node;
			channel_names[0] = name.data;
			num_channel_nodes = 1;
		} else {
			// Channel is a compound of multiple curves
			ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
				if (child->name != ufbxi_Channel) continue;
				if (!ufbxi_find_child(child, ufbxi_Key) && !ufbxi_find_child(child, ufbxi_Default)) continue;
				if (!ufbxi_get_val1(child, "C", (char**)&channel_names[num_channel_nodes])) continue;
				channel_nodes[num_channel_nodes] = child;
				if (++num_channel_nodes == 3) break;
			}
		}

		// Early return: No valid channels found, not an error
		if (num_channel_nodes == 0) return 1;

		uint64_t value_fbx_id = 0;
		ufbx_anim_value *value = ufbxi_push_synthetic_element(uc, &value_fbx_id, name.data, ufbx_anim_value, UFBX_ELEMENT_ANIM_VALUE);

		// Add a "virtual" connection between the animated property and the layer/target
		ufbxi_check(ufbxi_connect_oo(uc, value_fbx_id, layer_fbx_id));
		ufbxi_check(ufbxi_connect_op(uc, value_fbx_id, target_fbx_id, name));

		for (size_t i = 0; i < num_channel_nodes; i++) {
			ufbxi_check(ufbxi_read_take_anim_channel(uc, channel_nodes[i], value_fbx_id, channel_names[i], &value->default_value.v[i]));
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_take_object(ufbxi_context *uc, ufbxi_node *node, uint64_t layer_fbx_id)
{
	// Takes are used only in pre-7000 FBX versions so objects are identified
	// by their unique Type::Name pair that we use as unique IDs through the
	// pooled interned string pointers.
	const char *type_and_name;
	ufbxi_check(ufbxi_get_val1(node, "c", (char**)&type_and_name));
	uint64_t target_fbx_id = (uintptr_t)type_and_name | UFBXI_SYNTHETIC_ID_BIT;

	// Add all suitable Channels as animated properties
	ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
		ufbx_string name;
		if (child->name != ufbxi_Channel) continue;
		if (!ufbxi_get_val1(child, "S", &name)) continue;

		ufbxi_check(ufbxi_read_take_prop_channel(uc, child, target_fbx_id, layer_fbx_id, name));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_take(ufbxi_context *uc, ufbxi_node *node)
{
	uint64_t stack_fbx_id = 0, layer_fbx_id = 0;

	// Treat the Take as a post-7000 version animation stack and layer.
	ufbx_anim_stack *stack = ufbxi_push_synthetic_element(uc, &stack_fbx_id, NULL, ufbx_anim_stack, UFBX_ELEMENT_ANIM_STACK);
	ufbxi_check(stack);
	ufbxi_check(ufbxi_get_val1(node, "S", &stack->name));

	ufbx_anim_layer *layer = ufbxi_push_synthetic_element(uc, &layer_fbx_id, ufbxi_BaseLayer, ufbx_anim_layer, UFBX_ELEMENT_ANIM_LAYER);
	ufbxi_check(layer);

	ufbxi_check(ufbxi_connect_oo(uc, layer_fbx_id, stack_fbx_id));

	// Read stack properties from node
	int64_t begin = 0, end = 0;
	if (!ufbxi_find_val2(node, ufbxi_LocalTime, "LL", &begin, &end)) {
		ufbxi_check(ufbxi_find_val2(node, ufbxi_ReferenceTime, "LL", &begin, &end));
	}
	stack->time_begin = (double)begin * uc->ktime_to_sec;
	stack->time_end = (double)end * uc->ktime_to_sec;

	// Read all properties of objects included in the take
	ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
		// TODO: Do some object types have another name?
		if (child->name != ufbxi_Model) continue;

		ufbxi_check(ufbxi_read_take_object(uc, child, layer_fbx_id));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_takes(ufbxi_context *uc)
{
	for (;;) {
		ufbxi_node *node;
		ufbxi_check(ufbxi_parse_toplevel_child(uc, &node));
		if (!node) break;

		if (node->name == ufbxi_Take) {
			ufbxi_check(ufbxi_read_take(uc, node));
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_root(ufbxi_context *uc)
{
	// FBXHeaderExtension: Some metadata (optional)
	ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_FBXHeaderExtension));
	ufbxi_check(ufbxi_read_header_extension(uc));

	// The ASCII exporter version is stored in top-level
	if (uc->exporter == UFBX_EXPORTER_BLENDER_ASCII) {
		ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Creator));
		if (uc->top_node) {
			ufbxi_ignore(ufbxi_get_val1(uc->top_node, "S", &uc->scene.metadata.creator));
		}
	}

	// Resolve the exporter before continuing
	ufbxi_check(ufbxi_match_exporter(uc));
	if (uc->version < 7000) {
		ufbxi_check(ufbxi_init_node_prop_names(uc));
	}

	// Document: Read root ID
	if (uc->version >= 7000) {
		ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Documents));
		ufbxi_check(ufbxi_read_document(uc));
	} else {
		// Pre-7000: Root node has a specific type-name pair "Model::Scene"
		// (or reversed in binary). Use the interned name as ID as usual.
		const char *root_name = uc->from_ascii ? "Model::Scene" : "Scene\x00\x01Model";
		root_name = ufbxi_push_string_imp(&uc->string_pool, root_name, 12, false);
		ufbxi_check(root_name);
		uc->root_id = (uintptr_t)root_name | UFBXI_SYNTHETIC_ID_BIT;
	}

	// Add a nameless root node with the root ID
	{
		ufbxi_element_info root_info = { uc->root_id };
		root_info.name = ufbx_empty_string;
		ufbx_node *root = ufbxi_push_element(uc, &root_info, ufbx_node, UFBX_ELEMENT_NODE);
		ufbxi_check(root);
		ufbxi_check(ufbxi_push_copy(&uc->tmp_node_ids, uint32_t, 1, &root->element.element_id));

		if (uc->opts.use_root_transform) {
			root->local_transform = uc->opts.root_transform;
			root->node_to_parent = ufbx_transform_to_matrix(&uc->opts.root_transform);
		} else {
			root->local_transform = ufbx_identity_transform;
			root->node_to_parent = ufbx_identity_matrix;
		}
		root->is_root = true;
	}

	// Definitions: Object type counts and property templates (optional)
	ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Definitions));
	ufbxi_check(ufbxi_read_definitions(uc));

	// Objects: Actual scene data
	ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Objects));
	if (!uc->sure_fbx) {
		// If the file is a bit iffy about being a real FBX file reject it if
		// even the objects are not found.
		ufbxi_check_msg(uc->top_node, "Not an FBX file");
	}
	ufbxi_check(ufbxi_read_objects(uc));

	// Connections: Relationships between nodes
	ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Connections));
	ufbxi_check(ufbxi_read_connections(uc));

	// Takes: Pre-7000 animations, don't even try to read them in
	// post-7000 versions as the code has some assumptions about the version.
	if (uc->version < 7000) {
		ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_Takes));
		ufbxi_check(ufbxi_read_takes(uc));
	}

	// Check if there's a top-level GlobalSettings that we skimmed over
	ufbxi_check(ufbxi_parse_toplevel(uc, ufbxi_GlobalSettings));
	if (uc->top_node) {
		ufbxi_check(ufbxi_read_global_settings(uc, uc->top_node));
	}

	return 1;
}

typedef struct {
	const char *prop_name;
	ufbx_prop_type prop_type;
	const char *node_name;
	const char *node_fmt;
} ufbxi_legacy_prop;

// Must be alphabetically sorted!
static const ufbxi_legacy_prop ufbxi_legacy_light_props[] = {
	{ ufbxi_CastLight, UFBX_PROP_BOOLEAN, ufbxi_CastLight, "L" },
	{ ufbxi_CastShadows, UFBX_PROP_BOOLEAN, ufbxi_CastShadows, "L" },
	{ ufbxi_Color, UFBX_PROP_COLOR, ufbxi_Color, "RRR" },
	{ ufbxi_ConeAngle, UFBX_PROP_NUMBER, ufbxi_ConeAngle, "R" },
	{ ufbxi_HotSpot, UFBX_PROP_NUMBER, ufbxi_HotSpot, "R" },
	{ ufbxi_Intensity, UFBX_PROP_NUMBER, ufbxi_Intensity, "R" },
	{ ufbxi_LightType, UFBX_PROP_INTEGER, ufbxi_LightType, "L" },
};

// Must be alphabetically sorted!
static const ufbxi_legacy_prop ufbxi_legacy_camera_props[] = {
	{ ufbxi_ApertureMode, UFBX_PROP_INTEGER, ufbxi_ApertureMode, "L" },
	{ ufbxi_AspectH, UFBX_PROP_NUMBER, ufbxi_AspectH, "R" },
	{ ufbxi_AspectRatioMode, UFBX_PROP_INTEGER, "AspectType", "L" },
	{ ufbxi_AspectW, UFBX_PROP_NUMBER, ufbxi_AspectW, "R" },
	{ ufbxi_FieldOfView, UFBX_PROP_NUMBER, "Aperture", "R" },
	{ ufbxi_FieldOfViewX, UFBX_PROP_NUMBER, "FieldOfViewXProperty", "R" },
	{ ufbxi_FieldOfViewY, UFBX_PROP_NUMBER, "FieldOfViewYProperty", "R" },
	{ ufbxi_FilmHeight, UFBX_PROP_NUMBER, "CameraAperture", "_R" },
	{ ufbxi_FilmSqueezeRatio, UFBX_PROP_NUMBER, "SqueezeRatio", "R" },
	{ ufbxi_FilmWidth, UFBX_PROP_NUMBER, "CameraAperture", "R_" },
	{ ufbxi_FocalLength, UFBX_PROP_NUMBER, ufbxi_FocalLength, "R" },
};

// Must be alphabetically sorted!
static const ufbxi_legacy_prop ufbxi_legacy_bone_props[] = {
	{ ufbxi_Size, UFBX_PROP_NUMBER, ufbxi_Size, "R" },
};

// Must be alphabetically sorted!
static const ufbxi_legacy_prop ufbxi_legacy_material_props[] = {
	{ ufbxi_AmbientColor, UFBX_PROP_COLOR, "Ambient", "RRR" },
	{ ufbxi_DiffuseColor, UFBX_PROP_COLOR, "Diffuse", "RRR" },
	{ ufbxi_EmissiveColor, UFBX_PROP_COLOR, "Emissive", "RRR" },
	{ ufbxi_ShadingModel, UFBX_PROP_COLOR, ufbxi_ShadingModel, "S" },
	{ ufbxi_Shininess, UFBX_PROP_NUMBER, "Shininess", "R" },
	{ ufbxi_SpecularColor, UFBX_PROP_COLOR, "Specular", "RRR" },
};

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_legacy_prop(ufbxi_node *node, ufbx_prop *prop, const ufbxi_legacy_prop *legacy_prop)
{
	size_t value_ix = 0;

	const char *fmt = legacy_prop->node_fmt;
	for (size_t fmt_ix = 0; fmt[fmt_ix]; fmt_ix++) {
		char c = fmt[fmt_ix];
		switch (c) {
		case 'L':
			ufbx_assert(value_ix == 0);
			if (!ufbxi_get_val_at(node, fmt_ix, 'L', &prop->value_int)) return 0;
			prop->value_real = (ufbx_real)prop->value_int;
			prop->value_str = ufbx_empty_string;
			value_ix++;
			break;
		case 'R':
			ufbx_assert(value_ix < 3);
			if (!ufbxi_get_val_at(node, fmt_ix, 'R', &prop->value_real_arr[value_ix])) return 0;
			if (value_ix == 0) {
				prop->value_int = (int64_t)prop->value_real;
				prop->value_str = ufbx_empty_string;
			}
			value_ix++;
			break;
		case 'S':
			ufbx_assert(value_ix == 0);
			if (!ufbxi_get_val_at(node, fmt_ix, 'S', &prop->value_str)) return 0;
			prop->value_real = 0.0f;
			prop->value_int = 0;
			value_ix++;
			break;
		case '_':
			break;
		default:
			ufbx_assert(0 && "Unhandled legacy fmt");
			break;
		}
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static size_t ufbxi_read_legacy_props(ufbxi_node *node, ufbx_prop *props, const ufbxi_legacy_prop *legacy_props, size_t num_legacy)
{
	size_t num_props = 0;
	for (size_t legacy_ix = 0; legacy_ix < num_legacy; legacy_ix++) {
		const ufbxi_legacy_prop *legacy_prop = &legacy_props[legacy_ix];
		ufbx_prop *prop = &props[num_props];

		ufbxi_node *n = ufbxi_find_child_strcmp(node, legacy_prop->node_name);
		if (!n) continue;
		if (!ufbxi_read_legacy_prop(n, prop, legacy_prop)) continue;

		prop->name.data = legacy_prop->prop_name;
		prop->name.length = strlen(legacy_prop->prop_name);
		prop->internal_key = ufbxi_get_name_key(prop->name.data, prop->name.length);
		prop->flags = (ufbx_prop_flags)0;
		prop->type = legacy_prop->prop_type;
		num_props++;
	}

	return num_props;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_legacy_material(ufbxi_context *uc, ufbxi_node *node, uint64_t *p_fbx_id, const char *name)
{
	ufbx_material *ufbxi_restrict material = ufbxi_push_synthetic_element(uc, p_fbx_id, name, ufbx_material, UFBX_ELEMENT_MATERIAL);
	ufbxi_check(material);

	ufbx_prop tmp_props[ufbxi_arraycount(ufbxi_legacy_material_props)];
	size_t num_props = ufbxi_read_legacy_props(node, tmp_props, ufbxi_legacy_material_props, ufbxi_arraycount(ufbxi_legacy_material_props));

	material->shading_model_name = ufbx_empty_string;
	material->props.num_props = num_props;
	material->props.props = ufbxi_push_copy(&uc->result, ufbx_prop, num_props, tmp_props);
	ufbxi_check(material->props.props);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_legacy_link(ufbxi_context *uc, ufbxi_node *node, uint64_t *p_fbx_id, const char *name)
{
	ufbx_skin_cluster *ufbxi_restrict cluster = ufbxi_push_synthetic_element(uc, p_fbx_id, name, ufbx_skin_cluster, UFBX_ELEMENT_SKIN_CLUSTER);
	ufbxi_check(cluster);

	// TODO: Merge with ufbxi_read_skin_cluster(), at least partially?
	ufbxi_value_array *indices = ufbxi_find_array(node, ufbxi_Indexes, 'i');
	ufbxi_value_array *weights = ufbxi_find_array(node, ufbxi_Weights, 'r');

	if (indices && weights) {
		ufbxi_check(indices->size == weights->size);
		cluster->num_weights = indices->size;
		cluster->vertices = (int32_t*)indices->data;
		cluster->weights = (ufbx_real*)weights->data;
	}

	ufbxi_value_array *transform = ufbxi_find_array(node, ufbxi_Transform, 'r');
	ufbxi_value_array *transform_link = ufbxi_find_array(node, ufbxi_TransformLink, 'r');
	if (transform && transform_link) {
		ufbxi_check(transform->size >= 16);
		ufbxi_check(transform_link->size >= 16);

		ufbxi_read_transform_matrix(&cluster->mesh_node_to_bone, (ufbx_real*)transform->data);
		ufbxi_read_transform_matrix(&cluster->bind_to_world, (ufbx_real*)transform_link->data);
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_legacy_light(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_light *ufbxi_restrict light = ufbxi_push_element(uc, info, ufbx_light, UFBX_ELEMENT_LIGHT);
	ufbxi_check(light);

	ufbx_prop tmp_props[ufbxi_arraycount(ufbxi_legacy_light_props)];
	size_t num_props = ufbxi_read_legacy_props(node, tmp_props, ufbxi_legacy_light_props, ufbxi_arraycount(ufbxi_legacy_light_props));

	light->props.num_props = num_props;
	light->props.props = ufbxi_push_copy(&uc->result, ufbx_prop, num_props, tmp_props);
	ufbxi_check(light->props.props);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_legacy_camera(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_camera *ufbxi_restrict camera = ufbxi_push_element(uc, info, ufbx_camera, UFBX_ELEMENT_CAMERA);
	ufbxi_check(camera);

	ufbx_prop tmp_props[ufbxi_arraycount(ufbxi_legacy_camera_props)];
	size_t num_props = ufbxi_read_legacy_props(node, tmp_props, ufbxi_legacy_camera_props, ufbxi_arraycount(ufbxi_legacy_camera_props));

	camera->props.num_props = num_props;
	camera->props.props = ufbxi_push_copy(&uc->result, ufbx_prop, num_props, tmp_props);
	ufbxi_check(camera->props.props);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_legacy_limb_node(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	ufbx_bone *ufbxi_restrict bone = ufbxi_push_element(uc, info, ufbx_bone, UFBX_ELEMENT_BONE);
	ufbxi_check(bone);

	ufbx_prop tmp_props[ufbxi_arraycount(ufbxi_legacy_bone_props)];
	size_t num_props = 0;

	ufbxi_node *prop_node = ufbxi_find_child_strcmp(node, "Properties");
	if (prop_node) {
		num_props = ufbxi_read_legacy_props(prop_node, tmp_props, ufbxi_legacy_bone_props, ufbxi_arraycount(ufbxi_legacy_bone_props));
	}

	bone->props.num_props = num_props;
	bone->props.props = ufbxi_push_copy(&uc->result, ufbx_prop, num_props, tmp_props);
	ufbxi_check(bone->props.props);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_read_legacy_mesh(ufbxi_context *uc, ufbxi_node *node, ufbxi_element_info *info)
{
	// Only read polygon meshes, ignore eg. NURBS without error
	ufbxi_node *node_vertices = ufbxi_find_child(node, ufbxi_Vertices);
	ufbxi_node *node_indices = ufbxi_find_child(node, ufbxi_PolygonVertexIndex);
	if (!node_vertices || !node_indices) return 1;

	ufbx_mesh *ufbxi_restrict mesh = ufbxi_push_element(uc, info, ufbx_mesh, UFBX_ELEMENT_MESH);
	ufbxi_check(mesh);

	ufbxi_check(ufbxi_read_synthetic_blend_shapes(uc, node, info));

	mesh->vertex_position.value_reals = 3;
	mesh->vertex_normal.value_reals = 3;
	mesh->vertex_uv.value_reals = 2;
	mesh->vertex_tangent.value_reals = 3;
	mesh->vertex_bitangent.value_reals = 3;
	mesh->vertex_color.value_reals = 4;
	mesh->vertex_crease.value_reals = 1;
	mesh->skinned_position.value_reals = 3;
	mesh->skinned_normal.value_reals = 3;

	if (uc->opts.ignore_geometry) return 1;

	ufbxi_value_array *vertices = ufbxi_get_array(node_vertices, 'r');
	ufbxi_value_array *indices = ufbxi_get_array(node_indices, 'i');
	ufbxi_check(vertices && indices);
	ufbxi_check(vertices->size % 3 == 0);

	mesh->num_vertices = vertices->size / 3;
	mesh->num_indices = indices->size;

	int32_t *index_data = (int32_t*)indices->data;

	mesh->vertices = (ufbx_vec3*)vertices->data;
	mesh->vertex_indices = index_data;

	mesh->vertex_position.data = (ufbx_vec3*)vertices->data;
	mesh->vertex_position.indices = index_data;
	mesh->vertex_position.num_values = mesh->num_vertices;

	// Check/make sure that the last index is negated (last of polygon)
	if (mesh->num_indices > 0) {
		if (index_data[mesh->num_indices - 1] > 0) {
			if (uc->opts.strict) ufbxi_fail("Non-negated last index");
			index_data[mesh->num_indices - 1] = ~index_data[mesh->num_indices - 1];
		}
	}

	// Count the number of faces and allocate the index list
	// Indices less than zero (~actual_index) ends a polygon
	size_t num_total_faces = 0;
	ufbxi_for (int32_t, p_ix, index_data, mesh->num_indices) {
		if (*p_ix < 0) num_total_faces++;
	}
	mesh->faces = ufbxi_push(&uc->result, ufbx_face, num_total_faces);
	ufbxi_check(mesh->faces);

	size_t num_triangles = 0;

	ufbx_face *dst_face = mesh->faces;
	int32_t *p_face_begin = index_data;
	ufbxi_for (int32_t, p_ix, index_data, mesh->num_indices) {
		int32_t ix = *p_ix;
		// Un-negate final indices of polygons
		if (ix < 0) {
			ix = ~ix;
			*p_ix =  ix;
			uint32_t num_indices = (uint32_t)((p_ix - p_face_begin) + 1);
			dst_face->index_begin = (uint32_t)(p_face_begin - index_data);
			dst_face->num_indices = num_indices;
			if (num_indices >= 3) {
				num_triangles += num_indices - 2;
			}
			dst_face++;
			p_face_begin = p_ix + 1;
		}
		ufbxi_check((size_t)ix < mesh->num_vertices);
	}

	mesh->vertex_position.indices = index_data;
	mesh->num_faces = dst_face - mesh->faces;
	mesh->num_triangles = num_triangles;

	mesh->vertex_first_index = ufbxi_push(&uc->result, int32_t, mesh->num_vertices);
	ufbxi_check(mesh->vertex_first_index);

	ufbxi_for(int32_t, p_vx_ix, mesh->vertex_first_index, mesh->num_vertices) {
		*p_vx_ix = -1;
	}

	for (size_t ix = 0; ix < mesh->num_indices; ix++) {
		int32_t vx = mesh->vertex_indices[ix];
		if (vx >= 0 && (size_t)vx < mesh->num_vertices) {
			if (mesh->vertex_first_index[vx] < 0) {
				mesh->vertex_first_index[vx] = (int32_t)ix;
			}
		} else if (!uc->opts.allow_out_of_bounds_vertex_indices) {
			if (uc->opts.strict) ufbxi_fail("Index out of range");
			mesh->vertex_indices[ix] = 0;
		}
	}

	// HACK(consecutive-faces): Prepare for finalize to re-use a consecutive/zero
	// index buffer for face materials..
	uc->max_zero_indices = ufbxi_max_sz(uc->max_zero_indices, mesh->num_faces);
	uc->max_consecutive_indices = ufbxi_max_sz(uc->max_consecutive_indices, mesh->num_faces);

	// Normals are either per-vertex or per-index in legacy FBX files?
	// If the version is 5000 prefer per-vertex, otherwise per-index...
	ufbxi_value_array *normals = ufbxi_find_array(node, ufbxi_Normals, 'r');
	if (normals) {
		size_t num_normals = normals->size / 3;
		bool per_vertex = num_normals == mesh->num_vertices;
		bool per_index = num_normals == mesh->num_indices;
		if (per_vertex && (!per_index || uc->version == 5000)) {
			mesh->vertex_normal.num_values = num_normals;
			mesh->vertex_normal.unique_per_vertex = true;
			mesh->vertex_normal.data = (ufbx_vec3*)normals->data;
			mesh->vertex_normal.indices = mesh->vertex_indices;
		} else if (per_index) {
			uc->max_consecutive_indices = ufbxi_max_sz(uc->max_consecutive_indices, mesh->num_indices);
			mesh->vertex_normal.num_values = num_normals;
			mesh->vertex_normal.unique_per_vertex = false;
			mesh->vertex_normal.data = (ufbx_vec3*)normals->data;
			mesh->vertex_normal.indices = (int32_t*)ufbxi_sentinel_index_consecutive;
		}
	}

	// Optional UV values are stored pretty much like a modern vertex element
	ufbxi_node *uv_info = ufbxi_find_child(node, ufbxi_GeometryUVInfo);
	if (uv_info) {
		ufbx_uv_set *set = ufbxi_push_zero(&uc->result, ufbx_uv_set, 1);
		ufbxi_check(set);
		set->index = 0;
		set->name = ufbx_empty_string;
		set->vertex_uv.value_reals = 2;
		set->vertex_tangent.value_reals = 3;
		set->vertex_bitangent.value_reals = 3;
		ufbxi_check(ufbxi_read_vertex_element(uc, mesh, uv_info, &set->vertex_uv.data, &set->vertex_uv.indices,
			&set->vertex_uv.unique_per_vertex, &set->vertex_uv.num_values, ufbxi_TextureUV, ufbxi_TextureUVVerticeIndex, 'r', 2));

		mesh->vertex_uv = set->vertex_uv;
	}

	// Material indices
	{
		const char *mapping;
		ufbxi_check(ufbxi_find_val1(node, ufbxi_MaterialAssignation, "C", (char**)&mapping));
		if (mapping == ufbxi_ByPolygon) {
			ufbxi_check(ufbxi_read_truncated_array(uc, &mesh->face_material, node, ufbxi_Materials, 'i', mesh->num_faces));
		} else if (mapping == ufbxi_AllSame) {
			ufbxi_value_array *arr = ufbxi_find_array(node, ufbxi_Materials, 'i');
			int32_t material = 0;
			if (arr && arr->size >= 1) {
				material = ((int32_t*)arr->data)[0];
			}
			if (material == 0) {
				mesh->face_material = (int32_t*)ufbxi_sentinel_index_zero;
			} else {
				mesh->face_material = ufbxi_push(&uc->result, int32_t, mesh->num_faces);
				ufbxi_check(mesh->face_material);
				ufbxi_for(int32_t, p_mat, mesh->face_material, mesh->num_faces) {
					*p_mat = material;
				}
			}
		}
	}

	uint64_t skin_fbx_id = 0;
	ufbx_skin_deformer *skin = NULL;

	// Materials, Skin Clusters
	ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
		if (child->name == ufbxi_Material) {
			uint64_t fbx_id = 0;
			ufbx_string type_and_name, type, name;
			ufbxi_check(ufbxi_get_val1(child, "s", &type_and_name));
			ufbxi_check(ufbxi_split_type_and_name(uc, type_and_name, &type, &name));
			ufbxi_check(ufbxi_read_legacy_material(uc, child, &fbx_id, name.data));
			ufbxi_check(ufbxi_connect_oo(uc, fbx_id, info->fbx_id));
		} else if (child->name == ufbxi_Link) {
			uint64_t fbx_id = 0;
			ufbx_string type_and_name, type, name;
			ufbxi_check(ufbxi_get_val1(child, "s", &type_and_name));
			ufbxi_check(ufbxi_split_type_and_name(uc, type_and_name, &type, &name));
			ufbxi_check(ufbxi_read_legacy_link(uc, child, &fbx_id, name.data));

			uint64_t node_fbx_id = (uintptr_t)type_and_name.data | UFBXI_SYNTHETIC_ID_BIT;
			ufbxi_check(ufbxi_connect_oo(uc, node_fbx_id, fbx_id));
			if (!skin) {
				skin = ufbxi_push_synthetic_element(uc, &skin_fbx_id, info->name.data, ufbx_skin_deformer, UFBX_ELEMENT_SKIN_DEFORMER);
				ufbxi_check(skin);
				ufbxi_check(ufbxi_connect_oo(uc, skin_fbx_id, info->fbx_id));
			}
			ufbxi_check(ufbxi_connect_oo(uc, fbx_id, skin_fbx_id));
		}
	}

	mesh->skinned_is_local = true;
	mesh->skinned_position = mesh->vertex_position;
	mesh->skinned_normal = mesh->vertex_normal;

	return 1;
}

ufbxi_nodiscard static int ufbxi_read_legacy_model(ufbxi_context *uc, ufbxi_node *node)
{
	ufbx_string type_and_name, type, name;
	ufbxi_check(ufbxi_get_val1(node, "s", &type_and_name));
	ufbxi_check(ufbxi_split_type_and_name(uc, type_and_name, &type, &name));

	ufbxi_element_info info = { 0 };
	info.fbx_id = (uintptr_t)type_and_name.data | UFBXI_SYNTHETIC_ID_BIT;
	info.name = name;

	ufbx_node *elem_node = ufbxi_push_element(uc, &info, ufbx_node, UFBX_ELEMENT_NODE);
	ufbxi_check(elem_node);
	ufbxi_check(ufbxi_push_copy(&uc->tmp_node_ids, uint32_t, 1, &elem_node->element.element_id));

	ufbxi_element_info attrib_info = { 0 };
	attrib_info.fbx_id = info.fbx_id + 1;
	attrib_info.name = name;

	// If we make unused connections it doesn't matter..
	ufbxi_check(ufbxi_connect_oo(uc, attrib_info.fbx_id, info.fbx_id));

	const char *attrib_type = ufbxi_empty_char;
	ufbxi_ignore(ufbxi_find_val1(node, ufbxi_Type, "C", (char**)&attrib_type));

	bool has_attrib = true;
	if (attrib_type == ufbxi_Light) {
		ufbxi_check(ufbxi_read_legacy_light(uc, node, &attrib_info));
	} else if (attrib_type == ufbxi_Camera) {
		ufbxi_check(ufbxi_read_legacy_camera(uc, node, &attrib_info));
	} else if (attrib_type == ufbxi_LimbNode) {
		ufbxi_check(ufbxi_read_legacy_limb_node(uc, node, &attrib_info));
	} else if (ufbxi_find_child(node, ufbxi_Vertices)) {
		ufbxi_check(ufbxi_read_legacy_mesh(uc, node, &attrib_info));
	} else {
		has_attrib = false;
	}

	// Mark the node as having an attribute so property connections can be forwarded
	if (has_attrib) {
		uint32_t hash = ufbxi_hash64(info.fbx_id);
		ufbxi_fbx_attr_entry *entry = ufbxi_map_insert(&uc->fbx_attr_map, ufbxi_fbx_attr_entry, hash, &info.fbx_id);
		ufbxi_check(entry);
		entry->node_fbx_id = info.fbx_id;
		entry->attr_fbx_id = attrib_info.fbx_id;
	}

	// Children are represented as an array of strings
	ufbxi_value_array *children = ufbxi_find_array(node, ufbxi_Children, 's');
	if (children) {
		ufbx_string *names = (ufbx_string*)children->data;
		for (size_t i = 0; i < children->size; i++) {
			uint64_t child_fbx_id = (uintptr_t)names[i].data | UFBXI_SYNTHETIC_ID_BIT;
			ufbxi_check(ufbxi_connect_oo(uc, child_fbx_id, info.fbx_id));
		}
	}

	// Non-take animation channels
	ufbxi_for(ufbxi_node, child, node->children, node->num_children) {
		if (child->name == ufbxi_Channel) {
			ufbx_string channel_name;
			if (ufbxi_get_val1(child, "S", &channel_name)) {
				if (uc->legacy_implicit_anim_layer_id == 0) {
					// Defer creation so we won't be the first animation stack..
					uc->legacy_implicit_anim_layer_id = ((uintptr_t)uc + 1) | UFBXI_SYNTHETIC_ID_BIT;
				}
				ufbxi_check(ufbxi_read_take_prop_channel(uc, child, info.fbx_id, uc->legacy_implicit_anim_layer_id, channel_name));
			}
		}
	}

	return 1;
}

// Read a pre-6000 FBX file where everything is stored at the root level
ufbxi_nodiscard static int ufbxi_read_legacy_root(ufbxi_context *uc)
{
	ufbxi_check(ufbxi_init_node_prop_names(uc));

	// Some legacy FBX files have an `Fbx_Root` node that could be used as the
	// root node. However no other formats have root node with transforms so it
	// might be better to leave it as-is and create an empty one.
	{
		ufbx_node *root = ufbxi_push_synthetic_element(uc, &uc->root_id, ufbxi_empty_char, ufbx_node, UFBX_ELEMENT_NODE);
		ufbxi_check(root);
		ufbxi_check(ufbxi_push_copy(&uc->tmp_node_ids, uint32_t, 1, &root->element.element_id));
	}

	for (;;) {
		ufbxi_check(ufbxi_parse_legacy_toplevel(uc));
		if (!uc->top_node) break;

		ufbxi_node *node = uc->top_node;
		if (node->name == ufbxi_FBXHeaderExtension) {
			ufbxi_check(ufbxi_read_header_extension(uc));
		} else if (node->name == ufbxi_Takes) {
			ufbxi_check(ufbxi_read_takes(uc));
		} else if (node->name == ufbxi_Takes) {
			ufbxi_check(ufbxi_read_takes(uc));
		} else if (node->name == ufbxi_Model) {
			ufbxi_check(ufbxi_read_legacy_model(uc, node));
		}
	}

	// Create the implicit animation stack if necessary
	if (uc->legacy_implicit_anim_layer_id) {
		ufbxi_element_info layer_info = { 0 };
		layer_info.fbx_id = uc->legacy_implicit_anim_layer_id;
		layer_info.name.data = "(internal)";
		layer_info.name.length = strlen(layer_info.name.data);
		ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, &layer_info.name));
		ufbx_anim_layer *layer = ufbxi_push_element(uc, &layer_info, ufbx_anim_layer, UFBX_ELEMENT_ANIM_LAYER);
		ufbxi_check(layer);

		ufbxi_element_info stack_info = layer_info;
		stack_info.fbx_id = uc->legacy_implicit_anim_layer_id + 1;
		ufbx_anim_stack *stack = ufbxi_push_element(uc, &stack_info, ufbx_anim_stack, UFBX_ELEMENT_ANIM_STACK);
		ufbxi_check(stack);

		ufbxi_check(ufbxi_connect_oo(uc, layer_info.fbx_id, stack_info.fbx_id));
	}

	return 1;
}

static ufbx_element *ufbxi_find_element_by_fbx_id(ufbxi_context *uc, uint64_t fbx_id)
{
	uint32_t hash = ufbxi_hash64(fbx_id);
	ufbxi_fbx_id_entry *entry = ufbxi_map_find(&uc->fbx_id_map, ufbxi_fbx_id_entry, hash, &fbx_id);
	if (entry) {
		return uc->scene.elements.data[entry->element_id];
	}
	return NULL;
}

ufbxi_forceinline static bool ufbxi_cmp_name_element_less(const ufbx_name_element *a, const ufbx_name_element *b)
{
	if (a->internal_key != b->internal_key) return a->internal_key < b->internal_key;
	int cmp = strcmp(a->name.data, b->name.data);
	if (cmp != 0) return cmp < 0;
	return a->type < b->type;
}

ufbxi_forceinline static bool ufbxi_cmp_name_element_less_ref(const ufbx_name_element *a, ufbx_string name, ufbx_element_type type, uint32_t key)
{
	if (a->internal_key != key) return a->internal_key < key;
	int cmp = ufbxi_str_cmp(a->name, name);
	if (cmp != 0) return cmp < 0;
	return a->type < type;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_name_elements(ufbxi_context *uc, ufbx_name_element *name_elems, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_name_element)));
	ufbxi_macro_stable_sort(ufbx_name_element, 32, name_elems, uc->tmp_arr, count,
		( ufbxi_cmp_name_element_less(a, b) ) );
	return 1;
}

ufbxi_forceinline static bool ufbxi_cmp_node_less(ufbx_node *a, ufbx_node *b)
{
	if (a->node_depth != b->node_depth) return a->node_depth < b->node_depth;
	if (a->parent && b->parent) {
		uint32_t a_pid = a->parent->element.element_id, b_pid = b->parent->element.element_id;
		if (a_pid != b_pid) return a_pid < b_pid;
	} else {
		ufbx_assert(a->parent == NULL && b->parent == NULL);
	}
	return a->element.element_id < b->element.element_id;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_node_ptrs(ufbxi_context *uc, ufbx_node **nodes, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_node*)));
	ufbxi_macro_stable_sort(ufbx_node*, 32, nodes, uc->tmp_arr, count,
		( ufbxi_cmp_node_less(*a, *b) ) );
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_cmp_tmp_material_texture_less(const ufbxi_tmp_material_texture *a, const ufbxi_tmp_material_texture *b)
{
	if (a->material_id != b->material_id) return a->material_id < b->material_id;
	if (a->texture_id != b->texture_id) return a->texture_id < b->texture_id;
	return ufbxi_str_less(a->prop_name, b->prop_name);
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_tmp_material_textures(ufbxi_context *uc, ufbxi_tmp_material_texture *mat_texs, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbxi_tmp_material_texture)));
	ufbxi_macro_stable_sort(ufbxi_tmp_material_texture, 32, mat_texs, uc->tmp_arr, count,
		( ufbxi_cmp_tmp_material_texture_less(a, b) ));
	return 1;
}

// We need to be able to assume no padding!
ufbx_static_assert(connection_size, sizeof(ufbx_connection) == sizeof(ufbx_element*)*2 + sizeof(ufbx_string)*2);

ufbxi_forceinline static bool ufbxi_cmp_connection_less(ufbx_connection *a, ufbx_connection *b, size_t index)
{
	ufbx_element *a_elem = (&a->src)[index], *b_elem = (&b->src)[index];
	if (a_elem != b_elem) return a_elem < b_elem;
	int cmp = strcmp((&a->src_prop)[index].data, (&b->src_prop)[index].data);
	if (cmp != 0) return cmp < 0;
	cmp = strcmp((&a->src_prop)[index ^ 1].data, (&b->src_prop)[index ^ 1].data);
	return cmp < 0;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_connections(ufbxi_context *uc, ufbx_connection *connections, size_t count, size_t index)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_connection)));
	ufbxi_macro_stable_sort(ufbx_connection, 32, connections, uc->tmp_arr, count, ( ufbxi_cmp_connection_less(a, b, index) ));
	return 1;
}

static uint64_t ufbxi_find_attribute_fbx_id(ufbxi_context *uc, uint64_t node_fbx_id)
{
	uint32_t hash = ufbxi_hash64(node_fbx_id);
	ufbxi_fbx_attr_entry *entry = ufbxi_map_find(&uc->fbx_attr_map, ufbxi_fbx_attr_entry, hash, &node_fbx_id);
	if (entry) {
		return entry->attr_fbx_id;
	}
	return node_fbx_id;
}

static ufbxi_forceinline ufbx_real ufbxi_find_real(const ufbx_props *props, const char *name, ufbx_real def)
{
	ufbx_prop *prop = ufbxi_find_prop(props, name);
	if (prop) {
		return prop->value_real;
	} else {
		return def;
	}
}

static ufbxi_forceinline ufbx_vec3 ufbxi_find_vec3(const ufbx_props *props, const char *name, ufbx_real def_x, ufbx_real def_y, ufbx_real def_z)
{
	ufbx_prop *prop = ufbxi_find_prop(props, name);
	if (prop) {
		return prop->value_vec3;
	} else {
		ufbx_vec3 def = { def_x, def_y, def_z };
		return def;
	}
}

static ufbxi_forceinline int64_t ufbxi_find_int(const ufbx_props *props, const char *name, int64_t def)
{
	ufbx_prop *prop = ufbxi_find_prop(props, name);
	if (prop) {
		return prop->value_int;
	} else {
		return def;
	}
}

static ufbxi_forceinline int64_t ufbxi_find_enum(const ufbx_props *props, const char *name, int64_t def, int64_t max_value)
{
	ufbx_prop *prop = ufbxi_find_prop(props, name);
	if (prop) {
		int64_t value = prop->value_int;
		if (value >= 0 && value <= max_value) {
			return value;
		} else {
			return def;
		}
	} else {
		return def;
	}
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_resolve_connections(ufbxi_context *uc)
{
	size_t num_connections = uc->tmp_connections.num_items;
	ufbxi_tmp_connection *tmp_connections = ufbxi_push_pop(&uc->tmp, &uc->tmp_connections, ufbxi_tmp_connection, num_connections);
	ufbxi_buf_free(&uc->tmp_connections);
	ufbxi_check(tmp_connections);

	// NOTE: We truncate this array in case not all connections are resolved
	uc->scene.connections_src.data = ufbxi_push(&uc->result, ufbx_connection, num_connections);
	ufbxi_check(uc->scene.connections_src.data);

	// HACK: Translate property connections from node to attribute if
	// the property name is not included in the known node properties.
	if (uc->version < 7000) {
		ufbxi_for(ufbxi_tmp_connection, tmp_conn, tmp_connections, num_connections) {
			if (tmp_conn->src_prop.length > 0 && !ufbxi_is_node_property(uc, tmp_conn->src_prop.data)) {
				tmp_conn->src = ufbxi_find_attribute_fbx_id(uc, tmp_conn->src);
			}
			if (tmp_conn->dst_prop.length > 0 && !ufbxi_is_node_property(uc, tmp_conn->dst_prop.data)) {
				tmp_conn->dst = ufbxi_find_attribute_fbx_id(uc, tmp_conn->dst);
			}
		}
	}

	ufbxi_for(ufbxi_tmp_connection, tmp_conn, tmp_connections, num_connections) {
		ufbx_element *src = ufbxi_find_element_by_fbx_id(uc, tmp_conn->src);
		ufbx_element *dst = ufbxi_find_element_by_fbx_id(uc, tmp_conn->dst);
		if (!src || !dst) continue;

		ufbx_connection *conn = &uc->scene.connections_src.data[uc->scene.connections_src.count++];
		conn->src = src;
		conn->dst = dst;
		conn->src_prop = tmp_conn->src_prop;
		conn->dst_prop = tmp_conn->dst_prop;
	}

	uc->scene.connections_dst.count = uc->scene.connections_src.count;
	uc->scene.connections_dst.data = ufbxi_push_copy(&uc->result, ufbx_connection,
		uc->scene.connections_src.count, uc->scene.connections_src.data);
	ufbxi_check(uc->scene.connections_dst.data);

	ufbxi_check(ufbxi_sort_connections(uc, uc->scene.connections_src.data, uc->scene.connections_src.count, 0));
	ufbxi_check(ufbxi_sort_connections(uc, uc->scene.connections_dst.data, uc->scene.connections_dst.count, 1));

	// We don't need the temporary connections at this point anymore
	ufbxi_buf_free(&uc->tmp_connections);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_add_connections_to_elements(ufbxi_context *uc)
{
	ufbx_connection *conn_src = uc->scene.connections_src.data;
	ufbx_connection *conn_src_end = conn_src + uc->scene.connections_src.count;
	ufbx_connection *conn_dst = uc->scene.connections_dst.data;
	ufbx_connection *conn_dst_end = conn_dst + uc->scene.connections_dst.count;

	ufbxi_for_ptr(ufbx_element, p_elem, uc->scene.elements.data, uc->scene.elements.count) {
		ufbx_element *elem = *p_elem;
		uint32_t id = elem->element_id;

		while (conn_src < conn_src_end && conn_src->src->element_id < id) conn_src++;
		while (conn_dst < conn_dst_end && conn_dst->dst->element_id < id) conn_dst++;
		ufbx_connection *src_end = conn_src, *dst_end = conn_dst;

		while (src_end < conn_src_end && src_end->src->element_id == id) src_end++;
		while (dst_end < conn_dst_end && dst_end->dst->element_id == id) dst_end++;

		elem->connections_src.data = conn_src;
		elem->connections_src.count = (size_t)(src_end - conn_src);
		elem->connections_dst.data = conn_dst;
		elem->connections_dst.count = (size_t)(dst_end - conn_dst);

		// Setup animated properties
		// TODO: It seems we're invalidating a lot of properties here actually, maybe they
		// should be initially pushed to `tmp` instead of result if this happens so much..
		{
			ufbx_prop *prop = elem->props.props, *prop_end = prop + elem->props.num_props;
			ufbx_prop *copy_start = prop;
			bool needs_copy = false;
			size_t num_animated = 0, num_synthetic = 0;

			for (;;) {
				// Scan to the next animation connection
				for (; conn_dst < dst_end; conn_dst++) {
					if (conn_dst->dst_prop.length == 0) continue;
					if (conn_dst->src_prop.length > 0) break;
					if (conn_dst->src->type == UFBX_ELEMENT_ANIM_VALUE) break;
				}

				ufbx_string name = ufbx_empty_string;
				if (conn_dst < dst_end) {
					name = conn_dst->dst_prop;
				}
				if (name.length == 0) break;

				// NOTE: "Animated" properties also include connected ones as we need
				// to resolve them during evaluation
				num_animated++;

				ufbx_anim_value *anim_value = NULL;
				uint32_t flags = 0;
				for (; conn_dst < dst_end && conn_dst->dst_prop.data == name.data; conn_dst++) {
					if (conn_dst->src_prop.length > 0) {
						flags |= UFBX_PROP_FLAG_CONNECTED;
					} else if (conn_dst->src->type == UFBX_ELEMENT_ANIM_VALUE) {
						anim_value = (ufbx_anim_value*)conn_dst->src;
						flags |= UFBX_PROP_FLAG_ANIMATED;
					}
				}

				uint32_t key = ufbxi_get_name_key(name.data, name.length);
				while (prop != prop_end && ufbxi_name_key_less(prop, name.data, name.length, key)) prop++;

				if (prop != prop_end && prop->name.data == name.data) {
					prop->flags = (ufbx_prop_flags)(prop->flags | flags);
				} else {
					// Animated property that is not in the element property list
					// Copy the preceeding properties to the stack, then push a
					// synthetic property for the animated property.
					ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_prop, (size_t)(prop - copy_start), copy_start));
					copy_start = prop;
					needs_copy = true;

					// Let's hope we can find the property in the defaults at least
					ufbx_prop anim_def_prop;
					ufbx_prop *def_prop = NULL;
					if (elem->props.defaults) {
						def_prop = ufbxi_find_prop_with_key(elem->props.defaults, name.data, key);
					} else if (anim_value) {
						memset(&anim_def_prop, 0, sizeof(anim_def_prop));
						// Hack a couple of common types
						ufbx_prop_type type = UFBX_PROP_UNKNOWN;
						if (name.data == ufbxi_Lcl_Translation) type = UFBX_PROP_TRANSLATION;
						else if (name.data == ufbxi_Lcl_Rotation) type = UFBX_PROP_ROTATION;
						else if (name.data == ufbxi_Lcl_Scaling) type = UFBX_PROP_SCALING;
						anim_def_prop.type = type;
						anim_def_prop.value_vec3 = anim_value->default_value;
						anim_def_prop.value_int = (int64_t)anim_value->default_value.x;
						def_prop = &anim_def_prop;
					} else {
						flags |= UFBX_PROP_FLAG_NO_VALUE;
					}

					ufbx_prop *new_prop = ufbxi_push_zero(&uc->tmp_stack, ufbx_prop, 1);
					ufbxi_check(new_prop);
					if (def_prop) *new_prop = *def_prop;
					flags |= new_prop->flags;
					new_prop->flags = (ufbx_prop_flags)(UFBX_PROP_FLAG_ANIMATABLE | UFBX_PROP_FLAG_SYNTHETIC | flags);
					new_prop->name = name;
					new_prop->internal_key = key;
					new_prop->value_str = ufbx_empty_string;
					num_synthetic++;
				}
			}

			// Copy the properties if necessary
			if (needs_copy) {
				size_t num_new_props = elem->props.num_props + num_synthetic;
				ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_prop, (size_t)(prop_end - copy_start), copy_start));
				elem->props.props = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_prop, num_new_props);
				ufbxi_check(elem->props.props);
				elem->props.num_props = num_new_props;
			}
			elem->props.num_animated = num_animated;
		}

		conn_src = src_end;
		conn_dst = dst_end;
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_linearize_nodes(ufbxi_context *uc)
{
	size_t num_nodes = uc->tmp_node_ids.num_items;
	uint32_t *node_ids = ufbxi_push_pop(&uc->tmp, &uc->tmp_node_ids, uint32_t, num_nodes);
	ufbxi_buf_free(&uc->tmp_node_ids);
	ufbxi_check(node_ids);

	ufbx_node **node_ptrs = ufbxi_push(&uc->tmp_stack, ufbx_node*, num_nodes);
	ufbxi_check(node_ptrs);

	// Fetch the node pointers
	for (size_t i = 0; i < num_nodes; i++) {
		node_ptrs[i] = (ufbx_node*)uc->scene.elements.data[node_ids[i]];
		ufbx_assert(node_ptrs[i]->element.type == UFBX_ELEMENT_NODE);
	}

	uc->scene.root_node = node_ptrs[0];

	size_t *node_offsets = ufbxi_push_pop(&uc->tmp_stack, &uc->tmp_typed_element_offsets[UFBX_ELEMENT_NODE], size_t, num_nodes);
	ufbxi_check(node_offsets);

	// Hook up the parent nodes, we'll assume that there's no cycles at this point
	ufbxi_for_ptr(ufbx_node, p_node, node_ptrs, num_nodes) {
		ufbx_node *node = *p_node;

		// Pre-6000 files don't have any explicit root connections so they must always
		// be connected to ther root..
		if (node->parent == NULL && !(uc->opts.allow_nodes_out_of_root && uc->version >= 6000)) {
			if (node != uc->scene.root_node) {
				node->parent = uc->scene.root_node;
			}
		}

		ufbxi_for_list(ufbx_connection, conn, node->element.connections_dst) {
			if (conn->src_prop.length > 0 || conn->dst_prop.length > 0) continue;
			if (conn->src->type != UFBX_ELEMENT_NODE) continue;
			((ufbx_node*)conn->src)->parent = node;
		}
	}

	// Count the parent depths and child amounts
	ufbxi_for_ptr(ufbx_node, p_node, node_ptrs, num_nodes) {
		ufbx_node *node = *p_node;
		uint32_t depth = 0;

		for (ufbx_node *p = node->parent; p; p = p->parent) {
			depth += p->node_depth + 1;
			if (p->node_depth > 0) break;
			ufbxi_check_msg(depth <= num_nodes, "Cyclic node hierarchy");
		}

		node->node_depth = depth;

		// Second pass to cache the depths to avoid O(n^2)
		for (ufbx_node *p = node->parent; p; p = p->parent) {
			if (--depth <= p->node_depth) break;
			p->node_depth = depth;
		}
	}

	ufbxi_check(ufbxi_sort_node_ptrs(uc, node_ptrs, num_nodes));

	for (uint32_t i = 0; i < num_nodes; i++) {
		size_t *p_offset = ufbxi_push(&uc->tmp_typed_element_offsets[UFBX_ELEMENT_NODE], size_t, 1);
		ufbxi_check(p_offset);
		ufbx_node *node = node_ptrs[i];

		uint32_t original_id = node->element.typed_id;
		node->element.typed_id = i;
		*p_offset = node_offsets[original_id];
	}

	// Pop the temporary arrays
	ufbxi_pop(&uc->tmp_stack, size_t, num_nodes, NULL);
	ufbxi_pop(&uc->tmp_stack, ufbx_node*, num_nodes, NULL);

	return 1;
}


ufbxi_nodiscard ufbxi_noinline static ufbx_connection_list ufbxi_find_dst_connections(ufbx_element *element, const char *prop)
{
	if (!prop) prop = ufbxi_empty_char;

	size_t begin = element->connections_dst.count, end = begin;

	ufbxi_macro_lower_bound_eq(ufbx_connection, 32, &begin,
		element->connections_dst.data, 0, element->connections_dst.count,
		(strcmp(a->dst_prop.data, prop) < 0),
		(a->dst_prop.data == prop && a->src_prop.length == 0));

	ufbxi_macro_upper_bound_eq(ufbx_connection, 32, &end,
		element->connections_dst.data, begin, element->connections_dst.count,
		(a->dst_prop.data == prop && a->src_prop.length == 0));

	ufbx_connection_list result = { element->connections_dst.data + begin, end - begin };
	return result;
}

ufbxi_nodiscard ufbxi_noinline static ufbx_connection_list ufbxi_find_src_connections(ufbx_element *element, const char *prop)
{
	if (!prop) prop = ufbxi_empty_char;

	size_t begin = element->connections_src.count, end = begin;

	ufbxi_macro_lower_bound_eq(ufbx_connection, 32, &begin,
		element->connections_src.data, 0, element->connections_src.count,
		(strcmp(a->src_prop.data, prop) < 0),
		(a->src_prop.data == prop && a->dst_prop.length == 0));

	ufbxi_macro_upper_bound_eq(ufbx_connection, 32, &end,
		element->connections_src.data, begin, element->connections_src.count,
		(a->src_prop.data == prop && a->dst_prop.length == 0));

	ufbx_connection_list result = { element->connections_src.data + begin, end - begin };
	return result;
}

ufbxi_nodiscard static ufbx_element *ufbxi_get_element_node(ufbx_element *element)
{
	return element && element->instances.count > 0 ? &element->instances.data[0]->element : NULL;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_fetch_dst_elements(ufbxi_context *uc, void *p_dst_list, ufbx_element *element, bool search_node, const char *prop, ufbx_element_type src_type)
{
	size_t num_elements = 0;

	do {
		ufbx_connection_list conns = ufbxi_find_dst_connections(element, prop);
		ufbxi_for_list(ufbx_connection, conn, conns) {
			if (conn->src->type == src_type) {
				ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_element*, 1, &conn->src));
				num_elements++;
			}
		}
	} while (search_node && (element = ufbxi_get_element_node(element)) != NULL);

	ufbx_element_list *list = (ufbx_element_list*)p_dst_list;
	list->data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_element*, num_elements);
	list->count = num_elements;
	ufbxi_check(list->data);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_fetch_src_elements(ufbxi_context *uc, void *p_dst_list, ufbx_element *element, bool search_node, const char *prop, ufbx_element_type dst_type)
{
	size_t num_elements = 0;

	do {
		ufbx_connection_list conns = ufbxi_find_src_connections(element, prop);
		ufbxi_for_list(ufbx_connection, conn, conns) {
			if (conn->dst->type == dst_type) {
				ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_element*, 1, &conn->dst));
				num_elements++;
			}
		}
	} while (search_node && (element = ufbxi_get_element_node(element)) != NULL);

	ufbx_element_list *list = (ufbx_element_list*)p_dst_list;
	list->data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_element*, num_elements);
	list->count = num_elements;
	ufbxi_check(list->data);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static ufbx_element *ufbxi_fetch_dst_element(ufbx_element *element, bool search_node, const char *prop, ufbx_element_type src_type)
{
	do {
		ufbx_connection_list conns = ufbxi_find_dst_connections(element, prop);
		ufbxi_for_list(ufbx_connection, conn, conns) {
			if (conn->src->type == src_type) {
				return conn->src;
			}
		}
	} while (search_node && (element = ufbxi_get_element_node(element)) != NULL);

	return NULL;
}

ufbxi_nodiscard ufbxi_noinline static ufbx_element *ufbxi_fetch_src_element(ufbx_element *element, bool search_node, const char *prop, ufbx_element_type dst_type)
{
	do {
		ufbx_connection_list conns = ufbxi_find_src_connections(element, prop);
		ufbxi_for_list(ufbx_connection, conn, conns) {
			if (conn->dst->type == dst_type) {
				return conn->dst;
			}
		}
	} while (search_node && (element = ufbxi_get_element_node(element)) != NULL);

	return NULL;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_fetch_textures(ufbxi_context *uc, ufbx_material_texture_list *list, ufbx_element *element, bool search_node)
{
	size_t num_textures = 0;

	do {
		ufbxi_for_list(ufbx_connection, conn, element->connections_dst) {
			if (conn->src_prop.length > 0) continue;
			if (conn->src->type == UFBX_ELEMENT_TEXTURE) {
				ufbx_material_texture *tex = ufbxi_push(&uc->tmp_stack, ufbx_material_texture, 1);
				ufbxi_check(tex);
				tex->shader_prop = tex->material_prop = conn->dst_prop;
				tex->texture = (ufbx_texture*)conn->src;
				num_textures++;
			}
		}
	} while (search_node && (element = ufbxi_get_element_node(element)) != NULL);

	list->data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_material_texture, num_textures);
	list->count = num_textures;
	ufbxi_check(list->data);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_fetch_mesh_materials(ufbxi_context *uc, ufbx_mesh_material_list *list, ufbx_element *element, bool search_node)
{
	size_t num_materials = 0;

	do {
		ufbx_connection_list conns = ufbxi_find_dst_connections(element, NULL);
		ufbxi_for_list(ufbx_connection, conn, conns) {
			if (conn->src->type == UFBX_ELEMENT_MATERIAL) {
				ufbx_mesh_material mesh_mat = { (ufbx_material*)conn->src };
				ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_mesh_material, 1, &mesh_mat));
				num_materials++;
			}
		}
	} while (search_node && (element = ufbxi_get_element_node(element)) != NULL);

	list->data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_mesh_material, num_materials);
	list->count = num_materials;
	ufbxi_check(list->data);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_fetch_deformers(ufbxi_context *uc,  ufbx_element_list *list, ufbx_element *element, bool search_node)
{
	size_t num_deformers = 0;

	do {
		ufbxi_for_list(ufbx_connection, conn, element->connections_dst) {
			if (conn->src_prop.length > 0) continue;
			ufbx_element_type type = conn->src->type;
			if (type == UFBX_ELEMENT_SKIN_DEFORMER || type == UFBX_ELEMENT_BLEND_DEFORMER || type == UFBX_ELEMENT_CACHE_DEFORMER) {
				ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_element*, 1, &conn->src));
				num_deformers++;
			}
		}
	} while (search_node && (element = ufbxi_get_element_node(element)) != NULL);

	list->data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_element*, num_deformers);
	list->count = num_deformers;
	ufbxi_check(list->data);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_fetch_blend_keyframes(ufbxi_context *uc, ufbx_blend_keyframe_list *list, ufbx_element *element)
{
	size_t num_keyframes = 0;

	ufbx_connection_list conns = ufbxi_find_dst_connections(element, NULL);
	ufbxi_for_list(ufbx_connection, conn, conns) {
		if (conn->src->type == UFBX_ELEMENT_BLEND_SHAPE) {
			ufbx_blend_keyframe key = { (ufbx_blend_shape*)conn->src };
			ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_blend_keyframe, 1, &key));
			num_keyframes++;
		}
	}

	list->data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_blend_keyframe, num_keyframes);
	list->count = num_keyframes;
	ufbxi_check(list->data);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_fetch_texture_layers(ufbxi_context *uc, ufbx_texture_layer_list *list, ufbx_element *element)
{
	size_t num_layers = 0;

	ufbx_connection_list conns = ufbxi_find_dst_connections(element, NULL);
	ufbxi_for_list(ufbx_connection, conn, conns) {
		if (conn->src->type == UFBX_ELEMENT_TEXTURE) {
			ufbx_texture *texture = (ufbx_texture*)conn->src;
			ufbx_texture_layer layer = { texture };
			layer.alpha = ufbxi_find_real(&texture->props, ufbxi_Texture_alpha, 1.0f);
			layer.blend_mode = (ufbx_blend_mode)ufbxi_find_enum(&texture->props, ufbxi_BlendMode, UFBX_BLEND_REPLACE, UFBX_BLEND_OVERLAY);
			ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_texture_layer, 1, &layer));
			num_layers++;
		}
	}

	list->data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_texture_layer, num_layers);
	list->count = num_layers;
	ufbxi_check(list->data);

	return 1;
}

static ufbxi_forceinline bool ufbxi_prop_connection_less(const ufbx_connection *a, const char *prop)
{
	int cmp = strcmp(a->dst_prop.data, prop);
	if (cmp != 0) return cmp < 0;
	return a->src_prop.length == 0;
}

ufbxi_nodiscard ufbxi_noinline static ufbx_connection *ufbxi_find_prop_connection(const ufbx_element *element, const char *prop)
{
	if (!prop) prop = ufbxi_empty_char;

	size_t index = SIZE_MAX;

	ufbxi_macro_lower_bound_eq(ufbx_connection, 32, &index,
		element->connections_dst.data, 0, element->connections_dst.count,
		(ufbxi_prop_connection_less(a, prop)),
		(a->dst_prop.data == prop && a->src_prop.length > 0));

	return index < SIZE_MAX ? &element->connections_dst.data[index] : NULL;
}

ufbxi_forceinline static void ufbxi_patch_index_pointer(ufbxi_context *uc, int32_t **p_index)
{
	if (*p_index == ufbxi_sentinel_index_zero) {
		*p_index = uc->zero_indices;
	} else if (*p_index == ufbxi_sentinel_index_consecutive) {
		*p_index = uc->consecutive_indices;
	}
}

ufbxi_nodiscard static bool ufbxi_cmp_anim_prop_less(const ufbx_anim_prop *a, const ufbx_anim_prop *b)
{
	if (a->element != b->element) return a->element < b->element;
	if (a->internal_key != b->internal_key) return a->internal_key < b->internal_key;
	return ufbxi_str_less(a->prop_name, b->prop_name);
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_anim_props(ufbxi_context *uc, ufbx_anim_prop *aprops, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_anim_prop)));
	ufbxi_macro_stable_sort(ufbx_anim_prop, 32, aprops, uc->tmp_arr, count, ( ufbxi_cmp_anim_prop_less(a, b) ));
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_material_textures(ufbxi_context *uc, ufbx_material_texture *textures, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_material_texture)));
	ufbxi_macro_stable_sort(ufbx_material_texture, 32, textures, uc->tmp_arr, count, ( ufbxi_str_less(a->material_prop, b->material_prop) ));
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_videos_by_filename(ufbxi_context *uc, ufbx_video **videos, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_video*)));
	ufbxi_macro_stable_sort(ufbx_video*, 32, videos, uc->tmp_arr, count, ( ufbxi_str_less((*a)->absolute_filename, (*b)->absolute_filename) ));
	return 1;
}

ufbxi_nodiscard ufbxi_noinline static ufbx_anim_prop *ufbxi_find_anim_prop_start(ufbx_anim_layer *layer, const ufbx_element *element)
{
	size_t index = SIZE_MAX;
	ufbxi_macro_lower_bound_eq(ufbx_anim_prop, 16, &index, layer->anim_props.data, 0, layer->anim_props.count,
		(a->element < element), (a->element == element));
	return index != SIZE_MAX ? &layer->anim_props.data[index] : NULL;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_sort_skin_weights(ufbxi_context *uc, ufbx_skin_deformer *skin)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, skin->max_weights_per_vertex * sizeof(ufbx_skin_weight)));

	for (size_t i = 0; i < skin->vertices.count; i++) {
		ufbx_skin_vertex v = skin->vertices.data[i];
		ufbxi_macro_stable_sort(ufbx_skin_weight, 32, skin->weights.data + v.weight_begin, uc->tmp_arr, v.num_weights,
			( a->weight > b->weight ));
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_sort_blend_keyframes(ufbxi_context *uc, ufbx_blend_keyframe *keyframes, size_t count)
{
	ufbxi_check(ufbxi_grow_array(&uc->ator_tmp, &uc->tmp_arr, &uc->tmp_arr_size, count * sizeof(ufbx_blend_keyframe)));

	ufbxi_macro_stable_sort(ufbx_blend_keyframe, 32, keyframes, uc->tmp_arr, count,
		( a->target_weight < b->target_weight ));

	return 1;
}

ufbxi_noinline static bool ufbxi_matrix_all_zero(const ufbx_matrix *matrix)
{
	for (size_t i = 0; i < 12; i++) {
		if (matrix->v[i] != 0.0f) return false;
	}
	return true;
}

static ufbxi_forceinline bool ufbxi_is_vec3_zero(ufbx_vec3 v)
{
	return (v.x == 0.0) & (v.y == 0.0) & (v.z == 0.0);
}

static ufbxi_forceinline bool ufbxi_is_vec3_one(ufbx_vec3 v)
{
	return (v.x == 1.0) & (v.y == 1.0) & (v.z == 1.0);
}

static ufbxi_forceinline bool ufbxi_is_quat_identity(ufbx_quat v)
{
	return (v.x == 0.0) & (v.y == 0.0) & (v.z == 0.0) & (v.w == 1.0);
}

static ufbxi_forceinline bool ufbxi_is_transform_identity(ufbx_transform t)
{
	return (bool)((int)ufbxi_is_vec3_zero(t.translation) & (int)ufbxi_is_quat_identity(t.rotation) & (int)ufbxi_is_vec3_one(t.scale));
}

// Material tables

typedef void (*ufbxi_mat_transform_fn)(ufbx_vec3 *a);

typedef struct {
	int32_t index;
	ufbx_string prop;
	ufbxi_mat_transform_fn transform_fn;
} ufbxi_shader_mapping;

typedef struct {
	const ufbxi_shader_mapping *data;
	size_t count;
} ufbxi_shader_mapping_list;

static const ufbxi_shader_mapping ufbxi_base_fbx_mapping[] = {
	{ UFBX_MATERIAL_FBX_DIFFUSE_COLOR, ufbxi_string_literal("Diffuse") },
	{ UFBX_MATERIAL_FBX_DIFFUSE_COLOR, ufbxi_string_literal("DiffuseColor") },
	{ UFBX_MATERIAL_FBX_DIFFUSE_FACTOR, ufbxi_string_literal("DiffuseFactor") },
	{ UFBX_MATERIAL_FBX_SPECULAR_COLOR, ufbxi_string_literal("Specular") },
	{ UFBX_MATERIAL_FBX_SPECULAR_COLOR, ufbxi_string_literal("SpecularColor") },
	{ UFBX_MATERIAL_FBX_SPECULAR_FACTOR, ufbxi_string_literal("SpecularFactor") },
	{ UFBX_MATERIAL_FBX_SPECULAR_EXPONENT, ufbxi_string_literal("Shininess") },
	{ UFBX_MATERIAL_FBX_SPECULAR_EXPONENT, ufbxi_string_literal("ShininessExponent") },
	{ UFBX_MATERIAL_FBX_REFLECTION_COLOR, ufbxi_string_literal("Reflection") },
	{ UFBX_MATERIAL_FBX_REFLECTION_COLOR, ufbxi_string_literal("ReflectionColor") },
	{ UFBX_MATERIAL_FBX_REFLECTION_FACTOR, ufbxi_string_literal("ReflectionFactor") },
	{ UFBX_MATERIAL_FBX_TRANSPARENCY_COLOR, ufbxi_string_literal("Transparent") },
	{ UFBX_MATERIAL_FBX_TRANSPARENCY_COLOR, ufbxi_string_literal("TransparentColor") },
	{ UFBX_MATERIAL_FBX_TRANSPARENCY_FACTOR, ufbxi_string_literal("TransparentFactor") },
	{ UFBX_MATERIAL_FBX_TRANSPARENCY_FACTOR, ufbxi_string_literal("TransparencyFactor") },
	{ UFBX_MATERIAL_FBX_EMISSION_COLOR, ufbxi_string_literal("Emissive") },
	{ UFBX_MATERIAL_FBX_EMISSION_COLOR, ufbxi_string_literal("EmissiveColor") },
	{ UFBX_MATERIAL_FBX_EMISSION_FACTOR, ufbxi_string_literal("EmissiveFactor") },
	{ UFBX_MATERIAL_FBX_AMBIENT_COLOR, ufbxi_string_literal("Ambient") },
	{ UFBX_MATERIAL_FBX_AMBIENT_COLOR, ufbxi_string_literal("AmbientColor") },
	{ UFBX_MATERIAL_FBX_AMBIENT_FACTOR, ufbxi_string_literal("AmbientFactor") },
	{ UFBX_MATERIAL_FBX_NORMAL_MAP, ufbxi_string_literal("NormalMap") },
	{ UFBX_MATERIAL_FBX_BUMP, ufbxi_string_literal("Bump") },
	{ UFBX_MATERIAL_FBX_BUMP_FACTOR, ufbxi_string_literal("BumpFactor") },
	{ UFBX_MATERIAL_FBX_DISPLACEMENT, ufbxi_string_literal("Displacement") },
	{ UFBX_MATERIAL_FBX_DISPLACEMENT_FACTOR, ufbxi_string_literal("DisplacementFactor") },
	{ UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT, ufbxi_string_literal("VectorDisplacement") },
	{ UFBX_MATERIAL_FBX_VECTOR_DISPLACEMENT_FACTOR, ufbxi_string_literal("VectorDisplacementFactor") },
};

static void ufbxi_mat_transform_unknown_shininess(ufbx_vec3 *v) { if (v->x >= 0.0f) v->x = (ufbx_real)(1.0f - sqrt(v->x) * 0.1f); }

static const ufbxi_shader_mapping ufbxi_fbx_lambert_shader_pbr_mapping[] = {
	{ UFBX_MATERIAL_PBR_BASE_COLOR, ufbxi_string_literal("Diffuse") },
	{ UFBX_MATERIAL_PBR_BASE_COLOR, ufbxi_string_literal("DiffuseColor") },
	{ UFBX_MATERIAL_PBR_BASE_FACTOR, ufbxi_string_literal("DiffuseFactor") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_COLOR, ufbxi_string_literal("Transparent") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_COLOR, ufbxi_string_literal("TransparentColor") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR, ufbxi_string_literal("TransparentFactor") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR, ufbxi_string_literal("TransparencyFactor") },
	{ UFBX_MATERIAL_PBR_EMISSION_COLOR, ufbxi_string_literal("Emissive") },
	{ UFBX_MATERIAL_PBR_EMISSION_COLOR, ufbxi_string_literal("EmissiveColor") },
	{ UFBX_MATERIAL_PBR_EMISSION_FACTOR, ufbxi_string_literal("EmissiveFactor") },
	{ UFBX_MATERIAL_PBR_NORMAL_MAP, ufbxi_string_literal("NormalMap") },
};

static const ufbxi_shader_mapping ufbxi_fbx_phong_shader_pbr_mapping[] = {
	{ UFBX_MATERIAL_PBR_BASE_COLOR, ufbxi_string_literal("Diffuse") },
	{ UFBX_MATERIAL_PBR_BASE_COLOR, ufbxi_string_literal("DiffuseColor") },
	{ UFBX_MATERIAL_PBR_BASE_FACTOR, ufbxi_string_literal("DiffuseFactor") },
	{ UFBX_MATERIAL_PBR_SPECULAR_COLOR, ufbxi_string_literal("Specular") },
	{ UFBX_MATERIAL_PBR_SPECULAR_COLOR, ufbxi_string_literal("SpecularColor") },
	{ UFBX_MATERIAL_PBR_SPECULAR_FACTOR, ufbxi_string_literal("SpecularFactor") },
	{ UFBX_MATERIAL_PBR_ROUGHNESS, ufbxi_string_literal("Shininess"), &ufbxi_mat_transform_unknown_shininess },
	{ UFBX_MATERIAL_PBR_ROUGHNESS, ufbxi_string_literal("ShininessExponent"), &ufbxi_mat_transform_unknown_shininess },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_COLOR, ufbxi_string_literal("Transparent") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_COLOR, ufbxi_string_literal("TransparentColor") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR, ufbxi_string_literal("TransparentFactor") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR, ufbxi_string_literal("TransparencyFactor") },
	{ UFBX_MATERIAL_PBR_EMISSION_COLOR, ufbxi_string_literal("Emissive") },
	{ UFBX_MATERIAL_PBR_EMISSION_COLOR, ufbxi_string_literal("EmissiveColor") },
	{ UFBX_MATERIAL_PBR_EMISSION_FACTOR, ufbxi_string_literal("EmissiveFactor") },
	{ UFBX_MATERIAL_PBR_NORMAL_MAP, ufbxi_string_literal("NormalMap") },
};

static const ufbxi_shader_mapping ufbxi_osl_standard_shader_pbr_mapping[] = {
	{ UFBX_MATERIAL_PBR_BASE_FACTOR, ufbxi_string_literal("base") },
	{ UFBX_MATERIAL_PBR_BASE_COLOR, ufbxi_string_literal("base_color") },
	{ UFBX_MATERIAL_PBR_ROUGHNESS, ufbxi_string_literal("specular_roughness") },
	{ UFBX_MATERIAL_PBR_DIFFUSE_ROUGHNESS, ufbxi_string_literal("diffuse_roughness") },
	{ UFBX_MATERIAL_PBR_METALLIC, ufbxi_string_literal("metalness") },
	{ UFBX_MATERIAL_PBR_SPECULAR_FACTOR, ufbxi_string_literal("specular") },
	{ UFBX_MATERIAL_PBR_SPECULAR_COLOR, ufbxi_string_literal("specular_color") },
	{ UFBX_MATERIAL_PBR_SPECULAR_IOR, ufbxi_string_literal("specular_IOR") },
	{ UFBX_MATERIAL_PBR_SPECULAR_ANISOTROPY, ufbxi_string_literal("specular_anisotropy") },
	{ UFBX_MATERIAL_PBR_SPECULAR_ROTATION, ufbxi_string_literal("specular_rotation") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR, ufbxi_string_literal("transmission") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_COLOR, ufbxi_string_literal("transmission_color") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_DEPTH, ufbxi_string_literal("transmission_depth") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER, ufbxi_string_literal("transmission_scatter") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER_ANISOTROPY, ufbxi_string_literal("transmission_scatter_anisotropy") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_DISPERSION, ufbxi_string_literal("transmission_dispersion") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_ROUGHNESS, ufbxi_string_literal("transmission_extra_roughness") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_FACTOR, ufbxi_string_literal("subsurface") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_COLOR, ufbxi_string_literal("subsurface_color") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_RADIUS, ufbxi_string_literal("subsurface_radius") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_SCALE, ufbxi_string_literal("subsurface_scale") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_ANISOTROPY, ufbxi_string_literal("subsurface_anisotropy") },
	{ UFBX_MATERIAL_PBR_SHEEN_FACTOR, ufbxi_string_literal("sheen") },
	{ UFBX_MATERIAL_PBR_SHEEN_COLOR, ufbxi_string_literal("sheen_color") },
	{ UFBX_MATERIAL_PBR_SHEEN_ROUGHNESS, ufbxi_string_literal("sheen_roughness") },
	{ UFBX_MATERIAL_PBR_COAT_FACTOR, ufbxi_string_literal("coat") },
	{ UFBX_MATERIAL_PBR_COAT_COLOR, ufbxi_string_literal("coat_color") },
	{ UFBX_MATERIAL_PBR_COAT_ROUGHNESS, ufbxi_string_literal("coat_roughness") },
	{ UFBX_MATERIAL_PBR_COAT_IOR, ufbxi_string_literal("coat_IOR") },
	{ UFBX_MATERIAL_PBR_COAT_ANISOTROPY, ufbxi_string_literal("coat_anisotropy") },
	{ UFBX_MATERIAL_PBR_COAT_ROTATION, ufbxi_string_literal("coat_rotation") },
	{ UFBX_MATERIAL_PBR_COAT_NORMAL, ufbxi_string_literal("coat_normal") },
	{ UFBX_MATERIAL_PBR_THIN_FILM_THICKNESS, ufbxi_string_literal("thin_film_thickness") },
	{ UFBX_MATERIAL_PBR_THIN_FILM_IOR, ufbxi_string_literal("thin_film_IOR") },
	{ UFBX_MATERIAL_PBR_EMISSION_FACTOR, ufbxi_string_literal("emission") },
	{ UFBX_MATERIAL_PBR_EMISSION_COLOR, ufbxi_string_literal("emission_color") },
	{ UFBX_MATERIAL_PBR_OPACITY, ufbxi_string_literal("opacity") },
	{ UFBX_MATERIAL_PBR_NORMAL_MAP, ufbxi_string_literal("NormalMap") },
	{ UFBX_MATERIAL_PBR_NORMAL_MAP, ufbxi_string_literal("normalCamera") },
	{ UFBX_MATERIAL_PBR_TANGENT_MAP, ufbxi_string_literal("tangent") },
	{ UFBX_MATERIAL_PBR_THIN_WALLED, ufbxi_string_literal("thin_walled") },
};

static const ufbxi_shader_mapping ufbxi_arnold_shader_pbr_mapping[] = {
	{ UFBX_MATERIAL_PBR_BASE_FACTOR, ufbxi_string_literal("base") },
	{ UFBX_MATERIAL_PBR_BASE_COLOR, ufbxi_string_literal("baseColor") },
	{ UFBX_MATERIAL_PBR_ROUGHNESS, ufbxi_string_literal("specularRoughness") },
	{ UFBX_MATERIAL_PBR_DIFFUSE_ROUGHNESS, ufbxi_string_literal("diffuseRoughness") },
	{ UFBX_MATERIAL_PBR_METALLIC, ufbxi_string_literal("metalness") },
	{ UFBX_MATERIAL_PBR_SPECULAR_FACTOR, ufbxi_string_literal("specular") },
	{ UFBX_MATERIAL_PBR_SPECULAR_COLOR, ufbxi_string_literal("specularColor") },
	{ UFBX_MATERIAL_PBR_SPECULAR_IOR, ufbxi_string_literal("specularIOR") },
	{ UFBX_MATERIAL_PBR_SPECULAR_ANISOTROPY, ufbxi_string_literal("specularAnisotropy") },
	{ UFBX_MATERIAL_PBR_SPECULAR_ROTATION, ufbxi_string_literal("specularRotation") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_FACTOR, ufbxi_string_literal("transmission") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_COLOR, ufbxi_string_literal("transmissionColor") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_DEPTH, ufbxi_string_literal("transmissionDepth") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER, ufbxi_string_literal("transmissionScatter") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_SCATTER_ANISOTROPY, ufbxi_string_literal("transmissionScatterAnisotropy") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_DISPERSION, ufbxi_string_literal("transmissionDispersion") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_ROUGHNESS, ufbxi_string_literal("transmissionExtraRoughness") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_FACTOR, ufbxi_string_literal("subsurface") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_COLOR, ufbxi_string_literal("subsurfaceColor") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_RADIUS, ufbxi_string_literal("subsurfaceRadius") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_SCALE, ufbxi_string_literal("subsurfaceScale") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_ANISOTROPY, ufbxi_string_literal("subsurfaceAnisotropy") },
	{ UFBX_MATERIAL_PBR_SHEEN_FACTOR, ufbxi_string_literal("sheen") },
	{ UFBX_MATERIAL_PBR_SHEEN_COLOR, ufbxi_string_literal("sheenColor") },
	{ UFBX_MATERIAL_PBR_SHEEN_ROUGHNESS, ufbxi_string_literal("sheenRoughness") },
	{ UFBX_MATERIAL_PBR_COAT_FACTOR, ufbxi_string_literal("coat") },
	{ UFBX_MATERIAL_PBR_COAT_COLOR, ufbxi_string_literal("coatColor") },
	{ UFBX_MATERIAL_PBR_COAT_ROUGHNESS, ufbxi_string_literal("coatRoughness") },
	{ UFBX_MATERIAL_PBR_COAT_IOR, ufbxi_string_literal("coatIOR") },
	{ UFBX_MATERIAL_PBR_COAT_ANISOTROPY, ufbxi_string_literal("coatAnisotropy") },
	{ UFBX_MATERIAL_PBR_COAT_ROTATION, ufbxi_string_literal("coatRotation") },
	{ UFBX_MATERIAL_PBR_COAT_NORMAL, ufbxi_string_literal("coatNormal") },
	{ UFBX_MATERIAL_PBR_THIN_FILM_THICKNESS, ufbxi_string_literal("thinFilmThickness") },
	{ UFBX_MATERIAL_PBR_THIN_FILM_IOR, ufbxi_string_literal("thinFilmIOR") },
	{ UFBX_MATERIAL_PBR_EMISSION_FACTOR, ufbxi_string_literal("emission") },
	{ UFBX_MATERIAL_PBR_EMISSION_COLOR, ufbxi_string_literal("emissionColor") },
	{ UFBX_MATERIAL_PBR_OPACITY, ufbxi_string_literal("opacity") },
	{ UFBX_MATERIAL_PBR_INDIRECT_DIFFUSE, ufbxi_string_literal("indirectDiffuse") },
	{ UFBX_MATERIAL_PBR_INDIRECT_SPECULAR, ufbxi_string_literal("indirectSpecular") },
	{ UFBX_MATERIAL_PBR_NORMAL_MAP, ufbxi_string_literal("NormalMap") },
	{ UFBX_MATERIAL_PBR_NORMAL_MAP, ufbxi_string_literal("normalCamera") },
	{ UFBX_MATERIAL_PBR_TANGENT_MAP, ufbxi_string_literal("tangent") },
	{ UFBX_MATERIAL_PBR_MATTE_ENABLED, ufbxi_string_literal("aiEnableMatte") },
	{ UFBX_MATERIAL_PBR_MATTE_COLOR, ufbxi_string_literal("aiMatteColor") },
	{ UFBX_MATERIAL_PBR_MATTE_FACTOR, ufbxi_string_literal("aiMatteColorA") },
	{ UFBX_MATERIAL_PBR_SUBSURFACE_TYPE, ufbxi_string_literal("subsurfaceType") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_PRIORITY, ufbxi_string_literal("dielectricPriority") },
	{ UFBX_MATERIAL_PBR_TRANSMISSION_ENABLE_IN_AOV, ufbxi_string_literal("transmitAovs") },
	{ UFBX_MATERIAL_PBR_THIN_WALLED, ufbxi_string_literal("thinWalled") },
	{ UFBX_MATERIAL_PBR_CAUSTICS, ufbxi_string_literal("caustics") },
	{ UFBX_MATERIAL_PBR_INTERNAL_REFLECTIONS, ufbxi_string_literal("internalReflections") },
	{ UFBX_MATERIAL_PBR_EXIT_TO_BACKGROUND, ufbxi_string_literal("exitToBackground") },
};

static void ufbxi_mat_transform_blender_opacity(ufbx_vec3 *v) { v->x = 1.0f - v->x; }
static void ufbxi_mat_transform_blender_shininess(ufbx_vec3 *v) { if (v->x >= 0.0f) v->x = (ufbx_real)(1.0f - sqrt(v->x) * 0.1f); }

static const ufbxi_shader_mapping ufbxi_blender_phong_shader_pbr_mapping[] = {
	{ UFBX_MATERIAL_PBR_BASE_COLOR, ufbxi_string_literal("DiffuseColor") },
	{ UFBX_MATERIAL_PBR_OPACITY, ufbxi_string_literal("TransparencyFactor"), &ufbxi_mat_transform_blender_opacity },
	{ UFBX_MATERIAL_PBR_EMISSION_FACTOR, ufbxi_string_literal("EmissiveFactor") },
	{ UFBX_MATERIAL_PBR_EMISSION_COLOR, ufbxi_string_literal("EmissiveColor") },
	{ UFBX_MATERIAL_PBR_ROUGHNESS, ufbxi_string_literal("Shininess"), &ufbxi_mat_transform_blender_shininess },
	{ UFBX_MATERIAL_PBR_ROUGHNESS, ufbxi_string_literal("ShininessExponent"), &ufbxi_mat_transform_blender_shininess },
	{ UFBX_MATERIAL_PBR_METALLIC, ufbxi_string_literal("ReflectionFactor") },
	{ UFBX_MATERIAL_PBR_NORMAL_MAP, ufbxi_string_literal("NormalMap") },
};

static const ufbxi_shader_mapping_list ufbxi_shader_pbr_mappings[] = {
	{ ufbxi_fbx_phong_shader_pbr_mapping, ufbxi_arraycount(ufbxi_fbx_phong_shader_pbr_mapping) }, // UFBX_SHADER_UNKNOWN
	{ ufbxi_fbx_lambert_shader_pbr_mapping, ufbxi_arraycount(ufbxi_fbx_lambert_shader_pbr_mapping) }, // UFBX_SHADER_FBX_LAMBERT
	{ ufbxi_fbx_phong_shader_pbr_mapping, ufbxi_arraycount(ufbxi_fbx_phong_shader_pbr_mapping) }, // UFBX_SHADER_FBX_PHONG
	{ ufbxi_osl_standard_shader_pbr_mapping, ufbxi_arraycount(ufbxi_osl_standard_shader_pbr_mapping) }, // UFBX_SHADER_OSL_STANDARD
	{ ufbxi_arnold_shader_pbr_mapping, ufbxi_arraycount(ufbxi_arnold_shader_pbr_mapping) }, // UFBX_SHADER_ARNOLD
	{ ufbxi_blender_phong_shader_pbr_mapping, ufbxi_arraycount(ufbxi_blender_phong_shader_pbr_mapping) }, // UFBX_SHADER_BLENDER_PHONG
};

ufbx_static_assert(shader_pbr_mapping_list, ufbxi_arraycount(ufbxi_shader_pbr_mappings) == UFBX_NUM_SHADER_TYPES);

ufbxi_noinline static void ufbxi_fetch_mapping_maps(ufbx_material *material, ufbx_material_map *maps, ufbx_shader *shader, const ufbxi_shader_mapping *mappings, size_t count)
{
	ufbxi_for(const ufbxi_shader_mapping, mapping, mappings, count) {
		ufbx_string name = ufbx_find_shader_prop_len(shader, mapping->prop.data, mapping->prop.length);
		ufbx_texture *texture = ufbx_find_prop_texture_len(material, name.data, name.length);
		ufbx_prop *prop = ufbx_find_prop_len(&material->props, name.data, name.length);
		ufbx_material_map *map = &maps[mapping->index];

		if (prop) {
			map->value = prop->value_vec3;
			map->value_int = prop->value_int;
			map->has_value = true;
			if (mapping->transform_fn) {
				mapping->transform_fn(&map->value);
			}
		}
		if (texture) {
			map->texture = texture;
		}
	}
}

ufbxi_noinline static void ufbxi_update_factor(ufbx_material_map *factor_map, ufbx_material_map *color_map)
{
	if (!factor_map->has_value) {
		if (color_map->has_value && !ufbxi_is_vec3_zero(color_map->value)) {
			factor_map->value.x = 1.0f;
			factor_map->value_int = 1;
		} else {
			factor_map->value.x = 0.0f;
			factor_map->value_int = 0;
		}
	}
}

ufbxi_noinline static void ufbxi_fetch_maps(ufbx_scene *scene, ufbx_material *material)
{
	(void)scene;

	ufbx_shader *shader = material->shader;
	ufbx_assert(material->shader_type < UFBX_NUM_SHADER_TYPES);

	memset(&material->fbx, 0, sizeof(material->fbx));
	memset(&material->pbr, 0, sizeof(material->pbr));

	ufbxi_fetch_mapping_maps(material, material->fbx.maps, NULL, ufbxi_base_fbx_mapping, ufbxi_arraycount(ufbxi_base_fbx_mapping));

	ufbxi_shader_mapping_list list = ufbxi_shader_pbr_mappings[material->shader_type];
	ufbxi_fetch_mapping_maps(material, material->pbr.maps, shader, list.data, list.count);

	ufbxi_update_factor(&material->fbx.diffuse_factor, &material->fbx.diffuse_color);
	ufbxi_update_factor(&material->fbx.specular_factor, &material->fbx.specular_color);
	ufbxi_update_factor(&material->fbx.reflection_factor, &material->fbx.reflection_color);
	ufbxi_update_factor(&material->fbx.transparency_factor, &material->fbx.transparency_color);
	ufbxi_update_factor(&material->fbx.emission_factor, &material->fbx.emission_color);
	ufbxi_update_factor(&material->fbx.ambient_factor, &material->fbx.ambient_color);

	ufbxi_update_factor(&material->pbr.base_factor, &material->pbr.base_color);
	ufbxi_update_factor(&material->pbr.specular_factor, &material->pbr.specular_color);
	ufbxi_update_factor(&material->pbr.emission_factor, &material->pbr.emission_color);
}

typedef enum {
	UFBXI_CONSTRAINT_PROP_NODE,
	UFBXI_CONSTRAINT_PROP_IK_EFFECTOR,
	UFBXI_CONSTRAINT_PROP_IK_END_NODE,
	UFBXI_CONSTRAINT_PROP_AIM_UP,
	UFBXI_CONSTRAINT_PROP_TARGET,
} ufbxi_constraint_prop_type;

typedef struct {
	ufbxi_constraint_prop_type type;
	const char *name;
} ufbxi_constraint_prop;

static const ufbxi_constraint_prop ufbxi_constraint_props[] = {
	{ UFBXI_CONSTRAINT_PROP_NODE, "Constrained Object" },
	{ UFBXI_CONSTRAINT_PROP_NODE, "Constrained object (Child)" },
	{ UFBXI_CONSTRAINT_PROP_NODE, "First Joint" },
	{ UFBXI_CONSTRAINT_PROP_TARGET, "Source" },
	{ UFBXI_CONSTRAINT_PROP_TARGET, "Source (Parent)" },
	{ UFBXI_CONSTRAINT_PROP_TARGET, "Aim At Object" },
	{ UFBXI_CONSTRAINT_PROP_TARGET, "Pole Vector Object" },
	{ UFBXI_CONSTRAINT_PROP_IK_EFFECTOR, "Effector" },
	{ UFBXI_CONSTRAINT_PROP_IK_END_NODE, "End Joint" },
	{ UFBXI_CONSTRAINT_PROP_AIM_UP, "World Up Object" },
};

ufbxi_nodiscard ufbxi_noinline static int ufbxi_add_constraint_prop(ufbxi_context *uc, ufbx_constraint *constraint, ufbx_node *node, const char *prop)
{
	ufbxi_for(const ufbxi_constraint_prop, cprop, ufbxi_constraint_props, ufbxi_arraycount(ufbxi_constraint_props)) {
		if (strcmp(cprop->name, prop) != 0) continue;
		switch (cprop->type) {
		case UFBXI_CONSTRAINT_PROP_NODE: constraint->node = node; break;
		case UFBXI_CONSTRAINT_PROP_IK_EFFECTOR: constraint->ik_effector = node; break;
		case UFBXI_CONSTRAINT_PROP_IK_END_NODE: constraint->ik_end_node = node; break;
		case UFBXI_CONSTRAINT_PROP_AIM_UP: constraint->aim_up_node = node; break;
		case UFBXI_CONSTRAINT_PROP_TARGET: {
			ufbx_constraint_target *target = ufbxi_push_zero(&uc->tmp_stack, ufbx_constraint_target, 1);
			ufbxi_check(target);
			target->node = node;
			target->weight = 1.0f;
			target->transform = ufbx_identity_transform;
		} break;
		}
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_init_file_paths(ufbxi_context *uc)
{
	uc->scene.metadata.filename = uc->opts.filename;
	ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, &uc->scene.metadata.filename));

	ufbx_string root = uc->opts.filename;
	for (; root.length > 0; root.length--) {
		char c = root.data[root.length - 1];
		bool is_separator = c == '/';
#if defined(_WIN32)
		if (c == '\\') is_separator = true;
#endif
		if (is_separator) {
			root.length--;
			break;
		}
	}
	uc->scene.metadata.relative_root = root;
	ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, &uc->scene.metadata.relative_root));

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_resolve_relative_filename(ufbxi_context *uc, ufbx_string *dst, ufbx_string relative)
{
	// Skip leading directory separators and early return if the relative path is empty
	while (relative.length > 0 && (relative.data[0] == '/' || relative.data[0] == '\\')) {
		relative.data++;
		relative.length--;
	}
	if (relative.length == 0) {
		*dst = ufbx_empty_string;
		return 1;
	}

#if defined(_WIN32)
	char separator = '\\';
#else
	char separator = '/';
#endif

	ufbx_string prefix = uc->scene.metadata.relative_root;
	size_t result_cap = prefix.length + relative.length + 1;
	char *result = ufbxi_push(&uc->tmp_stack, char, result_cap);
	ufbxi_check(result);
	char *ptr = result;

	// Copy prefix and suffix converting separators in the process
	if (prefix.length > 0) {
		memcpy(ptr, prefix.data, prefix.length);
		ptr[prefix.length] = separator;
		ptr += prefix.length + 1;
	}
	for (size_t i = 0; i < relative.length; i++) {
		char c = relative.data[i];
		if (c == '/' || c == '\\') {
			c = separator;
		}
		*ptr++ = c;
	}

	// Intern the string and pop the temporary buffef
	dst->data = result;
	dst->length = (size_t)(ptr - result);
	ufbx_assert(dst->length <= result_cap);
	ufbxi_check(ufbxi_push_string_place_str(&uc->string_pool, dst));
	ufbxi_pop(&uc->tmp_stack, char, result_cap, NULL);

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_finalize_nurbs_basis(ufbxi_context *uc, ufbx_nurbs_basis *basis)
{
	if (basis->topology == UFBX_NURBS_TOPOLOGY_CLOSED) {
		basis->num_wrap_control_points = 1;
	} else if (basis->topology == UFBX_NURBS_TOPOLOGY_PERIODIC) {
		basis->num_wrap_control_points = basis->order - 1;
	} else {
		basis->num_wrap_control_points = 0;
	}

	if (basis->order > 0) {
		size_t degree = basis->order - 1;
		ufbx_real_list knots = basis->knot_vector;
		if (knots.count >= 2*degree + 1) {
			basis->t_min = knots.data[degree];
			basis->t_max = knots.data[knots.count - degree - 1];

			size_t max_spans = knots.count - 2*degree;
			ufbx_real *spans = ufbxi_push(&uc->result, ufbx_real, max_spans);
			ufbxi_check(spans);

			ufbx_real prev = -INFINITY;
			size_t num_spans = 0;
			for (size_t i = 0; i < max_spans; i++) {
				ufbx_real t = knots.data[degree + i];
				if (t != prev) {
					spans[num_spans++] = t;
					prev = t;
				}
			}

			basis->spans.data = spans;
			basis->spans.count = num_spans;
			basis->valid = true;
			for (size_t i = 1; i < knots.count; i++) {
				if (knots.data[i - 1] > knots.data[i]) {
					basis->valid = false;
					break;
				}
			}
		}
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_finalize_lod_group(ufbxi_context *uc, ufbx_lod_group *lod)
{
	size_t num_levels = 0;
	for (size_t i = 0; i < lod->instances.count; i++) {
		num_levels = ufbxi_max_sz(num_levels, lod->instances.data[0]->children.count);
	}

	char prop_name[64];
	for (size_t i = 0; ; i++) {
		int len = snprintf(prop_name, sizeof(prop_name), "Thresholds|Level%zu", i);
		ufbx_prop *prop = ufbx_find_prop_len(&lod->props, prop_name, (size_t)len);
		if (!prop) break;
		num_levels = ufbxi_max_sz(num_levels, i + 1);
	}

	ufbx_lod_level *levels = ufbxi_push_zero(&uc->result, ufbx_lod_level, num_levels);
	ufbxi_check(levels);

	lod->relative_distances = ufbx_find_bool(&lod->props, "ThresholdsUsedAsPercentage", false);
	lod->ignore_parent_transform = !ufbx_find_bool(&lod->props, "WorldSpace", true);

	lod->use_distance_limit = ufbx_find_bool(&lod->props, "MinMaxDistance", false);
	lod->distance_limit_min = ufbx_find_real(&lod->props, "MinDistance", (ufbx_real)-100.0);
	lod->distance_limit_max = ufbx_find_real(&lod->props, "MaxDistance", (ufbx_real)100.0);

	lod->lod_levels.data = levels;
	lod->lod_levels.count = num_levels;

	for (size_t i = 0; i < num_levels; i++) {
		ufbx_lod_level *level = &levels[i];

		if (i > 0) {
			int len = snprintf(prop_name, sizeof(prop_name), "Thresholds|Level%zu", i - 1);
			level->distance = ufbx_find_real_len(&lod->props, prop_name, (size_t)len, 0.0f);
		} else if (lod->relative_distances) {
			level->distance = (ufbx_real)100.0;
		}

		{
			int len = snprintf(prop_name, sizeof(prop_name), "DisplayLevels|Level%zu", i);
			int64_t display = ufbx_find_int_len(&lod->props, prop_name, (size_t)len, 0);
			if (display >= 0 && display <= 2) {
				level->display = (ufbx_lod_display)display;
			}
		}
	}

	return 1;
}

ufbxi_nodiscard ufbxi_noinline static int ufbxi_finalize_scene(ufbxi_context *uc)
{
	size_t num_elements = uc->num_elements;

	uc->scene.elements.count = num_elements;
	uc->scene.elements.data = ufbxi_push(&uc->result, ufbx_element*, num_elements);
	ufbxi_check(uc->scene.elements.data);

	uc->scene.metadata.element_buffer_size = uc->tmp_element_byte_offset;
	char *element_data = (char*)ufbxi_push_pop(&uc->result, &uc->tmp_elements, uint64_t, uc->tmp_element_byte_offset/8);
	ufbxi_buf_free(&uc->tmp_elements);
	ufbxi_check(element_data);

	size_t *element_offsets = ufbxi_push_pop(&uc->tmp, &uc->tmp_element_offsets, size_t, uc->tmp_element_offsets.num_items);
	ufbxi_buf_free(&uc->tmp_element_offsets);
	ufbxi_check(element_offsets);
	for (size_t i = 0; i < num_elements; i++) {
		uc->scene.elements.data[i] = (ufbx_element*)(element_data + element_offsets[i]);
	}
	uc->scene.elements.count = num_elements;
	ufbxi_buf_free(&uc->tmp_element_offsets);

	// Resolve and add the connections to elements
	ufbxi_check(ufbxi_resolve_connections(uc));
	ufbxi_check(ufbxi_add_connections_to_elements(uc));
	ufbxi_check(ufbxi_linearize_nodes(uc));

	for (size_t type = 0; type < UFBX_NUM_ELEMENT_TYPES; type++) {
		size_t num_typed = uc->tmp_typed_element_offsets[type].num_items;
		size_t *typed_offsets = ufbxi_push_pop(&uc->tmp, &uc->tmp_typed_element_offsets[type], size_t, num_typed);
		ufbxi_buf_free(&uc->tmp_typed_element_offsets[type]);
		ufbxi_check(typed_offsets);

		ufbx_element_list *typed_elems = &uc->scene.elements_by_type[type];
		typed_elems->count = num_typed;
		typed_elems->data = ufbxi_push(&uc->result, ufbx_element*, num_typed);
		ufbxi_check(typed_elems->data);

		for (size_t i = 0; i < num_typed; i++) {
			typed_elems->data[i] = (ufbx_element*)(element_data + typed_offsets[i]);
		}

		ufbxi_buf_free(&uc->tmp_typed_element_offsets[type]);
	}

	// Create named elements
	uc->scene.elements_by_name.count = num_elements;
	uc->scene.elements_by_name.data = ufbxi_push(&uc->result, ufbx_name_element, num_elements);
	ufbxi_check(uc->scene.elements_by_name.data);

	for (size_t i = 0; i < num_elements; i++) {

		ufbx_element *elem = uc->scene.elements.data[i];
		ufbx_name_element *name_elem = &uc->scene.elements_by_name.data[i];

		name_elem->name = elem->name;
		name_elem->type = elem->type;
		name_elem->internal_key = ufbxi_get_name_key(elem->name.data, elem->name.length);
		name_elem->element = elem;
	}

	ufbxi_check(ufbxi_sort_name_elements(uc, uc->scene.elements_by_name.data, num_elements));

	// Setup node children arrays and attribute pointers/lists
	ufbxi_for_ptr_list(ufbx_node, p_node, uc->scene.nodes) {
		ufbx_node *node = *p_node, *parent = node->parent;
		if (parent) {
			parent->children.count++;
			if (parent->children.data == NULL) {
				parent->children.data = p_node;
			}
		}

		ufbx_connection_list conns = ufbxi_find_dst_connections(&node->element, NULL);

		ufbxi_for_list(ufbx_connection, conn, conns) {
			ufbx_element *elem = conn->src;
			ufbx_element_type type = elem->type;
			if (!(type >= UFBX_ELEMENT_TYPE_FIRST_ATTRIB && type <= UFBX_ELEMENT_TYPE_LAST_ATTRIB)) continue;

			size_t index = node->all_attribs.count++;
			if (index == 0) {
				node->attrib = elem;
				node->attrib_type = type;
			} else {
				if (index == 1) {
					ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_element*, 1, &node->attrib));
				}
				ufbxi_check(ufbxi_push_copy(&uc->tmp_stack, ufbx_element*, 1, &elem));
			}

			switch (elem->type) {
			case UFBX_ELEMENT_MESH: node->mesh = (ufbx_mesh*)elem; break;
			case UFBX_ELEMENT_LIGHT: node->light = (ufbx_light*)elem; break;
			case UFBX_ELEMENT_CAMERA: node->camera = (ufbx_camera*)elem; break;
			case UFBX_ELEMENT_BONE: node->bone = (ufbx_bone*)elem; break;
			default: /* No shorthand */ break;
			}
		}

		if (node->all_attribs.count > 1) {
			node->all_attribs.data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_element*, node->all_attribs.count);
			ufbxi_check(node->all_attribs.data);
		} else if (node->all_attribs.count == 1) {
			node->all_attribs.data = &node->attrib;
		}
	}

	// Resolve bind pose bones that don't use the normal connection system
	ufbxi_for_ptr_list(ufbx_pose, p_pose, uc->scene.poses) {
		ufbx_pose *pose = *p_pose;

		// HACK: Transport `ufbxi_tmp_bone_pose` array through the `ufbx_bone_pose` pointer
		size_t num_bones = pose->bone_poses.count;
		ufbxi_tmp_bone_pose *tmp_poses = (ufbxi_tmp_bone_pose*)pose->bone_poses.data;
		pose->bone_poses.data = ufbxi_push(&uc->result, ufbx_bone_pose, num_bones);
		ufbxi_check(pose->bone_poses.data);

		// Filter only found bones
		pose->bone_poses.count = 0;
		for (size_t i = 0; i < num_bones; i++) {
			ufbx_element *elem = ufbxi_find_element_by_fbx_id(uc, tmp_poses[i].bone_fbx_id);
			if (!elem || elem->type != UFBX_ELEMENT_NODE) continue;

			ufbx_bone_pose *bone = &pose->bone_poses.data[pose->bone_poses.count++];
			bone->bone_node = (ufbx_node*)elem;
			bone->bone_to_world = tmp_poses[i].bone_to_world;

			if (pose->bind_pose) {
				ufbx_connection_list node_conns = ufbxi_find_src_connections(elem, NULL);
				ufbxi_for_list(ufbx_connection, conn, node_conns) {
					if (conn->dst->type != UFBX_ELEMENT_SKIN_CLUSTER) continue;
					ufbx_skin_cluster *cluster = (ufbx_skin_cluster*)conn->dst;
					if (ufbxi_matrix_all_zero(&cluster->bind_to_world)) {
						cluster->bind_to_world = bone->bone_to_world;
					}
				}
			}
		}
	}

	// Fetch pointers that may break elements

	// Setup node attribute instances
	for (int type = UFBX_ELEMENT_TYPE_FIRST_ATTRIB; type <= UFBX_ELEMENT_TYPE_LAST_ATTRIB; type++) {
		ufbxi_for_ptr_list(ufbx_element, p_elem, uc->scene.elements_by_type[type]) {
			ufbx_element *elem = *p_elem;
			ufbxi_check(ufbxi_fetch_src_elements(uc, &elem->instances, elem, false, NULL, UFBX_ELEMENT_NODE));
		}
	}

	bool search_node = uc->version < 7000;

	ufbxi_for_ptr_list(ufbx_skin_cluster, p_cluster, uc->scene.skin_clusters) {
		ufbx_skin_cluster *cluster = *p_cluster;
		cluster->bone_node = (ufbx_node*)ufbxi_fetch_dst_element(&cluster->element, false, NULL, UFBX_ELEMENT_NODE);
	}

	ufbxi_for_ptr_list(ufbx_skin_deformer, p_skin, uc->scene.skin_deformers) {
		ufbx_skin_deformer *skin = *p_skin;
		ufbxi_check(ufbxi_fetch_dst_elements(uc, &skin->clusters, &skin->element, false, NULL, UFBX_ELEMENT_SKIN_CLUSTER));

		// Remove clusters without a valid `bone`
		if (!uc->opts.connect_broken_elements) {
			size_t num_broken = 0;
			for (size_t i = 0; i < skin->clusters.count; i++) {
				if (!skin->clusters.data[i]->bone_node) {
					num_broken++;
				} else if (num_broken > 0) {
					skin->clusters.data[i - num_broken] = skin->clusters.data[i];
				}
			}
			skin->clusters.count -= num_broken;
		}

		size_t total_weights = 0;
		ufbxi_for_ptr_list(ufbx_skin_cluster, p_cluster, skin->clusters) {
			ufbx_skin_cluster *cluster = *p_cluster;
			ufbxi_check(SIZE_MAX - total_weights > cluster->num_weights);
			total_weights += cluster->num_weights;
		}

		size_t num_vertices = 0;

		// Iterate through meshes so we can pad the vertices to the largest one
		{
			ufbx_connection_list conns = ufbxi_find_src_connections(&skin->element, NULL);
			ufbxi_for_list(ufbx_connection, conn, conns) {
				ufbx_mesh *mesh = NULL;
				if (conn->dst_prop.length > 0) continue;
				if (conn->dst->type == UFBX_ELEMENT_MESH) {
					mesh = (ufbx_mesh*)conn->dst;
				} else if (conn->dst->type == UFBX_ELEMENT_NODE) {
					mesh = ((ufbx_node*)conn->dst)->mesh;
				}
				if (!mesh) continue;
				num_vertices = ufbxi_max_sz(num_vertices, mesh->num_vertices);
			}
		}

		if (!uc->opts.skip_skin_vertices) {
			skin->vertices.count = num_vertices;
			skin->vertices.data = ufbxi_push_zero(&uc->result, ufbx_skin_vertex, num_vertices);
			ufbxi_check(skin->vertices.data);

			skin->weights.count = total_weights;
			skin->weights.data = ufbxi_push_zero(&uc->result, ufbx_skin_weight, total_weights);
			ufbxi_check(skin->weights.data);

			// Count the number of weights per vertex
			ufbxi_for_ptr_list(ufbx_skin_cluster, p_cluster, skin->clusters) {
				ufbx_skin_cluster *cluster = *p_cluster;
				for (size_t i = 0; i < cluster->num_weights; i++) {
					int32_t vertex = cluster->vertices[i];
					if (vertex >= 0 && (size_t)vertex < num_vertices) {
						skin->vertices.data[vertex].num_weights++;
					}
				}
			}

			ufbx_real default_dq = skin->skinning_method == UFBX_SKINNING_DUAL_QUATERNION ? 1.0f : 0.0f;

			// Prefix sum to assign the vertex weight offsets and set up default DQ values
			uint32_t offset = 0;
			uint32_t max_weights = 0;
			for (size_t i = 0; i < num_vertices; i++) {
				skin->vertices.data[i].weight_begin = offset;
				skin->vertices.data[i].dq_weight = default_dq;
				uint32_t num_weights = skin->vertices.data[i].num_weights;
				offset += num_weights;
				skin->vertices.data[i].num_weights = 0;

				if (num_weights > max_weights) max_weights = num_weights;
			}
			ufbx_assert(offset <= total_weights);
			skin->max_weights_per_vertex = max_weights;

			// Copy the DQ weights to vertices
			for (size_t i = 0; i < skin->num_dq_weights; i++) {
				int32_t vertex = skin->dq_vertices[i];
				if (vertex >= 0 && (size_t)vertex < num_vertices) {
					skin->vertices.data[vertex].dq_weight = skin->dq_weights[i];
				}
			}

			// Copy the weights to vertices
			uint32_t cluster_index = 0;
			ufbxi_for_ptr_list(ufbx_skin_cluster, p_cluster, skin->clusters) {
				ufbx_skin_cluster *cluster = *p_cluster;
				for (size_t i = 0; i < cluster->num_weights; i++) {
					int32_t vertex = cluster->vertices[i];
					if (vertex >= 0 && (size_t)vertex < num_vertices) {
						uint32_t local_index = skin->vertices.data[vertex].num_weights++;
						uint32_t index = skin->vertices.data[vertex].weight_begin + local_index;
						skin->weights.data[index].cluster_index = cluster_index;
						skin->weights.data[index].weight = cluster->weights[i];
					}
				}
				cluster_index++;
			}

			// Sort the vertex weights by descending weight value
			ufbxi_check(ufbxi_sort_skin_weights(uc, skin));
		}
	}

	ufbxi_for_ptr_list(ufbx_blend_deformer, p_blend, uc->scene.blend_deformers) {
		ufbx_blend_deformer *blend = *p_blend;
		ufbxi_check(ufbxi_fetch_dst_elements(uc, &blend->channels, &blend->element, false, NULL, UFBX_ELEMENT_BLEND_CHANNEL));
	}

	ufbxi_for_ptr_list(ufbx_cache_deformer, p_deformer, uc->scene.cache_deformers) {
		ufbx_cache_deformer *deformer = *p_deformer;
		deformer->channel = ufbx_find_string(&deformer->props, "ChannelName", ufbx_empty_string);
		deformer->file = (ufbx_cache_file*)ufbxi_fetch_dst_element(&deformer->element, false, NULL, UFBX_ELEMENT_CACHE_FILE);
	}

	ufbxi_for_ptr_list(ufbx_cache_file, p_cache, uc->scene.cache_files) {
		ufbx_cache_file *cache = *p_cache;

		cache->absolute_filename = ufbx_find_string(&cache->props, "CacheAbsoluteFileName", ufbx_empty_string);
		cache->relative_filename = ufbx_find_string(&cache->props, "CacheFileName", ufbx_empty_string);
		int64_t type = ufbx_find_int(&cache->props, "CacheFileType", 0);
		if (type >= 0 && type <= UFBX_CACHE_FILE_FORMAT_MC) {
			cache->format = (ufbx_cache_file_format)type;
		}

		ufbxi_check(ufbxi_resolve_relative_filename(uc, &cache->filename, cache->relative_filename));
	}

	ufbx_assert(uc->tmp_full_weights.num_items == uc->scene.blend_channels.count);
	ufbx_real_list *full_weights = ufbxi_push_pop(&uc->tmp, &uc->tmp_full_weights, ufbx_real_list, uc->tmp_full_weights.num_items);
	ufbxi_buf_free(&uc->tmp_full_weights);
	ufbxi_check(full_weights);

	ufbxi_for_ptr_list(ufbx_blend_channel, p_channel, uc->scene.blend_channels) {
		ufbx_blend_channel *channel = *p_channel;

		ufbxi_check(ufbxi_fetch_blend_keyframes(uc, &channel->keyframes, &channel->element));

		for (size_t i = 0; i < channel->keyframes.count; i++) {
			ufbx_blend_keyframe *key = &channel->keyframes.data[i];
			if (i < full_weights->count) {
				key->target_weight = full_weights->data[i] / (ufbx_real)100.0;
			} else {
				key->target_weight = 1.0f;
			}
		}

		ufbxi_check(ufbxi_sort_blend_keyframes(uc, channel->keyframes.data, channel->keyframes.count));
		full_weights++;
	}
	ufbxi_buf_free(&uc->tmp_full_weights);

	{
		// Generate and patch procedural index buffers
		int32_t *zero_indices = ufbxi_push(&uc->result, int32_t, uc->max_zero_indices);
		int32_t *consecutive_indices = ufbxi_push(&uc->result, int32_t, uc->max_consecutive_indices);
		ufbxi_check(zero_indices && consecutive_indices);

		memset(zero_indices, 0, sizeof(int32_t) * uc->max_zero_indices);
		for (size_t i = 0; i < uc->max_consecutive_indices; i++) {
			consecutive_indices[i] = (int32_t)i;
		}

		uc->zero_indices = zero_indices;
		uc->consecutive_indices = consecutive_indices;

		ufbxi_for_ptr_list(ufbx_mesh, p_mesh, uc->scene.meshes) {
			ufbx_mesh *mesh = *p_mesh;

			ufbxi_patch_index_pointer(uc, &mesh->vertex_position.indices);
			ufbxi_patch_index_pointer(uc, &mesh->vertex_normal.indices);
			ufbxi_patch_index_pointer(uc, &mesh->vertex_bitangent.indices);
			ufbxi_patch_index_pointer(uc, &mesh->vertex_tangent.indices);
			ufbxi_patch_index_pointer(uc, &mesh->face_material);

			ufbxi_patch_index_pointer(uc, &mesh->skinned_position.indices);
			ufbxi_patch_index_pointer(uc, &mesh->skinned_normal.indices);

			ufbxi_for_list(ufbx_uv_set, set, mesh->uv_sets) {
				ufbxi_patch_index_pointer(uc, &set->vertex_uv.indices);
				ufbxi_patch_index_pointer(uc, &set->vertex_bitangent.indices);
				ufbxi_patch_index_pointer(uc, &set->vertex_tangent.indices);
			}

			ufbxi_for_list(ufbx_color_set, set, mesh->color_sets) {
				ufbxi_patch_index_pointer(uc, &set->vertex_color.indices);
			}

			// Assign first UV and color sets as the "canonical" ones
			if (mesh->uv_sets.count > 0) {
				mesh->vertex_uv = mesh->uv_sets.data[0].vertex_uv;
				mesh->vertex_bitangent = mesh->uv_sets.data[0].vertex_bitangent;
				mesh->vertex_tangent = mesh->uv_sets.data[0].vertex_tangent;
			}
			if (mesh->color_sets.count > 0) {
				mesh->vertex_color = mesh->color_sets.data[0].vertex_color;
			}

			ufbxi_check(ufbxi_fetch_mesh_materials(uc, &mesh->materials, &mesh->element, true));

			// Search for a non-standard `ufbx:UVBoundary` property in both the mesh and node
			ufbx_prop *uv_prop = ufbx_find_prop(&mesh->props, "ufbx:UVBoundary");
			if (!uv_prop) {
				ufbxi_for_ptr_list(ufbx_node, p_node, mesh->instances) {
					uv_prop = ufbx_find_prop(&(*p_node)->props, "ufbx:UVBoundary");
					if (uv_prop) break;
				}
			}
			if (uv_prop && uv_prop->value_int >= 0 && uv_prop->value_int <= UFBX_SUBDIVISION_BOUNDARY_SHARP_INTERIOR) {
				mesh->subdivision_uv_boundary = (ufbx_subdivision_boundary)uv_prop->value_int;
			} else {
				mesh->subdivision_uv_boundary = UFBX_SUBDIVISION_BOUNDARY_SHARP_BOUNDARY;
			}

			// Push a NULL material if necessary if requested
			if (mesh->materials.count == 0 && uc->opts.allow_null_material) {
				mesh->materials.data = ufbxi_push_zero(&uc->result, ufbx_mesh_material, 1);
				ufbxi_check(mesh->materials.data);
				mesh->materials.count = 1;
			}

			if (mesh->materials.count == 1) {
				// Use the shared consecutive index buffer for mesh faces if there's only one material
				// See HACK(consecutive-faces) in `ufbxi_read_mesh()`.
				ufbx_mesh_material *mat = &mesh->materials.data[0];
				mat->num_faces = mesh->num_faces;
				mat->num_triangles = mesh->num_triangles;
				mat->face_indices = uc->consecutive_indices;
				mesh->face_material = uc->zero_indices;
			} else if (mesh->materials.count > 0 && mesh->face_material) {
				size_t num_materials = mesh->materials.count;

				// Count the number of faces and triangles per material
				for (size_t i = 0; i < mesh->num_faces; i++) {
					ufbx_face face = mesh->faces[i];
					int32_t mat_ix = mesh->face_material[i];
					if (!(mat_ix >= 0 && (size_t)mat_ix < num_materials)) {
						mesh->face_material[i] = 0;
						mat_ix = 0;
					}
					mesh->materials.data[mat_ix].num_faces++;
					if (face.num_indices >= 3) {
						mesh->materials.data[mat_ix].num_triangles += face.num_indices - 2;
					}
				}

				// Allocate per-material buffers (clear `num_faces` to 0 to re-use it as
				// an index when fetching the face indices).
				ufbxi_for_list(ufbx_mesh_material, mat, mesh->materials) {
					mat->face_indices = ufbxi_push(&uc->result, int32_t, mat->num_faces);
					ufbxi_check(mat->face_indices);
					mat->num_faces = 0;
				}

				// Fetch the per-material face indices
				for (size_t i = 0; i < mesh->num_faces; i++) {
					int32_t mat_ix = mesh->face_material[i];
					if (mat_ix >= 0 && (size_t)mat_ix < num_materials) {
						ufbx_mesh_material *mat = &mesh->materials.data[mat_ix];
						mat->face_indices[mat->num_faces++] = (int32_t)i;
					}
				}
			} else {
				mesh->face_material = NULL;
			}

			// Blender writes materials attached to models even in 7x00
			// TODO: Should per-instance materials be supported?

			// Fetch deformers
			ufbxi_check(ufbxi_fetch_dst_elements(uc, &mesh->skin_deformers, &mesh->element, search_node, NULL, UFBX_ELEMENT_SKIN_DEFORMER));
			ufbxi_check(ufbxi_fetch_dst_elements(uc, &mesh->blend_deformers, &mesh->element, search_node, NULL, UFBX_ELEMENT_BLEND_DEFORMER));
			ufbxi_check(ufbxi_fetch_dst_elements(uc, &mesh->cache_deformers, &mesh->element, search_node, NULL, UFBX_ELEMENT_CACHE_DEFORMER));
			ufbxi_check(ufbxi_fetch_deformers(uc, &mesh->all_deformers, &mesh->element, search_node));

			// Update metadata
			if (mesh->max_face_triangles > uc->scene.metadata.max_face_triangles) {
				uc->scene.metadata.max_face_triangles = mesh->max_face_triangles;
			}
		}
	}

	ufbxi_for_ptr_list(ufbx_stereo_camera, p_stereo, uc->scene.stereo_cameras) {
		ufbx_stereo_camera *stereo = *p_stereo;
		stereo->left = (ufbx_camera*)ufbxi_fetch_dst_element(&stereo->element, search_node, ufbxi_LeftCamera, UFBX_ELEMENT_CAMERA);
		stereo->right = (ufbx_camera*)ufbxi_fetch_dst_element(&stereo->element, search_node, ufbxi_RightCamera, UFBX_ELEMENT_CAMERA);
	}

	ufbxi_for_ptr_list(ufbx_nurbs_curve, p_curve, uc->scene.nurbs_curves) {
		ufbx_nurbs_curve *curve = *p_curve;
		ufbxi_check(ufbxi_finalize_nurbs_basis(uc, &curve->basis));
	}

	ufbxi_for_ptr_list(ufbx_nurbs_surface, p_surface, uc->scene.nurbs_surfaces) {
		ufbx_nurbs_surface *surface = *p_surface;
		ufbxi_check(ufbxi_finalize_nurbs_basis(uc, &surface->basis_u));
		ufbxi_check(ufbxi_finalize_nurbs_basis(uc, &surface->basis_v));

		surface->material = (ufbx_material*)ufbxi_fetch_dst_element(&surface->element, true, NULL, UFBX_ELEMENT_MATERIAL);
	}

	ufbxi_for_ptr_list(ufbx_anim_stack, p_stack, uc->scene.anim_stacks) {
		ufbx_anim_stack *stack = *p_stack;
		ufbxi_check(ufbxi_fetch_dst_elements(uc, &stack->layers, &stack->element, false, NULL, UFBX_ELEMENT_ANIM_LAYER));

		stack->anim.layers.count = stack->layers.count;
		stack->anim.layers.data = ufbxi_push_zero(&uc->result, ufbx_anim_layer_desc, stack->layers.count);
		ufbxi_check(stack->anim.layers.data);

		for (size_t i = 0; i < stack->layers.count; i++) {
			ufbx_anim_layer_desc *desc = (ufbx_anim_layer_desc*)&stack->anim.layers.data[i];
			desc->layer = stack->layers.data[i];
			desc->weight = 1.0f;
		}
	}

	ufbxi_for_ptr_list(ufbx_anim_layer, p_layer, uc->scene.anim_layers) {
		ufbx_anim_layer *layer = *p_layer;
		ufbxi_check(ufbxi_fetch_dst_elements(uc, &layer->anim_values, &layer->element, false, NULL, UFBX_ELEMENT_ANIM_VALUE));

		ufbx_anim_layer_desc *layer_desc = ufbxi_push_zero(&uc->result, ufbx_anim_layer_desc, 1);;
		ufbxi_check(layer_desc);
		layer_desc->layer = layer;
		layer_desc->weight = 1.0f;

		layer->anim.layers.data = layer_desc;
		layer->anim.layers.count = 1;

		uint32_t min_id = UINT32_MAX, max_id = 0;

		// Combine the animated properties with elements (potentially duplicates!)
		size_t num_anim_props = 0;
		ufbxi_for_ptr_list(ufbx_anim_value, p_value, layer->anim_values) {
			ufbx_anim_value *value = *p_value;
			ufbxi_for_list(ufbx_connection, ac, value->element.connections_src) {
				if (ac->src_prop.length == 0 && ac->dst_prop.length > 0) {
					ufbx_anim_prop *aprop = ufbxi_push(&uc->tmp_stack, ufbx_anim_prop, 1);
					uint32_t id = ac->dst->element_id;
					min_id = ufbxi_min32(min_id, id);
					max_id = ufbxi_max32(max_id, id);
					uint32_t id_mask = ufbxi_arraycount(layer->element_id_bitmask) - 1;
					layer->element_id_bitmask[(id >> 5) & id_mask] |= 1u << (id & 31);
					ufbxi_check(aprop);
					aprop->anim_value = value;
					aprop->element = ac->dst;
					aprop->internal_key = ufbxi_get_name_key(ac->dst_prop.data, ac->dst_prop.length);
					aprop->prop_name = ac->dst_prop;
					num_anim_props++;
				}
			}
		}

		if (min_id != UINT32_MAX) {
			layer->min_element_id = min_id;
			layer->max_element_id = max_id;
		}

		switch (ufbxi_find_int(&layer->props, ufbxi_BlendMode, 0)) {
		case 0: // Additive
			layer->blended = true;
			layer->additive = true;
			break;
		case 1: // Override
			layer->blended = false;
			layer->additive = false;
			break;
		case 2: // Override Passthrough
			layer->blended = true;
			layer->additive = false;
			break;
		default: // Unknown
			layer->blended = false;
			layer->additive = false;
			break;
		}

		ufbx_prop *weight_prop = ufbxi_find_prop(&layer->props, ufbxi_Weight);
		if (weight_prop) {
			layer->weight = weight_prop->value_real / (ufbx_real)100.0;
			if (layer->weight < 0.0f) layer->weight = 0.0f;
			if (layer->weight > 0.99999f) layer->weight = 1.0f;
			layer->weight_is_animated = (weight_prop->flags & UFBX_PROP_FLAG_ANIMATED) != 0;
		} else {
			layer->weight = 1.0f;
			layer->weight_is_animated = false;
		}
		layer->compose_rotation = ufbxi_find_int(&layer->props, ufbxi_RotationAccumulationMode, 0) == 0;
		layer->compose_scale = ufbxi_find_int(&layer->props, ufbxi_ScaleAccumulationMode, 0) == 0;

		// Add a dummy NULL element animated prop at the end so we can iterate
		// animated props without worrying about boundary conditions..
		{
			ufbx_anim_prop *aprop = ufbxi_push_zero(&uc->tmp_stack, ufbx_anim_prop, 1);
			ufbxi_check(aprop);
		}

		layer->anim_props.data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_anim_prop, num_anim_props + 1);
		ufbxi_check(layer->anim_props.data);
		layer->anim_props.count = num_anim_props;
		ufbxi_check(ufbxi_sort_anim_props(uc, layer->anim_props.data, layer->anim_props.count));
	}

	ufbxi_for_ptr_list(ufbx_anim_value, p_value, uc->scene.anim_values) {
		ufbx_anim_value *value = *p_value;

		// TODO: Search for things like d|Visibility with a constructed name
		value->default_value.x = ufbxi_find_real(&value->props, ufbxi_X, value->default_value.x);
		value->default_value.x = ufbxi_find_real(&value->props, ufbxi_d_X, value->default_value.x);
		value->default_value.y = ufbxi_find_real(&value->props, ufbxi_Y, value->default_value.y);
		value->default_value.y = ufbxi_find_real(&value->props, ufbxi_d_Y, value->default_value.y);
		value->default_value.z = ufbxi_find_real(&value->props, ufbxi_Z, value->default_value.z);
		value->default_value.z = ufbxi_find_real(&value->props, ufbxi_d_Z, value->default_value.z);

		ufbxi_for_list(ufbx_connection, conn, value->element.connections_dst) {
			if (conn->src->type == UFBX_ELEMENT_ANIM_CURVE && conn->src_prop.length == 0) {
				ufbx_anim_curve *curve = (ufbx_anim_curve*)conn->src;

				uint32_t index = 0;
				const char *name = conn->dst_prop.data;
				if (name == ufbxi_Y || name == ufbxi_d_Y) index = 1;
				if (name == ufbxi_Z || name == ufbxi_d_Z) index = 2;

				ufbx_prop *prop = ufbx_find_prop_len(&value->props, conn->dst_prop.data, conn->dst_prop.length);
				if (prop) {
					value->default_value.v[index] = prop->value_real;
				}
				value->curves[index] = curve;
			}
		}
	}

	ufbxi_for_ptr_list(ufbx_shader, p_shader, uc->scene.shaders) {
		ufbx_shader *shader = *p_shader;
		ufbxi_check(ufbxi_fetch_dst_elements(uc, &shader->bindings, &shader->element, false, NULL, UFBX_ELEMENT_SHADER_BINDING));

		ufbx_prop *api = ufbx_find_prop(&shader->props, "RenderAPI");
		if (api) {
			if (!strcmp(api->value_str.data, "ARNOLD_SHADER_ID")) {
				shader->type = UFBX_SHADER_ARNOLD;
			} else if (!strcmp(api->value_str.data, "OSL")) {
				shader->type = UFBX_SHADER_OSL_STANDARD;
			}
		}
	}

	ufbxi_for_ptr_list(ufbx_material, p_material, uc->scene.materials) {
		ufbx_material *material = *p_material;
		material->shader = (ufbx_shader*)ufbxi_fetch_src_element(&material->element, false, NULL, UFBX_ELEMENT_SHADER);

		if (!strcmp(material->shading_model_name.data, "lambert") || !strcmp(material->shading_model_name.data, "Lambert")) {
			material->shader_type = UFBX_SHADER_FBX_LAMBERT;
		} else if (!strcmp(material->shading_model_name.data, "phong") || !strcmp(material->shading_model_name.data, "Phong")) {
			material->shader_type = UFBX_SHADER_FBX_PHONG;
		}

		if (material->shader) {
			material->shader_type = material->shader->type;
		} else {
			if (uc->exporter == UFBX_EXPORTER_BLENDER_BINARY && uc->exporter_version >= ufbx_pack_version(4,12,0)) {
				material->shader_type = UFBX_SHADER_BLENDER_PHONG;
			}
		}

		ufbxi_check(ufbxi_fetch_textures(uc, &material->textures, &material->element, false));
	}

	// Ugh.. Patch the textures from meshes for legacy LayerElement-style textures
	{
		ufbxi_for_ptr_list(ufbx_mesh, p_mesh, uc->scene.meshes) {
			ufbx_mesh *mesh = *p_mesh;
			size_t num_materials = mesh->materials.count;

			ufbxi_mesh_extra *extra = (ufbxi_mesh_extra*)ufbxi_get_element_extra(uc, mesh->element.element_id);
			if (!extra) continue;
			if (num_materials == 0) continue;

			// TODO: This leaks currently to result, probably doesn't matter..
			ufbx_texture_list textures;
			ufbxi_check(ufbxi_fetch_dst_elements(uc, &textures, &mesh->element, true, NULL, UFBX_ELEMENT_TEXTURE));

			size_t num_material_textures = 0;
			ufbxi_for(ufbxi_tmp_mesh_texture, tex, extra->texture_arr, extra->texture_count) {
				if (tex->all_same) {
					int32_t texture_id = tex->num_faces > 0 ? tex->face_texture[0] : 0;
					if (texture_id >= 0 && (size_t)texture_id < textures.count) {
						ufbxi_tmp_material_texture *mat_texs = ufbxi_push(&uc->tmp_stack, ufbxi_tmp_material_texture, num_materials);
						ufbxi_check(mat_texs);
						num_material_textures += num_materials;
						for (size_t i = 0; i < num_materials; i++) {
							mat_texs[i].material_id = (int32_t)i;
							mat_texs[i].texture_id = texture_id;
							mat_texs[i].prop_name = tex->prop_name;
						}
					}
				} else if (mesh->face_material) {
					size_t num_faces = ufbxi_min_sz(tex->num_faces, mesh->num_faces);
					int32_t prev_material = -1;
					int32_t prev_texture = -1;
					for (size_t i = 0; i < num_faces; i++) {
						int32_t texture_id = tex->face_texture[i];
						int32_t material_id = mesh->face_material[i];
						if (texture_id < 0 || (size_t)texture_id >= textures.count) continue;
						if (material_id < 0 || (size_t)material_id >= num_materials) continue;
						if (material_id == prev_material && texture_id == prev_texture) continue;
						prev_material = material_id;
						prev_texture = texture_id;

						ufbxi_tmp_material_texture *mat_tex = ufbxi_push(&uc->tmp_stack, ufbxi_tmp_material_texture, 1);
						ufbxi_check(mat_tex);
						mat_tex->material_id = material_id;
						mat_tex->texture_id = texture_id;
						mat_tex->prop_name = tex->prop_name;
						num_material_textures++;
					}
				}
			}

			// Push a sentinel material texture to the end so we don't need to
			// duplicate the material texture flushing code twice.
			{
				ufbxi_tmp_material_texture *mat_tex = ufbxi_push(&uc->tmp_stack, ufbxi_tmp_material_texture, 1);
				ufbxi_check(mat_tex);
				mat_tex->material_id = -1;
				mat_tex->texture_id = -1;
				mat_tex->prop_name = ufbx_empty_string;
			}

			ufbxi_tmp_material_texture *mat_texs = ufbxi_push_pop(&uc->tmp, &uc->tmp_stack, ufbxi_tmp_material_texture, num_material_textures + 1);
			ufbxi_check(mat_texs);
			ufbxi_check(ufbxi_sort_tmp_material_textures(uc, mat_texs, num_material_textures));

			int32_t prev_material = -2;
			int32_t prev_texture = -2;
			const char *prev_prop = NULL;
			size_t num_textures_in_material = 0;
			for (size_t i = 0; i < num_material_textures + 1; i++) {
				ufbxi_tmp_material_texture mat_tex = mat_texs[i];
				if (mat_tex.material_id != prev_material) {
					if (prev_material >= 0 && num_textures_in_material > 0) {
						ufbx_material *mat = mesh->materials.data[prev_material].material;
						if (mat->textures.count == 0) {
							ufbx_material_texture *texs = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_material_texture, num_textures_in_material);
							ufbxi_check(texs);
							mat->textures.data = texs;
							mat->textures.count = num_textures_in_material;
						} else {
							ufbxi_pop(&uc->tmp_stack, ufbx_material_texture, num_textures_in_material, NULL);
						}
					}

					if (mat_tex.material_id < 0) break;
					prev_material = mat_tex.material_id;
					prev_texture = -1;
					prev_prop = NULL;
					num_textures_in_material = 0;
				}
				if (mat_tex.texture_id == prev_texture && mat_tex.prop_name.data == prev_prop) continue;
				prev_texture = mat_tex.texture_id;
				prev_prop = mat_tex.prop_name.data;

				ufbx_material_texture *tex = ufbxi_push(&uc->tmp_stack, ufbx_material_texture, 1);
				ufbxi_check(tex);
				ufbx_assert(prev_texture >= 0 && (size_t)prev_texture < textures.count);
				tex->texture = textures.data[prev_texture];
				tex->shader_prop = tex->material_prop = mat_tex.prop_name;
				num_textures_in_material++;
			}
		}
	}

	// Second pass to fetch material maps
	ufbxi_for_ptr_list(ufbx_material, p_material, uc->scene.materials) {
		ufbx_material *material = *p_material;

		ufbxi_check(ufbxi_sort_material_textures(uc, material->textures.data, material->textures.count));
		ufbxi_fetch_maps(&uc->scene, material);

		// Fetch `ufbx_material_texture.shader_prop` names
		if (material->shader) {
			ufbxi_for_ptr_list(ufbx_shader_binding, p_binding, material->shader->bindings) {
				ufbx_shader_binding *binding = *p_binding;

				ufbxi_for_list(ufbx_shader_prop_binding, prop, binding->prop_bindings) {
					ufbx_string name = prop->material_prop;

					size_t index = SIZE_MAX;
					ufbxi_macro_lower_bound_eq(ufbx_material_texture, 4, &index, material->textures.data, 0, material->textures.count, 
						( ufbxi_str_less(a->material_prop, name) ), ( a->material_prop.data == name.data ));
					for (; index < material->textures.count && material->textures.data[index].shader_prop.data == name.data; index++) {
						material->textures.data[index].shader_prop = prop->shader_prop;
					}
				}
			}
		}
	}

	// HACK: If there are multiple textures in an FBX file that use the same embedded
	// texture they get duplicated Video elements instead of a shared one _and only one
	// of them has the content?!_ So let's gather all Video instances with content and
	// sort them by filename so we can patch the other ones..
	ufbx_video **content_videos = ufbxi_push(&uc->tmp, ufbx_video*, uc->scene.videos.count);
	ufbxi_check(content_videos);

	size_t num_content_videos = 0;
	ufbxi_for_ptr_list(ufbx_video, p_video, uc->scene.videos) {
		ufbx_video *video = *p_video;
		ufbxi_check(ufbxi_resolve_relative_filename(uc, &video->filename, video->relative_filename));
		if (video->content_size > 0) {
			content_videos[num_content_videos++] = video;
		}
	}

	if (num_content_videos > 0) {
		ufbxi_check(ufbxi_sort_videos_by_filename(uc, content_videos, num_content_videos));

		ufbxi_for_ptr_list(ufbx_video, p_video, uc->scene.videos) {
			ufbx_video *video = *p_video;
			if (video->content_size > 0) continue;

			size_t index = SIZE_MAX;
			ufbxi_macro_lower_bound_eq(ufbx_video*, 16, &index, content_videos, 0, num_content_videos,
				( ufbxi_str_less((*a)->absolute_filename, video->absolute_filename) ),
				( (*a)->absolute_filename.data == video->absolute_filename.data ));
			if (index != SIZE_MAX) {
				video->content = content_videos[index]->content;
				video->content_size = content_videos[index]->content_size;
			}
		}
	}

	ufbxi_for_ptr_list(ufbx_texture, p_texture, uc->scene.textures) {
		ufbx_texture *texture = *p_texture;
		ufbxi_texture_extra *extra = (ufbxi_texture_extra*)ufbxi_get_element_extra(uc, texture->element.element_id);

		ufbxi_check(ufbxi_resolve_relative_filename(uc, &texture->filename, texture->relative_filename));

		ufbx_prop *uv_set = ufbxi_find_prop(&texture->props, ufbxi_UVSet);
		if (uv_set) {
			texture->uv_set = uv_set->value_str;
		} else {
			texture->uv_set = ufbx_empty_string;
		}

		texture->video = (ufbx_video*)ufbxi_fetch_dst_element(&texture->element, false, NULL, UFBX_ELEMENT_VIDEO);
		if (texture->video) {
			texture->content = texture->video->content;
			texture->content_size = texture->video->content_size;
		}

		// Fetch layered texture layers and patch alphas/blend modes
		if (texture->type == UFBX_TEXTURE_LAYERED) {
			ufbxi_check(ufbxi_fetch_texture_layers(uc, &texture->layers, &texture->element));
			if (extra) {
				for (size_t i = 0, num = ufbxi_min_sz(extra->num_alphas, texture->layers.count); i < num; i++) {
					texture->layers.data[i].alpha = extra->alphas[i];
				}
				for (size_t i = 0, num = ufbxi_min_sz(extra->num_blend_modes, texture->layers.count); i < num; i++) {
					int32_t mode = extra->blend_modes[i];
					if (mode >= 0 && mode < UFBX_BLEND_OVERLAY) {
						texture->layers.data[i].blend_mode = (ufbx_blend_mode)mode;
					}
				}
			}
		}
	}

	ufbxi_for_ptr_list(ufbx_display_layer, p_layer, uc->scene.display_layers) {
		ufbx_display_layer *layer = *p_layer;
		ufbxi_check(ufbxi_fetch_dst_elements(uc, &layer->nodes, &layer->element, false, NULL, UFBX_ELEMENT_NODE));
	}

	ufbxi_for_ptr_list(ufbx_selection_set, p_set, uc->scene.selection_sets) {
		ufbx_selection_set *set = *p_set;
		ufbxi_check(ufbxi_fetch_dst_elements(uc, &set->nodes, &set->element, false, NULL, UFBX_ELEMENT_SELECTION_NODE));
	}

	ufbxi_for_ptr_list(ufbx_selection_node, p_node, uc->scene.selection_nodes) {
		ufbx_selection_node *node = *p_node;
		node->target_node = (ufbx_node*)ufbxi_fetch_dst_element(&node->element, false, NULL, UFBX_ELEMENT_NODE);
		node->target_mesh = (ufbx_mesh*)ufbxi_fetch_dst_element(&node->element, false, NULL, UFBX_ELEMENT_MESH);
		if (!node->target_mesh && node->target_node) {
			node->target_mesh = node->target_node->mesh;
		} else if (!node->target_node && node->target_mesh && node->target_mesh->instances.count > 0) {
			node->target_node = node->target_mesh->instances.data[0];
		}
	}

	ufbxi_for_ptr_list(ufbx_constraint, p_constraint, uc->scene.constraints) {
		ufbx_constraint *constraint = *p_constraint;

		size_t tmp_base = uc->tmp_stack.num_items;

		// Find property connections in _both_ src and dst connections as they are inconsistent
		// in pre-7000 files. For example "Constrained Object" is a "PO" connection in 6100.
		ufbxi_for_list(ufbx_connection, conn, constraint->element.connections_src) {
			if (conn->src_prop.length == 0 || conn->dst->type != UFBX_ELEMENT_NODE) continue;
			ufbxi_check(ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)conn->dst, conn->src_prop.data));
		}
		ufbxi_for_list(ufbx_connection, conn, constraint->element.connections_dst) {
			if (conn->dst_prop.length == 0 || conn->src->type != UFBX_ELEMENT_NODE) continue;
			ufbxi_check(ufbxi_add_constraint_prop(uc, constraint, (ufbx_node*)conn->src, conn->dst_prop.data));
		}

		size_t num_targets = uc->tmp_stack.num_items - tmp_base;
		constraint->targets.count = num_targets;
		constraint->targets.data = ufbxi_push_pop(&uc->result, &uc->tmp_stack, ufbx_constraint_target, num_targets);
		ufbxi_check(constraint->targets.data);
	}

	if (uc->scene.anim_stacks.count > 0) {
		uc->scene.anim = uc->scene.anim_stacks.data[0]->anim;

		// Combine all animation stacks into one
		size_t num_layers = 0;
		ufbxi_for_ptr_list(ufbx_anim_stack, p_stack, uc->scene.anim_stacks) {
			num_layers += (*p_stack)->layers.count;
		}

		ufbx_anim_layer_desc *descs = ufbxi_push_zero(&uc->result, ufbx_anim_layer_desc, num_layers);
		ufbxi_check(descs);
		uc->scene.combined_anim.layers.data = descs;
		uc->scene.combined_anim.layers.count = num_layers;

		ufbx_anim_layer_desc *desc = descs;
		ufbxi_for_ptr_list(ufbx_anim_stack, p_stack, uc->scene.anim_stacks) {
			ufbxi_for_ptr_list(ufbx_anim_layer, p_layer, (*p_stack)->layers) {
				desc->layer = *p_layer;
				desc->weight = 1.0f;
				desc++;
			}
		}
	}

	ufbxi_for_ptr_list(ufbx_lod_group, p_lod, uc->scene.lod_groups) {
		ufbxi_check(ufbxi_finalize_lod_group(uc, *p_lod));
	}

	uc->scene.metadata.ktime_to_sec = uc->ktime_to_sec;

	// Maya seems to use scale of 100/3, Blender binary uses exactly 33, ASCII has always value of 1.0
	if (uc->version < 6000) {
		uc->scene.metadata.bone_prop_size_unit = 1.0f;
	} else if (uc->exporter == UFBX_EXPORTER_BLENDER_BINARY) {
		uc->scene.metadata.bone_prop_size_unit = 33.0f;
	} else if (uc->exporter == UFBX_EXPORTER_BLENDER_ASCII) {
		uc->scene.metadata.bone_prop_size_unit = 1.0f;
	} else {
		uc->scene.metadata.bone_prop_size_unit = (ufbx_real)(100.0/3.0);
	}
	if (uc->exporter == UFBX_EXPORTER_BLENDER_ASCII) {
		uc->scene.metadata.bone_prop_limb_length_relative = false;
	} else {
		uc->scene.metadata.bone_prop_limb_length_relative = true;
	}

	return 1;
}

// -- Interpret the read scene

static ufbxi_forceinline void ufbxi_add_translate(ufbx_transform *t, ufbx_vec3 v)
{
	t->translation.x += v.x;
	t->translation.y += v.y;
	t->translation.z += v.z;
}

static ufbxi_forceinline void ufbxi_sub_translate(ufbx_transform *t, ufbx_vec3 v)
{
	t->translation.x -= v.x;
	t->translation.y -= v.y;
	t->translation.z -= v.z;
}

static ufbxi_forceinline void ufbxi_mul_scale(ufbx_transform *t, ufbx_vec3 v)
{
	t->translation.x *= v.x;
	t->translation.y *= v.y;
	t->translation.z *= v.z;
	t->scale.x *= v.x;
	t->scale.y *= v.y;
	t->scale.z *= v.z;
}

static const ufbx_vec3 ufbxi_one_vec3 = { 1.0f, 1.0f, 1.0f };

#define UFBXI_PI ((ufbx_real)3.14159265358979323846)
#define UFBXI_DEG_TO_RAD ((ufbx_real)(UFBXI_PI / 180.0))
#define UFBXI_RAD_TO_DEG ((ufbx_real)(180.0 / UFBXI_PI))
#define UFBXI_MM_TO_INCH ((ufbx_real)0.0393700787)

static ufbxi_forceinline ufbx_quat ufbxi_mul_quat(ufbx_quat a, ufbx_quat b)
{
	ufbx_quat r;
	r.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
	r.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
	r.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;
	r.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
	return r;
}

static ufbxi_forceinline void ufbxi_add_weighted_vec3(ufbx_vec3 *r, ufbx_vec3 b, ufbx_real w)
{
	r->x += b.x * w;
	r->y += b.y * w;
	r->z += b.z * w;
}

static ufbxi_forceinline void ufbxi_add_weighted_quat(ufbx_quat *r, ufbx_quat b, ufbx_real w)
{
	r->x += b.x * w;
	r->y += b.y * w;
	r->z += b.z * w;
	r->w += b.w * w;
}

static ufbxi_forceinline void ufbxi_add_weighted_mat(ufbx_matrix *r, const ufbx_matrix *b, ufbx_real w)
{
	ufbxi_add_weighted_vec3(&r->cols[0], b->cols[0], w);
	ufbxi_add_weighted_vec3(&r->cols[1], b->cols[1], w);
	ufbxi_add_weighted_vec3(&r->cols[2], b->cols[2], w);
	ufbxi_add_weighted_vec3(&r->cols[3], b->cols[3], w);
}

static void ufbxi_mul_rotate(ufbx_transform *t, ufbx_vec3 v, ufbx_rotation_order order)
{
	if (ufbxi_is_vec3_zero(v)) return;

	ufbx_quat q = ufbx_euler_to_quat(v, order);
	if (t->rotation.w != 1.0) {
		t->rotation = ufbxi_mul_quat(q, t->rotation);
	} else {
		t->rotation = q;
	}

	if (!ufbxi_is_vec3_zero(t->translation)) {
		t->translation = ufbx_quat_rotate_vec3(q, t->translation);
	}
}

static void ufbxi_mul_inv_rotate(ufbx_transform *t, ufbx_vec3 v, ufbx_rotation_order order)
{
	if (ufbxi_is_vec3_zero(v)) return;

	ufbx_quat q = ufbx_euler_to_quat(v, order);
	q.x = -q.x; q.y = -q.y; q.z = -q.z;
	if (t->rotation.w != 1.0) {
		t->rotation = ufbxi_mul_quat(q, t->rotation);
	} else {
		t->rotation = q;
	}

	if (!ufbxi_is_vec3_zero(t->translation)) {
		t->translation = ufbx_quat_rotate_vec3(q, t->translation);
	}
}

// -- Updating state from properties

ufbxi_noinline static ufbx_transform ufbxi_get_transform(const ufbx_props *props, ufbx_rotation_order order)
{
	ufbx_vec3 scale_pivot = ufbxi_find_vec3(props, ufbxi_ScalingPivot, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 rot_pivot = ufbxi_find_vec3(props, ufbxi_RotationPivot, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 scale_offset = ufbxi_find_vec3(props, ufbxi_ScalingOffset, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 rot_offset = ufbxi_find_vec3(props, ufbxi_RotationOffset, 0.0f, 0.0f, 0.0f);

	ufbx_vec3 translation = ufbxi_find_vec3(props, ufbxi_Lcl_Translation, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 rotation = ufbxi_find_vec3(props, ufbxi_Lcl_Rotation, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 scaling = ufbxi_find_vec3(props, ufbxi_Lcl_Scaling, 1.0f, 1.0f, 1.0f);

	ufbx_vec3 pre_rotation = ufbxi_find_vec3(props, ufbxi_PreRotation, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 post_rotation = ufbxi_find_vec3(props, ufbxi_PostRotation, 0.0f, 0.0f, 0.0f);

	ufbx_transform t = { { 0,0,0 }, { 0,0,0,1 }, { 1,1,1 }};

	// WorldTransform = ParentWorldTransform * T * Roff * Rp * Rpre * R * Rpost * Rp-1 * Soff * Sp * S * Sp-1
	// NOTE: Rpost is inverted (!) after converting from PostRotation Euler angles

	ufbxi_sub_translate(&t, scale_pivot);
	ufbxi_mul_scale(&t, scaling);
	ufbxi_add_translate(&t, scale_pivot);

	ufbxi_add_translate(&t, scale_offset);

	ufbxi_sub_translate(&t, rot_pivot);
	ufbxi_mul_inv_rotate(&t, post_rotation, UFBX_ROTATION_XYZ);
	ufbxi_mul_rotate(&t, rotation, order);
	ufbxi_mul_rotate(&t, pre_rotation, UFBX_ROTATION_XYZ);
	ufbxi_add_translate(&t, rot_pivot);

	ufbxi_add_translate(&t, rot_offset);

	ufbxi_add_translate(&t, translation);

	return t;
}

static ufbx_transform ufbxi_get_geometry_transform(const ufbx_props *props)
{
	ufbx_vec3 translation = ufbxi_find_vec3(props, ufbxi_GeometricTranslation, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 rotation = ufbxi_find_vec3(props, ufbxi_GeometricRotation, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 scaling = ufbxi_find_vec3(props, ufbxi_GeometricScaling, 1.0f, 1.0f, 1.0f);

	ufbx_transform t = { { 0,0,0 }, { 0,0,0,1 }, { 1,1,1 }};

	// WorldTransform = ParentWorldTransform * T * R * S * (OT * OR * OS)

	ufbxi_mul_scale(&t, scaling);
	ufbxi_mul_rotate(&t, rotation, UFBX_ROTATION_XYZ);
	ufbxi_add_translate(&t, translation);

	return t;
}

ufbxi_noinline static ufbx_transform ufbxi_get_texture_transform(const ufbx_props *props)
{
	ufbx_vec3 scale_pivot = ufbxi_find_vec3(props, ufbxi_TextureScalingPivot, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 rot_pivot = ufbxi_find_vec3(props, ufbxi_TextureRotationPivot, 0.0f, 0.0f, 0.0f);

	ufbx_vec3 translation = ufbxi_find_vec3(props, ufbxi_Translation, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 rotation = ufbxi_find_vec3(props, ufbxi_Rotation, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 scaling = ufbxi_find_vec3(props, ufbxi_Scaling, 1.0f, 1.0f, 1.0f);

	ufbx_transform t = { { 0,0,0 }, { 0,0,0,1 }, { 1,1,1 }};

	ufbxi_sub_translate(&t, scale_pivot);
	ufbxi_mul_scale(&t, scaling);
	ufbxi_add_translate(&t, scale_pivot);

	ufbxi_sub_translate(&t, rot_pivot);
	ufbxi_mul_rotate(&t, rotation, UFBX_ROTATION_XYZ);
	ufbxi_add_translate(&t, rot_pivot);

	ufbxi_add_translate(&t, translation);

	if (ufbxi_find_int(props, ufbxi_UVSwap, 0) != 0) {
		const ufbx_vec3 swap_scale = { -1.0f, 0.0f, 0.0f };
		const ufbx_vec3 swap_rotate = { 0.0f, 0.0f, -90.0f };
		ufbxi_mul_scale(&t, swap_scale);
		ufbxi_mul_rotate(&t, swap_rotate, UFBX_ROTATION_XYZ);
	}

	return t;
}

ufbxi_noinline static ufbx_transform ufbxi_get_constraint_transform(const ufbx_props *props)
{
	ufbx_vec3 translation = ufbxi_find_vec3(props, ufbxi_Translation, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 rotation = ufbxi_find_vec3(props, ufbxi_Rotation, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 rotation_offset = ufbxi_find_vec3(props, ufbxi_RotationOffset, 0.0f, 0.0f, 0.0f);
	ufbx_vec3 scaling = ufbxi_find_vec3(props, ufbxi_Scaling, 1.0f, 1.0f, 1.0f);

	ufbx_transform t = { { 0,0,0 }, { 0,0,0,1 }, { 1,1,1 }};

	ufbxi_mul_scale(&t, scaling);
	ufbxi_mul_rotate(&t, rotation, UFBX_ROTATION_XYZ);
	ufbxi_mul_rotate(&t, rotation_offset, UFBX_ROTATION_XYZ);
	ufbxi_add_translate(&t, translation);

	return t;
}

ufbxi_noinline static void ufbxi_update_node(ufbx_node *node)
{
	node->rotation_order = (ufbx_rotation_order)ufbxi_find_enum(&node->props, ufbxi_RotationOrder, UFBX_ROTATION_XYZ, UFBX_ROTATION_SPHERIC);
	node->euler_rotation = ufbxi_find_vec3(&node->props, ufbxi_Lcl_Rotation, 0.0f, 0.0f, 0.0f);

	node->inherit_type = (ufbx_inherit_type)ufbxi_find_enum(&node->props, ufbxi_InheritType, UFBX_INHERIT_NORMAL, UFBX_INHERIT_NO_SCALE);
	if (!node->is_root) {
		node->local_transform = ufbxi_get_transform(&node->props, node->rotation_order);
		node->geometry_transform = ufbxi_get_geometry_transform(&node->props);
		node->node_to_parent = ufbx_transform_to_matrix(&node->local_transform); 
	}

	ufbx_node *parent = node->parent;
	if (parent) {
		node->world_transform.rotation = ufbxi_mul_quat(parent->world_transform.rotation, node->local_transform.rotation);
		node->world_transform.translation = ufbx_transform_position(&parent->node_to_world, node->local_transform.translation);
		if (node->inherit_type != UFBX_INHERIT_NO_SCALE) {
			node->world_transform.scale.x = parent->world_transform.scale.x * node->local_transform.scale.x;
			node->world_transform.scale.y = parent->world_transform.scale.y * node->local_transform.scale.y;
			node->world_transform.scale.z = parent->world_transform.scale.z * node->local_transform.scale.z;
		} else {
			node->world_transform.scale = node->local_transform.scale;
		}

		if (node->inherit_type == UFBX_INHERIT_NORMAL) {
			node->node_to_world = ufbx_matrix_mul(&parent->node_to_world, &node->node_to_parent);
		} else {
			node->node_to_world = ufbx_transform_to_matrix(&node->world_transform);
		}
	} else {
		node->world_transform = node->local_transform;
		node->node_to_world = node->node_to_parent;
	}

	if (!ufbxi_is_transform_identity(node->geometry_transform)) {
		node->geometry_to_node = ufbx_transform_to_matrix(&node->geometry_transform); 
		node->geometry_to_world = ufbx_matrix_mul(&node->node_to_world, &node->geometry_to_node);
	} else {
		node->geometry_to_node = ufbx_identity_matrix;
		node->geometry_to_world = node->node_to_world;
	}

	node->visible = ufbxi_find_int(&node->props, ufbxi_Visibility, 1) != 0;
}

ufbxi_noinline static void ufbxi_update_light(ufbx_light *light)
{
	// NOTE: FBX seems to store intensities 100x of what's specified in at least
	// Maya and Blender, should there be a quirks mode to not do this for specific
	// exporters. Does the FBX SDK do this transparently as well?
	light->intensity = ufbxi_find_real(&light->props, ufbxi_Intensity, 100.0f) / (ufbx_real)100.0;

	light->color = ufbxi_find_vec3(&light->props, ufbxi_Color, 1.0f, 1.0f, 1.0f);
	light->type = (ufbx_light_type)ufbxi_find_enum(&light->props, ufbxi_LightType, 0, UFBX_LIGHT_VOLUME);
	int64_t default_decay = light->type == UFBX_LIGHT_DIRECTIONAL ? UFBX_LIGHT_DECAY_NONE : UFBX_LIGHT_DECAY_QUADRATIC;
	light->decay = (ufbx_light_decay)ufbxi_find_enum(&light->props, ufbxi_DecayType, default_decay, UFBX_LIGHT_DECAY_CUBIC);
	light->area_shape = (ufbx_light_area_shape)ufbxi_find_enum(&light->props, ufbxi_AreaLightShape, 0, UFBX_LIGHT_AREA_SPHERE);
	light->inner_angle = ufbxi_find_real(&light->props, ufbxi_HotSpot, 0.0f);
	light->inner_angle = ufbxi_find_real(&light->props, ufbxi_InnerAngle, light->inner_angle);
	light->outer_angle = ufbxi_find_real(&light->props, ufbxi_Cone_angle, 0.0f);
	light->outer_angle = ufbxi_find_real(&light->props, ufbxi_ConeAngle, light->outer_angle);
	light->outer_angle = ufbxi_find_real(&light->props, ufbxi_OuterAngle, light->outer_angle);
	light->cast_light = ufbxi_find_int(&light->props, ufbxi_CastLight, 1) != 0;
	light->cast_shadows = ufbxi_find_int(&light->props, ufbxi_CastShadows, 0) != 0;

	// TODO: Can this vary
	light->local_direction.x = 0.0f;
	light->local_direction.y = -1.0f;
	light->local_direction.z = 0.0f;
}

typedef struct {
	ufbx_vec2 film_size;
	ufbx_real squeeze_ratio;
} ufbxi_aperture_format;

static const ufbxi_aperture_format ufbxi_aperture_formats[] = {
	{ (ufbx_real)1.000, (ufbx_real)1.000, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_CUSTOM
	{ (ufbx_real)0.404, (ufbx_real)0.295, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_16MM_THEATRICAL
	{ (ufbx_real)0.493, (ufbx_real)0.292, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_SUPER_16MM
	{ (ufbx_real)0.864, (ufbx_real)0.630, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_35MM_ACADEMY
	{ (ufbx_real)0.816, (ufbx_real)0.612, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_35MM_TV_PROJECTION
	{ (ufbx_real)0.980, (ufbx_real)0.735, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_35MM_FULL_APERTURE
	{ (ufbx_real)0.825, (ufbx_real)0.446, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_35MM_185_PROJECTION
	{ (ufbx_real)0.864, (ufbx_real)0.732, (ufbx_real)2.0 }, // UFBX_APERTURE_FORMAT_35MM_ANAMORPHIC
	{ (ufbx_real)2.066, (ufbx_real)0.906, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_70MM_PROJECTION
	{ (ufbx_real)1.485, (ufbx_real)0.991, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_VISTAVISION
	{ (ufbx_real)2.080, (ufbx_real)1.480, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_DYNAVISION
	{ (ufbx_real)2.772, (ufbx_real)2.072, (ufbx_real)1.0 }, // UFBX_APERTURE_FORMAT_IMAX
};

ufbxi_noinline static void ufbxi_update_camera(ufbx_camera *camera)
{
	camera->aspect_mode = (ufbx_aspect_mode) ufbxi_find_enum(&camera->props, ufbxi_AspectRatioMode, 0, UFBX_ASPECT_MODE_FIXED_HEIGHT);
	camera->aperture_mode = (ufbx_aperture_mode)ufbxi_find_enum(&camera->props, ufbxi_ApertureMode, UFBX_APERTURE_MODE_VERTICAL, UFBX_APERTURE_MODE_FOCAL_LENGTH);
	camera->aperture_format = (ufbx_aperture_format)ufbxi_find_enum(&camera->props, ufbxi_ApertureFormat, UFBX_APERTURE_FORMAT_CUSTOM, UFBX_APERTURE_FORMAT_IMAX);
	camera->gate_fit = (ufbx_gate_fit)ufbxi_find_enum(&camera->props, ufbxi_GateFit, 0, UFBX_GATE_FIT_STRETCH);

	// Search both W/H and Width/Height but prefer the latter
	ufbx_real aspect_x = ufbxi_find_real(&camera->props, ufbxi_AspectW, 0.0f);
	ufbx_real aspect_y = ufbxi_find_real(&camera->props, ufbxi_AspectH, 0.0f);
	aspect_x = ufbxi_find_real(&camera->props, ufbxi_AspectWidth, aspect_x);
	aspect_y = ufbxi_find_real(&camera->props, ufbxi_AspectHeight, aspect_y);

	ufbx_real fov = ufbxi_find_real(&camera->props, ufbxi_FieldOfView, 0.0f);
	ufbx_real fov_x = ufbxi_find_real(&camera->props, ufbxi_FieldOfViewX, 0.0f);
	ufbx_real fov_y = ufbxi_find_real(&camera->props, ufbxi_FieldOfViewY, 0.0f);

	ufbx_real focal_length = ufbxi_find_real(&camera->props, ufbxi_FocalLength, 0.0f);

	ufbx_vec2 film_size = ufbxi_aperture_formats[camera->aperture_format].film_size;
	ufbx_real squeeze_ratio = ufbxi_aperture_formats[camera->aperture_format].squeeze_ratio;

	film_size.x = ufbxi_find_real(&camera->props, ufbxi_FilmWidth, film_size.x);
	film_size.y = ufbxi_find_real(&camera->props, ufbxi_FilmHeight, film_size.y);
	squeeze_ratio = ufbxi_find_real(&camera->props, ufbxi_FilmSqueezeRatio, squeeze_ratio);

	if (aspect_x <= 0.0f && aspect_y <= 0.0f) {
		aspect_x = film_size.x > 0.0f ? film_size.x : 1.0f;
		aspect_y = film_size.y > 0.0f ? film_size.y : 1.0f;
	} else if (aspect_x <= 0.0f) {
		if (film_size.x > 0.0f && film_size.y > 0.0f) {
			aspect_x = aspect_y / film_size.y * film_size.x;
		} else {
			aspect_x = aspect_y;
		}
	} else if (aspect_y <= 0.0f) {
		if (film_size.x > 0.0f && film_size.y > 0.0f) {
			aspect_y = aspect_x / film_size.x * film_size.y;
		} else {
			aspect_y = aspect_x;
		}
	}

	film_size.y *= squeeze_ratio;

	camera->focal_length_mm = focal_length;
	camera->film_size_inch = film_size;
	camera->squeeze_ratio = squeeze_ratio;

	switch (camera->aspect_mode) {
	case UFBX_ASPECT_MODE_WINDOW_SIZE:
	case UFBX_ASPECT_MODE_FIXED_RATIO:
		camera->resolution_is_pixels = false;
		camera->resolution.x = aspect_x;
		camera->resolution.y = aspect_y;
		break;
	case UFBX_ASPECT_MODE_FIXED_RESOLUTION:
		camera->resolution_is_pixels = true;
		camera->resolution.x = aspect_x;
		camera->resolution.y = aspect_y;
		break;
	case UFBX_ASPECT_MODE_FIXED_WIDTH:
		camera->resolution_is_pixels = true;
		camera->resolution.x = aspect_x;
		camera->resolution.y = aspect_x * aspect_y;
		break;
	case UFBX_ASPECT_MODE_FIXED_HEIGHT:
		camera->resolution_is_pixels = true;
		camera->resolution.x = aspect_y * aspect_x;
		camera->resolution.y = aspect_y;
		break;
	}

	ufbx_real aspect_ratio = camera->resolution.x / camera->resolution.y;
	ufbx_real film_ratio = film_size.x / film_size.y;

	ufbx_gate_fit effective_fit = camera->gate_fit;
	if (effective_fit == UFBX_GATE_FIT_FILL) {
		effective_fit = aspect_ratio > film_ratio ? UFBX_GATE_FIT_HORIZONTAL : UFBX_GATE_FIT_VERTICAL;
	} else if (effective_fit == UFBX_GATE_FIT_OVERSCAN) {
		effective_fit = aspect_ratio < film_ratio ? UFBX_GATE_FIT_HORIZONTAL : UFBX_GATE_FIT_VERTICAL;
	}

	switch (effective_fit) {
	case UFBX_GATE_FIT_NONE:
		camera->aperture_size_inch = camera->film_size_inch;
		break;
	case UFBX_GATE_FIT_VERTICAL:
		camera->aperture_size_inch.x = camera->film_size_inch.y * aspect_ratio;
		camera->aperture_size_inch.y = camera->film_size_inch.y;
		break;
	case UFBX_GATE_FIT_HORIZONTAL:
		camera->aperture_size_inch.x = camera->film_size_inch.x;
		camera->aperture_size_inch.y = camera->film_size_inch.x / aspect_ratio;
		break;
	case UFBX_GATE_FIT_FILL:
	case UFBX_GATE_FIT_OVERSCAN:
		camera->aperture_size_inch = camera->film_size_inch;
		ufbx_assert(0 && "Unreachable, set to vertical/horizontal above");
		break;
	case UFBX_GATE_FIT_STRETCH:
		camera->aperture_size_inch = camera->film_size_inch;
		// TODO: Not sure what to do here...
		break;
	}

	switch (camera->aperture_mode) {
	case UFBX_APERTURE_MODE_HORIZONTAL_AND_VERTICAL:
		camera->field_of_view_deg.x = fov_x;
		camera->field_of_view_deg.y = fov_y;
		camera->field_of_view_tan.x = (ufbx_real)tan(fov_x * (UFBXI_DEG_TO_RAD * 0.5f));
		camera->field_of_view_tan.y = (ufbx_real)tan(fov_y * (UFBXI_DEG_TO_RAD * 0.5f));
		break;
	case UFBX_APERTURE_MODE_HORIZONTAL:
		camera->field_of_view_deg.x = fov;
		camera->field_of_view_tan.x = (ufbx_real)tan(fov * (UFBXI_DEG_TO_RAD * 0.5f));
		camera->field_of_view_tan.y = camera->field_of_view_tan.x / aspect_ratio;
		camera->field_of_view_deg.y = (ufbx_real)atan(camera->field_of_view_tan.y) * UFBXI_RAD_TO_DEG * 2.0f;
		break;
	case UFBX_APERTURE_MODE_VERTICAL:
		camera->field_of_view_deg.y = fov;
		camera->field_of_view_tan.y = (ufbx_real)tan(fov * (UFBXI_DEG_TO_RAD * 0.5f));
		camera->field_of_view_tan.x = camera->field_of_view_tan.y * aspect_ratio;
		camera->field_of_view_deg.x = (ufbx_real)atan(camera->field_of_view_tan.x) * UFBXI_RAD_TO_DEG * 2.0f;
		break;
	case UFBX_APERTURE_MODE_FOCAL_LENGTH:
		camera->field_of_view_tan.x = camera->aperture_size_inch.x / (camera->focal_length_mm * UFBXI_MM_TO_INCH) * 0.5f;
		camera->field_of_view_tan.y = camera->aperture_size_inch.y / (camera->focal_length_mm * UFBXI_MM_TO_INCH) * 0.5f;
		camera->field_of_view_deg.x = (ufbx_real)atan(camera->field_of_view_tan.x) * UFBXI_RAD_TO_DEG * 2.0f;
		camera->field_of_view_deg.y = (ufbx_real)atan(camera->field_of_view_tan.y) * UFBXI_RAD_TO_DEG * 2.0f;
		break;
	}
}

ufbxi_noinline static void ufbxi_update_bone(ufbx_scene *scene, ufbx_bone *bone)
{
	ufbx_real unit = scene->metadata.bone_prop_size_unit;

	bone->radius = ufbxi_find_real(&bone->props, ufbxi_Size, unit) / unit;
	if (scene->metadata.bone_prop_limb_length_relative) {
		bone->relative_length = ufbxi_find_real(&bone->props, ufbxi_LimbLength, 1.0f);
	} else {
		bone->relative_length = 1.0f;
	}
}

ufbxi_noinline static void ufbxi_update_line_curve(ufbx_line_curve *line)
{
	line->color = ufbxi_find_vec3(&line->props, ufbxi_Color, 1.0f, 1.0f, 1.0f);
}

ufbxi_noinline static void ufbxi_update_skin_cluster(ufbx_skin_cluster *cluster)
{
	if (cluster->bone_node) {
		cluster->geometry_to_world = ufbx_matrix_mul(&cluster->bone_node->node_to_world, &cluster->geometry_to_bone);
	} else {
		cluster->geometry_to_world = ufbx_matrix_mul(&cluster->bind_to_world, &cluster->geometry_to_bone);
	}
	cluster->geometry_to_world_transform = ufbx_matrix_to_transform(&cluster->geometry_to_world);
}

ufbxi_noinline static void ufbxi_update_blend_channel(ufbx_blend_channel *channel)
{
	ufbx_real weight = ufbxi_find_real(&channel->props, ufbxi_DeformPercent, 0.0f) * (ufbx_real)0.01;
	channel->weight = weight;

	ptrdiff_t num_keys = (ptrdiff_t)channel->keyframes.count;
	if (num_keys > 0) {
		ufbx_blend_keyframe *keys = channel->keyframes.data;

		// Reset the effective weights to zero and find the split around zero
		ptrdiff_t last_negative = -1;
		for (ptrdiff_t i = 0; i < num_keys; i++) {
			keys[i].effective_weight = (ufbx_real)0.0;
			if (keys[i].target_weight < 0.0) last_negative = i;
		}

		// Find either the next or last keyframe away from zero
		ufbx_blend_keyframe zero_key = { NULL };
		ufbx_blend_keyframe *prev = &zero_key, *next = &zero_key;
		if (weight > 0.0) {
			if (last_negative >= 0) prev = &keys[last_negative];
			for (ptrdiff_t i = last_negative + 1; i < num_keys; i++) {
				prev = next;
				next = &keys[i];
				if (next->target_weight > weight) break;
			}
		} else {
			if (last_negative + 1 < num_keys) prev = &keys[last_negative + 1];
			for (ptrdiff_t i = last_negative; i >= 0; i--) {
				prev = next;
				next = &keys[i];
				if (next->target_weight < weight) break;
			}
		}

		// Linearly interpolate between the endpoints with the weight
		ufbx_real delta = next->target_weight - prev->target_weight;
		if (delta != 0.0) {
			ufbx_real t = (weight - prev->target_weight) / delta;
			prev->effective_weight = 1.0f - t;
			next->effective_weight = t;
		}
	}
}

ufbxi_noinline static void ufbxi_update_material(ufbx_scene *scene, ufbx_material *material)
{
	if (material->props.num_animated > 0) {
		ufbxi_fetch_maps(scene, material);
	}
}

ufbxi_noinline static void ufbxi_update_texture(ufbx_texture *texture)
{
	texture->transform = ufbxi_get_texture_transform(&texture->props);
	if (!ufbxi_is_transform_identity(texture->transform)) {
		texture->texture_to_uv = ufbx_transform_to_matrix(&texture->transform);
		texture->uv_to_texture = ufbx_matrix_invert(&texture->texture_to_uv);
	} else {
		texture->texture_to_uv = ufbx_identity_matrix;
		texture->uv_to_texture = ufbx_identity_matrix;
	}
	texture->wrap_u = (ufbx_wrap_mode)ufbxi_find_enum(&texture->props, ufbxi_WrapModeU, 0, UFBX_WRAP_CLAMP);
	texture->wrap_v = (ufbx_wrap_mode)ufbxi_find_enum(&texture->props, ufbxi_WrapModeV, 0, UFBX_WRAP_CLAMP);
}

ufbxi_noinline static void ufbxi_update_anim_stack(ufbx_scene *scene, ufbx_anim_stack *stack)
{
	ufbx_prop *begin, *end;
	begin = ufbxi_find_prop(&stack->props, ufbxi_LocalStart);
	end = ufbxi_find_prop(&stack->props, ufbxi_LocalStop);
	if (begin && end) {
		stack->time_begin = (double)begin->value_int * scene->metadata.ktime_to_sec;
		stack->time_end = (double)end->value_int * scene->metadata.ktime_to_sec;
	} else {
		begin = ufbxi_find_prop(&stack->props, ufbxi_ReferenceStart);
		end = ufbxi_find_prop(&stack->props, ufbxi_ReferenceStop);
		if (begin && end) {
			stack->time_begin = (double)begin->value_int * scene->metadata.ktime_to_sec;
			stack->time_end = (double)end->value_int * scene->metadata.ktime_to_sec;
		}
	}
}

ufbxi_noinline static void ufbxi_update_display_layer(ufbx_display_layer *layer)
{
	layer->visible = ufbxi_find_int(&layer->props, ufbxi_Show, 1) != 0;
	layer->frozen = ufbxi_find_int(&layer->props, ufbxi_Freeze, 1) != 0;
	layer->ui_color = ufbxi_find_vec3(&layer->props, ufbxi_Color, 0.8f, 0.8f, 0.8f);
}

ufbxi_noinline static void ufbxi_update_constraint(ufbx_constraint *constraint)
{
	ufbx_props *props = &constraint->props;
	ufbx_constraint_type constraint_type = constraint->type;

	constraint->transform_offset = ufbxi_get_constraint_transform(props);

	constraint->weight = ufbxi_find_real(props, ufbxi_Weight, 100.0f) / (ufbx_real)100.0;

	ufbxi_for_list(ufbx_constraint_target, target, constraint->targets) {
		ufbx_node *node = target->node;

		// Node names are at most 255 bytes so the suffixed names are bounded
		char name_buf[256 + 8];
		size_t name_len = node->name.length;
		ufbx_assert(name_len < 256);
		memcpy(name_buf, node->name.data, name_len);

		ufbx_real weight_scale = (ufbx_real)100.0;
		if (constraint_type == UFBX_CONSTRAINT_SINGLE_CHAIN_IK) {
			// IK weights seem to be not scaled 100x?
			weight_scale = (ufbx_real)1.0;
		}

		memcpy(name_buf + name_len, ".Weight", 7 + 1);
		target->weight = ufbx_find_real_len(props, name_buf, name_len + 7, weight_scale) / weight_scale;

		if (constraint_type == UFBX_CONSTRAINT_PARENT) {
			memcpy(name_buf + name_len, ".Offset T", 9 + 1);
			ufbx_vec3 t = ufbx_find_vec3_len(props, name_buf, name_len + 9, ufbx_zero_vec3);
			name_buf[name_len + 8] = 'R';
			ufbx_vec3 r = ufbx_find_vec3_len(props, name_buf, name_len + 9, ufbx_zero_vec3);
			name_buf[name_len + 8] = 'S';
			ufbx_vec3 s = ufbx_find_vec3_len(props, name_buf, name_len + 9, ufbxi_one_vec3);

			target->transform.translation = t;
			target->transform.rotation = ufbx_euler_to_quat(r, UFBX_ROTATION_XYZ);
			target->transform.scale = s;
		}
	}

	constraint->active = ufbx_find_int(props, "Active", 1) != 0;
	if (constraint_type == UFBX_CONSTRAINT_AIM) {
		constraint->constrain_rotation[0] = ufbx_find_int(props, "AffectX", 1) != 0;
		constraint->constrain_rotation[1] = ufbx_find_int(props, "AffectY", 1) != 0;
		constraint->constrain_rotation[2] = ufbx_find_int(props, "AffectZ", 1) != 0;

		const ufbx_vec3 default_aim = { 1.0f, 0.0f, 0.0f };
		const ufbx_vec3 default_up = { 0.0f, 1.0f, 0.0f };

		int64_t up_type = ufbx_find_int(props, "WorldUpType", 0);
		if (up_type >= 0 && up_type < UFBX_CONSTRAINT_AIM_UP_NONE) {
			constraint->aim_up_type = (ufbx_constraint_aim_up_type)up_type;
		}
		constraint->aim_vector = ufbx_find_vec3(props, "AimVector", default_aim);
		constraint->aim_up_vector = ufbx_find_vec3(props, "UpVector", default_up);

	} else if (constraint_type == UFBX_CONSTRAINT_PARENT) {
		constraint->constrain_translation[0] = ufbx_find_int(props, "AffectTranslationX", 1) != 0;
		constraint->constrain_translation[1] = ufbx_find_int(props, "AffectTranslationY", 1) != 0;
		constraint->constrain_translation[2] = ufbx_find_int(props, "AffectTranslationZ", 1) != 0;
		constraint->constrain_rotation[0] = ufbx_find_int(props, "AffectRotationX", 1) != 0;
		constraint->constrain_rotation[1] = ufbx_find_int(props, "AffectRotationY", 1) != 0;
		constraint->constrain_rotation[2] = ufbx_find_int(props, "AffectRotationZ", 1) != 0;
		constraint->constrain_scale[0] = ufbx_find_int(props, "AffectScalingX", 0) != 0;
		constraint->constrain_scale[1] = ufbx_find_int(props, "AffectScalingY", 0) != 0;
		constraint->constrain_scale[2] = ufbx_find_int(props, "AffectScalingZ", 0) != 0;
	} else if (constraint_type == UFBX_CONSTRAINT_POSITION) {
		constraint->constrain_translation[0] = ufbx_find_int(props, "AffectX", 1) != 0;
		constraint->constrain_translation[1] = ufbx_find_int(props, "AffectY", 1) != 0;
		constraint->constrain_translation[2] = ufbx_find_int(props, "AffectZ", 1) != 0;
	} else if (constraint_type == UFBX_CONSTRAINT_ROTATION) {
		constraint->constrain_rotation[0] = ufbx_find_int(props, "AffectX", 1) != 0;
		constraint->constrain_rotation[1] = ufbx_find_int(props, "AffectY", 1) != 0;
		constraint->constrain_rotation[2] = ufbx_find_int(props, "AffectZ", 1) != 0;
	} else if (constraint_type == UFBX_CONSTRAINT_SCALE) {
		constraint->constrain_scale[0] = ufbx_find_int(props, "AffectX", 1) != 0;
		constraint->constrain_scale[1] = ufbx_find_int(props, "AffectY", 1) != 0;
		constraint->constrain_scale[2] = ufbx_find_int(props, "AffectZ", 1) != 0;
	} else if (constraint_type == UFBX_CONSTRAINT_SINGLE_CHAIN_IK) {
		constraint->constrain_rotation[0] = true;
		constraint->constrain_rotation[1] = true;
		constraint->constrain_rotation[2] = true;
		constraint->ik_pole_vector = ufbx_find_vec3(props, "PoleVectorType", ufbx_zero_vec3);
	}
}

ufbxi_noinline static void ufbxi_update_initial_clusters(ufbx_scene *scene)
{
	ufbxi_for_ptr_list(ufbx_skin_cluster, p_cluster, scene->skin_clusters) {
		ufbx_skin_cluster *cluster = *p_cluster;
		cluster->geometry_to_bone = cluster->mesh_node_to_bone;
	}

	// Patch initial `mesh_node_to_bone`
	ufbxi_for_ptr_list(ufbx_skin_cluster, p_cluster, scene->skin_clusters) {
		ufbx_skin_cluster *cluster = *p_cluster;

		ufbx_skin_deformer *skin = (ufbx_skin_deformer*)ufbxi_fetch_src_element(&cluster->element, false, NULL, UFBX_ELEMENT_SKIN_DEFORMER);
		if (!skin) continue;

		ufbx_node *node = (ufbx_node*)ufbxi_fetch_src_element(&skin->element, false, NULL, UFBX_ELEMENT_NODE);
		if (!node) {
			ufbx_mesh *mesh = (ufbx_mesh*)ufbxi_fetch_src_element(&skin->element, false, NULL, UFBX_ELEMENT_MESH);
			if (mesh && mesh->instances.count > 0) {
				node = mesh->instances.data[0];
			}
		}
		if (!node) continue;

		if (ufbxi_matrix_all_zero(&cluster->mesh_node_to_bone)) {
			ufbx_matrix world_to_bind = ufbx_matrix_invert(&cluster->bind_to_world);
			cluster->mesh_node_to_bone = ufbx_matrix_mul(&world_to_bind, &node->node_to_world);
		}

		cluster->geometry_to_bone = ufbx_matrix_mul(&cluster->mesh_node_to_bone, &node->geometry_to_node);
	}
}

ufbx_coordinate_axis ufbxi_find_axis(const ufbx_props *props, const char *axis_name, const char *sign_name)
{
	int64_t axis = ufbxi_find_int(props, axis_name, 3);
	int64_t sign = ufbxi_find_int(props, sign_name, 2);

	switch (axis) {
	case 0: return sign > 0 ? UFBX_COORDINATE_AXIS_POSITIVE_X : UFBX_COORDINATE_AXIS_NEGATIVE_X;
	case 1: return sign > 0 ? UFBX_COORDINATE_AXIS_POSITIVE_Y : UFBX_COORDINATE_AXIS_NEGATIVE_Y;
	case 2: return sign > 0 ? UFBX_COORDINATE_AXIS_POSITIVE_Z : UFBX_COORDINATE_AXIS_NEGATIVE_Z;
	default: return UFBX_COORDINATE_AXIS_UNKNOWN;
	}
}

static const ufbx_real ufbxi_time_mode_fps[] = {
	24.0f,   // UFBX_TIME_MODE_DEFAULT
	120.0f,  // UFBX_TIME_MODE_120_FPS
	100.0f,  // UFBX_TIME_MODE_100_FPS
	60.0f,   // UFBX_TIME_MODE_60_FPS
	50.0f,   // UFBX_TIME_MODE_50_FPS
	48.0f,   // UFBX_TIME_MODE_48_FPS
	30.0f,   // UFBX_TIME_MODE_30_FPS
	30.0f,   // UFBX_TIME_MODE_30_FPS_DROP
	29.97f,  // UFBX_TIME_MODE_NTSC_DROP_FRAME
	29.97f,  // UFBX_TIME_MODE_NTSC_FULL_FRAME
	25.0f,   // UFBX_TIME_MODE_PAL
	24.0f,   // UFBX_TIME_MODE_24_FPS
	1000.0f, // UFBX_TIME_MODE_1000_FPS
	23.976f, // UFBX_TIME_MODE_FILM_FULL_FRAME
	24.0f,   // UFBX_TIME_MODE_CUSTOM
	96.0f,   // UFBX_TIME_MODE_96_FPS
	72.0f,   // UFBX_TIME_MODE_72_FPS
	59.94f,  // UFBX_TIME_MODE_59_94_FPS
};

static void ufbxi_update_scene(ufbx_scene *scene, bool initial)
{
	ufbxi_for_ptr_list(ufbx_node, p_node, scene->nodes) {
		ufbxi_update_node(*p_node);
	}

	ufbxi_for_ptr_list(ufbx_light, p_light, scene->lights) {
		ufbxi_update_light(*p_light);
	}

	ufbxi_for_ptr_list(ufbx_camera, p_camera, scene->cameras) {
		ufbxi_update_camera(*p_camera);
	}

	ufbxi_for_ptr_list(ufbx_bone, p_bone, scene->bones) {
		ufbxi_update_bone(scene, *p_bone);
	}

	ufbxi_for_ptr_list(ufbx_line_curve, p_line, scene->line_curves) {
		ufbxi_update_line_curve(*p_line);
	}

	if (initial) {
		ufbxi_update_initial_clusters(scene);
	}

	ufbxi_for_ptr_list(ufbx_skin_cluster, p_cluster, scene->skin_clusters) {
		ufbxi_update_skin_cluster(*p_cluster);
	}

	ufbxi_for_ptr_list(ufbx_blend_channel, p_channel, scene->blend_channels) {
		ufbxi_update_blend_channel(*p_channel);
	}

	ufbxi_for_ptr_list(ufbx_material, p_material, scene->materials) {
		ufbxi_update_material(scene, *p_material);
	}

	ufbxi_for_ptr_list(ufbx_texture, p_texture, scene->textures) {
		ufbxi_update_texture(*p_texture);
	}

	ufbxi_for_ptr_list(ufbx_anim_stack, p_stack, scene->anim_stacks) {
		ufbxi_update_anim_stack(scene, *p_stack);
	}

	ufbxi_for_ptr_list(ufbx_display_layer, p_layer, scene->display_layers) {
		ufbxi_update_display_layer(*p_layer);
	}

	ufbxi_for_ptr_list(ufbx_constraint, p_constraint, scene->constraints) {
		ufbxi_update_constraint(*p_constraint);
	}
}

static ufbxi_noinline void ufbxi_update_scene_metadata(ufbx_metadata *metadata)
{
	ufbx_props *props = &metadata->scene_props;
	metadata->original_application.vendor = ufbx_find_string(props, "Original|ApplicationVendor", ufbx_empty_string);
	metadata->original_application.name = ufbx_find_string(props, "Original|ApplicationName", ufbx_empty_string);
	metadata->original_application.version = ufbx_find_string(props, "Original|ApplicationVersion", ufbx_empty_string);
	metadata->latest_application.vendor = ufbx_find_string(props, "LastSaved|ApplicationVendor", ufbx_empty_string);
	metadata->latest_application.name = ufbx_find_string(props, "LastSaved|ApplicationName", ufbx_empty_string);
	metadata->latest_application.version = ufbx_find_string(props, "LastSaved|ApplicationVersion", ufbx_empty_string);
}

static const ufbx_real ufbxi_pow10_targets[] = {
	0.0f,
	(ufbx_real)1e-8, (ufbx_real)1e-7, (ufbx_real)1e-6, (ufbx_real)1e-5,
	(ufbx_real)1e-4, (ufbx_real)1e-3, (ufbx_real)1e-2, (ufbx_real)1e-1,
	(ufbx_real)1e+0, (ufbx_real)1e+1, (ufbx_real)1e+2, (ufbx_real)1e+3,
	(ufbx_real)1e+4, (ufbx_real)1e+5, (ufbx_real)1e+6, (ufbx_real)1e+7,
	(ufbx_real)1e+8, (ufbx_real)1e+9,
};

static ufbxi_noinline ufbx_real ufbxi_round_if_near(const ufbx_real *targets, size_t num_targets, ufbx_real value)
{
	for (size_t i = 0; i < num_targets; i++) {
		double target = targets[i];
		double error = target * 9.5367431640625e-7;
		if (error < 0.0) error = -error;
		if (error < 7.52316384526264005e-37) error = 7.52316384526264005e-37;
		if (value >= target - error && value <= target + error) {
			return (ufbx_real)target;
		}
	}
	return value;
}

static ufbxi_noinline void ufbxi_update_scene_settings(ufbx_scene_settings *settings)
{
	ufbx_real unit_scale_factor = ufbxi_find_real(&settings->props, ufbxi_UnitScaleFactor, 1.0f);
	ufbx_real original_unit_scale_factor = ufbxi_find_real(&settings->props, ufbxi_OriginalUnitScaleFactor, unit_scale_factor);

	settings->axes.up = ufbxi_find_axis(&settings->props, ufbxi_UpAxis, ufbxi_UpAxisSign);
	settings->axes.front = ufbxi_find_axis(&settings->props, ufbxi_FrontAxis, ufbxi_FrontAxisSign);
	settings->axes.right = ufbxi_find_axis(&settings->props, ufbxi_CoordAxis, ufbxi_CoordAxisSign);
	settings->unit_meters = ufbxi_round_if_near(ufbxi_pow10_targets, ufbxi_arraycount(ufbxi_pow10_targets), unit_scale_factor * (ufbx_real)0.01);
	settings->original_unit_meters = ufbxi_round_if_near(ufbxi_pow10_targets, ufbxi_arraycount(ufbxi_pow10_targets), original_unit_scale_factor * (ufbx_real)0.01);
	settings->frames_per_second = ufbxi_find_real(&settings->props, ufbxi_CustomFrameRate, 24.0f);
	settings->ambient_color = ufbxi_find_vec3(&settings->props, ufbxi_AmbientColor, 0.0f, 0.0f, 0.0f);
	settings->original_axis_up = ufbxi_find_axis(&settings->props, ufbxi_OriginalUpAxis, ufbxi_OriginalUpAxisSign);

	ufbx_prop *default_camera = ufbxi_find_prop(&settings->props, ufbxi_DefaultCamera);
	if (default_camera) {
		settings->default_camera = default_camera->value_str;
	} else {
		settings->default_camera = ufbx_empty_string;
	}

	settings->time_mode = (ufbx_time_mode)ufbxi_find_enum(&settings->props, ufbxi_TimeMode, UFBX_TIME_MODE_24_FPS, UFBX_TIME_MODE_59_94_FPS);
	settings->time_protocol = (ufbx_time_protocol)ufbxi_find_enum(&settings->props, ufbxi_TimeProtocol, UFBX_TIME_PROTOCOL_DEFAULT, UFBX_TIME_PROTOCOL_DEFAULT);
	settings->snap_mode = (ufbx_snap_mode)ufbxi_find_enum(&settings->props, ufbxi_SnapOnFrameMode, UFBX_SNAP_MODE_NONE, UFBX_SNAP_MODE_SNAP_AND_PLAY);

	if (settings->time_mode != UFBX_TIME_MODE_CUSTOM) {
		settings->frames_per_second = ufbxi_time_mode_fps[settings->time_mode];
	}
}

// -- Geometry caches

typedef struct {
	ufbx_geometry_cache cache;
	uint32_t magic;
	bool owned_by_scene;

	ufbxi_allocator ator;
	ufbxi_buf result_buf;
	ufbxi_buf string_buf;
} ufbxi_geometry_cache_imp;

typedef struct {
	ufbx_string name;
	ufbx_string interpretation;
	uint32_t sample_rate;
	uint32_t start_time;
	uint32_t end_time;
	uint32_t current_time;
	uint32_t conescutive_fails;
	bool try_load;
} ufbxi_cache_tmp_channel;

typedef enum {
	UFBXI_CACHE_XML_TYPE_NONE,
	UFBXI_CACHE_XML_TYPE_FILE_PER_FRAME,
	UFBXI_CACHE_XML_TYPE_SINGLE_FILE,
} ufbxi_cache_xml_type;

typedef enum {
	UFBXI_CACHE_XML_FORMAT_NONE,
	UFBXI_CACHE_XML_FORMAT_MCC,
	UFBXI_CACHE_XML_FORMAT_MCX,
} ufbxi_cache_xml_format;

typedef struct {
	ufbx_error error;
	ufbx_string filename;
	bool owned_by_scene;

	ufbx_geometry_cache_opts opts;

	ufbxi_allocator *ator_tmp;
	ufbxi_allocator ator_result;

	ufbxi_buf result;
	ufbxi_buf tmp;
	ufbxi_buf tmp_stack;

	ufbxi_cache_tmp_channel *channels;
	size_t num_channels;

	// Temporary array
	char *tmp_arr;
	size_t tmp_arr_size;

	ufbxi_string_pool string_pool;

	ufbx_open_file_fn *open_file_fn;
	void *open_file_user;

	double frames_per_second;

	ufbx_string stream_filename;
	ufbx_stream stream;

	bool mc_for8;

	ufbx_string xml_filename;
	uint32_t xml_ticks_per_frame;
	ufbxi_cache_xml_type xml_type;
	ufbxi_cache_xml_format xml_format;

	ufbx_string channel_name;

	char *name_buf;
	size_t name_cap;

	uint64_t file_offset;
	const char *pos, *pos_end;

	ufbx_geometry_cache cache;
	ufbxi_geometry_cache_imp *imp;

	char buffer[128];
} ufbxi_cache_context;

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_read(ufbxi_cache_context *cc, void *dst, size_t size, bool allow_eof)
{
	size_t buffered = ufbxi_min_sz((size_t)(cc->pos_end - cc->pos), size);
	memcpy(dst, cc->pos, buffered);
	cc->pos += buffered;
	size -= buffered;
	cc->file_offset += buffered;
	if (size == 0) return 1;
	dst = (char*)dst + buffered;

	if (size >= sizeof(cc->buffer)) {
		size_t num_read = cc->stream.read_fn(cc->stream.user, dst, size);
		ufbxi_check_err_msg(&cc->error, num_read <= size, "IO error");
		if (!allow_eof) {
			ufbxi_check_err_msg(&cc->error, num_read == size, "Truncated file");
		}
		cc->file_offset += num_read;
		size -= num_read;
		dst = (char*)dst + num_read;
	} else {
		size_t num_read = cc->stream.read_fn(cc->stream.user, cc->buffer, sizeof(cc->buffer));
		ufbxi_check_err_msg(&cc->error, num_read <= sizeof(cc->buffer), "IO error");
		if (!allow_eof) {
			ufbxi_check_err_msg(&cc->error, num_read >= size, "Truncated file");
		}
		cc->pos = cc->buffer;
		cc->pos_end = cc->buffer + sizeof(cc->buffer);

		memcpy(dst, cc->pos, size);
		cc->pos += size;
		cc->file_offset += size;

		size_t num_written = ufbxi_min_sz(size, num_read);
		size -= num_written;
		dst = (char*)dst + num_written;
	}

	if (size > 0) {
		memset(dst, 0, size);
	}

	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_skip(ufbxi_cache_context *cc, uint64_t size)
{
	cc->file_offset += size;

	uint64_t buffered = ufbxi_min64((uint64_t)(cc->pos_end - cc->pos), size);
	cc->pos += buffered;
	size -= buffered;

	if (cc->stream.skip_fn) {
		while (size >= UFBXI_MAX_SKIP_SIZE) {
			size -= UFBXI_MAX_SKIP_SIZE;
			ufbxi_check_err_msg(&cc->error, cc->stream.skip_fn(cc->stream.user, UFBXI_MAX_SKIP_SIZE - 1), "Truncated file");

			// Check that we can read at least one byte in case the file is broken
			// and causes us to seek indefinitely forwards as `fseek()` does not
			// report if we hit EOF...
			char single_byte[1];
			size_t num_read = cc->stream.read_fn(cc->stream.user, single_byte, 1);
			ufbxi_check_err_msg(&cc->error, num_read <= 1, "IO error");
			ufbxi_check_err_msg(&cc->error, num_read == 1, "Truncated file");
		}

		if (size > 0) {
			ufbxi_check_err_msg(&cc->error, cc->stream.skip_fn(cc->stream.user, (size_t)size), "Truncated file");
		}

	} else {
		char skip_buf[2048];
		while (size > 0) {
			size_t to_skip = (size_t)ufbxi_min64(size, sizeof(skip_buf));
			size -= to_skip;
			ufbxi_check_err_msg(&cc->error, cc->stream.read_fn(cc->stream.user, skip_buf, to_skip), "Truncated file");
		}
	}

	return 1;
}

#define ufbxi_cache_mc_tag(a,b,c,d) ((uint32_t)(a)<<24u | (uint32_t)(b)<<16 | (uint32_t)(c)<<8u | (uint32_t)(d))

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_mc_read_tag(ufbxi_cache_context *cc, uint32_t *p_tag)
{
	char buf[4];
	ufbxi_check_err(&cc->error, ufbxi_cache_read(cc, buf, 4, true));
	*p_tag = (uint32_t)(uint8_t)buf[0]<<24u | (uint32_t)(uint8_t)buf[1]<<16 | (uint32_t)(uint8_t)buf[2]<<8u | (uint32_t)(uint8_t)buf[3];
	if (*p_tag == ufbxi_cache_mc_tag('F','O','R','8')) {
		cc->mc_for8 = true;
	}
	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_mc_read_u32(ufbxi_cache_context *cc, uint32_t *p_value)
{
	char buf[4];
	ufbxi_check_err(&cc->error, ufbxi_cache_read(cc, buf, 4, false));
	*p_value = (uint32_t)(uint8_t)buf[0]<<24u | (uint32_t)(uint8_t)buf[1]<<16 | (uint32_t)(uint8_t)buf[2]<<8u | (uint32_t)(uint8_t)buf[3];
	if (cc->mc_for8) {
		ufbxi_check_err(&cc->error, ufbxi_cache_read(cc, buf, 4, false));
	}
	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_mc_read_u64(ufbxi_cache_context *cc, uint64_t *p_value)
{
	if (!cc->mc_for8) {
		uint32_t v32;
		ufbxi_check_err(&cc->error, ufbxi_cache_mc_read_u32(cc, &v32));
		*p_value = v32;
	} else {
		char buf[8];
		ufbxi_check_err(&cc->error, ufbxi_cache_read(cc, buf, 8, false));
		uint32_t hi = (uint32_t)(uint8_t)buf[0]<<24u | (uint32_t)(uint8_t)buf[1]<<16 | (uint32_t)(uint8_t)buf[2]<<8u | (uint32_t)(uint8_t)buf[3];
		uint32_t lo = (uint32_t)(uint8_t)buf[4]<<24u | (uint32_t)(uint8_t)buf[5]<<16 | (uint32_t)(uint8_t)buf[6]<<8u | (uint32_t)(uint8_t)buf[7];
		*p_value = (uint64_t)hi << 32u | (uint64_t)lo;
	}
	return 1;
}

static const uint8_t ufbxi_cache_data_format_size[] = {
	0, 4, 12, 8, 24,
};

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_load_mc(ufbxi_cache_context *cc)
{
	uint32_t version = 0, time_start = 0, time_end = 0;
	uint32_t count = 0, time = 0;
	char skip_buf[8];

	for (;;) {
		uint32_t tag;
		uint64_t size;
		ufbxi_check_err(&cc->error, ufbxi_cache_mc_read_tag(cc, &tag));
		if (tag == 0) break;

		if (tag == ufbxi_cache_mc_tag('C','A','C','H') || tag == ufbxi_cache_mc_tag('M','Y','C','H')) {
			continue;
		}
		if (cc->mc_for8) {
			ufbxi_check_err(&cc->error, ufbxi_cache_read(cc, skip_buf, 4, false));
		}

		ufbxi_check_err(&cc->error, ufbxi_cache_mc_read_u64(cc, &size));
		uint64_t begin = cc->file_offset;

		size_t alignment = cc->mc_for8 ? 8 : 4;

		ufbx_cache_data_format format = UFBX_CACHE_DATA_UNKNOWN;
		switch (tag) {
		case ufbxi_cache_mc_tag('F','O','R','4'): cc->mc_for8 = false; break;
		case ufbxi_cache_mc_tag('F','O','R','8'): cc->mc_for8 = true; break;
		case ufbxi_cache_mc_tag('V','R','S','N'): ufbxi_check_err(&cc->error, ufbxi_cache_mc_read_u32(cc, &version)); break;
		case ufbxi_cache_mc_tag('S','T','I','M'):
			ufbxi_check_err(&cc->error, ufbxi_cache_mc_read_u32(cc, &time_start));
			time = time_start;
			break;
		case ufbxi_cache_mc_tag('E','T','I','M'): ufbxi_check_err(&cc->error, ufbxi_cache_mc_read_u32(cc, &time_end)); break;
		case ufbxi_cache_mc_tag('T','I','M','E'): ufbxi_check_err(&cc->error, ufbxi_cache_mc_read_u32(cc, &time)); break;
		case ufbxi_cache_mc_tag('C','H','N','M'): {
			ufbxi_check_err(&cc->error, size > 0 && size < SIZE_MAX);
			size_t length = (size_t)size - 1;
			size_t padded_length = ((size_t)size + alignment - 1) & ~(alignment - 1);
			ufbxi_check_err(&cc->error, ufbxi_grow_array(cc->ator_tmp, &cc->name_buf, &cc->name_cap, padded_length));
			ufbxi_check_err(&cc->error, ufbxi_cache_read(cc, cc->name_buf, padded_length, false));
			cc->channel_name.data = cc->name_buf;
			cc->channel_name.length = length;
			ufbxi_check_err(&cc->error, ufbxi_push_string_place_str(&cc->string_pool, &cc->channel_name));
		} break;
		case ufbxi_cache_mc_tag('S','I','Z','E'): ufbxi_check_err(&cc->error, ufbxi_cache_mc_read_u32(cc, &count)); break;
		case ufbxi_cache_mc_tag('F','V','C','A'): format = UFBX_CACHE_DATA_VEC3_FLOAT; break;
		case ufbxi_cache_mc_tag('D','V','C','A'): format = UFBX_CACHE_DATA_VEC3_DOUBLE; break;
		case ufbxi_cache_mc_tag('F','B','C','A'): format = UFBX_CACHE_DATA_REAL_FLOAT; break;
		case ufbxi_cache_mc_tag('D','B','C','A'): format = UFBX_CACHE_DATA_REAL_DOUBLE; break;
		case ufbxi_cache_mc_tag('D','B','L','A'): format = UFBX_CACHE_DATA_REAL_DOUBLE; break;
		default: ufbxi_fail_err(&cc->error, "Unknown tag");
		}

		if (format != UFBX_CACHE_DATA_UNKNOWN) {
			ufbx_cache_frame *frame = ufbxi_push_zero(&cc->tmp_stack, ufbx_cache_frame, 1);
			ufbxi_check_err(&cc->error, frame);

			uint32_t elem_size = ufbxi_cache_data_format_size[format];
			uint64_t total_size = (uint64_t)elem_size * (uint64_t)count;
			ufbxi_check_err(&cc->error, size >= elem_size * count);

			frame->channel = cc->channel_name;
			frame->time = (double)time * (1.0/6000.0);
			frame->filename = cc->stream_filename;
			frame->data_format = format;
			frame->data_encoding = UFBX_CACHE_DATA_ENCODING_BIG_ENDIAN;
			frame->data_offset = cc->file_offset;
			frame->data_count = count;
			frame->data_element_bytes = elem_size;
			frame->data_total_bytes = total_size;
			frame->file_format = UFBX_CACHE_FILE_FORMAT_MC;

			uint64_t end = begin + ((size + alignment - 1) & ~(uint64_t)(alignment - 1));
			ufbxi_check_err(&cc->error, end >= cc->file_offset);
			uint64_t left = end - cc->file_offset;
			ufbxi_check_err(&cc->error, ufbxi_cache_skip(cc, left));
		}
	}

	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_load_pc2(ufbxi_cache_context *cc)
{
	char header[32];
	ufbxi_check_err(&cc->error, ufbxi_cache_read(cc, header, sizeof(header), false));

	uint32_t version = ufbxi_read_u32(header + 12);
	uint32_t num_points = ufbxi_read_u32(header + 16);
	double start_frame = ufbxi_read_f32(header + 20);
	double frames_per_sample = ufbxi_read_f32(header + 24);
	uint32_t num_samples = ufbxi_read_u32(header + 28);

	(void)version;

	ufbx_cache_frame *frames = ufbxi_push_zero(&cc->tmp_stack, ufbx_cache_frame, num_samples);
	ufbxi_check_err(&cc->error, frames);

	uint64_t total_points = (uint64_t)num_points * (uint64_t)num_samples;
	ufbxi_check_err(&cc->error, total_points < UINT64_MAX / 12);

	uint64_t offset = cc->file_offset;

	// Skip almost to the end of the data and try to read one byte as there's
	// nothing after the data so we can't detect EOF..
	if (total_points > 0) {
		char last_byte[1];
		ufbxi_check_err(&cc->error, ufbxi_cache_skip(cc, total_points * 12 - 1));
		ufbxi_check_err(&cc->error, ufbxi_cache_read(cc, last_byte, 1, false));
	}

	for (uint32_t i = 0; i < num_samples; i++) {
		ufbx_cache_frame *frame = &frames[i];

		double sample_frame = start_frame + (double)i * frames_per_sample;
		frame->channel = cc->channel_name;
		frame->time = sample_frame / cc->frames_per_second;
		frame->filename = cc->stream_filename;
		frame->data_format = UFBX_CACHE_DATA_VEC3_FLOAT;
		frame->data_encoding = UFBX_CACHE_DATA_ENCODING_LITTLE_ENDIAN;
		frame->data_offset = offset;
		frame->data_count = num_points;
		frame->data_element_bytes = 12;
		frame->data_total_bytes = num_points * 12;
		frame->file_format = UFBX_CACHE_FILE_FORMAT_PC2;
		offset += num_points * 12;
	}

	return 1;
}

static ufbxi_noinline int ufbxi_cache_sort_tmp_channels(ufbxi_cache_context *cc, ufbxi_cache_tmp_channel *channels, size_t count)
{
	ufbxi_check_err(&cc->error, ufbxi_grow_array(cc->ator_tmp, &cc->tmp_arr, &cc->tmp_arr_size, count * sizeof(ufbxi_cache_tmp_channel)));
	ufbxi_macro_stable_sort(ufbxi_cache_tmp_channel, 16, channels, cc->tmp_arr, count,
		( ufbxi_str_less(a->name, b->name) ));
	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_load_xml_imp(ufbxi_cache_context *cc, ufbxi_xml_document *doc)
{
	cc->xml_ticks_per_frame = 250;
	cc->xml_filename = cc->stream_filename;

	ufbxi_xml_tag *tag_root = ufbxi_xml_find_child(doc->root, "Autodesk_Cache_File");
	if (tag_root) {
		ufbxi_xml_tag *tag_type = ufbxi_xml_find_child(tag_root, "cacheType");
		ufbxi_xml_tag *tag_fps = ufbxi_xml_find_child(tag_root, "cacheTimePerFrame");
		ufbxi_xml_tag *tag_channels = ufbxi_xml_find_child(tag_root, "Channels");

		size_t num_extra = 0;
		ufbxi_for(ufbxi_xml_tag, tag, tag_root->children, tag_root->num_children) {
			if (tag->num_children != 1) continue;
			if (strcmp(tag->name.data, "extra") != 0) continue;
			ufbx_string *extra = ufbxi_push(&cc->tmp_stack, ufbx_string, 1);
			ufbxi_check_err(&cc->error, extra);
			*extra = tag->children[0].text;
			ufbxi_check_err(&cc->error, ufbxi_push_string_place_str(&cc->string_pool, extra));
			num_extra++;
		}
		cc->cache.extra_info.count = num_extra;
		cc->cache.extra_info.data = ufbxi_push_pop(&cc->result, &cc->tmp_stack, ufbx_string, num_extra);
		ufbxi_check_err(&cc->error, cc->cache.extra_info.data);

		if (tag_type) {
			ufbxi_xml_attrib *type = ufbxi_xml_find_attrib(tag_type, "Type");
			ufbxi_xml_attrib *format = ufbxi_xml_find_attrib(tag_type, "Format");
			if (type) {
				if (!strcmp(type->value.data, "OneFilePerFrame")) {
					cc->xml_type = UFBXI_CACHE_XML_TYPE_FILE_PER_FRAME;
				} else if (!strcmp(type->value.data, "OneFile")) {
					cc->xml_type = UFBXI_CACHE_XML_TYPE_SINGLE_FILE;
				}
			}
			if (format) {
				if (!strcmp(format->value.data, "mcc")) {
					cc->xml_format = UFBXI_CACHE_XML_FORMAT_MCC;
				} else if (!strcmp(format->value.data, "mcx")) {
					cc->xml_format = UFBXI_CACHE_XML_FORMAT_MCX;
				}
			}
		}

		if (tag_fps) {
			ufbxi_xml_attrib *fps = ufbxi_xml_find_attrib(tag_fps, "TimePerFrame");
			if (fps) {
				int value = atoi(fps->value.data);
				if (value > 0) {
					cc->xml_ticks_per_frame = (uint32_t)value;
				}
			}
		}

		if (tag_channels) {
			cc->channels = ufbxi_push_zero(&cc->tmp, ufbxi_cache_tmp_channel, tag_channels->num_children);
			ufbxi_check_err(&cc->error, cc->channels);

			ufbxi_for(ufbxi_xml_tag, tag, tag_channels->children, tag_channels->num_children) {
				ufbxi_xml_attrib *name = ufbxi_xml_find_attrib(tag, "ChannelName");
				ufbxi_xml_attrib *type = ufbxi_xml_find_attrib(tag, "ChannelType");
				ufbxi_xml_attrib *interpretation = ufbxi_xml_find_attrib(tag, "ChannelInterpretation");
				if (!(name && type && interpretation)) continue;

				ufbxi_cache_tmp_channel *channel = &cc->channels[cc->num_channels++];
				channel->name = name->value;
				channel->interpretation = interpretation->value;
				ufbxi_check_err(&cc->error, ufbxi_push_string_place_str(&cc->string_pool, &channel->name));
				ufbxi_check_err(&cc->error, ufbxi_push_string_place_str(&cc->string_pool, &channel->interpretation));

				ufbxi_xml_attrib *sampling_rate = ufbxi_xml_find_attrib(tag, "SamplingRate");
				ufbxi_xml_attrib *start_time = ufbxi_xml_find_attrib(tag, "StartTime");
				ufbxi_xml_attrib *end_time = ufbxi_xml_find_attrib(tag, "EndTime");
				if (sampling_rate && start_time && end_time) {
					channel->sample_rate = (uint32_t)atoi(sampling_rate->value.data);
					channel->start_time = (uint32_t)atoi(start_time->value.data);
					channel->end_time = (uint32_t)atoi(end_time->value.data);
					channel->current_time = channel->start_time;
					channel->try_load = true;
				}
			}
		}
	}

	ufbxi_check_err(&cc->error, ufbxi_cache_sort_tmp_channels(cc, cc->channels, cc->num_channels));
	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_load_xml(ufbxi_cache_context *cc)
{
	ufbxi_xml_load_opts opts = { 0 };
	opts.ator = cc->ator_tmp;
	opts.read_fn = cc->stream.read_fn;
	opts.read_user = cc->stream.user;
	opts.prefix = cc->pos;
	opts.prefix_length = cc->pos_end - cc->pos;
	ufbxi_xml_document *doc = ufbxi_load_xml(&opts, &cc->error);
	ufbxi_check_err(&cc->error, doc);

	int xml_ok = ufbxi_cache_load_xml_imp(cc, doc);
	ufbxi_free_xml(doc);
	ufbxi_check_err(&cc->error, xml_ok);

	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_load_file(ufbxi_cache_context *cc, ufbx_string filename)
{
	cc->stream_filename = filename;
	ufbxi_check_err(&cc->error, ufbxi_push_string_place_str(&cc->string_pool, &cc->stream_filename));

	// Assume all files have at least 16 bytes of header
	size_t magic_len = cc->stream.read_fn(cc->stream.user, cc->buffer, 16);
	ufbxi_check_err_msg(&cc->error, magic_len <= 16, "IO error");
	ufbxi_check_err_msg(&cc->error, magic_len == 16, "Truncated file");
	cc->pos = cc->buffer;
	cc->pos_end = cc->buffer + 16;

	cc->file_offset = 0;

	if (!memcmp(cc->buffer, "POINTCACHE2", 11)) {
		ufbxi_check_err(&cc->error, ufbxi_cache_load_pc2(cc));
	} else if (!memcmp(cc->buffer, "FOR4", 4) || !memcmp(cc->buffer, "FOR8", 4)) {
		ufbxi_check_err(&cc->error, ufbxi_cache_load_mc(cc));
	} else {
		ufbxi_check_err(&cc->error, ufbxi_cache_load_xml(cc));
	}

	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_try_open_file(ufbxi_cache_context *cc, ufbx_string filename, bool *p_found)
{
	memset(&cc->stream, 0, sizeof(cc->stream));
#if defined(UFBX_REGRESSION)
	ufbx_assert(strlen(filename.data) == filename.length);
#endif
	if (!cc->open_file_fn(cc->open_file_user, &cc->stream, filename.data, filename.length)) {
		return 1;
	}

	int ok = ufbxi_cache_load_file(cc, filename);
	*p_found = true;

	if (cc->stream.close_fn) {
		cc->stream.close_fn(cc->stream.user);
	}

	return ok;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_cache_load_frame_files(ufbxi_cache_context *cc)
{
	if (cc->xml_filename.length == 0) return 1;

	const char *extension = NULL;
	switch (cc->xml_format) {
	case UFBXI_CACHE_XML_FORMAT_MCC: extension = "mc"; break;
	case UFBXI_CACHE_XML_FORMAT_MCX: extension = "mcx"; break;
	default: return 1;
	}

	// Ensure worst case space for `path/filenameFrame123Tick456.mcx`
	size_t name_buf_len = cc->xml_filename.length + 64;
	char *name_buf = ufbxi_push(&cc->tmp, char, name_buf_len);
	ufbxi_check_err(&cc->error, name_buf);

	// Find the prefix before `.xml`
	size_t prefix_len = cc->xml_filename.length;
	for (size_t i = prefix_len; i > 0; --i) {
		if (cc->xml_filename.data[i - 1] == '.') {
			prefix_len = i - 1;
			break;
		}
	}
	memcpy(name_buf, cc->xml_filename.data, prefix_len);

	char *suffix_data = name_buf + prefix_len;
	size_t suffix_len = name_buf_len - prefix_len;

	ufbx_string filename;
	filename.data = name_buf;

	if (cc->xml_type == UFBXI_CACHE_XML_TYPE_SINGLE_FILE) {
		filename.length = prefix_len + (size_t)snprintf(suffix_data, suffix_len, ".%s", extension);
		bool found = false;
		ufbxi_check_err(&cc->error, ufbxi_cache_try_open_file(cc, filename, &found));
	} else if (cc->xml_type == UFBXI_CACHE_XML_TYPE_FILE_PER_FRAME) {
		uint32_t lowest_time = 0;
		for (;;) {
			// Find the first `time >= lowest_time` value that has data in some channel
			uint32_t time = UINT32_MAX;
			ufbxi_for(ufbxi_cache_tmp_channel, chan, cc->channels, cc->num_channels) {
				if (!chan->try_load || chan->conescutive_fails > 10) continue;
				uint32_t sample_rate = chan->sample_rate ? chan->sample_rate : cc->xml_ticks_per_frame;
				if (chan->current_time < lowest_time) {
					uint32_t delta = (lowest_time - chan->current_time - 1) / sample_rate;
					chan->current_time += delta * sample_rate;
					if (UINT32_MAX - chan->current_time >= sample_rate) {
						chan->current_time += sample_rate;
					} else {
						chan->try_load = false;
						continue;
					}
				}
				if (chan->current_time <= chan->end_time) {
					time = ufbxi_min32(time, chan->current_time);
				}
			}
			if (time == UINT32_MAX) break;

			// Try to load a file at the specified frame/tick
			uint32_t frame = time / cc->xml_ticks_per_frame;
			uint32_t tick = time % cc->xml_ticks_per_frame;
			if (tick == 0) {
				filename.length = prefix_len + (size_t)snprintf(suffix_data, suffix_len, "Frame%u.%s", frame, extension);
			} else {
				filename.length = prefix_len + (size_t)snprintf(suffix_data, suffix_len, "Frame%uTick%u.%s", frame, tick, extension);
			}
			bool found = false;
			ufbxi_check_err(&cc->error, ufbxi_cache_try_open_file(cc, filename, &found));

			// Update channel status
			ufbxi_for(ufbxi_cache_tmp_channel, chan, cc->channels, cc->num_channels) {
				if (chan->current_time == time) {
					chan->conescutive_fails = found ? 0 : chan->conescutive_fails + 1;
				}
			}

			lowest_time = time + 1;
		}
	}

	return 1;
}

static ufbxi_forceinline bool ufbxi_cmp_cache_frame_less(const ufbx_cache_frame *a, const ufbx_cache_frame *b)
{
	if (a->channel.data != b->channel.data) {
#if defined(UFBX_REGRESSION)
		// Channel names should be interned
		ufbx_assert(!ufbxi_str_equal(a->channel, b->channel));
#endif
		return ufbxi_str_less(a->channel, b->channel);
	}
	return a->time < b->time;
}

static ufbxi_noinline int ufbxi_cache_sort_frames(ufbxi_cache_context *cc, ufbx_cache_frame *frames, size_t count)
{
	ufbxi_check_err(&cc->error, ufbxi_grow_array(cc->ator_tmp, &cc->tmp_arr, &cc->tmp_arr_size, count * sizeof(ufbx_cache_frame)));
	ufbxi_macro_stable_sort(ufbx_cache_frame, 16, frames, cc->tmp_arr, count,
		( ufbxi_cmp_cache_frame_less(a, b) ));
	return 1;
}

typedef struct {
	ufbx_cache_interpretation interpretation;
	const char *name;
} ufbxi_cache_interpretation_name;

static const ufbxi_cache_interpretation_name ufbxi_cache_interpretation_names[] = {
	{ UFBX_CACHE_INTERPRETATION_VERTEX_POSITION, "positions" },
	{ UFBX_CACHE_INTERPRETATION_VERTEX_NORMAL, "normals" },
};

static ufbxi_noinline int ufbxi_cache_setup_channels(ufbxi_cache_context *cc)
{
	ufbxi_cache_tmp_channel *tmp_chan = cc->channels, *tmp_end = tmp_chan + cc->num_channels;

	size_t begin = 0, num_channels = 0;
	while (begin < cc->cache.frames.count) {
		ufbx_cache_frame *frame = &cc->cache.frames.data[begin];
		size_t end = begin + 1;
		while (end < cc->cache.frames.count && cc->cache.frames.data[end].channel.data == frame->channel.data) {
			end++;
		}

		ufbx_cache_channel *chan = ufbxi_push_zero(&cc->tmp_stack, ufbx_cache_channel, 1);
		ufbxi_check_err(&cc->error, chan);

		chan->name = frame->channel;
		chan->interpretation_name = ufbx_empty_string;
		chan->frames.data = frame;
		chan->frames.count = end - begin;

		while (tmp_chan < tmp_end && ufbxi_str_less(tmp_chan->name, chan->name)) {
			tmp_chan++;
		}
		if (tmp_chan < tmp_end && ufbxi_str_equal(tmp_chan->name, chan->name)) {
			chan->interpretation_name = tmp_chan->interpretation;
		}

		if (frame->file_format == UFBX_CACHE_FILE_FORMAT_PC2) {
			chan->interpretation = UFBX_CACHE_INTERPRETATION_VERTEX_POSITION;
		} else {
			ufbxi_for(const ufbxi_cache_interpretation_name, name, ufbxi_cache_interpretation_names, ufbxi_arraycount(ufbxi_cache_interpretation_names)) {
				if (!strcmp(chan->interpretation_name.data, name->name)) {
					chan->interpretation = name->interpretation;
					break;
				}
			}
		}

		num_channels++;
		begin = end;
	}

	cc->cache.channels.data = ufbxi_push_pop(&cc->result, &cc->tmp_stack, ufbx_cache_channel, num_channels);
	ufbxi_check_err(&cc->error, cc->cache.channels.data);
	cc->cache.channels.count = num_channels;

	return 1;
}

static ufbxi_noinline int ufbxi_cache_load_imp(ufbxi_cache_context *cc, ufbx_string filename)
{
	// `ufbx_geometry_cache_opts` must be cleared to zero first!
	ufbx_assert(cc->opts._begin_zero == 0 && cc->opts._end_zero == 0);
	ufbxi_check_err_msg(&cc->error, cc->opts._begin_zero == 0 && cc->opts._end_zero == 0, "Uninitialized options");

	cc->tmp.ator = cc->ator_tmp;
	cc->tmp_stack.ator = cc->ator_tmp;

	cc->channel_name.data = ufbxi_empty_char;

	if (!cc->open_file_fn) {
		cc->open_file_fn = ufbx_open_file;
	}

	// Make sure the filename we pass to `open_file_fn()` is NULL-terminated
	char *filename_data = ufbxi_push(&cc->tmp, char, filename.length + 1);
	ufbxi_check_err(&cc->error, filename_data);
	memcpy(filename_data, filename.data, filename.length);
	filename_data[filename.length] = '\0';
	ufbx_string filename_copy = { filename_data, filename.length };

	// TODO: NULL termination!
	bool found = false;
	ufbxi_check_err(&cc->error, ufbxi_cache_try_open_file(cc, filename_copy, &found));
	if (!found) {
		ufbxi_fail_err_msg(&cc->error, "open_file_fn()", "File not found");
	}

	cc->cache.root_filename = cc->stream_filename;

	ufbxi_check_err(&cc->error, ufbxi_cache_load_frame_files(cc));

	size_t num_frames = cc->tmp_stack.num_items;
	cc->cache.frames.count = num_frames;
	cc->cache.frames.data = ufbxi_push_pop(&cc->result, &cc->tmp_stack, ufbx_cache_frame, num_frames);
	ufbxi_check_err(&cc->error, cc->cache.frames.data);

	ufbxi_check_err(&cc->error, ufbxi_cache_sort_frames(cc, cc->cache.frames.data, cc->cache.frames.count));
	ufbxi_check_err(&cc->error, ufbxi_cache_setup_channels(cc));

	// Must be last allocation!
	cc->imp = ufbxi_push(&cc->result, ufbxi_geometry_cache_imp, 1);
	ufbxi_check_err(&cc->error, cc->imp);

	cc->imp->cache = cc->cache;
	cc->imp->magic = UFBXI_CACHE_IMP_MAGIC;
	cc->imp->owned_by_scene = cc->owned_by_scene;
	cc->imp->ator = cc->ator_result;
	cc->imp->result_buf = cc->result;
	cc->imp->result_buf.ator = &cc->imp->ator;
	cc->imp->string_buf = cc->string_pool.buf;
	cc->imp->string_buf.ator = &cc->imp->ator;

	return 1;
}

ufbxi_noinline static ufbx_geometry_cache *ufbxi_cache_load(ufbxi_cache_context *cc, ufbx_string filename)
{
	int ok = ufbxi_cache_load_imp(cc, filename);

	ufbxi_buf_free(&cc->tmp);
	ufbxi_buf_free(&cc->tmp_stack);
	ufbxi_free(cc->ator_tmp, char, cc->name_buf, cc->name_cap);
	ufbxi_free(cc->ator_tmp, char, cc->tmp_arr, cc->tmp_arr_size);
	if (!cc->owned_by_scene) {
		ufbxi_map_free(&cc->string_pool.map);
		ufbxi_free_ator(cc->ator_tmp);
	}

	if (ok) {
		return &cc->imp->cache;
	} else {
		ufbxi_fix_error_type(&cc->error, "Failed to load geometry cache");
		if (!cc->owned_by_scene) {
			ufbxi_buf_free(&cc->string_pool.buf);
			ufbxi_free_ator(&cc->ator_result);
		}
		return NULL;
	}
}

ufbxi_noinline static ufbx_geometry_cache *ufbxi_load_geometry_cache(ufbx_string filename, const ufbx_geometry_cache_opts *user_opts, ufbx_error *p_error)
{
	ufbx_geometry_cache_opts opts;
	if (user_opts) {
		opts = *user_opts;
	} else {
		memset(&opts, 0, sizeof(opts));
	}

	ufbxi_cache_context cc = { UFBX_ERROR_NONE };
	ufbxi_allocator ator_tmp = { 0 };
	ufbxi_init_ator(&cc.error, &ator_tmp, &opts.temp_allocator);
	ufbxi_init_ator(&cc.error, &cc.ator_result, &opts.result_allocator);
	cc.ator_tmp = &ator_tmp;

	cc.opts = opts;

	cc.open_file_fn = opts.open_file_fn;
	cc.open_file_user = opts.open_file_user;

	cc.string_pool.error = &cc.error;
	ufbxi_map_init(&cc.string_pool.map, cc.ator_tmp, &ufbxi_map_cmp_string, NULL);
	cc.string_pool.buf.ator = &cc.ator_result;
	cc.string_pool.buf.unordered = true;
	cc.string_pool.initial_size = 64;
	cc.result.ator = &cc.ator_result;

	cc.frames_per_second = opts.frames_per_second > 0.0 ? opts.frames_per_second : 30.0;

	ufbx_geometry_cache *cache = ufbxi_cache_load(&cc, filename);
	if (p_error) {
		if (cache) {
			p_error->type = UFBX_ERROR_NONE;
			p_error->description = NULL;
			p_error->stack_size = 0;
		} else {
			*p_error = cc.error;
		}
	}
	return cache;
}

// -- External files

typedef enum {
	UFBXI_EXTERNAL_FILE_GEOMETRY_CACHE,
} ufbxi_external_file_type;

typedef struct {
	ufbxi_external_file_type type;
	ufbx_string filename;
	ufbx_string absolute_filename;
	size_t index;
	void *data;
	size_t data_size;
} ufbxi_external_file;

static int ufbxi_cmp_external_file(const void *va, const void *vb)
{
	const ufbxi_external_file *a = (const ufbxi_external_file*)va, *b = (const ufbxi_external_file*)vb;
	if (a->type != b->type) return a->type < b->type ? -1 : 1;
	int cmp = ufbxi_str_cmp(a->filename, b->filename);
	if (cmp != 0) return cmp;
	if (a->index != b->index) return a->index < b->index ? -1 : 1;
	return 0;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_load_external_cache(ufbxi_context *uc, ufbxi_external_file *file)
{
	ufbxi_cache_context cc = { UFBX_ERROR_NONE };
	cc.owned_by_scene = true;

	cc.open_file_fn = uc->opts.open_file_fn;
	cc.open_file_user = uc->opts.open_file_user;
	cc.frames_per_second = uc->scene.settings.frames_per_second;

	// Temporarily "borrow" allocators for the geometry cache
	cc.ator_tmp = &uc->ator_tmp;
	cc.string_pool = uc->string_pool;
	cc.result = uc->result;

	ufbx_geometry_cache *cache = ufbxi_cache_load(&cc, file->filename);
	if (!cache) {
		if (cc.error.type == UFBX_ERROR_FILE_NOT_FOUND) {
			memset(&cc.error, 0, sizeof(cc.error));
			cache = ufbxi_cache_load(&cc, file->absolute_filename);
		}
	}

	// Return the "borrowed" allocators
	uc->string_pool = cc.string_pool;
	uc->result = cc.result;

	if (!cache) {
		uc->error = cc.error;
		return 0;
	}

	file->data = cache;
	return 1;
}

static ufbxi_noinline ufbxi_external_file *ufbxi_find_external_file(ufbxi_external_file *files, size_t num_files, ufbxi_external_file_type type, const char *name)
{
	size_t ix = SIZE_MAX;
	ufbxi_macro_lower_bound_eq(ufbxi_external_file, 32, &ix, files, 0, num_files,
		( type != a->type ? type < a->type : strcmp(a->filename.data, name) < 0 ),
		( a->type == type && a->filename.data == name ));
	return ix != SIZE_MAX ? &files[ix] : NULL;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_load_external_files(ufbxi_context *uc)
{
	size_t num_files = 0;

	// Gather external files to deduplicate them
	ufbxi_for_ptr_list(ufbx_cache_file, p_cache, uc->scene.cache_files) {
		ufbx_cache_file *cache = *p_cache;
		if (cache->filename.length > 0) {
			ufbxi_external_file *file = ufbxi_push_zero(&uc->tmp_stack, ufbxi_external_file, 1);
			ufbxi_check(file);
			file->index = num_files++;
			file->type = UFBXI_EXTERNAL_FILE_GEOMETRY_CACHE;
			file->filename = cache->filename;
			file->absolute_filename = cache->absolute_filename;
		}
	}

	// Sort and load the external files
	ufbxi_external_file *files = ufbxi_push_pop(&uc->tmp, &uc->tmp_stack, ufbxi_external_file, num_files);
	ufbxi_check(files);
	qsort(files, num_files, sizeof(ufbxi_external_file), &ufbxi_cmp_external_file);

	ufbxi_external_file_type prev_type = UFBXI_EXTERNAL_FILE_GEOMETRY_CACHE;
	const char *prev_name = NULL;
	ufbxi_for(ufbxi_external_file, file, files, num_files) {
		if (file->filename.data == prev_name && file->type == prev_type) continue;
		if (file->type == UFBXI_EXTERNAL_FILE_GEOMETRY_CACHE) {
			ufbxi_check(ufbxi_load_external_cache(uc, file));
		}
		prev_name = file->filename.data;
		prev_type = file->type;
	}

	// Patch the loaded files
	ufbxi_for_ptr_list(ufbx_cache_file, p_cache, uc->scene.cache_files) {
		ufbx_cache_file *cache = *p_cache;
		ufbxi_external_file *file = ufbxi_find_external_file(files, num_files,
			UFBXI_EXTERNAL_FILE_GEOMETRY_CACHE, cache->filename.data);
		if (file && file->data) {
			cache->external_cache = (ufbx_geometry_cache*)file->data;
		}
	}

	// Patch the geometry deformers
	ufbxi_for_ptr_list(ufbx_cache_deformer, p_deformer, uc->scene.cache_deformers) {
		ufbx_cache_deformer *deformer = *p_deformer;
		if (!deformer->file || !deformer->file->external_cache) continue;
		ufbx_geometry_cache *cache = deformer->file->external_cache;
		ufbx_string channel = deformer->channel;
		deformer->external_cache = cache;
		size_t ix = SIZE_MAX;
		ufbxi_macro_lower_bound_eq(ufbx_cache_channel, 16, &ix, cache->channels.data, 0, cache->channels.count,
			( ufbxi_str_less(a->name, channel) ), ( a->name.data == channel.data ));
		if (ix != SIZE_MAX) {
			deformer->external_channel = &cache->channels.data[ix];
		}
	}

	return 1;
}

static ufbxi_noinline void ufbxi_transform_to_axes(ufbxi_context *uc, ufbx_coordinate_axes dst_axes)
{
	if (!ufbx_coordinate_axes_valid(uc->scene.settings.axes)) return;
	ufbx_coordinate_axes src_axes = uc->scene.settings.axes;

	uint32_t src_x = (uint32_t)src_axes.right;
	uint32_t dst_x = (uint32_t)dst_axes.right;
	uint32_t src_y = (uint32_t)src_axes.up;
	uint32_t dst_y = (uint32_t)dst_axes.up;
	uint32_t src_z = (uint32_t)src_axes.front;
	uint32_t dst_z = (uint32_t)dst_axes.front;

	if (src_x == dst_x && src_y == dst_y && src_z == dst_z) return;

	ufbx_matrix axis_mat = { 0 };

	// Remap axes (axis enum divided by 2) potentially flipping if the signs (enum parity) doesn't match
	axis_mat.cols[src_x >> 1].v[dst_x >> 1] = ((src_x ^ dst_x) & 1) == 0 ? 1.0f : -1.0f;
	axis_mat.cols[src_y >> 1].v[dst_y >> 1] = ((src_y ^ dst_y) & 1) == 0 ? 1.0f : -1.0f;
	axis_mat.cols[src_z >> 1].v[dst_z >> 1] = ((src_z ^ dst_z) & 1) == 0 ? 1.0f : -1.0f;

	if (!ufbxi_is_transform_identity(uc->scene.root_node->local_transform)) {
		ufbx_matrix root_mat = ufbx_transform_to_matrix(&uc->scene.root_node->local_transform);
		axis_mat = ufbx_matrix_mul(&root_mat, &axis_mat);
	}

	uc->scene.root_node->local_transform = ufbx_matrix_to_transform(&axis_mat);
	uc->scene.root_node->node_to_parent = axis_mat;
}

static ufbxi_noinline void ufbxi_scale_anim_curve(ufbx_anim_curve *curve, ufbx_real scale)
{
	if (!curve) return;
	ufbxi_for_list(ufbx_keyframe, key, curve->keyframes) {
		key->value *= scale;
	}
}

static ufbxi_noinline void ufbxi_scale_anim_value(ufbx_anim_value *value, ufbx_real scale)
{
	if (!value) return;
	ufbxi_scale_anim_curve(value->curves[0], scale);
	ufbxi_scale_anim_curve(value->curves[1], scale);
	ufbxi_scale_anim_curve(value->curves[2], scale);
}

static ufbxi_noinline int ufbxi_scale_units(ufbxi_context *uc, ufbx_real target_meters)
{
	if (uc->scene.settings.unit_meters <= 0.0f) return 1;
	target_meters = ufbxi_round_if_near(ufbxi_pow10_targets, ufbxi_arraycount(ufbxi_pow10_targets), target_meters);

	ufbx_real ratio = uc->scene.settings.unit_meters / target_meters;
	ratio = ufbxi_round_if_near(ufbxi_pow10_targets, ufbxi_arraycount(ufbxi_pow10_targets), ratio);
	if (ratio == 1.0f) return 1;

	uc->scene.root_node->local_transform.scale.x *= ratio;
	uc->scene.root_node->local_transform.scale.y *= ratio;
	uc->scene.root_node->local_transform.scale.z *= ratio;
	uc->scene.root_node->node_to_parent.m00 *= ratio;
	uc->scene.root_node->node_to_parent.m01 *= ratio;
	uc->scene.root_node->node_to_parent.m02 *= ratio;
	uc->scene.root_node->node_to_parent.m10 *= ratio;
	uc->scene.root_node->node_to_parent.m11 *= ratio;
	uc->scene.root_node->node_to_parent.m12 *= ratio;
	uc->scene.root_node->node_to_parent.m20 *= ratio;
	uc->scene.root_node->node_to_parent.m21 *= ratio;
	uc->scene.root_node->node_to_parent.m22 *= ratio;

	// HACK: Pre-fetch `ufbx_node.inherit_type` as we need it multiple times below.
	// This is a bit inconsistent but will get overwritten in `ufbxi_update_node()`.
	if (!uc->opts.no_prop_unit_scaling || !uc->opts.no_anim_curve_unit_scaling) {
		ufbxi_for_ptr_list(ufbx_node, p_node, uc->scene.nodes) {
			ufbx_node *node = *p_node;
			node->inherit_type = (ufbx_inherit_type)ufbxi_find_enum(&node->props, ufbxi_InheritType, UFBX_INHERIT_NORMAL, UFBX_INHERIT_NO_SCALE);
		}
	}

	if (!uc->opts.no_prop_unit_scaling) {
		ufbxi_for_ptr_list(ufbx_node, p_node, uc->scene.nodes) {
			ufbx_node *node = *p_node;
			if (node->inherit_type != UFBX_INHERIT_NO_SCALE) continue;

			// Find only in own properties
			ufbx_props own_props = node->props;
			own_props.defaults = NULL;
			ufbx_prop *prop = ufbxi_find_prop(&own_props, ufbxi_Lcl_Scaling);
			if (prop) {
				prop->value_vec3.x *= ratio;
				prop->value_vec3.y *= ratio;
				prop->value_vec3.z *= ratio;
				prop->value_int = (int64_t)prop->value_vec3.x;
			} else {
				// We need to add a new Lcl Scaling property based on the defaults
				ufbx_props *defaults = node->props.defaults;
				ufbx_vec3 scale = { 1.0f, 1.0f, 1.0f };
				if (defaults) {
					prop = ufbxi_find_prop(defaults, ufbxi_Lcl_Scaling);
					if (prop) {
						scale = prop->value_vec3;
					}
				}

				scale.x *= ratio;
				scale.y *= ratio;
				scale.z *= ratio;

				ufbx_prop new_prop = { 0 };
				new_prop.name.data = ufbxi_Lcl_Scaling;
				new_prop.name.length = sizeof(ufbxi_Lcl_Scaling) - 1;
				new_prop.internal_key = ufbxi_get_name_key(ufbxi_Lcl_Scaling, sizeof(ufbxi_Lcl_Scaling) - 1);
				new_prop.type = UFBX_PROP_SCALING;
				new_prop.flags = UFBX_PROP_FLAG_SYNTHETIC;
				new_prop.value_str = ufbx_empty_string;
				new_prop.value_vec3 = scale;
				new_prop.value_int = (int64_t)new_prop.value_vec3.x;

				size_t new_num_props = node->props.num_props + 1;
				ufbx_prop *props_copy = ufbxi_push(&uc->result, ufbx_prop, new_num_props);
				ufbxi_check(props_copy);

				memcpy(props_copy, node->props.props, node->props.num_props * sizeof(ufbx_prop));
				props_copy[node->props.num_props] = new_prop;

				ufbxi_check(ufbxi_sort_properties(uc, props_copy, new_num_props));

				node->props.props = props_copy;
				node->props.num_props = new_num_props;
			}
		}
	}

	if (!uc->opts.no_anim_curve_unit_scaling) {
		ufbxi_for_ptr_list(ufbx_anim_layer, p_layer, uc->scene.anim_layers) {
			ufbx_anim_layer *layer = *p_layer;
			ufbxi_for_list(ufbx_anim_prop, aprop, layer->anim_props) {
				if (aprop->prop_name.data == ufbxi_Lcl_Scaling) {
					ufbx_element *elem = aprop->element;
					if (elem->type != UFBX_ELEMENT_NODE) continue;
					ufbx_node *node = (ufbx_node*)elem;
					if (node->inherit_type != UFBX_INHERIT_NO_SCALE) continue;
					ufbxi_scale_anim_value(aprop->anim_value, ratio);
				}
			}
		}
	}

	return 1;
}

// -- Curve evaluation

static ufbxi_forceinline double ufbxi_find_cubic_bezier_t(double p1, double p2, double x0)
{
	double p1_3 = p1 * 3.0, p2_3 = p2 * 3.0;
	double a = p1_3 - p2_3 + 1.0;
	double b = p2_3 - p1_3 - p1_3;
	double c = p1_3;

	double a_3 = 3.0*a, b_2 = 2.0*b;
	double t = x0;
	double x1, t2, t3;

	// Manually unroll three iterations of Newton-Rhapson, this is enough
	// for most tangents
	t2 = t*t; t3 = t2*t; x1 = a*t3 + b*t2 + c*t - x0;
	t -= x1 / (a_3*t2 + b_2*t + c);

	t2 = t*t; t3 = t2*t; x1 = a*t3 + b*t2 + c*t - x0;
	t -= x1 / (a_3*t2 + b_2*t + c);

	t2 = t*t; t3 = t2*t; x1 = a*t3 + b*t2 + c*t - x0;
	t -= x1 / (a_3*t2 + b_2*t + c);

	const double eps = 0.00001;
	if (x1 >= -eps && x1 <= eps) return t;

	// Perform more iterations until we reach desired accuracy
	for (size_t i = 0; i < 4; i++) {
		t2 = t*t; t3 = t2*t; x1 = a*t3 + b*t2 + c*t - x0;
		t -= x1 / (a_3*t2 + b_2*t + c);
		if (x1 >= -eps && x1 <= eps) break;
	}
	return t;
}

ufbxi_nodiscard static int ufbxi_evaluate_skinning(ufbx_scene *scene, ufbx_error *error, ufbxi_buf *buf_result, ufbxi_buf *buf_tmp,
	double time, bool load_caches, ufbx_geometry_cache_data_opts *cache_opts)
{
	size_t max_skinned_indices = 0;

	ufbxi_for_ptr_list(ufbx_mesh, p_mesh, scene->meshes) {
		ufbx_mesh *mesh = *p_mesh;
		if (mesh->blend_deformers.count == 0 && mesh->skin_deformers.count == 0 && (mesh->cache_deformers.count == 0 || !load_caches)) continue;
		max_skinned_indices = ufbxi_max_sz(max_skinned_indices, mesh->num_indices);
	}

	ufbx_topo_edge *topo = ufbxi_push(buf_tmp, ufbx_topo_edge, max_skinned_indices);
	ufbxi_check_err(error, topo);

	ufbxi_for_ptr_list(ufbx_mesh, p_mesh, scene->meshes) {
		ufbx_mesh *mesh = *p_mesh;
		if (mesh->blend_deformers.count == 0 && mesh->skin_deformers.count == 0 && (mesh->cache_deformers.count == 0 || !load_caches)) continue;
		if (mesh->num_vertices == 0) continue;

		size_t num_vertices = mesh->num_vertices;
		ufbx_vec3 *result_pos = ufbxi_push(buf_result, ufbx_vec3, num_vertices + 1);
		ufbxi_check_err(error, result_pos);

		result_pos[0] = ufbx_zero_vec3;
		result_pos++;

		bool cached_position = false, cached_normals = false;
		if (load_caches && mesh->cache_deformers.count > 0) {
			ufbxi_for_ptr_list(ufbx_cache_deformer, p_cache, mesh->cache_deformers) {
				ufbx_cache_channel *channel = (*p_cache)->external_channel;
				if (!channel) continue;

				if (channel->interpretation == UFBX_CACHE_INTERPRETATION_VERTEX_POSITION && !cached_position) {
					size_t num_read = ufbx_sample_geometry_cache_vec3(channel, time, result_pos, num_vertices, cache_opts);
					if (num_read == num_vertices) {
						mesh->skinned_is_local = true;
						cached_position = true;
					}
				} else if (channel->interpretation == UFBX_CACHE_INTERPRETATION_VERTEX_NORMAL && !cached_normals) {
					// TODO: Is this right at all?
					size_t num_normals = mesh->skinned_normal.num_values;
					ufbx_vec3 *normal_data = ufbxi_push(buf_result, ufbx_vec3, num_normals + 1);
					ufbxi_check_err(error, normal_data);
					normal_data[0] = ufbx_zero_vec3;
					normal_data++;

					size_t num_read = ufbx_sample_geometry_cache_vec3(channel, time, normal_data, num_normals, cache_opts);
					if (num_read == num_normals) {
						cached_normals = true;
						mesh->skinned_normal.data = normal_data;
					} else {
						ufbxi_pop(buf_result, ufbx_vec3, num_normals + 1, NULL);
					}
				}
			}
		}

		if (!cached_position) {
			memcpy(result_pos, mesh->vertices, num_vertices * sizeof(ufbx_vec3));

			ufbxi_for_ptr_list(ufbx_blend_deformer, p_blend, mesh->blend_deformers) {
				ufbx_add_blend_vertex_offsets(*p_blend, result_pos, num_vertices, 1.0f);
			}

			// TODO: What should we do about multiple skins??
			if (mesh->skin_deformers.count > 0) {
				ufbx_matrix *fallback = mesh->instances.count > 0 ? &mesh->instances.data[0]->geometry_to_world : NULL;
				ufbx_skin_deformer *skin = mesh->skin_deformers.data[0];
				for (size_t i = 0; i < num_vertices; i++) {
					ufbx_matrix mat = ufbx_get_skin_vertex_matrix(skin, i, fallback);
					result_pos[i] = ufbx_transform_position(&mat, result_pos[i]);
				}

				mesh->skinned_is_local = false;
			}
		}

		mesh->skinned_position.data = result_pos;

		if (!cached_normals) {
			int32_t *normal_indices = ufbxi_push(buf_result, int32_t, mesh->num_indices);
			ufbxi_check_err(error, normal_indices);

			ufbx_compute_topology(mesh, topo);
			size_t num_normals = ufbx_generate_normal_mapping(mesh, topo, normal_indices, false);

			if (num_normals == mesh->num_vertices) {
				mesh->skinned_normal.unique_per_vertex = true;
			}

			ufbx_vec3 *normal_data = ufbxi_push(buf_result, ufbx_vec3, num_normals + 1);
			ufbxi_check_err(error, normal_data);

			normal_data[0] = ufbx_zero_vec3;
			normal_data++;

			ufbx_compute_normals(mesh, &mesh->skinned_position, normal_indices, normal_data, num_normals);

			mesh->skinned_normal.data = normal_data;
			mesh->skinned_normal.indices = normal_indices;
			mesh->skinned_normal.num_values = num_normals;
		}
	}

	return 1;
}

ufbxi_nodiscard static int ufbxi_load_imp(ufbxi_context *uc)
{
	// `ufbx_load_opts` must be cleared to zero first!
	ufbx_assert(uc->opts._begin_zero == 0 && uc->opts._end_zero == 0);
	ufbxi_check_msg(uc->opts._begin_zero == 0 && uc->opts._end_zero == 0, "Uninitialized options");

	ufbxi_check(ufbxi_load_strings(uc));
	ufbxi_check(ufbxi_load_maps(uc));
	ufbxi_check(ufbxi_begin_parse(uc));
	if (uc->version < 6000) {
		ufbxi_check(ufbxi_read_legacy_root(uc));
	} else {
		ufbxi_check(ufbxi_read_root(uc));
	}

	ufbxi_update_scene_metadata(&uc->scene.metadata);
	ufbxi_check(ufbxi_init_file_paths(uc));
	ufbxi_check(ufbxi_finalize_scene(uc));

	ufbxi_update_scene_settings(&uc->scene.settings);

	// Axis conversion
	if (ufbx_coordinate_axes_valid(uc->opts.target_axes)) {
		ufbxi_transform_to_axes(uc, uc->opts.target_axes);
	}

	// Unit conversion
	if (uc->opts.target_unit_meters > 0.0f) {
		ufbxi_check(ufbxi_scale_units(uc, uc->opts.target_unit_meters));
	}

	ufbxi_update_scene(&uc->scene, true);

	if (uc->opts.load_external_files) {
		ufbxi_check(ufbxi_load_external_files(uc));
	}

	// Evaluate skinning if requested
	if (uc->opts.evaluate_skinning) {
		ufbx_geometry_cache_data_opts cache_opts = { 0 };
		cache_opts.open_file_fn = uc->opts.open_file_fn;
		cache_opts.open_file_user = uc->opts.open_file_user;
		ufbxi_check(ufbxi_evaluate_skinning(&uc->scene, &uc->error, &uc->result, &uc->tmp,
			0.0, uc->opts.load_external_files, &cache_opts));
	}

	// Copy local data to the scene
	uc->scene.metadata.version = uc->version;
	uc->scene.metadata.ascii = uc->from_ascii;
	uc->scene.metadata.big_endian = uc->file_big_endian;
	uc->scene.metadata.geometry_ignored = uc->opts.ignore_geometry;
	uc->scene.metadata.animation_ignored = uc->opts.ignore_animation;
	uc->scene.metadata.embedded_ignored = uc->opts.ignore_embedded;

	// Retain the scene, this must be the final allocation as we copy
	// `ator_result` to `ufbx_scene_imp`.
	ufbxi_scene_imp *imp = ufbxi_push(&uc->result, ufbxi_scene_imp, 1);
	ufbxi_check(imp);

	imp->magic = UFBXI_SCENE_IMP_MAGIC;
	imp->scene = uc->scene;
	imp->ator = uc->ator_result;
	imp->ator.error = NULL;

	// Copy retained buffers and translate the allocator struct to the one
	// contained within `ufbxi_scene_imp`
	imp->result_buf = uc->result;
	imp->result_buf.ator = &imp->ator;
	imp->string_buf = uc->string_pool.buf;
	imp->string_buf.ator = &imp->ator;

	imp->scene.metadata.result_memory_used = imp->ator.current_size;
	imp->scene.metadata.temp_memory_used = uc->ator_tmp.current_size;
	imp->scene.metadata.result_allocs = imp->ator.num_allocs;
	imp->scene.metadata.temp_allocs = uc->ator_tmp.num_allocs;

	uc->scene_imp = imp;

	return 1;
}

static ufbxi_noinline void ufbxi_free_temp(ufbxi_context *uc)
{
	ufbxi_map_free(&uc->string_pool.map);

	ufbxi_map_free(&uc->prop_type_map);
	ufbxi_map_free(&uc->fbx_id_map);
	ufbxi_map_free(&uc->fbx_attr_map);
	ufbxi_map_free(&uc->node_prop_set);

	ufbxi_buf_free(&uc->tmp);
	ufbxi_buf_free(&uc->tmp_parse);
	ufbxi_buf_free(&uc->tmp_stack);
	ufbxi_buf_free(&uc->tmp_connections);
	ufbxi_buf_free(&uc->tmp_node_ids);
	ufbxi_buf_free(&uc->tmp_elements);
	ufbxi_buf_free(&uc->tmp_element_offsets);
	for (size_t i = 0; i < UFBX_NUM_ELEMENT_TYPES; i++) {
		ufbxi_buf_free(&uc->tmp_typed_element_offsets[i]);
	}
	ufbxi_buf_free(&uc->tmp_mesh_textures);
	ufbxi_buf_free(&uc->tmp_full_weights);

	ufbxi_free(&uc->ator_tmp, ufbxi_node, uc->top_nodes, uc->top_nodes_cap);
	ufbxi_free(&uc->ator_tmp, void*, uc->element_extra_arr, uc->element_extra_cap);

	ufbxi_free(&uc->ator_tmp, char, uc->ascii.token.str_data, uc->ascii.token.str_cap);
	ufbxi_free(&uc->ator_tmp, char, uc->ascii.prev_token.str_data, uc->ascii.prev_token.str_cap);

	ufbxi_free(&uc->ator_tmp, char, uc->read_buffer, uc->read_buffer_size);
	ufbxi_free(&uc->ator_tmp, char, uc->tmp_arr, uc->tmp_arr_size);
	ufbxi_free(&uc->ator_tmp, char, uc->swap_arr, uc->swap_arr_size);

	ufbxi_free_ator(&uc->ator_tmp);
}

static ufbxi_noinline void ufbxi_free_result(ufbxi_context *uc)
{
	ufbxi_buf_free(&uc->result);
	ufbxi_buf_free(&uc->string_pool.buf);

	ufbxi_free_ator(&uc->ator_result);
}

static ufbx_scene *ufbxi_load(ufbxi_context *uc, const ufbx_load_opts *user_opts, ufbx_error *p_error)
{
	// Test endianness
	{
		uint8_t buf[2];
		uint16_t val = 0xbbaa;
		memcpy(buf, &val, 2);
		uc->local_big_endian = buf[0] == 0xbb;
	}

	if (user_opts) {
		uc->opts = *user_opts;
	} else {
		memset(&uc->opts, 0, sizeof(uc->opts));
	}

	if (uc->opts.file_size_estimate) {
		uc->progress_bytes_total = uc->opts.file_size_estimate;
	}

	if (uc->opts.filename.length == SIZE_MAX) {
		uc->opts.filename.length = uc->opts.filename.data ? strlen(uc->opts.filename.data) : 0;
	}

	ufbx_inflate_retain inflate_retain;
	inflate_retain.initialized = false;

	ufbxi_init_ator(&uc->error, &uc->ator_tmp, &uc->opts.temp_allocator);
	ufbxi_init_ator(&uc->error, &uc->ator_result, &uc->opts.result_allocator);

	if (uc->opts.read_buffer_size == 0) {
		uc->opts.read_buffer_size = 0x4000;
	}

	if (!uc->opts.progress_fn || uc->opts.progress_interval_hint >= SIZE_MAX) {
		uc->progress_interval = SIZE_MAX;
	} else if (uc->opts.progress_interval_hint > 0) {
		uc->progress_interval = (size_t)uc->opts.progress_interval_hint;
	} else {
		uc->progress_interval = 0x4000;
	}

	if (!uc->opts.open_file_fn) {
		uc->opts.open_file_fn = &ufbx_open_file;
	}

	uc->string_pool.error = &uc->error;
	ufbxi_map_init(&uc->string_pool.map, &uc->ator_tmp, &ufbxi_map_cmp_string, NULL);
	uc->string_pool.buf.ator = &uc->ator_result;
	uc->string_pool.buf.unordered = true;
	uc->string_pool.initial_size = 1024;

	ufbxi_map_init(&uc->prop_type_map, &uc->ator_tmp, &ufbxi_map_cmp_const_char_ptr, NULL);
	ufbxi_map_init(&uc->fbx_id_map, &uc->ator_tmp, &ufbxi_map_cmp_uint64, NULL);
	ufbxi_map_init(&uc->fbx_attr_map, &uc->ator_tmp, &ufbxi_map_cmp_uint64, NULL);
	ufbxi_map_init(&uc->node_prop_set, &uc->ator_tmp, &ufbxi_map_cmp_const_char_ptr, NULL);

	uc->tmp.ator = &uc->ator_tmp;
	uc->tmp_parse.ator = &uc->ator_tmp;
	uc->tmp_stack.ator = &uc->ator_tmp;
	uc->tmp_connections.ator = &uc->ator_tmp;
	uc->tmp_node_ids.ator = &uc->ator_tmp;
	uc->tmp_elements.ator = &uc->ator_tmp;
	uc->tmp_element_offsets.ator = &uc->ator_tmp;
	for (size_t i = 0; i < UFBX_NUM_ELEMENT_TYPES; i++) {
		uc->tmp_typed_element_offsets[i].ator = &uc->ator_tmp;
	}
	uc->tmp_mesh_textures.ator = &uc->ator_tmp;
	uc->tmp_full_weights.ator = &uc->ator_tmp;

	uc->result.ator = &uc->ator_result;

	uc->tmp.unordered = true;
	uc->tmp_parse.unordered = true;
	uc->result.unordered = true;

	uc->inflate_retain = &inflate_retain;

	int ok = ufbxi_load_imp(uc);

	ufbxi_free_temp(uc);

	if (uc->close_fn) {
		uc->close_fn(uc->read_user);
	}

	if (ok) {
		if (p_error) {
			p_error->type = UFBX_ERROR_NONE;
			p_error->description = NULL;
			p_error->stack_size = 0;
		}
		return &uc->scene_imp->scene;
	} else {
		ufbxi_fix_error_type(&uc->error, "Failed to load");
		if (p_error) *p_error = uc->error;
		ufbxi_free_result(uc);
		return NULL;
	}
}

// -- TODO: Find a place for these...

ufbx_inline ufbx_vec3 ufbxi_add3(ufbx_vec3 a, ufbx_vec3 b) {
	ufbx_vec3 v = { a.x + b.x, a.y + b.y, a.z + b.z };
	return v;
}

ufbx_inline ufbx_vec3 ufbxi_sub3(ufbx_vec3 a, ufbx_vec3 b) {
	ufbx_vec3 v = { a.x - b.x, a.y - b.y, a.z - b.z };
	return v;
}

ufbx_inline ufbx_vec3 ufbxi_mul3(ufbx_vec3 a, ufbx_real b) {
	ufbx_vec3 v = { a.x * b, a.y * b, a.z * b };
	return v;
}

ufbx_inline ufbx_real ufbxi_dot3(ufbx_vec3 a, ufbx_vec3 b) {
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

ufbx_inline ufbx_real ufbxi_length3(ufbx_vec3 v)
{
	return (ufbx_real)sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

ufbx_inline ufbx_vec3 ufbxi_cross3(ufbx_vec3 a, ufbx_vec3 b) {
	ufbx_vec3 v = { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
	return v;
}

ufbx_inline ufbx_vec3 ufbxi_normalize3(ufbx_vec3 a) {
	ufbx_real len = (ufbx_real)sqrt(ufbxi_dot3(a, a));
	if (len != 0.0) {
		return ufbxi_mul3(a, (ufbx_real)1.0 / len);
	} else {
		ufbx_vec3 zero = { (ufbx_real)0 };
		return zero;
	}
}

// -- Animation evaluation

static int ufbxi_cmp_prop_override(const void *va, const void *vb)
{
	const ufbx_prop_override *a = (const ufbx_prop_override*)va, *b = (const ufbx_prop_override*)vb;
	if (a->element_id != b->element_id) return a->element_id < b->element_id ? -1 : 1;
	if (a->internal_key != b->internal_key) return a->internal_key < b->internal_key ? -1 : 1;
	return strcmp(a->prop_name, b->prop_name);
}

static ufbxi_forceinline bool ufbxi_override_less_than_prop(const ufbx_prop_override *over, uint32_t element_id, const ufbx_prop *prop)
{
	if (over->element_id != element_id) return over->element_id < element_id;
	if (over->internal_key != prop->internal_key) return over->internal_key < prop->internal_key;
	return strcmp(over->prop_name, prop->name.data);
}

static ufbxi_forceinline bool ufbxi_override_equals_to_prop(const ufbx_prop_override *over, uint32_t element_id, const ufbx_prop *prop)
{
	if (over->element_id != element_id) return false;
	if (over->internal_key != prop->internal_key) return false;
	return strcmp(over->prop_name, prop->name.data) == 0;
}

static ufbxi_forceinline ufbxi_unused bool ufbxi_prop_override_is_prepared(const ufbx_prop_override *over)
{
	if (over->internal_key != ufbxi_get_name_key_c(over->prop_name)) return false;
	if (over->value_str == NULL) return false;
	return true;
}

static ufbxi_noinline bool ufbxi_find_prop_override(const ufbx_const_prop_override_list *overrides, uint32_t element_id, ufbx_prop *prop)
{
	if (overrides->count > 0) {
		// If this assert fails make sure to call `ufbx_prepare_prop_overrides()` first!
		ufbx_assert(ufbxi_prop_override_is_prepared(&overrides->data[0]));
	}

	size_t ix = SIZE_MAX;
	ufbxi_macro_lower_bound_eq(ufbx_prop_override, 16, &ix, overrides->data, 0, overrides->count,
		( ufbxi_override_less_than_prop(a, element_id, prop) ),
		( ufbxi_override_equals_to_prop(a, element_id, prop) ));

	if (ix != SIZE_MAX) {
		const ufbx_prop_override *over = &overrides->data[ix];
		const uint32_t clear_flags = UFBX_PROP_FLAG_NO_VALUE | UFBX_PROP_FLAG_NOT_FOUND;
		prop->flags = (ufbx_prop_flags)((prop->flags & ~clear_flags) | UFBX_PROP_FLAG_OVERRIDDEN);
		prop->value_vec3 = over->value;
		prop->value_int = over->value_int;
		prop->value_str = ufbxi_str_c(over->value_str);
		return true;
	} else {
		return false;
	}
}

static ufbxi_noinline ufbx_const_prop_override_list ufbxi_find_element_prop_overrides(const ufbx_const_prop_override_list *overrides, uint32_t element_id)
{
	if (overrides->count > 0) {
		// If this assert fails make sure to call `ufbx_prepare_prop_overrides()` first!
		ufbx_assert(ufbxi_prop_override_is_prepared(&overrides->data[0]));
	}

	size_t begin = overrides->count, end = begin;

	ufbxi_macro_lower_bound_eq(ufbx_prop_override, 32, &begin, overrides->data, 0, overrides->count,
		(a->element_id < element_id),
		(a->element_id == element_id));

	ufbxi_macro_upper_bound_eq(ufbx_prop_override, 32, &end, overrides->data, begin, overrides->count,
		(a->element_id == element_id));

	ufbx_const_prop_override_list result = { overrides->data + begin, end - begin };
	return result;
}

typedef struct ufbxi_anim_layer_combine_ctx {
	ufbx_anim anim;
	const ufbx_element *element;
	double time;
	ufbx_rotation_order rotation_order;
	bool has_rotation_order;
} ufbxi_anim_layer_combine_ctx;

static double ufbxi_pow_abs(double v, double e)
{
	if (e <= 0.0) return 1.0;
	if (e >= 1.0) return v;
	double sign = v < 0.0 ? -1.0 : 1.0;
	return sign * pow(v * sign, e);
}

static ufbxi_noinline void ufbxi_combine_anim_layer(ufbxi_anim_layer_combine_ctx *ctx, ufbx_anim_layer *layer, ufbx_real weight, const char *prop_name, ufbx_vec3 *result, const ufbx_vec3 *value)
{
	if (layer->compose_rotation && layer->blended && prop_name == ufbxi_Lcl_Rotation && !ctx->has_rotation_order) {
		ufbx_prop rp = ufbx_evaluate_prop_len(&ctx->anim, ctx->element, ufbxi_RotationOrder, sizeof(ufbxi_RotationOrder) - 1, ctx->time);
		// NOTE: Defaults to 0 (UFBX_ROTATION_XYZ) gracefully if property is not found
		if (rp.value_int >= 0 && rp.value_int <= UFBX_ROTATION_SPHERIC) {
			ctx->rotation_order = (ufbx_rotation_order)rp.value_int;
		} else {
			ctx->rotation_order = UFBX_ROTATION_XYZ;
		}
		ctx->has_rotation_order = true;
	}

	if (layer->additive) {
		if (layer->compose_scale && prop_name == ufbxi_Lcl_Scaling) {
			result->x *= (ufbx_real)ufbxi_pow_abs(value->x, weight);
			result->y *= (ufbx_real)ufbxi_pow_abs(value->y, weight);
			result->z *= (ufbx_real)ufbxi_pow_abs(value->z, weight);
		} else if (layer->compose_rotation && prop_name == ufbxi_Lcl_Rotation) {
			ufbx_quat a = ufbx_euler_to_quat(*result, ctx->rotation_order);
			ufbx_quat b = ufbx_euler_to_quat(*value, ctx->rotation_order);
			b = ufbx_quat_slerp(ufbx_identity_quat, b, weight);
			ufbx_quat res = ufbx_quat_mul(a, b);
			*result = ufbx_quat_to_euler(res, ctx->rotation_order);
		} else {
			result->x += value->x * weight;
			result->y += value->y * weight;
			result->z += value->z * weight;
		}
	} else if (layer->blended) {
		ufbx_real res_weight = 1.0f - weight;
		if (layer->compose_scale && prop_name == ufbxi_Lcl_Scaling) {
			result->x = (ufbx_real)(ufbxi_pow_abs(result->x, res_weight) * ufbxi_pow_abs(value->x, weight));
			result->y = (ufbx_real)(ufbxi_pow_abs(result->y, res_weight) * ufbxi_pow_abs(value->y, weight));
			result->z = (ufbx_real)(ufbxi_pow_abs(result->z, res_weight) * ufbxi_pow_abs(value->z, weight));
		} else if (layer->compose_rotation && prop_name == ufbxi_Lcl_Rotation) {
			ufbx_quat a = ufbx_euler_to_quat(*result, ctx->rotation_order);
			ufbx_quat b = ufbx_euler_to_quat(*value, ctx->rotation_order);
			ufbx_quat res = ufbx_quat_slerp(a, b, weight);
			*result = ufbx_quat_to_euler(res, ctx->rotation_order);
		} else {
			result->x = result->x * res_weight + value->x * weight;
			result->y = result->y * res_weight + value->y * weight;
			result->z = result->z * res_weight + value->z * weight;
		}
	} else {
		*result = *value;
	}
}

static ufbxi_forceinline bool ufbxi_anim_layer_might_contain_id(const ufbx_anim_layer *layer, uint32_t id)
{
	uint32_t id_mask = ufbxi_arraycount(layer->element_id_bitmask) - 1;
	bool ok = id - layer->min_element_id <= (layer->max_element_id - layer->min_element_id);
	ok &= (layer->element_id_bitmask[(id >> 5) & id_mask] & (1u << (id & 31))) != 0;
	return ok;
}

static ufbxi_noinline void ufbxi_evaluate_props(const ufbx_anim *anim, const ufbx_element *element, double time, ufbx_prop *props, size_t num_props)
{
	ufbxi_anim_layer_combine_ctx combine_ctx = { *anim, element, time };

	uint32_t element_id = element->element_id;
	ufbxi_for_list(const ufbx_anim_layer_desc, layer_desc, anim->layers) {
		ufbx_anim_layer *layer = layer_desc->layer;
		if (!ufbxi_anim_layer_might_contain_id(layer, element_id)) continue;

		// Find the weight for the current layer
		// TODO: Should this be searched from multipler layers?
		// TODO: Use weight from layer_desc
		ufbx_real weight = layer->weight;
		if (layer->weight_is_animated && layer->blended) {
			ufbx_anim_prop *weight_aprop = ufbxi_find_anim_prop_start(layer, &layer->element);
			if (weight_aprop) {
				weight = ufbx_evaluate_anim_value_real(weight_aprop->anim_value, time) / (ufbx_real)100.0;
				if (weight < 0.0f) weight = 0.0f;
				if (weight > 0.99999f) weight = 1.0f;
			}
		}

		ufbx_anim_prop *aprop = ufbxi_find_anim_prop_start(layer, element);
		if (!aprop) continue;

		for (size_t i = 0; i < num_props; i++) {
			ufbx_prop *prop = &props[i];

			// Don't evaluate on top of overridden properties
			if ((prop->flags & UFBX_PROP_FLAG_OVERRIDDEN) != 0) continue;

			// Connections override animation by default
			if ((prop->flags & UFBX_PROP_FLAG_CONNECTED) != 0 && !anim->ignore_connections) continue;

			// Skip until we reach `aprop >= prop`
			while (aprop->element == element && aprop->internal_key < prop->internal_key) aprop++;
			if (aprop->prop_name.data != prop->name.data) {
				while (aprop->element == element && strcmp(aprop->prop_name.data, prop->name.data) < 0) aprop++;
			}

			// TODO: Should we skip the blending for the first layer _per property_
			// This could be done by having `UFBX_PROP_FLAG_ANIMATION_EVALUATED`
			// that gets set for the first layer of animation that is applied.
			if (aprop->prop_name.data == prop->name.data) {
				ufbx_vec3 v = ufbx_evaluate_anim_value_vec3(aprop->anim_value, time);
				if (layer_desc == anim->layers.data) {
					prop->value_vec3 = v;
				} else {
					ufbxi_combine_anim_layer(&combine_ctx, layer, weight, prop->name.data, &prop->value_vec3, &v);
				}
			}
		}
	}

	ufbxi_for(ufbx_prop, prop, props, num_props) {
		prop->value_int = (int64_t)prop->value_real;
	}
}

static ufbxi_noinline ufbx_vec3 ufbxi_evaluate_connected_prop(const ufbx_anim *anim, const ufbx_element *element, const char *name, double time)
{
	ufbx_connection *conn = ufbxi_find_prop_connection(element, name);

	for (size_t i = 0; i < 1000 && conn; i++) {
		ufbx_connection *next_conn = ufbxi_find_prop_connection(conn->src, conn->src_prop.data);
		if (!next_conn) break;
		conn = next_conn;
	}

	ufbx_prop ep = ufbx_evaluate_prop_len(anim, conn->src, conn->src_prop.data, conn->src_prop.length, time);
	return ep.value_vec3;
}

static ufbxi_noinline ufbx_props ufbxi_evaluate_selected_props(const ufbx_anim *anim, const ufbx_element *element, double time, ufbx_prop *props, const char **prop_names, size_t max_props)
{
	const char *name = prop_names[0];
	uint32_t key = ufbxi_get_name_key_c(name);
	size_t num_props = 0;

	const ufbx_prop_override *over = NULL, *over_end = NULL;
	if (anim->prop_overrides.count > 0) {
		ufbx_const_prop_override_list list = ufbxi_find_element_prop_overrides(&anim->prop_overrides, element->element_id);
		over = list.data;
		over_end = over + list.count;
	}

#if defined(UFBX_REGRESSION)
	for (size_t i = 1; i < max_props; i++) {
		ufbx_assert(strcmp(prop_names[i - 1], prop_names[i]) < 0);
	}
#endif

	size_t name_ix = 0;
	for (size_t i = 0; i < element->props.num_props; i++) {
		ufbx_prop *prop = &element->props.props[i];

		while (name_ix < max_props) {
			if (key > prop->internal_key) break;

			if (over) {
				bool found_override = false;
				for (; over != over_end; over++) {
					ufbx_prop *dst = &props[num_props];
					if (over->internal_key < key || strcmp(over->prop_name, name) < 0) {
						continue;
					} else if (over->internal_key == key && strcmp(over->prop_name, name) == 0) {
						dst->name = ufbxi_str_c(name);
						dst->internal_key = key;
						dst->type = UFBX_PROP_UNKNOWN;
						dst->flags = UFBX_PROP_FLAG_OVERRIDDEN;
					} else {
						break;
					}
					dst->value_str = ufbxi_str_c(over->value_str);
					dst->value_int = over->value_int;
					dst->value_vec3 = over->value;
					num_props++;
					found_override = true;
				}
				if (found_override) break;
			}

			if (name == prop->name.data) {
				if (prop->flags & UFBX_PROP_FLAG_ANIMATED) {
					props[num_props++] = *prop;
				} else if ((prop->flags & UFBX_PROP_FLAG_CONNECTED) != 0 && !anim->ignore_connections) {
					ufbx_prop *dst = &props[num_props++];
					*dst = *prop;
					dst->value_vec3 = ufbxi_evaluate_connected_prop(anim, element, name, time);
				}
				break;
			} else if (strcmp(name, prop->name.data) < 0) {
				name_ix++;
				if (name_ix < max_props) {
					name = prop_names[name_ix];
					key = ufbxi_get_name_key_c(name);
				}
			} else {
				break;
			}
		}
	}

	if (over) {
		for (; over != over_end && name_ix < max_props; over++) {
			ufbx_prop *dst = &props[num_props];
			if (over->internal_key < key || strcmp(over->prop_name, name) < 0) {
				continue;
			} else if (over->internal_key == key && strcmp(over->prop_name, name) == 0) {
				dst->name = ufbxi_str_c(name);
				dst->internal_key = key;
				dst->type = UFBX_PROP_UNKNOWN;
				dst->flags = UFBX_PROP_FLAG_OVERRIDDEN;
			} else {
				name_ix++;
				if (name_ix < max_props) {
					name = prop_names[name_ix];
					key = ufbxi_get_name_key_c(name);
				}
			}
			dst->value_str = ufbxi_str_c(over->value_str);
			dst->value_int = over->value_int;
			dst->value_vec3 = over->value;
			num_props++;
		}
	}

	ufbxi_evaluate_props(anim, element, time, props, num_props);

	ufbx_props prop_list;
	prop_list.props = props;
	prop_list.num_props = prop_list.num_animated = num_props;
	prop_list.defaults = (ufbx_props*)&element->props;
	return prop_list;
}

typedef struct {
	char *src_element;
	char *dst_element;

	ufbx_scene src_scene;
	ufbx_evaluate_opts opts;
	ufbx_anim anim;
	double time;

	ufbx_error error;

	// Allocators
	ufbxi_allocator ator_result;
	ufbxi_allocator ator_tmp;

	ufbxi_buf result;
	ufbxi_buf tmp;

	ufbx_scene scene;

	ufbxi_scene_imp *scene_imp;
} ufbxi_eval_context;

static ufbxi_forceinline ufbx_element *ufbxi_translate_element(ufbxi_eval_context *ec, void *elem)
{
	return elem ? (ufbx_element*)(ec->dst_element + ((char*)elem - ec->src_element)) : NULL;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_translate_element_list(ufbxi_eval_context *ec, void *p_list)
{
	ufbx_element_list *list = (ufbx_element_list*)p_list;
	size_t count = list->count;
	ufbx_element **src = list->data;
	ufbx_element **dst = ufbxi_push(&ec->result, ufbx_element*, count);
	ufbxi_check_err(&ec->error, dst);
	list->data = dst;
	for (size_t i = 0; i < count; i++) {
		dst[i] = ufbxi_translate_element(ec, src[i]);
	}
	return 1;
}

ufbxi_nodiscard static int ufbxi_translate_anim(ufbxi_eval_context *ec, ufbx_anim *anim)
{
	ufbx_anim_layer_desc *layers = ufbxi_push(&ec->result, ufbx_anim_layer_desc, anim->layers.count);
	ufbxi_check_err(&ec->error, layers);
	for (size_t i = 0; i < anim->layers.count; i++) {
		layers[i] = anim->layers.data[i];
		layers[i].layer = (ufbx_anim_layer*)ufbxi_translate_element(ec, layers[i].layer);
	}
	anim->layers.data = layers;
	return 1;
}

ufbxi_nodiscard static int ufbxi_evaluate_imp(ufbxi_eval_context *ec)
{
	// `ufbx_evaluate_opts` must be cleared to zero first!
	ufbx_assert(ec->opts._begin_zero == 0 && ec->opts._end_zero == 0);
	ufbxi_check_err_msg(&ec->error, ec->opts._begin_zero == 0 && ec->opts._end_zero == 0, "Uninitialized options");

	ec->scene = ec->src_scene;
	size_t num_elements = ec->scene.elements.count;

	char *element_data = (char*)ufbxi_push(&ec->result, uint64_t, ec->scene.metadata.element_buffer_size/8);
	ufbxi_check_err(&ec->error, element_data);

	ec->scene.elements.data = ufbxi_push(&ec->result, ufbx_element*, num_elements);
	ufbxi_check_err(&ec->error, ec->scene.elements.data);

	ec->src_element = (char*)ec->src_scene.elements.data[0];
	ec->dst_element = element_data;

	for (size_t i = 0; i < UFBX_NUM_ELEMENT_TYPES; i++) {
		ec->scene.elements_by_type[i].data = ufbxi_push(&ec->result, ufbx_element*, ec->scene.elements_by_type[i].count);
		ufbxi_check_err(&ec->error, ec->scene.elements_by_type[i].data);
	}

	size_t num_connections = ec->scene.connections_dst.count;
	ec->scene.connections_src.data = ufbxi_push(&ec->result, ufbx_connection, num_connections);
	ec->scene.connections_dst.data = ufbxi_push(&ec->result, ufbx_connection, num_connections);
	ufbxi_check_err(&ec->error, ec->scene.connections_src.data);
	ufbxi_check_err(&ec->error, ec->scene.connections_dst.data);
	for (size_t i = 0; i < num_connections; i++) {
		ufbx_connection *src = &ec->scene.connections_src.data[i];
		ufbx_connection *dst = &ec->scene.connections_dst.data[i];
		*src = ec->src_scene.connections_src.data[i];
		*dst = ec->src_scene.connections_dst.data[i];
		src->src = ufbxi_translate_element(ec, src->src);
		src->dst = ufbxi_translate_element(ec, src->dst);
		dst->src = ufbxi_translate_element(ec, dst->src);
		dst->dst = ufbxi_translate_element(ec, dst->dst);
	}

	ec->scene.elements_by_name.data = ufbxi_push(&ec->result, ufbx_name_element, num_elements);
	ufbxi_check_err(&ec->error, ec->scene.elements_by_name.data);

	ec->scene.root_node = (ufbx_node*)ufbxi_translate_element(ec, ec->scene.root_node);
	ufbxi_check_err(&ec->error, ufbxi_translate_anim(ec, &ec->scene.anim));
	ufbxi_check_err(&ec->error, ufbxi_translate_anim(ec, &ec->scene.combined_anim));

	for (size_t i = 0; i < num_elements; i++) {
		ufbx_element *src = ec->src_scene.elements.data[i];
		ufbx_element *dst = ufbxi_translate_element(ec, src);
		size_t size = ufbx_element_type_size[src->type];
		ufbx_assert(size > 0);
		memcpy(dst, src, size);

		ec->scene.elements.data[i] = dst;
		ec->scene.elements_by_type[src->type].data[src->typed_id] = dst;

		dst->connections_src.data = ec->scene.connections_src.data + (dst->connections_src.data - ec->src_scene.connections_src.data);
		dst->connections_dst.data = ec->scene.connections_dst.data + (dst->connections_dst.data - ec->src_scene.connections_dst.data);
		if (dst->instances.count > 0) {
			ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &dst->instances));
		}

		ufbx_name_element named = ec->src_scene.elements_by_name.data[i];
		named.element = ufbxi_translate_element(ec, named.element);
		ec->scene.elements_by_name.data[i] = named;
	}

	ufbxi_for_ptr_list(ufbx_node, p_node, ec->scene.nodes) {
		ufbx_node *node = *p_node;
		node->parent = (ufbx_node*)ufbxi_translate_element(ec, node->parent);
		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &node->children));

		node->attrib = ufbxi_translate_element(ec, node->attrib);
		node->mesh = (ufbx_mesh*)ufbxi_translate_element(ec, node->mesh);
		node->light = (ufbx_light*)ufbxi_translate_element(ec, node->light);
		node->camera = (ufbx_camera*)ufbxi_translate_element(ec, node->camera);
		node->bone = (ufbx_bone*)ufbxi_translate_element(ec, node->bone);

		if (node->all_attribs.count > 1) {
			ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &node->all_attribs));
		} else if (node->all_attribs.count == 1) {
			node->all_attribs.data = &node->attrib;
		}
	}

	ufbxi_for_ptr_list(ufbx_mesh, p_mesh, ec->scene.meshes) {
		ufbx_mesh *mesh = *p_mesh;

		ufbx_mesh_material *materials = ufbxi_push(&ec->result, ufbx_mesh_material, mesh->materials.count);
		ufbxi_check_err(&ec->error, materials);
		for (size_t i = 0; i < mesh->materials.count; i++) {
			materials[i] = mesh->materials.data[i];
			materials[i].material = (ufbx_material*)ufbxi_translate_element(ec, materials[i].material);
		}
		mesh->materials.data = materials;

		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &mesh->skin_deformers));
		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &mesh->blend_deformers));
		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &mesh->cache_deformers));
		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &mesh->all_deformers));
	}

	ufbxi_for_ptr_list(ufbx_stereo_camera, p_stereo, ec->scene.stereo_cameras) {
		ufbx_stereo_camera *stereo = *p_stereo;
		stereo->left = (ufbx_camera*)ufbxi_translate_element(ec, stereo->left);
		stereo->right = (ufbx_camera*)ufbxi_translate_element(ec, stereo->right);
	}

	ufbxi_for_ptr_list(ufbx_skin_deformer, p_skin, ec->scene.skin_deformers) {
		ufbx_skin_deformer *skin = *p_skin;
		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &skin->clusters));
	}

	ufbxi_for_ptr_list(ufbx_skin_cluster, p_cluster, ec->scene.skin_clusters) {
		ufbx_skin_cluster *cluster = *p_cluster;
		cluster->bone_node = (ufbx_node*)ufbxi_translate_element(ec, cluster->bone_node);
	}

	ufbxi_for_ptr_list(ufbx_blend_deformer, p_blend, ec->scene.blend_deformers) {
		ufbx_blend_deformer *blend = *p_blend;
		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &blend->channels));
	}

	ufbxi_for_ptr_list(ufbx_blend_channel, p_chan, ec->scene.blend_channels) {
		ufbx_blend_channel *chan = *p_chan;

		ufbx_blend_keyframe *keys = ufbxi_push(&ec->result, ufbx_blend_keyframe, chan->keyframes.count);
		ufbxi_check_err(&ec->error, keys);
		for (size_t i = 0; i < chan->keyframes.count; i++) {
			keys[i] = chan->keyframes.data[i];
			keys[i].shape = (ufbx_blend_shape*)ufbxi_translate_element(ec, keys[i].shape);
		}
		chan->keyframes.data = keys;
	}

	ufbxi_for_ptr_list(ufbx_cache_deformer, p_deformer, ec->scene.cache_deformers) {
		ufbx_cache_deformer *deformer = *p_deformer;
		deformer->file = (ufbx_cache_file*)ufbxi_translate_element(ec, deformer->file);
	}

	ufbxi_for_ptr_list(ufbx_material, p_material, ec->scene.materials) {
		ufbx_material *material = *p_material;

		material->shader = (ufbx_shader*)ufbxi_translate_element(ec, material->shader);
		for (size_t i = 0; i < UFBX_NUM_MATERIAL_FBX_MAPS; i++) {
			ufbx_material_map *map = &material->fbx.maps[i];
			map->texture = (ufbx_texture*)ufbxi_translate_element(ec, map->texture);
		}
		for (size_t i = 0; i < UFBX_NUM_MATERIAL_PBR_MAPS; i++) {
			ufbx_material_map *map = &material->pbr.maps[i];
			map->texture = (ufbx_texture*)ufbxi_translate_element(ec, map->texture);
		}

		ufbx_material_texture *textures = ufbxi_push(&ec->result, ufbx_material_texture, material->textures.count);
		ufbxi_check_err(&ec->error, textures);
		for (size_t i = 0; i < material->textures.count; i++) {
			textures[i] = material->textures.data[i];
			textures[i].texture = (ufbx_texture*)ufbxi_translate_element(ec, textures[i].texture);
		}
		material->textures.data = textures;
	}

	ufbxi_for_ptr_list(ufbx_texture, p_texture, ec->scene.textures) {
		ufbx_texture *texture = *p_texture;
		texture->video = (ufbx_video*)ufbxi_translate_element(ec, texture->video);

		ufbx_texture_layer *layers = ufbxi_push(&ec->result, ufbx_texture_layer, texture->layers.count);
		ufbxi_check_err(&ec->error, layers);
		for (size_t i = 0; i < texture->layers.count; i++) {
			layers[i] = texture->layers.data[i];
			layers[i].texture = (ufbx_texture*)ufbxi_translate_element(ec, layers[i].texture);
		}
		texture->layers.data = layers;
	}

	ufbxi_for_ptr_list(ufbx_shader, p_shader, ec->scene.shaders) {
		ufbx_shader *shader = *p_shader;
		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &shader->bindings));
	}

	ufbxi_for_ptr_list(ufbx_display_layer, p_layer, ec->scene.display_layers) {
		ufbx_display_layer *layer = *p_layer;

		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &layer->nodes));
	}

	ufbxi_for_ptr_list(ufbx_selection_set, p_set, ec->scene.selection_sets) {
		ufbx_selection_set *set = *p_set;

		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &set->nodes));
	}

	ufbxi_for_ptr_list(ufbx_selection_node, p_node, ec->scene.selection_nodes) {
		ufbx_selection_node *node = *p_node;

		node->target_node = (ufbx_node*)ufbxi_translate_element(ec, node->target_node);
		node->target_mesh = (ufbx_mesh*)ufbxi_translate_element(ec, node->target_mesh);
	}

	ufbxi_for_ptr_list(ufbx_constraint, p_constraint, ec->scene.constraints) {
		ufbx_constraint *constraint = *p_constraint;

		constraint->node = (ufbx_node*)ufbxi_translate_element(ec, constraint->node);
		constraint->aim_up_node = (ufbx_node*)ufbxi_translate_element(ec, constraint->aim_up_node);
		constraint->ik_effector = (ufbx_node*)ufbxi_translate_element(ec, constraint->ik_effector);
		constraint->ik_end_node = (ufbx_node*)ufbxi_translate_element(ec, constraint->ik_end_node);

		ufbx_constraint_target *targets = ufbxi_push(&ec->result, ufbx_constraint_target, constraint->targets.count);
		ufbxi_check_err(&ec->error, targets);
		for (size_t i = 0; i < constraint->targets.count; i++) {
			targets[i] = constraint->targets.data[i];
			targets[i].node = (ufbx_node*)ufbxi_translate_element(ec, targets[i].node);
		}
		constraint->targets.data = targets;
	}

	ufbxi_for_ptr_list(ufbx_anim_stack, p_stack, ec->scene.anim_stacks) {
		ufbx_anim_stack *stack = *p_stack;

		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &stack->layers));
		ufbxi_check_err(&ec->error, ufbxi_translate_anim(ec, &stack->anim));
	}

	ufbxi_for_ptr_list(ufbx_anim_layer, p_layer, ec->scene.anim_layers) {
		ufbx_anim_layer *layer = *p_layer;

		ufbxi_check_err(&ec->error, ufbxi_translate_element_list(ec, &layer->anim_values));
		ufbx_anim_prop *props = ufbxi_push(&ec->result, ufbx_anim_prop, layer->anim_props.count);
		ufbxi_check_err(&ec->error, props);
		for (size_t i = 0; i < layer->anim_props.count; i++) {
			props[i] = layer->anim_props.data[i];
			props[i].element = ufbxi_translate_element(ec, props[i].element);
			props[i].anim_value = (ufbx_anim_value*)ufbxi_translate_element(ec, props[i].anim_value);
		}
		layer->anim_props.data = props;
	}

	ufbxi_check_err(&ec->error, ufbxi_translate_anim(ec, &ec->anim));

	ufbxi_for_ptr_list(ufbx_anim_value, p_value, ec->scene.anim_values) {
		ufbx_anim_value *value = *p_value;
		value->curves[0] = (ufbx_anim_curve*)ufbxi_translate_element(ec, value->curves[0]);
		value->curves[1] = (ufbx_anim_curve*)ufbxi_translate_element(ec, value->curves[1]);
		value->curves[2] = (ufbx_anim_curve*)ufbxi_translate_element(ec, value->curves[2]);
	}

	ufbx_anim anim = ec->anim;
	ufbx_const_prop_override_list overrides_left = ec->anim.prop_overrides;

	// Evaluate the properties
	ufbxi_for_ptr_list(ufbx_element, p_elem, ec->scene.elements) {
		ufbx_element *elem = *p_elem;
		size_t num_animated = elem->props.num_animated;

		// Setup the overrides for this element if found
		if (overrides_left.count > 0) {
			if (overrides_left.data[0].element_id <= elem->element_id) {
				anim.prop_overrides = ufbxi_find_element_prop_overrides(&overrides_left, elem->element_id);
				overrides_left.data = anim.prop_overrides.data + anim.prop_overrides.count;
				overrides_left.count = ec->anim.prop_overrides.data + ec->anim.prop_overrides.count - overrides_left.data;
				num_animated += anim.prop_overrides.count;
			}
		}

		if (num_animated == 0) continue;

		ufbx_prop *props = ufbxi_push(&ec->result, ufbx_prop, num_animated);
		ufbxi_check_err(&ec->error, props);

		elem->props = ufbx_evaluate_props(&anim, elem, ec->time, props, num_animated);
		elem->props.defaults = &ec->src_scene.elements.data[elem->element_id]->props;

		anim.prop_overrides.count = 0;
	}

	// Update all derived values
	ufbxi_update_scene(&ec->scene, false);

	// Evaluate skinning if requested
	if (ec->opts.evaluate_skinning) {
		ufbx_geometry_cache_data_opts cache_opts = { 0 };
		cache_opts.open_file_fn = ec->opts.open_file_fn;
		cache_opts.open_file_user = ec->opts.open_file_user;
		ufbxi_check_err(&ec->error, ufbxi_evaluate_skinning(&ec->scene, &ec->error, &ec->result, &ec->tmp,
			ec->time, ec->opts.load_external_files, &cache_opts));
	}

	// Retain the scene, this must be the final allocation as we copy
	// `ator_result` to `ufbx_scene_imp`.
	ufbxi_scene_imp *imp = ufbxi_push_zero(&ec->result, ufbxi_scene_imp, 1);
	ufbxi_check_err(&ec->error, imp);

	imp->magic = UFBXI_SCENE_IMP_MAGIC;
	imp->scene = ec->scene;
	imp->ator = ec->ator_result;
	imp->ator.error = NULL;

	// Copy retained buffers and translate the allocator struct to the one
	// contained within `ufbxi_scene_imp`
	imp->result_buf = ec->result;
	imp->result_buf.ator = &imp->ator;

	imp->scene.metadata.result_memory_used = imp->ator.current_size;
	imp->scene.metadata.temp_memory_used = ec->ator_tmp.current_size;
	imp->scene.metadata.result_allocs = imp->ator.num_allocs;
	imp->scene.metadata.temp_allocs = ec->ator_tmp.num_allocs;

	ec->scene_imp = imp;
	ec->result.ator = &ec->ator_result;

	return 1;
}

ufbxi_nodiscard static ufbx_scene *ufbxi_evaluate_scene(ufbxi_eval_context *ec, ufbx_scene *scene, const ufbx_anim *anim, double time, const ufbx_evaluate_opts *user_opts, ufbx_error *p_error)
{
	if (user_opts) {
		ec->opts = *user_opts;
	} else {
		memset(&ec->opts, 0, sizeof(ec->opts));
	}

	ec->src_scene = *scene;
	ec->anim = anim ? *anim : scene->anim;
	ec->time = time;

	ufbxi_init_ator(&ec->error, &ec->ator_tmp, &ec->opts.temp_allocator);
	ufbxi_init_ator(&ec->error, &ec->ator_result, &ec->opts.result_allocator);

	ec->result.ator = &ec->ator_result;
	ec->tmp.ator = &ec->ator_tmp;

	ec->result.unordered = true;
	ec->tmp.unordered = true;

	if (ufbxi_evaluate_imp(ec)) {
		ufbxi_buf_free(&ec->tmp);
		ufbxi_free_ator(&ec->ator_tmp);
		if (p_error) {
			p_error->type = UFBX_ERROR_NONE;
			p_error->description = NULL;
			p_error->stack_size = 0;
		}
		return &ec->scene_imp->scene;
	} else {
		ufbxi_fix_error_type(&ec->error, "Failed to evaluate");
		if (p_error) *p_error = ec->error;
		ufbxi_buf_free(&ec->tmp);
		ufbxi_buf_free(&ec->result);
		ufbxi_free_ator(&ec->ator_tmp);
		ufbxi_free_ator(&ec->ator_result);
		return NULL;
	}
}

// -- NURBS

typedef struct {
	int32_t x, y, z;
} ufbxi_spatial_key;

typedef struct {
	ufbxi_spatial_key key;
	ufbx_vec3 position;
} ufbxi_spatial_bucket;

typedef struct {
	ufbx_error error;

	ufbx_tessellate_opts opts;

	const ufbx_nurbs_surface *surface;

	ufbxi_allocator ator_tmp;
	ufbxi_allocator ator_result;

	ufbxi_buf tmp;
	ufbxi_buf result;

	ufbxi_map position_map;

	ufbx_mesh mesh;

	ufbxi_mesh_imp *imp;

} ufbxi_tessellate_context;

static ufbxi_forceinline uint32_t ufbxi_hash_spatial_key(ufbxi_spatial_key key)
{
	uint32_t h = 0;
	h = (h<<6) + (h>>2) + 0x9e3779b9 + ufbxi_hash32((uint32_t)key.x);
	h = (h<<6) + (h>>2) + 0x9e3779b9 + ufbxi_hash32((uint32_t)key.y);
	h = (h<<6) + (h>>2) + 0x9e3779b9 + ufbxi_hash32((uint32_t)key.z);
	return h;
}

static int ufbxi_map_cmp_spatial_key(void *user, const void *va, const void *vb)
{
	(void)user;
	ufbxi_spatial_key a = *(const ufbxi_spatial_key*)va, b = *(const ufbxi_spatial_key*)vb;
	if (a.x != b.x) return a.x < b.x ? -1 : +1;
	if (a.y != b.y) return a.y < b.y ? -1 : +1;
	if (a.z != b.z) return a.z < b.z ? -1 : +1;
	return 0;
}

static int32_t ufbxi_noinline ufbxi_spatial_coord(ufbx_real v)
{
	bool negative = false;
	if (v < 0.0f) {
		v = -v;
		negative = true;
	}
	if (!(v < 1.27605887595e+38f)) {
		return INT32_MIN;
	}

	const int32_t min_exponent = -20;
	const int32_t mantissa_bits = 21;
	const int32_t mantissa_max = 1 << mantissa_bits;

	int exponent = 0;
	double mantissa = frexp(v, &exponent);
	if (exponent < min_exponent) {
		return 0;
	} else {
		int32_t biased = (int32_t)(exponent - min_exponent);
		int32_t truncated_mantissa = (int32_t)(mantissa * (double)(mantissa_max*2)) - mantissa_max;
		int32_t value = (biased << mantissa_bits) + truncated_mantissa;
		return negative ? -value : value;
	}
}

static uint32_t ufbxi_noinline ufbxi_insert_spatial_imp(ufbxi_map *map, int32_t kx, int32_t ky, int32_t kz)
{
	ufbxi_spatial_key key = { kx, ky, kz };
	uint32_t hash = ufbxi_hash_spatial_key(key);
	ufbxi_spatial_bucket *bucket = ufbxi_map_find(map, ufbxi_spatial_bucket, hash, &key);
	if (bucket) {
		return (uint32_t)(bucket - (ufbxi_spatial_bucket*)map->items);
	} else {
		return UINT32_MAX;
	}
}

static uint32_t ufbxi_noinline ufbxi_insert_spatial(ufbxi_map *map, const ufbx_vec3 *pos)
{
	int32_t kx = ufbxi_spatial_coord(pos->x);
	int32_t ky = ufbxi_spatial_coord(pos->y);
	int32_t kz = ufbxi_spatial_coord(pos->z);

	uint32_t ix = UINT32_MAX;
	if (kx != INT32_MIN && ky != INT32_MIN && kz != INT32_MIN) {
		const int32_t low_bits = 2, low_mask = (1 << low_bits) - 1;

		kx += low_mask/2;
		ky += low_mask/2;
		kz += low_mask/2;

		int32_t dx = (((kx+1) & low_mask) < 2) ? (((kx >> (low_bits-1)) & 1) ? +1 : -1) : 0;
		int32_t dy = (((ky+1) & low_mask) < 2) ? (((ky >> (low_bits-1)) & 1) ? +1 : -1) : 0;
		int32_t dz = (((kz+1) & low_mask) < 2) ? (((kz >> (low_bits-1)) & 1) ? +1 : -1) : 0;
		int32_t dnum = (dx&1) + (dy&1) + (dz&1);

		kx >>= low_bits;
		ky >>= low_bits;
		kz >>= low_bits;

		ix = ufbxi_insert_spatial_imp(map, kx, ky, kz);

		if (dnum >= 1 && ix == UINT32_MAX) {
			if (dx) ix = ufbxi_insert_spatial_imp(map, kx+dx, ky, kz);
			if (dy) ix = ufbxi_insert_spatial_imp(map, kx, ky+dy, kz);
			if (dz) ix = ufbxi_insert_spatial_imp(map, kx, ky, kz+dz);

			if (dnum >= 2 && ix == UINT32_MAX) {
				if (dx && dy) ix = ufbxi_insert_spatial_imp(map, kx+dx, ky+dy, kz);
				if (dy && dz) ix = ufbxi_insert_spatial_imp(map, kx, ky+dy, kz+dz);
				if (dx && dz) ix = ufbxi_insert_spatial_imp(map, kx+dx, ky, kz+dz);

				if (dnum >= 3 && ix == UINT32_MAX) {
					ix = ufbxi_insert_spatial_imp(map, kx+dx, ky+dy, kz+dz);
				}
			}
		}
	}

	if (ix == UINT32_MAX) {
		ix = map->size;
		ufbxi_spatial_key key = { kx, ky, kz };
		uint32_t hash = ufbxi_hash_spatial_key(key);
		ufbxi_spatial_bucket *bucket = ufbxi_map_insert(map, ufbxi_spatial_bucket, hash, &key);
		ufbxi_check_return_err(map->ator->error, bucket, UINT32_MAX);
		bucket->key = key;
		bucket->position = *pos;
	}

	return ix;
}

static ufbxi_forceinline ufbx_real ufbxi_nurbs_weight(const ufbx_real_list *knots, size_t knot, size_t degree, ufbx_real u)
{
	if (knot >= knots->count) return 0.0f;
	if (knots->count - knot < degree) return 0.0f;
	ufbx_real prev_u = knots->data[knot], next_u = knots->data[knot + degree];
	if (prev_u >= next_u) return 0.0f;
	if (u <= prev_u) return 0.0f;
	if (u >= next_u) return 1.0f;
	return (u - prev_u) / (next_u - prev_u);
}

static ufbxi_forceinline ufbx_real ufbxi_nurbs_deriv(const ufbx_real_list *knots, size_t knot, size_t degree)
{
	if (knot >= knots->count) return 0.0f;
	if (knots->count - knot < degree) return 0.0f;
	ufbx_real prev_u = knots->data[knot], next_u = knots->data[knot + degree];
	if (prev_u >= next_u) return 0.0f;
	return (ufbx_real)degree / (next_u - prev_u);
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_finalize_mesh(ufbxi_buf *buf, ufbx_error *error, ufbx_mesh *mesh)
{
	size_t num_materials = mesh->materials.count;

	mesh->vertex_first_index = ufbxi_push(buf, int32_t, mesh->num_vertices);
	ufbxi_check_err(error, mesh->vertex_first_index);

	ufbxi_for(int32_t, p_vx_ix, mesh->vertex_first_index, mesh->num_vertices) {
		*p_vx_ix = -1;
	}
	for (size_t ix = 0; ix < mesh->num_indices; ix++) {
		int32_t vx = mesh->vertex_indices[ix];
		if (mesh->vertex_first_index[vx] < 0) {
			mesh->vertex_first_index[vx] = (int32_t)ix;
		}
	}

	// See `ufbxi_finalize_scene()`
	ufbxi_for_list(ufbx_mesh_material, mat, mesh->materials) {
		mat->face_indices = ufbxi_push(buf, int32_t, mat->num_faces);
		ufbxi_check_err(error, mat->face_indices);
		mat->num_faces = 0;
	}

	for (size_t i = 0; i < mesh->num_faces; i++) {
		int32_t mat_ix = mesh->face_material ? mesh->face_material[i] : 0;
		if (mat_ix >= 0 && (size_t)mat_ix < num_materials) {
			ufbx_mesh_material *mat = &mesh->materials.data[mat_ix];
			mat->face_indices[mat->num_faces++] = (int32_t)i;
		}
	}

	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_tessellate_nurbs_surface_imp(ufbxi_tessellate_context *tc)
{
	// `ufbx_tessellate_opts` must be cleared to zero first!
	ufbx_assert(tc->opts._begin_zero == 0 && tc->opts._end_zero == 0);
	ufbxi_check_err_msg(&tc->error, tc->opts._begin_zero == 0 && tc->opts._end_zero == 0, "Uninitialized options");

	if (tc->opts.span_subdivision_u <= 0) {
		tc->opts.span_subdivision_u = 4;
	}
	if (tc->opts.span_subdivision_v <= 0) {
		tc->opts.span_subdivision_v = 4;
	}

	size_t sub_u = tc->opts.span_subdivision_u;
	size_t sub_v = tc->opts.span_subdivision_v;

	const ufbx_nurbs_surface *surface = tc->surface;
	ufbx_mesh *mesh = &tc->mesh;

	ufbxi_init_ator(&tc->error, &tc->ator_tmp, &tc->opts.temp_allocator);
	ufbxi_init_ator(&tc->error, &tc->ator_result, &tc->opts.result_allocator);

	tc->result.unordered = true;
	tc->tmp.unordered = true;

	tc->result.ator = &tc->ator_result;
	tc->tmp.ator = &tc->ator_tmp;

	bool open_u = surface->basis_u.topology == UFBX_NURBS_TOPOLOGY_OPEN;
	bool open_v = surface->basis_v.topology == UFBX_NURBS_TOPOLOGY_OPEN;

	size_t spans_u = surface->basis_u.spans.count;
	size_t spans_v = surface->basis_v.spans.count;

	// Check conservatively that we don't overflow anything
	{
		size_t over_spans_u = spans_u * 2 * sizeof(ufbx_real);
		size_t over_spans_v = spans_v * 2 * sizeof(ufbx_real);
		size_t over_u = over_spans_u * sub_u;
		size_t over_v = over_spans_v * sub_v;
		size_t over_uv = over_u * over_v;
		ufbxi_check_err(&tc->error, !ufbxi_does_overflow(over_u, over_spans_u, sub_u));
		ufbxi_check_err(&tc->error, !ufbxi_does_overflow(over_v, over_spans_v, sub_v));
		ufbxi_check_err(&tc->error, !ufbxi_does_overflow(over_uv, over_u, over_v));
	}

	ufbxi_map_init(&tc->position_map, &tc->ator_tmp, &ufbxi_map_cmp_spatial_key, NULL);

	size_t faces_u = (spans_u - 1) * sub_u;
	size_t faces_v = (spans_v - 1) * sub_v;

	size_t indices_u = spans_u + (spans_u - 1) * sub_u;
	size_t indices_v = spans_v + (spans_v - 1) * sub_v;

	size_t num_faces = faces_u * faces_v;
	size_t num_indices = indices_u * indices_v;

	int32_t *position_ix = ufbxi_push(&tc->tmp, int32_t, num_indices);
	ufbx_vec2 *uvs = ufbxi_push(&tc->result, ufbx_vec2, num_indices + 1);
	ufbx_vec3 *normals = ufbxi_push(&tc->result, ufbx_vec3, num_indices + 1);
	ufbx_vec3 *tangents = ufbxi_push(&tc->result, ufbx_vec3, num_indices + 1);
	ufbx_vec3 *bitangents = ufbxi_push(&tc->result, ufbx_vec3, num_indices + 1);
	ufbxi_check_err(&tc->error, position_ix && uvs && normals && tangents && bitangents);

	*uvs++ = ufbx_zero_vec2;
	*normals++ = ufbx_zero_vec3;
	*tangents++ = ufbx_zero_vec3;
	*bitangents++ = ufbx_zero_vec3;

	for (size_t span_v = 0; span_v < spans_v; span_v++) {
		size_t splits_v = span_v + 1 == spans_v ? 1 : sub_v;

		for (size_t split_v = 0; split_v < splits_v; split_v++) {
			size_t ix_v = span_v * sub_v + split_v;

			ufbx_real v = surface->basis_v.spans.data[span_v];
			if (split_v > 0) {
				ufbx_real t = (ufbx_real)split_v / splits_v;
				v = v * (1.0f - t) + t * surface->basis_v.spans.data[span_v + 1];
			}
			ufbx_real original_v = v;
			if (span_v + 1 == spans_v && !open_v) {
				v = surface->basis_v.spans.data[0];
			}

			for (size_t span_u = 0; span_u < spans_u; span_u++) {
				size_t splits_u = span_u + 1 == spans_u ? 1 : sub_u;
				for (size_t split_u = 0; split_u < splits_u; split_u++) {
					size_t ix_u = span_u * sub_u + split_u;

					ufbx_real u = surface->basis_u.spans.data[span_u];
					if (split_u > 0) {
						ufbx_real t = (ufbx_real)split_u / splits_u;
						u = u * (1.0f - t) + t * surface->basis_u.spans.data[span_u + 1];
					}
					ufbx_real original_u = u;
					if (span_u + 1 == spans_u && !open_u) {
						u = surface->basis_u.spans.data[0];
					}

					ufbx_real pu = u, pv = v;
					ufbx_surface_point point = ufbx_evaluate_nurbs_surface(surface, pu, pv);
					ufbx_vec3 pos = point.position;

					uint32_t pos_ix = ufbxi_insert_spatial(&tc->position_map, &pos);
					ufbxi_check_err(&tc->error, pos_ix != UINT32_MAX);

					const ufbx_real fudge_eps = 1e-10f;
					bool fudged = false;
					if (ufbxi_dot3(point.derivative_u, point.derivative_u) < 1e-20f || ufbxi_dot3(point.derivative_v, point.derivative_v) < 1e-20f) {
						pu += pu + fudge_eps >= surface->basis_u.t_max ? -fudge_eps : fudge_eps;
						pv += pv + fudge_eps >= surface->basis_v.t_max ? -fudge_eps : fudge_eps;
						fudged = true;
					}
					if (fudged) {
						point = ufbx_evaluate_nurbs_surface(surface, pu, pv);
					}

					ufbx_vec3 tangent_u = ufbxi_normalize3(point.derivative_u);
					ufbx_vec3 tangent_v = ufbxi_normalize3(point.derivative_v);
					ufbx_vec3 normal = ufbxi_normalize3(ufbxi_cross3(tangent_u, tangent_v));
					if (surface->flip_normals) {
						normal.x = -normal.x;
						normal.y = -normal.y;
						normal.z = -normal.z;
					}

					size_t ix = ix_v * indices_u + ix_u;
					position_ix[ix] = (int32_t)pos_ix;
					uvs[ix].x = original_u;
					uvs[ix].y = original_v;
					normals[ix] = normal;
					tangents[ix] = tangent_u;
					bitangents[ix] = tangent_v;
				}
			}
		}
	}

	ufbx_face *faces = ufbxi_push(&tc->result, ufbx_face, num_faces);
	int32_t *vertex_ix = ufbxi_push(&tc->result, int32_t, num_faces * 4);
	int32_t *attrib_ix = ufbxi_push(&tc->result, int32_t, num_faces * 4);
	ufbxi_check_err(&tc->error, faces && vertex_ix && attrib_ix);

	size_t face_ix = 0;
	size_t dst_index = 0;

	size_t num_triangles = 0;

	for (size_t face_v = 0; face_v < faces_v; face_v++) {
		for (size_t face_u = 0; face_u < faces_u; face_u++) {

			attrib_ix[dst_index + 0] = (int32_t)((face_v + 0) * indices_u + (face_u + 0));
			attrib_ix[dst_index + 1] = (int32_t)((face_v + 0) * indices_u + (face_u + 1));
			attrib_ix[dst_index + 2] = (int32_t)((face_v + 1) * indices_u + (face_u + 1));
			attrib_ix[dst_index + 3] = (int32_t)((face_v + 1) * indices_u + (face_u + 0));

			vertex_ix[dst_index + 0] = position_ix[attrib_ix[dst_index + 0]];
			vertex_ix[dst_index + 1] = position_ix[attrib_ix[dst_index + 1]];
			vertex_ix[dst_index + 2] = position_ix[attrib_ix[dst_index + 2]];
			vertex_ix[dst_index + 3] = position_ix[attrib_ix[dst_index + 3]];

			bool is_triangle = false;
			for (size_t prev_ix = 0; prev_ix < 4; prev_ix++) {
				size_t next_ix = (prev_ix + 1) % 4;
				if (vertex_ix[dst_index + prev_ix] == vertex_ix[dst_index + next_ix]) {
					for (size_t i = next_ix; i < 3; i++) {
						attrib_ix[dst_index + i] = attrib_ix[dst_index + i + 1];
						vertex_ix[dst_index + i] = vertex_ix[dst_index + i + 1];
					}
					is_triangle = true;
					break;
				}
			}

			faces[face_ix].index_begin = (uint32_t)dst_index;
			faces[face_ix].num_indices = is_triangle ? 3 : 4;
			dst_index += is_triangle ? 3 : 4;
			num_triangles += is_triangle ? 1 : 2;
			face_ix++;
		}
	}

	size_t num_positions = tc->position_map.size;
	ufbx_vec3 *positions = ufbxi_push(&tc->result, ufbx_vec3, num_positions + 1);
	ufbxi_check_err(&tc->error, positions);

	*positions++ = ufbx_zero_vec3;

	ufbxi_spatial_bucket *buckets = (ufbxi_spatial_bucket*)tc->position_map.items;
	for (size_t i = 0; i < num_positions; i++) {
		positions[i] = buckets[i].position;
	}

	mesh->element.type = UFBX_ELEMENT_MESH;
	mesh->element.typed_id = UINT32_MAX;
	mesh->element.element_id = UINT32_MAX;

	mesh->vertices = positions;
	mesh->num_vertices = num_positions;
	mesh->vertex_indices = vertex_ix;

	mesh->faces = faces;

	mesh->vertex_position.data = positions;
	mesh->vertex_position.indices = vertex_ix;
	mesh->vertex_position.num_values = num_positions;
	mesh->vertex_position.value_reals = 3;
	mesh->vertex_position.unique_per_vertex = true;

	mesh->vertex_uv.data = uvs;
	mesh->vertex_uv.indices = attrib_ix;
	mesh->vertex_uv.num_values = num_indices;
	mesh->vertex_uv.value_reals = 2;

	mesh->vertex_normal.data = normals;
	mesh->vertex_normal.indices = attrib_ix;
	mesh->vertex_normal.num_values = num_indices;
	mesh->vertex_normal.value_reals = 3;

	mesh->vertex_tangent.data = tangents;
	mesh->vertex_tangent.indices = attrib_ix;
	mesh->vertex_tangent.num_values = num_indices;
	mesh->vertex_tangent.value_reals = 3;

	mesh->vertex_bitangent.data = bitangents;
	mesh->vertex_bitangent.indices = attrib_ix;
	mesh->vertex_bitangent.num_values = num_indices;
	mesh->vertex_bitangent.value_reals = 3;

	mesh->vertex_crease.value_reals = 1;
	mesh->vertex_color.value_reals = 4;

	ufbx_uv_set *uv_set = ufbxi_push(&tc->result, ufbx_uv_set, 1);
	ufbxi_check_err(&tc->error, uv_set);

	uv_set->index = 0;
	uv_set->name = ufbx_empty_string;
	uv_set->vertex_uv = mesh->vertex_uv;
	uv_set->vertex_tangent = mesh->vertex_tangent;
	uv_set->vertex_bitangent = mesh->vertex_bitangent;

	mesh->uv_sets.data = uv_set;
	mesh->uv_sets.count = 1;

	mesh->num_faces = num_faces;
	mesh->num_triangles = num_triangles;
	mesh->num_indices = dst_index;
	mesh->max_face_triangles = 2;

	if (surface->material) {
		mesh->face_material = ufbxi_push_zero(&tc->result, int32_t, num_faces);
		ufbxi_check_err(&tc->error, mesh->face_material);

		ufbx_mesh_material *mat = ufbxi_push_zero(&tc->result, ufbx_mesh_material, 1);
		ufbxi_check_err(&tc->error, mat);

		mat->material = surface->material;
		mat->num_triangles = num_triangles;
		mat->num_faces = num_faces;

		mesh->materials.data = mat;
		mesh->materials.count = 1;
	}

	mesh->skinned_is_local = true;
	mesh->skinned_position = mesh->vertex_position;
	mesh->skinned_normal = mesh->vertex_normal;

	ufbxi_check_err(&tc->error, ufbxi_finalize_mesh(&tc->result, &tc->error, mesh));

	tc->imp = ufbxi_push(&tc->result, ufbxi_mesh_imp, 1);
	ufbxi_check_err(&tc->error, tc->imp);
	tc->imp->magic = UFBXI_MESH_IMP_MAGIC;
	tc->imp->mesh = tc->mesh;
	tc->imp->ator = tc->ator_result;
	tc->imp->result_buf = tc->result;
	tc->imp->mesh.subdivision_evaluated = true;

	return 1;
}

// -- Topology

typedef struct {
	ufbx_real split;
	uint32_t index;
	uint32_t slow_left;
	uint32_t slow_right;
	uint32_t slow_end;
} ufbxi_kd_node;

typedef struct {
	const ufbx_mesh *mesh;
	ufbx_face face;
	ufbx_vertex_vec3 positions;
	ufbx_vec3 axes[3];
	ufbxi_kd_node kd_nodes[1 << (UFBXI_KD_FAST_DEPTH + 1)];
	uint32_t *kd_indices;
} ufbxi_ngon_context;

typedef struct {
	ufbx_real min_t[2];
	ufbx_real max_t[2];
	ufbx_vec2 points[3];
	uint32_t indices[3];
} ufbxi_kd_triangle;

ufbxi_forceinline static ufbx_vec2 ufbxi_ngon_project(ufbxi_ngon_context *nc, ufbx_vec3 point)
{
	ufbx_vec2 p;
	p.x = ufbxi_dot3(nc->axes[0], point);
	p.y = ufbxi_dot3(nc->axes[1], point);
	return p;
}

ufbxi_forceinline static ufbx_real ufbxi_orient2d(ufbx_vec2 a, ufbx_vec2 b, ufbx_vec2 c)
{
	return (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
}

ufbxi_forceinline static bool ufbxi_kd_check_point(ufbxi_ngon_context *nc, const ufbxi_kd_triangle *tri, uint32_t index, ufbx_vec3 point)
{
	if (index == tri->indices[0] || index == tri->indices[1] || index == tri->indices[2]) return false;
	ufbx_vec2 p = ufbxi_ngon_project(nc, point);

	ufbx_real u = ufbxi_orient2d(p, tri->points[0], tri->points[1]);
	ufbx_real v = ufbxi_orient2d(p, tri->points[1], tri->points[2]);
	ufbx_real w = ufbxi_orient2d(p, tri->points[2], tri->points[0]);

	if (u <= 0.0f && v <= 0.0f && w <= 0.0f) return true;
	if (u >= 0.0f && v >= 0.0f && w >= 0.0f) return true;
	return false;
}

ufbxi_noinline static bool ufbxi_kd_check_slow(ufbxi_ngon_context *nc, const ufbxi_kd_triangle *tri, uint32_t begin, uint32_t count, uint32_t axis)
{
	ufbx_vertex_vec3 pos = nc->mesh->vertex_position;
	uint32_t *kd_indices = nc->kd_indices;

	while (count > 0) {
		uint32_t num_left = count / 2;
		uint32_t begin_right = begin + num_left + 1;
		uint32_t num_right = count - (num_left + 1);

		uint32_t index = kd_indices[begin + num_left];
		ufbx_vec3 point = pos.data[pos.indices[nc->face.index_begin + index]];
		ufbx_real split = ufbxi_dot3(point, nc->axes[axis]);
		bool hit_left = tri->min_t[axis] <= split;
		bool hit_right = tri->max_t[axis] >= split;

		if (hit_left && hit_right) {
			if (ufbxi_kd_check_point(nc, tri, index, point)) {
				return true;
			}

			if (ufbxi_kd_check_slow(nc, tri, begin_right, num_right, axis ^ 1)) {
				return true;
			}
		}

		axis ^= 1;
		if (hit_left) {
			count = num_left;
		} else {
			begin = begin_right;
			count = num_right;
		}
	}

	return false;
}

ufbxi_noinline static bool ufbxi_kd_check_fast(ufbxi_ngon_context *nc, const ufbxi_kd_triangle *tri, uint32_t kd_index, uint32_t axis, uint32_t depth)
{
	ufbx_vertex_vec3 pos = nc->mesh->vertex_position;

	for (;;) {
		ufbxi_kd_node node = nc->kd_nodes[kd_index];
		if (node.slow_end == 0) return false;

		bool hit_left = tri->min_t[axis] <= node.split;
		bool hit_right = tri->max_t[axis] >= node.split;

		uint32_t side = hit_left ? 0 : 1;
		uint32_t child_kd_index = kd_index * 2 + 1 + side;
		if (hit_left && hit_right) {

			// Check for the point on the split plane
			ufbx_vec3 point = pos.data[pos.indices[nc->face.index_begin + node.index]];
			if(ufbxi_kd_check_point(nc, tri, node.index, point)) {
				return true;
			}

			// Recurse always to the right if we hit both sides
			if (depth + 1 == UFBXI_KD_FAST_DEPTH) {
				if (ufbxi_kd_check_slow(nc, tri, node.slow_right, node.slow_end - node.slow_right, axis ^ 1)) {
					return true;
				}
			} else {
				if (ufbxi_kd_check_fast(nc, tri, child_kd_index + 1, axis ^ 1, depth + 1)) {
					return true;
				}
			}
		}

		depth++;
		axis ^= 1;
		kd_index = child_kd_index;

		if (depth == UFBXI_KD_FAST_DEPTH) {
			if (hit_left) {
				return ufbxi_kd_check_slow(nc, tri, node.slow_left, node.slow_right - node.slow_left, axis);
			} else {
				return ufbxi_kd_check_slow(nc, tri, node.slow_right, node.slow_end - node.slow_right, axis);
			}
		}
	}
}

ufbxi_noinline static void ufbxi_kd_build(ufbxi_ngon_context *nc, uint32_t *indices, uint32_t *tmp, uint32_t num, uint32_t axis, uint32_t fast_index, uint32_t depth)
{
	if (num == 0) return;

	ufbx_vertex_vec3 pos = nc->mesh->vertex_position;
	ufbx_vec3 axis_dir = nc->axes[axis];
	ufbx_face face = nc->face;

	// Sort the remaining indices based on the axis
	ufbxi_macro_stable_sort(uint32_t, 16, indices, tmp, num,
		( ufbxi_dot3(axis_dir, pos.data[pos.indices[face.index_begin + *a]])
			< ufbxi_dot3(axis_dir, pos.data[pos.indices[face.index_begin + *b]]) ));

	uint32_t num_left = num / 2;
	uint32_t begin_right = num_left + 1;
	uint32_t num_right = num - begin_right;
	uint32_t dst_right = num_left + 1;
	if (depth < UFBXI_KD_FAST_DEPTH) {
		uint32_t skip_left = 1u << (UFBXI_KD_FAST_DEPTH - depth - 1);
		dst_right = dst_right > skip_left ? dst_right - skip_left : 0;

		uint32_t index = indices[num_left];
		ufbxi_kd_node *kd = &nc->kd_nodes[fast_index];

		kd->split = ufbxi_dot3(axis_dir, pos.data[pos.indices[face.index_begin + index]]);
		kd->index = index;

		if (depth + 1 == UFBXI_KD_FAST_DEPTH) {
			kd->slow_left = (uint32_t)(indices - nc->kd_indices);
			kd->slow_right = kd->slow_left + num_left;
			kd->slow_end = kd->slow_right + num_right;
		} else {
			kd->slow_left = UINT32_MAX;
			kd->slow_right = UINT32_MAX;
			kd->slow_end = UINT32_MAX;
		}
	}

	uint32_t child_fast = fast_index * 2 + 1;
	ufbxi_kd_build(nc, indices, tmp, num_left, axis ^ 1, child_fast + 0, depth + 1);

	if (dst_right != begin_right) {
		memmove(indices + dst_right, indices + begin_right, num_right * sizeof(uint32_t));
	}

	ufbxi_kd_build(nc, indices + dst_right, tmp, num_right, axis ^ 1, child_fast + 1, depth + 1);
}

ufbxi_noinline static uint32_t ufbxi_triangulate_ngon(ufbxi_ngon_context *nc, uint32_t *indices, uint32_t num_indices)
{
	ufbx_face face = nc->face;

	// Form an orthonormal basis to project the polygon into a 2D plane
	ufbx_vec3 normal = ufbx_get_weighted_face_normal(&nc->mesh->vertex_position, face);
	ufbx_real len = ufbxi_length3(normal);
	if (len > 1e-20f) {
		normal = ufbxi_mul3(normal, 1.0f / len);
	} else {
		normal.x = 1.0f;
		normal.y = 0.0f;
		normal.z = 0.0f;
	}

	ufbx_vec3 axis;
	if (normal.x*normal.x < 0.5f) {
		axis.x = 1.0f;
		axis.y = 0.0f;
		axis.z = 0.0f;
	} else {
		axis.x = 0.0f;
		axis.y = 1.0f;
		axis.z = 0.0f;
	}
	nc->axes[0] = ufbxi_normalize3(ufbxi_cross3(axis, normal));
	nc->axes[1] = ufbxi_normalize3(ufbxi_cross3(normal, nc->axes[0]));
	nc->axes[2] = normal;

	uint32_t *kd_indices = indices;
	nc->kd_indices = kd_indices;

	uint32_t *kd_tmp = indices + face.num_indices;
	ufbx_vertex_vec3 pos = nc->mesh->vertex_position;

	// Collect all the reflex corners for intersection testing.
	uint32_t num_kd_indices = 0;
	{
		ufbx_vec2 a = ufbxi_ngon_project(nc, pos.data[pos.indices[face.index_begin + face.num_indices - 1]]);
		ufbx_vec2 b = ufbxi_ngon_project(nc, pos.data[pos.indices[face.index_begin + 0]]);
		for (uint32_t i = 0; i < face.num_indices; i++) {
			uint32_t next = i + 1 < face.num_indices ? i + 1 : 0;
			ufbx_vec2 c = ufbxi_ngon_project(nc, pos.data[pos.indices[face.index_begin + next]]);

			if (ufbxi_orient2d(a, b, c) < 0.0f) {
				kd_indices[num_kd_indices++] = i;
			}

			a = b;
			b = c;
		}
	}

	// Build a KD-tree out of the collected reflex vertices.
	uint32_t num_skip_indices = (1u << (UFBXI_KD_FAST_DEPTH + 1)) - 1;
	uint32_t kd_slow_indices = num_kd_indices > num_skip_indices ? num_kd_indices - num_skip_indices : 0;
	ufbx_assert(kd_slow_indices + face.num_indices * 2 <= num_indices);
	ufbxi_kd_build(nc, kd_indices, kd_tmp, num_kd_indices, 0, 0, 0);

	uint32_t *edges = indices + num_indices - face.num_indices * 2;

	// Initialize `edges` to be a connectivity structure where:
	//  `edges[2*i + 0]` is the prevous vertex of `i`
	//  `edges[2*i + 1]` is the next vertex of `i`
	// When clipped we mark indices with the high bit (0x80000000)
	for (uint32_t i = 0; i < face.num_indices; i++) {
		edges[i*2 + 0] = i > 0 ? i - 1 : face.num_indices - 1;
		edges[i*2 + 1] = i + 1 < face.num_indices ? i + 1 : 0;
	}

	// Core of the ear clipping algorithm.
	// Iterate through the polygon corners looking for potential ears satisfying:
	//   - Angle must be less than 180deg
	//   - The triangle formed by the two edges must be contained within the polygon
	// As these properties change only locally between modifications we only need
	// to iterate the polygon once if we move backwards one step every time we clip an ear.
	uint32_t indices_left = face.num_indices;
	{
		ufbxi_kd_triangle tri;

		uint32_t ix = 1;
		ufbx_vec2 a = ufbxi_ngon_project(nc, pos.data[pos.indices[face.index_begin + 0]]);
		ufbx_vec2 b = ufbxi_ngon_project(nc, pos.data[pos.indices[face.index_begin + 1]]);
		ufbx_vec2 c = ufbxi_ngon_project(nc, pos.data[pos.indices[face.index_begin + 2]]);
		bool prev_was_reflex = false;

		uint32_t num_steps = 0;

		while (indices_left > 3) {
			uint32_t prev = edges[ix*2 + 0];
			uint32_t next = edges[ix*2 + 1];
			if (ufbxi_orient2d(a, b, c) > 0.0f) {

				tri.points[0] = a;
				tri.points[1] = b;
				tri.points[2] = c;
				tri.indices[0] = prev;
				tri.indices[1] = ix;
				tri.indices[2] = next;
				tri.min_t[0] = ufbxi_min_real(ufbxi_min_real(a.x, b.x), c.x);
				tri.min_t[1] = ufbxi_min_real(ufbxi_min_real(a.y, b.y), c.y);
				tri.max_t[0] = ufbxi_max_real(ufbxi_max_real(a.x, b.x), c.x);
				tri.max_t[1] = ufbxi_max_real(ufbxi_max_real(a.y, b.y), c.y);

				// If there is no reflex angle contained within the triangle formed
				// by `{ a, b, c }` connect the vertices `a - c` (prev, next) directly.
				if (!ufbxi_kd_check_fast(nc, &tri, 0, 0, 0)) {

					// Mark as clipped
					edges[ix*2 + 0] |= 0x80000000;
					edges[ix*2 + 1] |= 0x80000000;

					edges[next*2 + 0] = prev;
					edges[prev*2 + 1] = next;

					indices_left -= 1;

					// Move backwards only if the previous was reflex as now it would
					// cover a superset of the previous area, failing the intersection test.
					if (prev_was_reflex) {
						prev_was_reflex = false;
						ix = prev;
						b = a;
						uint32_t prev_prev = edges[prev*2 + 0];
						a = ufbxi_ngon_project(nc, pos.data[pos.indices[face.index_begin + prev_prev]]);
					} else {
						ix = next;
						b = c;
						uint32_t next_next = edges[next*2 + 1];
						c = ufbxi_ngon_project(nc, pos.data[pos.indices[face.index_begin + next_next]]);
					}
					continue;
				}

				prev_was_reflex = false;
			} else {
				prev_was_reflex = true;
			}

			// Continue forward
			ix = next;
			a = b;
			b = c;
			next = edges[next*2 + 1];
			c = ufbxi_ngon_project(nc, pos.data[pos.indices[face.index_begin + next]]);
			num_steps++;

			// If we have walked around the entire polygon it is irregular and
			// ear cutting won't find any more triangles.
			// TODO: This could be stricter?
			if (num_steps >= face.num_indices*2) break;
		}

		// Fallback: Cut non-ears until the polygon is completed.
		// TODO: Could do something better here..
		while (indices_left > 3) {
			uint32_t prev = edges[ix*2 + 0];
			uint32_t next = edges[ix*2 + 1];

			// Mark as clipped
			edges[ix*2 + 0] |= 0x80000000;
			edges[ix*2 + 1] |= 0x80000000;

			edges[prev*2 + 1] = next;
			edges[next*2 + 0] = prev;

			indices_left -= 1;
			ix = next;
		}

		// Now we have a single triangle left at `ix`.
		edges[ix*2 + 0] |= 0x80000000;
		edges[ix*2 + 1] |= 0x80000000;
	}

	// Expand the adjacency information `edges` into proper triangles.
	// Care needs to be taken here as both refer to the same memory area:
	// The last 4 triangles may overlap in source and destination so we write
	// them to a stack buffer and copy them over in the end.
	uint32_t max_triangles = face.num_indices - 2;
	uint32_t num_triangles = 0, num_last_triangles = 0;
	uint32_t last_triangles[4*3];

	uint32_t index_begin = face.index_begin;
	for (uint32_t ix = 0; ix < face.num_indices; ix++) {
		uint32_t prev = edges[ix*2 + 0];
		uint32_t next = edges[ix*2 + 1];
		if (!(prev & 0x80000000)) continue;

		uint32_t *dst = indices + num_triangles * 3;
		if (num_triangles + 4 >= max_triangles) {
			dst = last_triangles + num_last_triangles * 3;
			num_last_triangles++;
		}

		dst[0] = index_begin + (prev & 0x7fffffff);
		dst[1] = index_begin + ix;
		dst[2] = index_begin + (next & 0x7fffffff);
		num_triangles++;
	}

	// Copy over the last triangles
	ufbx_assert(num_triangles == max_triangles);
	memcpy(indices + (max_triangles - num_last_triangles) * 3, last_triangles, num_last_triangles * 3 * sizeof(uint32_t));

	return num_triangles;
}

static int ufbxi_cmp_topo_index_prev_next(const void *va, const void *vb)
{
	const ufbx_topo_edge *a = (const ufbx_topo_edge*)va, *b = (const ufbx_topo_edge*)vb;
	if (a->prev != b->prev) return a->prev < b->prev ? -1 : +1;
	if (a->next != b->next) return a->next < b->next ? -1 : +1;
	return 0;
}

static int ufbxi_cmp_topo_index_index(const void *va, const void *vb)
{
	const ufbx_topo_edge *a = (const ufbx_topo_edge*)va, *b = (const ufbx_topo_edge*)vb;
	if (a->index != b->index) return a->index < b->index ? -1 : +1;
	return 0;
}

static void ufbxi_compute_topology(const ufbx_mesh *mesh, ufbx_topo_edge *topo)
{
	size_t num_indices = mesh->num_indices;

	// Temporarily use `prev` and `next` for vertices
	for (uint32_t fi = 0; fi < mesh->num_faces; fi++) {
		ufbx_face face = mesh->faces[fi];
		for (uint32_t pi = 0; pi < face.num_indices; pi++) {
			ufbx_topo_edge *te = &topo[face.index_begin + pi];
			uint32_t ni = (pi + 1) % face.num_indices;
			int32_t va = mesh->vertex_indices[face.index_begin + pi];
			int32_t vb = mesh->vertex_indices[face.index_begin + ni];

			if (vb < va) {
				int32_t vt = va; va = vb; vb = vt;
			}
			te->index = face.index_begin + pi;
			te->twin = -1;
			te->edge = -1;
			te->prev = va;
			te->next = vb;
			te->face = fi;
			te->flags = (ufbx_topo_flags)0;
		}
	}

	// TODO: Macro unstable/non allocating sort
	qsort(topo, num_indices, sizeof(ufbx_topo_edge), &ufbxi_cmp_topo_index_prev_next);

	if (mesh->edges) {
		for (uint32_t ei = 0; ei < mesh->num_edges; ei++) {
			ufbx_edge edge = mesh->edges[ei];
			int32_t va = mesh->vertex_indices[edge.indices[0]];
			int32_t vb = mesh->vertex_indices[edge.indices[1]];
			if (vb < va) {
				int32_t vt = va; va = vb; vb = vt;
			}

			size_t ix = num_indices;
			ufbxi_macro_lower_bound_eq(ufbx_topo_edge, 32, &ix, topo, 0, num_indices,
				(a->prev == va ? a->next < vb : a->prev < va), (a->prev == va && a->next == vb));

			for (; ix < num_indices && topo[ix].prev == va && topo[ix].next == vb; ix++) {
				topo[ix].edge = ei;
			}
		}
	}

	// Connect paired edges
	for (size_t i0 = 0; i0 < num_indices; ) {
		size_t i1 = i0;

		int32_t a = topo[i0].prev, b = topo[i0].next;
		while (i1 + 1 < num_indices && topo[i1 + 1].prev == a && topo[i1 + 1].next == b) i1++;

		if (i1 == i0 + 1) {
			topo[i0].twin = topo[i1].index;
			topo[i1].twin = topo[i0].index;
		} else if (i1 > i0 + 1) {
			for (size_t i = i0; i <= i1; i++) {
				topo[i].flags = (ufbx_topo_flags)(topo[i].flags | UFBX_TOPO_NON_MANIFOLD);
			}
		}

		i0 = i1 + 1;
	}

	// TODO: Macro unstable/non allocating sort
	qsort(topo, num_indices, sizeof(ufbx_topo_edge), &ufbxi_cmp_topo_index_index);

	// Fix `prev` and `next` to the actual index values
	for (uint32_t fi = 0; fi < mesh->num_faces; fi++) {
		ufbx_face face = mesh->faces[fi];
		for (uint32_t i = 0; i < face.num_indices; i++) {
			ufbx_topo_edge *to = &topo[face.index_begin + i];
			to->prev = (int32_t)(face.index_begin + (i + face.num_indices - 1) % face.num_indices);
			to->next = (int32_t)(face.index_begin + (i + 1) % face.num_indices);
		}
	}
}

static bool ufbxi_is_edge_smooth(const ufbx_mesh *mesh, ufbx_topo_edge *indices, int32_t index, bool assume_smooth)
{
	if (mesh->edge_smoothing) {
		int32_t edge = indices[index].edge;
		if (edge >= 0 && mesh->edge_smoothing[edge]) return true;
	}

	if (mesh->face_smoothing) {
		if (mesh->face_smoothing[indices[index].face]) return true;
		int32_t twin = indices[index].twin;
		if (twin >= 0) {
			if (mesh->face_smoothing[indices[twin].face]) return true;
		}
	}

	if (!mesh->edge_smoothing && !mesh->face_smoothing && mesh->vertex_normal.data) {
		int32_t twin = indices[index].twin;
		if (twin >= 0 && mesh->vertex_normal.data) {
			ufbx_vec3 a0 = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
			ufbx_vec3 a1 = ufbx_get_vertex_vec3(&mesh->vertex_normal, indices[index].next);
			ufbx_vec3 b0 = ufbx_get_vertex_vec3(&mesh->vertex_normal, indices[twin].next);
			ufbx_vec3 b1 = ufbx_get_vertex_vec3(&mesh->vertex_normal, twin);
			if (a0.x == b0.x && a0.y == b0.y && a0.z == b0.z) return true;
			if (a1.x == b1.x && a1.y == b1.y && a1.z == b1.z) return true;
		}
	} else if (assume_smooth) {
		return true;
	}

	return false;
}

ufbxi_noinline static bool ufbxi_is_edge_split(const ufbx_vertex_attrib *attrib, const ufbx_topo_edge *topo, int32_t index)
{		  
	int32_t twin = topo[index].twin;
	if (twin >= 0) {
		int32_t a0 = attrib->indices[index];
		int32_t a1 = attrib->indices[topo[index].next];
		int32_t b0 = attrib->indices[topo[twin].next];
		int32_t b1 = attrib->indices[twin];
		if (a0 == b0 && a1 == b1) return false;
		size_t num_reals = attrib->value_reals;
		size_t size = sizeof(ufbx_real) * num_reals;
		ufbx_real *da0 = (ufbx_real*)attrib->data + a0 * num_reals;
		ufbx_real *da1 = (ufbx_real*)attrib->data + a1 * num_reals;
		ufbx_real *db0 = (ufbx_real*)attrib->data + b0 * num_reals;
		ufbx_real *db1 = (ufbx_real*)attrib->data + b1 * num_reals;
		if (!memcmp(da0, db0, size) && !memcmp(da1, db1, size)) return false;
		return true;
	}

	return false;
}

static ufbx_real ufbxi_edge_crease(const ufbx_mesh *mesh, bool split, const ufbx_topo_edge *topo, int32_t index)
{
	if (topo[index].twin < 0) return 1.0f;
	if (split) return 1.0f;
	if (mesh->edge_crease && topo[index].edge >= 0) return mesh->edge_crease[topo[index].edge] * 10.0f;
	return 0.0f;
}

typedef struct {
	ufbxi_mesh_imp *imp;

	ufbx_error error;

	ufbx_mesh src_mesh;
	ufbx_mesh dst_mesh;
	ufbx_topo_edge *topo;

	ufbx_subdivide_opts opts;

	ufbxi_allocator ator_result;
	ufbxi_allocator ator_tmp;

	ufbxi_buf result;
	ufbxi_buf tmp;
	ufbxi_buf source;

} ufbxi_subdivide_context;

ufbxi_noinline static int ufbxi_subdivide_layer(ufbxi_subdivide_context *sc, ufbx_vertex_attrib *layer, ufbx_subdivision_boundary boundary)
{
	if (!layer->data) return 1;

	const ufbx_mesh *mesh = &sc->src_mesh;
	const ufbx_topo_edge *topo = sc->topo;

	int32_t *edge_indices = ufbxi_push(&sc->result, int32_t, mesh->num_indices);
	ufbxi_check_err(&sc->error, edge_indices);

	size_t num_edge_values = 0;
	for (int32_t ix = 0; ix < (int32_t)mesh->num_indices; ix++) {
		int32_t twin = topo[ix].twin;
		if (twin >= 0 && twin < ix && !ufbxi_is_edge_split(layer, topo, ix)) {
			edge_indices[ix] = edge_indices[twin];
		} else {
			edge_indices[ix] = (int32_t)num_edge_values++;
		}
	}

	size_t reals = layer->value_reals;

	size_t num_initial_values = (num_edge_values + mesh->num_faces + mesh->num_indices) * reals;
	ufbx_real *values = ufbxi_push(&sc->tmp, ufbx_real, num_initial_values);
	ufbxi_check_err(&sc->error, values);

	ufbx_real *face_values = values;
	ufbx_real *edge_values = face_values + mesh->num_faces * reals;
	ufbx_real *vertex_values = edge_values + num_edge_values * reals;

	size_t num_vertex_values = 0;

	int32_t *vertex_indices = ufbxi_push(&sc->result, int32_t, mesh->num_indices);
	ufbxi_check_err(&sc->error, vertex_indices);

	// Assume initially unique per vertex, remove if not the case
	layer->unique_per_vertex = true;

	bool sharp_corners = false;
	bool sharp_splits = false;
	bool sharp_all = false;

	switch (boundary) {
	case UFBX_SUBDIVISION_BOUNDARY_DEFAULT:
	case UFBX_SUBDIVISION_BOUNDARY_LEGACY:
	case UFBX_SUBDIVISION_BOUNDARY_SHARP_NONE:
		// All smooth
		break;
	case UFBX_SUBDIVISION_BOUNDARY_SHARP_CORNERS:
		sharp_corners = true;
		break;
	case UFBX_SUBDIVISION_BOUNDARY_SHARP_BOUNDARY:
		sharp_corners = true;
		sharp_splits = true;
		break;
	case UFBX_SUBDIVISION_BOUNDARY_SHARP_INTERIOR:
		sharp_all = true;
		break;
	}

	// Mark unused indices as -1 so we can patch non-manifold
	for (size_t i = 0; i < mesh->num_indices; i++) {
		vertex_indices[i] = -1;
	}

	// Face points
	for (size_t fi = 0; fi < mesh->num_faces; fi++) {
		ufbx_face face = mesh->faces[fi];
		ufbx_real *dst = face_values + fi * reals;

		for (size_t i = 0; i < reals; i++) dst[i] = 0.0f;

		for (uint32_t ci = 0; ci < face.num_indices; ci++) {
			int32_t ix = (int32_t)(face.index_begin + ci);
			ufbx_real *value = layer->data + layer->indices[ix] * reals;
			for (size_t i = 0; i < reals; i++) dst[i] += value[i];
		}

		ufbx_real weight = 1.0f / face.num_indices;
		for (size_t i = 0; i < reals; i++) dst[i] *= weight;
	}

	// Edge points
	for (int32_t ix = 0; ix < (int32_t)mesh->num_indices; ix++) {
		ufbx_real *dst = edge_values + edge_indices[ix] * reals;

		int32_t twin = topo[ix].twin;
		bool split = ufbxi_is_edge_split(layer, topo, ix);

		if (split || (topo[ix].flags & UFBX_TOPO_NON_MANIFOLD) != 0) {
			layer->unique_per_vertex = false;
		}

		ufbx_real crease = 0.0f;
		if (split || twin < 0) {
			crease = 1.0f;
		} else if (topo[ix].edge >= 0 && mesh->edge_crease) {
			crease = mesh->edge_crease[topo[ix].edge] * 10.0f;
		}
		if (sharp_all) crease = 1.0f;

		ufbx_real *v0 = layer->data + layer->indices[ix] * reals;
		ufbx_real *v1 = layer->data + layer->indices[topo[ix].next] * reals;

		if (twin >= 0 && topo[ix].twin < ix && !split) {
			// Already calculated
		} else if (crease <= 0.0f) {
			ufbx_real *f0 = face_values + topo[ix].face * reals;
			ufbx_real *f1 = face_values + topo[twin].face * reals;
			for (size_t i = 0; i < reals; i++) dst[i] = (v0[i] + v1[i] + f0[i] + f1[i]) * 0.25f;
		} else if (crease >= 1.0f) {
			for (size_t i = 0; i < reals; i++) dst[i] = (v0[i] + v1[i]) * 0.5f;
		} else if (crease < 1.0f) {
			ufbx_real *f0 = face_values + topo[ix].face * reals;
			ufbx_real *f1 = face_values + topo[twin].face * reals;
			for (size_t i = 0; i < reals; i++) {
				ufbx_real smooth = (v0[i] + v1[i] + f0[i] + f1[i]) * 0.25f;
				ufbx_real hard = (v0[i] + v1[i]) * 0.5f;
				dst[i] = smooth * (1.0f - crease) + hard * crease;
			}
		}
	}

	// Vertex points
	for (size_t vi = 0; vi < mesh->num_vertices; vi++) {
		int32_t original_start = mesh->vertex_first_index[vi];
		if (original_start < 0) continue;

		// Find a topological boundary, or if not found a split edge
		int32_t start = original_start;
		for (int32_t cur = start;;) {
			int32_t prev = ufbx_topo_prev_vertex_edge(topo, cur);
			if (prev < 0) { start = cur; break; } // Topological boundary: Stop and use as start
			if (ufbxi_is_edge_split(layer, topo, prev)) start = cur; // Split edge: Consider as start
			if (prev == original_start) break; // Loop: Stop, use original start or split if found
			cur = prev;
		}

		original_start = start;
		while (start >= 0) {
			if (start != original_start) {
				layer->unique_per_vertex = false;
			}

			int32_t value_index = (int32_t)num_vertex_values++;
			ufbx_real *dst = vertex_values + value_index * reals;

			// We need to compute the average crease value and keep track of
			// two creased edges, if there's more we use the corner rule that
			// does not need the information.
			ufbx_real total_crease = 0.0f;
			size_t num_crease = 0;
			size_t num_split = 0;
			bool on_boundary = false;
			bool non_manifold = false;
			int32_t edge_points[2];

			// At start we always have two edges and a single face
			int32_t start_prev = topo[start].prev;
			int32_t end_edge = topo[start_prev].twin;
			size_t valence = 2;

			non_manifold |= (topo[start].flags & UFBX_TOPO_NON_MANIFOLD) != 0;
			non_manifold |= (topo[start_prev].flags & UFBX_TOPO_NON_MANIFOLD) != 0;

			const ufbx_real *v0 = layer->data + layer->indices[start] * reals;

			{
				const ufbx_real *e0 = layer->data + layer->indices[topo[start].next] * reals;
				const ufbx_real *e1 = layer->data + layer->indices[start_prev] * reals;
				const ufbx_real *f0 = face_values + topo[start].face * reals;
				for (size_t i = 0; i < reals; i++) dst[i] = e0[i] + e1[i] + f0[i];
			}

			bool start_split = ufbxi_is_edge_split(layer, topo, start);
			bool prev_split = end_edge >= 0 && ufbxi_is_edge_split(layer, topo, end_edge);

			// Either of the first two edges may be creased
			ufbx_real start_crease = ufbxi_edge_crease(mesh, start_split, topo, start);
			if (start_crease > 0.0f) {
				total_crease += start_crease;
				edge_points[num_crease++] = topo[start].next;
			}
			ufbx_real prev_crease = ufbxi_edge_crease(mesh, prev_split, topo, start_prev);
			if (prev_crease > 0.0f) {
				total_crease += prev_crease;
				edge_points[num_crease++] = start_prev;
			}

			if (end_edge >= 0) {
				if (prev_split) {
					num_split++;
				}
			} else {
				on_boundary = true;
			}

			vertex_indices[start] = value_index;

			if (start_split) {
				// We need to special case if the first edge is split as we have
				// handled it already in the code above..
				start = ufbx_topo_next_vertex_edge(topo, start);
				num_split++;
			} else {
				// Follow vertex edges until we either hit a topological/split boundary
				// or loop back to the left edge we accounted for in `start_prev`
				int32_t cur = start;
				for (;;) {
					cur = ufbx_topo_next_vertex_edge(topo, cur);

					// Topological boundary: Finished
					if (cur < 0) {
						on_boundary = true;
						start = -1;
						break;
					}

					non_manifold |= (topo[cur].flags & UFBX_TOPO_NON_MANIFOLD) != 0;
					vertex_indices[cur] = value_index;

					bool split = ufbxi_is_edge_split(layer, topo, cur);

					// Looped: Add the face from the other side still if not split
					if (cur == end_edge && !split) {
						const ufbx_real *f0 = face_values + topo[cur].face * reals;
						for (size_t i = 0; i < reals; i++) dst[i] += f0[i];
						start = -1;
						break;
					}

					// Add the edge crease, this also handles boundaries as they
					// have an implicit crease of 1.0 using `ufbxi_edge_crease()`
					ufbx_real cur_crease = ufbxi_edge_crease(mesh, split, topo, cur);
					if (cur_crease > 0.0f) {
						total_crease += cur_crease;
						if (num_crease < 2) edge_points[num_crease] = topo[cur].next;
						num_crease++;
					}

					// Add the new edge and face to the sum
					const ufbx_real *e0 = layer->data + layer->indices[topo[cur].next] * reals;
					const ufbx_real *f0 = face_values + topo[cur].face * reals;
					for (size_t i = 0; i < reals; i++) dst[i] += e0[i] + f0[i];
					valence++;

					// If we landed at a split edge advance to the next one
					// and continue from there in the outer loop
					if (split) {
						start = ufbx_topo_next_vertex_edge(topo, cur);
						num_split++;
						break;
					}
				}
			}

			if (start == original_start) start = -1;

			// Weights for various subdivision masks
			ufbx_real fe_weight = 1.0f / (ufbx_real)(valence*valence);
			ufbx_real v_weight = (ufbx_real)(valence - 2) / (ufbx_real)valence;

			// Select the right subdivision mask depending on valence and crease
			if (num_crease > 2
				|| (sharp_corners && valence == 2 && (num_split > 0 || on_boundary))
				|| (sharp_splits && (num_split > 0 || on_boundary))
				|| sharp_all
				|| non_manifold) {
				// Corner: Copy as-is
				for (size_t i = 0; i < reals; i++) dst[i] = v0[i];
			} else if (num_crease == 2) {
				// Boundary: Interpolate edge
				ufbx_real *e0 = layer->data + layer->indices[edge_points[0]] * reals;
				ufbx_real *e1 = layer->data + layer->indices[edge_points[1]] * reals;

				total_crease *= 0.5f;
				if (total_crease < 0.0f) total_crease = 0.0f;
				if (total_crease > 1.0f) total_crease = 1.0f;

				for (size_t i = 0; i < reals; i++) {
					ufbx_real smooth = dst[i] * fe_weight + v0[i] * v_weight;
					ufbx_real hard = (e0[i] + e1[i]) * 0.125f + v0[i] * 0.75f;
					dst[i] = smooth * (1.0f - total_crease) + hard * total_crease;
				}
			} else {
				// Regular: Weighted sum with the accumulated edge/face points
				for (size_t i = 0; i < reals; i++) {
					dst[i] = dst[i] * fe_weight + v0[i] * v_weight;
				}
			}

		}
	}

	// Copy non-manifold vertex values as-is
	for (size_t old_ix = 0; old_ix < mesh->num_indices; old_ix++) {
		int32_t ix = vertex_indices[old_ix];
		if (ix < 0) {
			ix = (int32_t)num_vertex_values++;
			vertex_indices[old_ix] = ix;
			const ufbx_real *src = layer->data + layer->indices[old_ix] * reals;
			ufbx_real *dst = vertex_values + ix * reals;
			for (size_t i = 0; i < reals; i++) dst[i] = src[i];
		}
	}

	int32_t *new_indices = ufbxi_push(&sc->result, int32_t, mesh->num_indices * 4);
	ufbxi_check_err(&sc->error, new_indices);

	int32_t face_start = 0;
	int32_t edge_start = (int32_t)(face_start + mesh->num_faces);
	int32_t vert_start = (int32_t)(edge_start + num_edge_values);
	int32_t *p_ix = new_indices;
	for (size_t ix = 0; ix < mesh->num_indices; ix++) {
		p_ix[0] = vert_start + vertex_indices[ix];
		p_ix[1] = edge_start + edge_indices[ix];
		p_ix[2] = face_start + topo[ix].face;
		p_ix[3] = edge_start + edge_indices[topo[ix].prev];
		p_ix += 4;
	}

	size_t num_values = num_edge_values + mesh->num_faces + num_vertex_values;
	ufbx_real *new_values = ufbxi_push(&sc->result, ufbx_real, (num_values+1) * reals);
	ufbxi_check_err(&sc->error, new_values);

	memset(new_values, 0, reals * sizeof(ufbx_real));
	new_values += reals;

	memcpy(new_values, values, num_values * reals * sizeof(ufbx_real));

	layer->data = new_values;
	layer->indices = new_indices;
	layer->num_values = num_values;
	layer->value_reals = reals;

	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_subdivide_mesh_level(ufbxi_subdivide_context *sc)
{
	const ufbx_mesh *mesh = &sc->src_mesh;
	ufbx_mesh *result = &sc->dst_mesh;

	*result = *mesh;

	ufbx_topo_edge *topo = ufbxi_push(&sc->tmp, ufbx_topo_edge, mesh->num_indices);
	ufbxi_check_err(&sc->error, topo);
	ufbx_compute_topology(mesh, topo);
	sc->topo = topo;

	ufbxi_check_err(&sc->error, ufbxi_subdivide_layer(sc, (ufbx_vertex_attrib*)&result->vertex_position, sc->opts.boundary));

	memset(&result->vertex_uv, 0, sizeof(result->vertex_uv));
	memset(&result->vertex_tangent, 0, sizeof(result->vertex_tangent));
	memset(&result->vertex_bitangent, 0, sizeof(result->vertex_bitangent));
	memset(&result->vertex_color, 0, sizeof(result->vertex_color));

	result->uv_sets.data = ufbxi_push_copy(&sc->result, ufbx_uv_set, result->uv_sets.count, result->uv_sets.data);
	ufbxi_check_err(&sc->error,	result->uv_sets.data);

	result->color_sets.data = ufbxi_push_copy(&sc->result, ufbx_color_set, result->color_sets.count, result->color_sets.data);
	ufbxi_check_err(&sc->error,	result->color_sets.data);

	ufbxi_for_list(ufbx_uv_set, set, result->uv_sets) {
		ufbxi_check_err(&sc->error, ufbxi_subdivide_layer(sc, (ufbx_vertex_attrib*)&set->vertex_uv, sc->opts.uv_boundary));
		if (sc->opts.interpolate_tangents) {
			ufbxi_check_err(&sc->error, ufbxi_subdivide_layer(sc, (ufbx_vertex_attrib*)&set->vertex_tangent, sc->opts.uv_boundary));
			ufbxi_check_err(&sc->error, ufbxi_subdivide_layer(sc, (ufbx_vertex_attrib*)&set->vertex_bitangent, sc->opts.uv_boundary));
		} else {
			memset(&set->vertex_tangent, 0, sizeof(set->vertex_tangent));
			memset(&set->vertex_bitangent, 0, sizeof(set->vertex_bitangent));
			set->vertex_uv.value_reals = 2;
			set->vertex_tangent.value_reals = 3;
			set->vertex_bitangent.value_reals = 3;
		}
	}

	ufbxi_for_list(ufbx_color_set, set, result->color_sets) {
		ufbxi_check_err(&sc->error, ufbxi_subdivide_layer(sc, (ufbx_vertex_attrib*)&set->vertex_color, sc->opts.uv_boundary));
	}

	if (result->uv_sets.count > 0) {
		result->vertex_uv = result->uv_sets.data[0].vertex_uv;
		result->vertex_bitangent = result->uv_sets.data[0].vertex_bitangent;
		result->vertex_tangent = result->uv_sets.data[0].vertex_tangent;
	}
	if (result->color_sets.count > 0) {
		result->vertex_color = result->color_sets.data[0].vertex_color;
	}

	if (sc->opts.interpolate_normals && !sc->opts.ignore_normals) {
		ufbxi_check_err(&sc->error, ufbxi_subdivide_layer(sc, (ufbx_vertex_attrib*)&mesh->vertex_normal, sc->opts.boundary));
		ufbxi_for(ufbx_vec3, normal, result->vertex_normal.data, result->vertex_normal.num_values) {
			*normal = ufbxi_normalize3(*normal);
		}
		if (mesh->skinned_normal.data == mesh->vertex_normal.data) {
			result->skinned_normal = result->vertex_normal;
		} else {
			ufbxi_check_err(&sc->error, ufbxi_subdivide_layer(sc, (ufbx_vertex_attrib*)&result->skinned_normal, sc->opts.boundary));
			ufbxi_for(ufbx_vec3, normal, result->skinned_normal.data, result->skinned_normal.num_values) {
				*normal = ufbxi_normalize3(*normal);
			}
		}
	}

	// TODO: Vertex crease, we should probably use it?
	ufbxi_subdivide_layer(sc, (ufbx_vertex_attrib*)&mesh->vertex_crease, sc->opts.boundary);

	if (mesh->skinned_position.data == mesh->vertex_position.data) {
		result->skinned_position = result->vertex_position;
	} else {
		ufbxi_check_err(&sc->error, ufbxi_subdivide_layer(sc, (ufbx_vertex_attrib*)&result->skinned_position, sc->opts.boundary));
	}

	result->num_vertices = result->vertex_position.num_values;
	result->vertex_indices = result->vertex_position.indices;
	result->vertices = result->vertex_position.data;

	result->num_indices = mesh->num_indices * 4;
	result->num_faces = mesh->num_indices;
	result->num_triangles = mesh->num_indices * 2;
	result->faces = ufbxi_push(&sc->result, ufbx_face, result->num_faces);
	ufbxi_check_err(&sc->error, result->faces);

	for (size_t i = 0; i < result->num_faces; i++) {
		result->faces[i].index_begin = (uint32_t)(i * 4);
		result->faces[i].num_indices = 4;
	}

	if (mesh->edges) {
		result->num_edges = mesh->num_edges*2 + result->num_faces;
		result->edges = ufbxi_push(&sc->result, ufbx_edge, result->num_edges);
		ufbxi_check_err(&sc->error, result->edges);

		if (mesh->edge_crease) {
			result->edge_crease = ufbxi_push(&sc->result, ufbx_real, result->num_edges);
			ufbxi_check_err(&sc->error, result->edge_crease);
		}
		if (mesh->edge_smoothing) {
			result->edge_smoothing = ufbxi_push(&sc->result, bool, result->num_edges);
			ufbxi_check_err(&sc->error, result->edge_smoothing);
		}

		size_t di = 0;
		for (size_t i = 0; i < mesh->num_edges; i++) {
			ufbx_edge edge = mesh->edges[i];
			int32_t face_ix = topo[edge.indices[0]].face;
			ufbx_face face = mesh->faces[face_ix];
			int32_t offset = edge.indices[0] - face.index_begin;
			int32_t next = (offset + 1) % (int32_t)face.num_indices;

			int32_t a = (face.index_begin + offset) * 4;
			int32_t b = (face.index_begin + next) * 4;

			result->edges[di + 0].indices[0] = a;
			result->edges[di + 0].indices[1] = a + 1;
			result->edges[di + 1].indices[0] = b + 3;
			result->edges[di + 1].indices[1] = b;

			if (mesh->edge_crease) {
				ufbx_real crease = mesh->edge_crease[i];
				if (crease < 0.999f) crease -= 0.1f;
				if (crease < 0.0f) crease = 0.0f;
				result->edge_crease[di + 0] = crease;
				result->edge_crease[di + 1] = crease;
			}

			if (mesh->edge_smoothing) {
				result->edge_smoothing[di + 0] = mesh->edge_smoothing[i];
				result->edge_smoothing[di + 1] = mesh->edge_smoothing[i];
			}

			di += 2;
		}

		for (size_t fi = 0; fi < result->num_faces; fi++) {
			result->edges[di].indices[0] = (int32_t)(fi * 4 + 1);
			result->edges[di].indices[1] = (int32_t)(fi * 4 + 2);

			if (result->edge_crease) {
				result->edge_crease[di] = 0.0f;
			}

			if (result->edge_smoothing) {
				result->edge_smoothing[di + 0] = true;
			}

			di++;
		}
	}

	if (result->face_material) {
		result->face_material = ufbxi_push(&sc->result, int32_t, result->num_faces);
		ufbxi_check_err(&sc->error, result->face_material);
	}
	if (result->face_smoothing) {
		result->face_smoothing = ufbxi_push(&sc->result, bool, result->num_faces);
		ufbxi_check_err(&sc->error, result->face_smoothing);
	}

	size_t num_materials = result->materials.count;
	result->materials.data = ufbxi_push_copy(&sc->result, ufbx_mesh_material, num_materials, result->materials.data);
	ufbxi_check_err(&sc->error, result->materials.data);
	ufbxi_for_list(ufbx_mesh_material, mat, result->materials) {
		mat->num_faces = 0;
		mat->num_triangles = 0;
	}

	size_t index_offset = 0;
	for (size_t i = 0; i < mesh->num_faces; i++) {
		ufbx_face face = mesh->faces[i];

		int32_t mat = 0;
		if (mesh->face_material) {
			mat = mesh->face_material[i];
			for (size_t ci = 0; ci < face.num_indices; ci++) {
				result->face_material[index_offset + ci] = mat;
			}
		}
		if (mat >= 0 && (size_t)mat < num_materials) {
			result->materials.data[mat].num_faces += face.num_indices;
		}
		if (mesh->face_smoothing) {
			bool flag = mesh->face_smoothing[i];
			for (size_t ci = 0; ci < face.num_indices; ci++) {
				result->face_smoothing[index_offset + ci] = flag;
			}
		}
		index_offset += face.num_indices;
	}

	ufbxi_check_err(&sc->error, ufbxi_finalize_mesh(&sc->result, &sc->error, result));

	return 1;
}

ufbxi_nodiscard static ufbxi_noinline int ufbxi_subdivide_mesh_imp(ufbxi_subdivide_context *sc, size_t level)
{
	// `ufbx_subdivide_opts` must be cleared to zero first!
	ufbx_assert(sc->opts._begin_zero == 0 && sc->opts._end_zero == 0);
	ufbxi_check_err_msg(&sc->error, sc->opts._begin_zero == 0 && sc->opts._end_zero == 0, "Uninitialized options");

	if (sc->opts.boundary == UFBX_SUBDIVISION_BOUNDARY_DEFAULT) {
		sc->opts.boundary = sc->src_mesh.subdivision_boundary;
	}

	if (sc->opts.uv_boundary == UFBX_SUBDIVISION_BOUNDARY_DEFAULT) {
		sc->opts.uv_boundary = sc->src_mesh.subdivision_uv_boundary;
	}

	ufbxi_init_ator(&sc->error, &sc->ator_tmp, &sc->opts.temp_allocator);
	ufbxi_init_ator(&sc->error, &sc->ator_result, &sc->opts.result_allocator);

	sc->result.unordered = true;
	sc->source.unordered = true;
	sc->tmp.unordered = true;

	sc->source.ator = &sc->ator_tmp;
	sc->tmp.ator = &sc->ator_tmp;

	for (size_t i = 1; i < level; i++) {
		sc->result.ator = &sc->ator_tmp;

		ufbxi_check_err(&sc->error, ufbxi_subdivide_mesh_level(sc));

		sc->src_mesh = sc->dst_mesh;

		ufbxi_buf_free(&sc->source);
		ufbxi_buf_free(&sc->tmp);
		sc->source = sc->result;
		memset(&sc->result, 0, sizeof(sc->result));
	}

	sc->result.ator = &sc->ator_result;
	ufbxi_check_err(&sc->error, ufbxi_subdivide_mesh_level(sc));
	ufbxi_buf_free(&sc->tmp);

	ufbx_mesh *mesh = &sc->dst_mesh;
	memset(&mesh->vertex_normal, 0, sizeof(mesh->vertex_normal));
	memset(&mesh->skinned_normal, 0, sizeof(mesh->skinned_normal));

	// Subdivision always results in a mesh that consists only of quads
	mesh->max_face_triangles = 2;
	mesh->num_bad_faces = 0;

	// TODO: Move this to an utility?
	mesh->vertex_position.value_reals = 3;
	mesh->vertex_normal.value_reals = 3;
	mesh->vertex_uv.value_reals = 2;
	mesh->vertex_tangent.value_reals = 3;
	mesh->vertex_bitangent.value_reals = 3;
	mesh->vertex_color.value_reals = 4;
	mesh->vertex_crease.value_reals = 1;
	mesh->skinned_position.value_reals = 3;
	mesh->skinned_normal.value_reals = 3;

	if (!sc->opts.interpolate_normals && !sc->opts.ignore_normals) {

		ufbx_topo_edge *topo = ufbxi_push(&sc->tmp, ufbx_topo_edge, mesh->num_indices);
		ufbxi_check_err(&sc->error, topo);
		ufbx_compute_topology(mesh, topo);

		int32_t *normal_indices = ufbxi_push(&sc->result, int32_t, mesh->num_indices);
		ufbxi_check_err(&sc->error, normal_indices);

		size_t num_normals = ufbx_generate_normal_mapping(mesh, topo, normal_indices, true);
		if (num_normals == mesh->num_vertices) {
			mesh->skinned_normal.unique_per_vertex = true;
		}

		ufbx_vec3 *normal_data = ufbxi_push(&sc->result, ufbx_vec3, num_normals + 1);
		ufbxi_check_err(&sc->error, normal_data);
		normal_data[0] = ufbx_zero_vec3;
		normal_data++;

		ufbx_compute_normals(mesh, &mesh->skinned_position, normal_indices, normal_data, num_normals);

		mesh->vertex_normal.data = normal_data;
		mesh->vertex_normal.indices = normal_indices;
		mesh->vertex_normal.num_values = num_normals;
		mesh->vertex_normal.value_reals = 3;

		mesh->skinned_normal = mesh->vertex_normal;
	}

	sc->imp = ufbxi_push(&sc->result, ufbxi_mesh_imp, 1);
	ufbxi_check_err(&sc->error, sc->imp);
	sc->imp->magic = UFBXI_MESH_IMP_MAGIC;
	sc->imp->mesh = sc->dst_mesh;
	sc->imp->ator = sc->ator_result;
	sc->imp->result_buf = sc->result;
	sc->imp->mesh.from_tessellated_nurbs = true;

	return 1;
}

ufbxi_noinline static ufbx_mesh *ufbxi_subdivide_mesh(const ufbx_mesh *mesh, size_t level, const ufbx_subdivide_opts *user_opts, ufbx_error *p_error)
{
	ufbxi_subdivide_context sc = { 0 };
	if (user_opts) {
		sc.opts = *user_opts;
	}

	sc.src_mesh = *mesh;

	int ok = ufbxi_subdivide_mesh_imp(&sc, level);

	ufbxi_buf_free(&sc.tmp);
	ufbxi_buf_free(&sc.source);

	if (ok) {
		ufbxi_free_ator(&sc.ator_tmp);
		if (p_error) {
			p_error->type = UFBX_ERROR_NONE;
			p_error->description = NULL;
			p_error->stack_size = 0;
		}
		return &sc.imp->mesh;
	} else {
		ufbxi_fix_error_type(&sc.error, "Failed to subdivide");
		if (p_error) *p_error = sc.error;
		ufbxi_buf_free(&sc.result);
		ufbxi_free_ator(&sc.ator_tmp);
		ufbxi_free_ator(&sc.ator_result);
		return NULL;
	}
}

// -- Utility

static int ufbxi_map_cmp_vertex(void *user, const void *va, const void *vb)
{
	size_t size = *(size_t*)user;
#if defined(UFBX_REGRESSION)
	ufbx_assert(size % 8 == 0);
#endif
	for (size_t i = 0; i < size; i += 8) {
		uint64_t a = *(const uint64_t*)((const char*)va + i);
		uint64_t b = *(const uint64_t*)((const char*)vb + i);
		if (a != b) return a < b ? -1 : +1;
	}
	return 0;
}

typedef struct {
	char *begin, *ptr;
	size_t vertex_size;
	size_t packed_offset;
} ufbxi_vertex_stream;

static ufbxi_noinline size_t ufbxi_generate_indices(const ufbx_vertex_stream *user_streams, size_t num_streams, uint32_t *indices, size_t num_indices, const ufbx_allocator *allocator, ufbx_error *error)
{
	bool fail = false;

	ufbxi_allocator ator = { 0 };
	ufbxi_init_ator(error, &ator, allocator);

	ufbxi_vertex_stream local_streams[16];
	uint64_t local_packed_vertex[64];

	ufbxi_vertex_stream *streams = NULL;
	if (num_streams > ufbxi_arraycount(local_streams)) {
		streams = ufbxi_alloc(&ator, ufbxi_vertex_stream, num_streams);
		if (!streams) fail = true;
	} else {
		streams = local_streams;
	}

	size_t packed_size = 0;
	if (!fail) {
		for (size_t i = 0; i < num_streams; i++) {
			size_t vertex_size = user_streams[i].vertex_size;
			size_t align = ufbxi_size_align_mask(vertex_size);
			packed_size = ufbxi_align_to_mask(packed_size, align);
			streams[i].ptr = streams[i].begin = (char*)user_streams[i].data;
			streams[i].vertex_size = vertex_size;
			streams[i].packed_offset = packed_size;
			packed_size += vertex_size;
		}
		packed_size = ufbxi_align_to_mask(packed_size, 7);
	}

	char *packed_vertex = NULL;
	if (!fail) {
		if (packed_size > sizeof(local_packed_vertex)) {
			ufbx_assert(packed_size % 8 == 0);
			packed_vertex = (char*)ufbxi_alloc(&ator, uint64_t, packed_size / 8);
			if (!packed_vertex) fail = true;
		} else {
			packed_vertex = (char*)local_packed_vertex;
		}
	}

	ufbxi_map map = { 0 };
	ufbxi_map_init(&map, &ator, &ufbxi_map_cmp_vertex, &packed_size);

	if (num_indices > 0 && !ufbxi_map_grow_size(&map, packed_size, num_indices)) {
		fail = true;
	}

	if (!fail) {
		memset(packed_vertex, 0, packed_size);

		for (size_t i = 0; i < num_indices; i++) {
			for (size_t si = 0; si < num_streams; si++) {
				size_t size = streams[si].vertex_size, offset = streams[si].packed_offset;
				char *ptr = streams[si].ptr;
				memcpy(packed_vertex + offset, ptr, size);
				streams[si].ptr = ptr + size;
			}

			uint32_t hash = ufbxi_hash_string(packed_vertex, packed_size);
			void *entry = ufbxi_map_find_size(&map, packed_size, hash, packed_vertex);
			if (!entry) {
				entry = ufbxi_map_insert_size(&map, packed_size, hash, packed_vertex);
				if (!entry) {
					fail = true;
					break;
				}
				memcpy(entry, packed_vertex, packed_size);
			}
			uint32_t index = (uint32_t)(((char*)entry - (char*)map.items) / packed_size);
			indices[i] = index;
		}
	}

	size_t result_vertices = 0;
	if (!fail) {
		result_vertices = map.size;

		for (size_t si = 0; si < num_streams; si++) {
			size_t vertex_size = streams[si].vertex_size;
			char *dst = streams[si].begin;
			char *src = (char*)map.items + streams[si].packed_offset;
			for (size_t i = 0; i < result_vertices; i++) {
				memcpy(dst, src, vertex_size);
				dst += vertex_size;
				src += packed_size;
			}
		}

		error->stack_size = 0;
		error->description = NULL;
		error->type = UFBX_ERROR_NONE;
	} else {
		ufbxi_fix_error_type(error, "Failed to generate indices");
	}

	if (streams && streams != local_streams) {
		ufbxi_free(&ator, ufbxi_vertex_stream, streams, num_streams);
	}
	if (packed_vertex && packed_vertex != (char*)local_packed_vertex) {
		ufbxi_free(&ator, uint64_t, packed_vertex, packed_size / 8);
	}

	ufbxi_map_free(&map);
	ufbxi_free_ator(&ator);

	return result_vertices;

}

// -- API

#ifdef __cplusplus
extern "C" {
#endif

const ufbx_string ufbx_empty_string = { ufbxi_empty_char, 0 };
const ufbx_matrix ufbx_identity_matrix = { 1,0,0, 0,1,0, 0,0,1, 0,0,0 };
const ufbx_transform ufbx_identity_transform = { {0,0,0}, {0,0,0,1}, {1,1,1} };
const ufbx_vec2 ufbx_zero_vec2 = { 0,0 };
const ufbx_vec3 ufbx_zero_vec3 = { 0,0,0 };
const ufbx_vec4 ufbx_zero_vec4 = { 0,0,0,0 };
const ufbx_quat ufbx_identity_quat = { 0,0,0,1 };

const ufbx_coordinate_axes ufbx_axes_right_handed_y_up = {
	UFBX_COORDINATE_AXIS_POSITIVE_X, UFBX_COORDINATE_AXIS_POSITIVE_Y, UFBX_COORDINATE_AXIS_POSITIVE_Z,
};
const ufbx_coordinate_axes ufbx_axes_right_handed_z_up = {
	UFBX_COORDINATE_AXIS_POSITIVE_X, UFBX_COORDINATE_AXIS_POSITIVE_Z, UFBX_COORDINATE_AXIS_NEGATIVE_Y,
};
const ufbx_coordinate_axes ufbx_axes_left_handed_y_up = {
	UFBX_COORDINATE_AXIS_POSITIVE_X, UFBX_COORDINATE_AXIS_POSITIVE_Y, UFBX_COORDINATE_AXIS_NEGATIVE_Z,
};
const ufbx_coordinate_axes ufbx_axes_left_handed_z_up = {
	UFBX_COORDINATE_AXIS_POSITIVE_X, UFBX_COORDINATE_AXIS_POSITIVE_Z, UFBX_COORDINATE_AXIS_POSITIVE_Y,
};

const size_t ufbx_element_type_size[UFBX_NUM_ELEMENT_TYPES] = {
	sizeof(ufbx_unknown),
	sizeof(ufbx_node),
	sizeof(ufbx_mesh),
	sizeof(ufbx_light),
	sizeof(ufbx_camera),
	sizeof(ufbx_bone),
	sizeof(ufbx_empty),
	sizeof(ufbx_line_curve),
	sizeof(ufbx_nurbs_curve),
	sizeof(ufbx_nurbs_surface),
	sizeof(ufbx_nurbs_trim_surface),
	sizeof(ufbx_nurbs_trim_boundary),
	sizeof(ufbx_procedural_geometry),
	sizeof(ufbx_stereo_camera),
	sizeof(ufbx_camera_switcher),
	sizeof(ufbx_lod_group),
	sizeof(ufbx_skin_deformer),
	sizeof(ufbx_skin_cluster),
	sizeof(ufbx_blend_deformer),
	sizeof(ufbx_blend_channel),
	sizeof(ufbx_blend_shape),
	sizeof(ufbx_cache_deformer),
	sizeof(ufbx_cache_file),
	sizeof(ufbx_material),
	sizeof(ufbx_texture),
	sizeof(ufbx_video),
	sizeof(ufbx_shader),
	sizeof(ufbx_shader_binding),
	sizeof(ufbx_anim_stack),
	sizeof(ufbx_anim_layer),
	sizeof(ufbx_anim_value),
	sizeof(ufbx_anim_curve),
	sizeof(ufbx_display_layer),
	sizeof(ufbx_selection_set),
	sizeof(ufbx_selection_node),
	sizeof(ufbx_character),
	sizeof(ufbx_constraint),
	sizeof(ufbx_pose),
	sizeof(ufbx_metadata_object),
};

bool ufbx_open_file(void *user, ufbx_stream *stream, const char *path, size_t path_len)
{
	(void)user;

	ufbxi_allocator tmp_ator = { 0 };
	ufbx_error tmp_error = { UFBX_ERROR_NONE };
	ufbxi_init_ator(&tmp_error, &tmp_ator, NULL);
	FILE *f = ufbxi_fopen(path, path_len, &tmp_ator);
	if (!f) return false;

	stream->read_fn = &ufbxi_file_read;
	stream->skip_fn = &ufbxi_file_skip;
	stream->close_fn = &ufbxi_file_close;
	stream->user = f;
	return true;
}

ufbx_scene *ufbx_load_memory(const void *data, size_t size, const ufbx_load_opts *opts, ufbx_error *error)
{
	ufbxi_context uc = { UFBX_ERROR_NONE };
	uc.data_begin = uc.data = (const char *)data;
	uc.data_size = size;
	uc.progress_bytes_total = size;
	return ufbxi_load(&uc, opts, error);
}

ufbx_scene *ufbx_load_file(const char *filename, const ufbx_load_opts *opts, ufbx_error *error)
{
	return ufbx_load_file_len(filename, SIZE_MAX, opts, error);
}

ufbx_scene *ufbx_load_file_len(const char *filename, size_t filename_len, const ufbx_load_opts *opts, ufbx_error *error)
{
	ufbxi_allocator tmp_ator = { 0 };
	ufbx_error tmp_error = { UFBX_ERROR_NONE };
	ufbxi_init_ator(&tmp_error, &tmp_ator, opts ? &opts->temp_allocator : NULL);

	FILE *file = ufbxi_fopen(filename, filename_len, &tmp_ator);
	if (!file) {
		if (error) {
			error->stack_size = 1;
			error->type = UFBX_ERROR_FILE_NOT_FOUND;
			error->description = "File not found";
			error->stack[0].description = "File not found";
			error->stack[0].function = __FUNCTION__;
			error->stack[0].source_line = __LINE__;
		}
		return NULL;
	}

	ufbx_load_opts opts_copy;
	if (opts) {
		opts_copy = *opts;
	} else {
		memset(&opts_copy, 0, sizeof(opts_copy));
	}
	if (opts_copy.filename.length == 0 || opts_copy.filename.data == NULL) {
		opts_copy.filename.data = filename;
		opts_copy.filename.length = filename_len;
	}

	ufbx_scene *scene = ufbx_load_stdio(file, &opts_copy, error);

	fclose(file);

	return scene;
}

ufbx_scene *ufbx_load_stdio(void *file_void, const ufbx_load_opts *opts, ufbx_error *error)
{
	FILE *file = (FILE*)file_void;

	ufbxi_context uc = { UFBX_ERROR_NONE };
	uc.read_fn = &ufbxi_file_read;
	uc.skip_fn = &ufbxi_file_skip;
	uc.read_user = file;

	if (opts && opts->progress_fn && opts->file_size_estimate == 0) {
		uint64_t begin = ufbxi_ftell(file);
		if (begin < UINT64_MAX) {
			fpos_t pos;
			if (fgetpos(file, &pos) == 0) {
				if (fseek(file, 0, SEEK_END) == 0) {
					uint64_t end = ufbxi_ftell(file);
					if (end != UINT64_MAX && begin < end) {
						uc.progress_bytes_total = end - begin;
					}

					// Both `rewind()` and `fsetpos()` to reset error and EOF
					rewind(file);
					fsetpos(file, &pos);
				}
			}
		}
	}

	ufbx_scene *scene = ufbxi_load(&uc, opts, error);
	return scene;
}

ufbx_scene *ufbx_load_stream(const void *prefix, size_t prefix_size, const ufbx_stream *stream, const ufbx_load_opts *opts, ufbx_error *error)
{
	ufbxi_context uc = { UFBX_ERROR_NONE };
	uc.data_begin = uc.data = (const char *)prefix;
	uc.data_size = prefix_size;
	uc.read_fn = stream->read_fn;
	uc.skip_fn = stream->skip_fn;
	uc.close_fn = stream->close_fn;
	uc.read_user = stream->user;
	ufbx_scene *scene = ufbxi_load(&uc, opts, error);
	return scene;
}

void ufbx_free_scene(ufbx_scene *scene)
{
	if (!scene) return;

	ufbxi_scene_imp *imp = (ufbxi_scene_imp*)scene;
	ufbx_assert(imp->magic == UFBXI_SCENE_IMP_MAGIC);
	if (imp->magic != UFBXI_SCENE_IMP_MAGIC) return;
	imp->magic = 0;

	ufbxi_buf_free(&imp->string_buf);

	// We need to free `result_buf` last and be careful to copy it to
	// the stack since the `ufbxi_scene_imp` that contains it is allocated
	// from the same result buffer!
	ufbxi_allocator ator = imp->ator;
	ufbxi_buf result = imp->result_buf;
	result.ator = &ator;
	ufbxi_buf_free(&result);
	ufbxi_free_ator(&ator);
}

ufbx_prop *ufbx_find_prop_len(const ufbx_props *props, const char *name, size_t name_len)
{
	uint32_t key = ufbxi_get_name_key(name, name_len);

	do {
		ufbx_prop *prop_data = props->props;
		size_t begin = 0;
		size_t end = props->num_props;
		while (end - begin >= 16) {
			size_t mid = (begin + end) >> 1;
			const ufbx_prop *p = &prop_data[mid];
			if (p->internal_key < key) {
				begin = mid + 1;
			} else { 
				end = mid;
			}
		}

		end = props->num_props;
		for (; begin < end; begin++) {
			const ufbx_prop *p = &prop_data[begin];
			if (p->internal_key > key) break;
			if (p->internal_key == key && p->name.length == name_len && !memcmp(p->name.data, name, name_len)) {
				return (ufbx_prop*)p;
			}
		}

		props = props->defaults;
	} while (props);

	return NULL;
}

ufbx_real ufbx_find_real_len(const ufbx_props *props, const char *name, size_t name_len, ufbx_real def)
{
	ufbx_prop *prop = ufbx_find_prop_len(props, name, name_len);
	if (prop) {
		return prop->value_real;
	} else {
		return def;
	}
}

ufbx_vec3 ufbx_find_vec3_len(const ufbx_props *props, const char *name, size_t name_len, ufbx_vec3 def)
{
	ufbx_prop *prop = ufbx_find_prop_len(props, name, name_len);
	if (prop) {
		return prop->value_vec3;
	} else {
		return def;
	}
}

int64_t ufbx_find_int_len(const ufbx_props *props, const char *name, size_t name_len, int64_t def)
{
	ufbx_prop *prop = ufbx_find_prop_len(props, name, name_len);
	if (prop) {
		return prop->value_int;
	} else {
		return def;
	}
}

bool ufbx_find_bool_len(const ufbx_props *props, const char *name, size_t name_len, bool def)
{
	ufbx_prop *prop = ufbx_find_prop_len(props, name, name_len);
	if (prop) {
		return prop->value_int != 0;
	} else {
		return def;
	}
}

ufbx_string ufbx_find_string_len(const ufbx_props *props, const char *name, size_t name_len, ufbx_string def)
{
	ufbx_prop *prop = ufbx_find_prop_len(props, name, name_len);
	if (prop) {
		return prop->value_str;
	} else {
		return def;
	}
}

ufbx_element *ufbx_find_element_len(ufbx_scene *scene, ufbx_element_type type, const char *name, size_t name_len)
{
	if (!scene) return NULL;
	ufbx_string name_str = { name, name_len };
	uint32_t key = ufbxi_get_name_key(name, name_len);

	size_t index = SIZE_MAX;
	ufbxi_macro_lower_bound_eq(ufbx_name_element, 16, &index, scene->elements_by_name.data, 0, scene->elements_by_name.count,
		( ufbxi_cmp_name_element_less_ref(a, name_str, type, key) ), ( ufbxi_str_equal(a->name, name_str) && a->type == type ));

	return index < SIZE_MAX ? scene->elements_by_name.data[index].element : NULL;
}

ufbx_node *ufbx_find_node_len(ufbx_scene *scene, const char *name, size_t name_len)
{
	return (ufbx_node*)ufbx_find_element_len(scene, UFBX_ELEMENT_NODE, name, name_len);
}

ufbx_anim_stack *ufbx_find_anim_stack_len(ufbx_scene *scene, const char *name, size_t name_len)
{
	return (ufbx_anim_stack*)ufbx_find_element_len(scene, UFBX_ELEMENT_ANIM_STACK, name, name_len);
}

ufbx_anim_prop *ufbx_find_anim_prop_len(ufbx_anim_layer *layer, ufbx_element *element, const char *prop, size_t prop_len)
{
	ufbx_assert(layer);
	ufbx_assert(element);
	if (!layer || !element) return NULL;

	ufbx_string prop_str = { prop, prop_len };

	size_t index = SIZE_MAX;
	ufbxi_macro_lower_bound_eq(ufbx_anim_prop, 16, &index, layer->anim_props.data, 0, layer->anim_props.count,
		( a->element != element ? a->element < element : ufbxi_str_less(a->prop_name, prop_str) ),
		( a->element == element && ufbxi_str_equal(a->prop_name, prop_str) ));

	if (index == SIZE_MAX) return NULL;
	return &layer->anim_props.data[index];
}

ufbx_anim_prop_list ufbx_find_anim_props(ufbx_anim_layer *layer, ufbx_element *element)
{
	ufbx_anim_prop_list result = { 0 };
	ufbx_assert(layer);
	ufbx_assert(element);
	if (!layer || !element) return result;

	size_t begin = layer->anim_props.count, end = begin;
	ufbxi_macro_lower_bound_eq(ufbx_anim_prop, 16, &begin, layer->anim_props.data, 0, layer->anim_props.count,
		( a->element < element ), ( a->element == element ));

	ufbxi_macro_upper_bound_eq(ufbx_anim_prop, 16, &end, layer->anim_props.data, begin, layer->anim_props.count,
		( a->element == element ));

	if (begin != end) {
		result.data = layer->anim_props.data + begin;
		result.count = end - begin;
	}

	return result;
}

ufbx_matrix ufbx_get_compatible_matrix_for_normals(ufbx_node *node)
{
	if (!node) return ufbx_identity_matrix;

	ufbx_transform geom_rot = ufbx_identity_transform;
	geom_rot.rotation = node->geometry_transform.rotation;
	ufbx_matrix geom_rot_mat = ufbx_transform_to_matrix(&geom_rot);

	ufbx_matrix norm_mat = ufbx_matrix_mul(&node->node_to_world, &geom_rot_mat);
	norm_mat = ufbx_matrix_for_normals(&norm_mat);
	return norm_mat;
}

ufbx_real ufbx_evaluate_curve(const ufbx_anim_curve *curve, double time, ufbx_real default_value)
{
	if (!curve) return default_value;
	if (curve->keyframes.count <= 1) {
		if (curve->keyframes.count == 1) {
			return curve->keyframes.data[0].value;
		} else {
			return default_value;
		}
	}

	size_t begin = 0;
	size_t end = curve->keyframes.count;
	const ufbx_keyframe *keys = curve->keyframes.data;
	while (end - begin >= 8) {
		size_t mid = (begin + end) >> 1;
		if (keys[mid].time < time) {
			begin = mid + 1;
		} else { 
			end = mid;
		}
	}

	end = curve->keyframes.count;
	for (; begin < end; begin++) {
		const ufbx_keyframe *next = &keys[begin];
		if (next->time < time) continue;

		// First keyframe
		if (begin == 0) return next->value;

		const ufbx_keyframe *prev = next - 1;

		double rcp_delta = 1.0 / (next->time - prev->time);
		double t = (time - prev->time) * rcp_delta;

		switch (prev->interpolation) {

		case UFBX_INTERPOLATION_CONSTANT_PREV:
			return prev->value;

		case UFBX_INTERPOLATION_CONSTANT_NEXT:
			return next->value;

		case UFBX_INTERPOLATION_LINEAR:
			return prev->value*(1.0 - t) + next->value*t;

		case UFBX_INTERPOLATION_CUBIC:
		{
			double x1 = prev->right.dx * rcp_delta;
			double x2 = 1.0 - next->left.dx * rcp_delta;
			t = ufbxi_find_cubic_bezier_t(x1, x2, t);

			double t2 = t*t, t3 = t2*t;
			double u = 1.0 - t, u2 = u*u, u3 = u2*u;

			double y0 = prev->value;
			double y3 = next->value;
			double y1 = y0 + prev->right.dy;
			double y2 = y3 - next->left.dy;

			return u3*y0 + 3.0 * (u2*t*y1 + u*t2*y2) + t3*y3;
		}

		}
	}

	// Last keyframe
	return curve->keyframes.data[curve->keyframes.count - 1].value;
}

ufbx_real ufbx_evaluate_anim_value_real(const ufbx_anim_value *anim_value, double time)
{
	if (!anim_value) {
		return 0.0f;
	}

	ufbx_real res = anim_value->default_value.x;
	if (anim_value->curves[0]) res = ufbx_evaluate_curve(anim_value->curves[0], time, res);
	return res;
}

ufbx_vec2 ufbx_evaluate_anim_value_vec2(const ufbx_anim_value *anim_value, double time)
{
	if (!anim_value) {
		ufbx_vec2 zero = { 0.0f };
		return zero;
	}

	ufbx_vec2 res = { anim_value->default_value.x, anim_value->default_value.y };
	if (anim_value->curves[0]) res.x = ufbx_evaluate_curve(anim_value->curves[0], time, res.x);
	if (anim_value->curves[1]) res.y = ufbx_evaluate_curve(anim_value->curves[1], time, res.y);
	return res;
}

ufbx_vec3 ufbx_evaluate_anim_value_vec3(const ufbx_anim_value *anim_value, double time)
{
	if (!anim_value) {
		ufbx_vec3 zero = { 0.0f };
		return zero;
	}

	ufbx_vec3 res = anim_value->default_value;
	if (anim_value->curves[0]) res.x = ufbx_evaluate_curve(anim_value->curves[0], time, res.x);
	if (anim_value->curves[1]) res.y = ufbx_evaluate_curve(anim_value->curves[1], time, res.y);
	if (anim_value->curves[2]) res.z = ufbx_evaluate_curve(anim_value->curves[2], time, res.z);
	return res;
}

ufbx_prop ufbx_evaluate_prop_len(const ufbx_anim *anim, const ufbx_element *element, const char *name, size_t name_len, double time)
{
	ufbx_prop result;

	ufbx_prop *prop = ufbx_find_prop_len(&element->props, name, name_len);
	if (prop) {
		result = *prop;
	} else {
		memset(&result, 0, sizeof(result));
		result.name.data = name;
		result.name.length = name_len;
		result.internal_key = ufbxi_get_name_key(name, name_len);
		result.flags = UFBX_PROP_FLAG_NOT_FOUND;
	}

	if (anim->prop_overrides.count > 0) {
		ufbxi_find_prop_override(&anim->prop_overrides, element->element_id, &result);
		return result;
	}

	if ((result.flags & (UFBX_PROP_FLAG_ANIMATED|UFBX_PROP_FLAG_CONNECTED)) == 0) return result;

	if ((prop->flags & UFBX_PROP_FLAG_CONNECTED) != 0 && !anim->ignore_connections) {
		result.value_vec3 = ufbxi_evaluate_connected_prop(anim, element, prop->name.data, time);
	}

	ufbxi_evaluate_props(anim, element, time, &result, 1);

	return result;
}

ufbx_props ufbx_evaluate_props(const ufbx_anim *anim, ufbx_element *element, double time, ufbx_prop *buffer, size_t buffer_size)
{
	ufbx_props ret = { NULL };
	if (!element) return ret;

	const ufbx_prop_override *over = NULL, *over_end = NULL;
	if (anim->prop_overrides.count > 0) {
		ufbx_const_prop_override_list list = ufbxi_find_element_prop_overrides(&anim->prop_overrides, element->element_id);
		over = list.data;
		over_end = over + list.count;
	}

	size_t num_anim = 0;
	ufbxi_for(ufbx_prop, prop, element->props.props, element->props.num_props) {
		bool found_override = false;
		for (; over != over_end && num_anim < buffer_size; over++) {
			ufbx_prop *dst = &buffer[num_anim];
			if (over->internal_key < prop->internal_key
				|| (over->internal_key == prop->internal_key && strcmp(over->prop_name, prop->name.data) < 0)) {
				dst->name = ufbxi_str_c(over->prop_name);
				dst->internal_key = over->internal_key;
				dst->type = UFBX_PROP_UNKNOWN;
				dst->flags = UFBX_PROP_FLAG_OVERRIDDEN;
			} else if (over->internal_key == prop->internal_key && strcmp(over->prop_name, prop->name.data) == 0) {
				*dst = *prop;
				dst->flags = (ufbx_prop_flags)(dst->flags | UFBX_PROP_FLAG_OVERRIDDEN);
			} else {
				break;
			}
			dst->value_str = ufbxi_str_c(over->value_str);
			dst->value_int = over->value_int;
			dst->value_vec3 = over->value;
			num_anim++;
			found_override = true;
		}

		if (!(prop->flags & (UFBX_PROP_FLAG_ANIMATED|UFBX_PROP_FLAG_CONNECTED))) continue;
		if (num_anim >= buffer_size) break;
		if (found_override) continue;

		ufbx_prop *dst = &buffer[num_anim++];
		*dst = *prop;

		if ((prop->flags & UFBX_PROP_FLAG_CONNECTED) != 0 && !anim->ignore_connections) {
			dst->value_vec3 = ufbxi_evaluate_connected_prop(anim, element, prop->name.data, time);
		}
	}

	for (; over != over_end && num_anim < buffer_size; over++) {
		ufbx_prop *dst = &buffer[num_anim++];
		dst->name = ufbxi_str_c(over->prop_name);
		dst->internal_key = over->internal_key;
		dst->type = UFBX_PROP_UNKNOWN;
		dst->flags = UFBX_PROP_FLAG_OVERRIDDEN;
		dst->value_str = ufbxi_str_c(over->value_str);
		dst->value_int = over->value_int;
		dst->value_vec3 = over->value;
	}

	ufbxi_evaluate_props(anim, element, time, buffer, num_anim);

	ret.props = buffer;
	ret.num_props = ret.num_animated = num_anim;
	ret.defaults = &element->props;
	return ret;
}

ufbx_transform ufbx_evaluate_transform(const ufbx_anim *anim, const ufbx_node *node, double time)
{
	ufbx_assert(anim);
	ufbx_assert(node);
	if (!node) return ufbx_identity_transform;
	if (!anim) return node->local_transform;
	if (node->is_root) return node->local_transform;

	const char *prop_names[] = {
		ufbxi_Lcl_Rotation,
		ufbxi_Lcl_Scaling,
		ufbxi_Lcl_Translation,
		ufbxi_PostRotation,
		ufbxi_PreRotation,
		ufbxi_RotationOffset,
		ufbxi_RotationOrder,
		ufbxi_RotationPivot,
		ufbxi_ScalingOffset,
		ufbxi_ScalingPivot,
	};

	ufbx_prop buf[ufbxi_arraycount(prop_names)];
	ufbx_props props = ufbxi_evaluate_selected_props(anim, &node->element, time, buf, prop_names, ufbxi_arraycount(prop_names));
	ufbx_rotation_order order = (ufbx_rotation_order)ufbxi_find_enum(&props, ufbxi_RotationOrder, UFBX_ROTATION_XYZ, UFBX_ROTATION_SPHERIC);
	return ufbxi_get_transform(&props, order);
}

ufbx_real ufbx_evaluate_blend_weight(const ufbx_anim *anim, const ufbx_blend_channel *channel, double time)
{
	const char *prop_names[] = {
		ufbxi_DeformPercent,
	};

	ufbx_prop buf[ufbxi_arraycount(prop_names)];
	ufbx_props props = ufbxi_evaluate_selected_props(anim, &channel->element, time, buf, prop_names, ufbxi_arraycount(prop_names));
	return ufbxi_find_real(&props, ufbxi_DeformPercent, channel->weight * (ufbx_real)100.0) * (ufbx_real)0.01;
}

ufbx_const_prop_override_list ufbx_prepare_prop_overrides(ufbx_prop_override *overrides, size_t num_overrides)
{
	ufbxi_for(ufbx_prop_override, over, overrides, num_overrides) {
		if (over->prop_name == NULL) {
			over->prop_name = ufbxi_empty_char;
		}
		if (over->value_str == NULL) {
			over->value_str = ufbxi_empty_char;
		}
		if (over->value_int == 0) {
			over->value_int = (int64_t)over->value.x;
		} else if (over->value.x == 0.0) {
			over->value.x = (ufbx_real)over->value_int;
		}

		size_t len = strlen(over->prop_name);
		over->prop_name = ufbxi_find_canonical_string(over->prop_name, len);
		over->internal_key = ufbxi_get_name_key(over->prop_name, len);
	}

	// TODO: Macro for non-stable sort
	qsort(overrides, num_overrides, sizeof(ufbx_prop_override), &ufbxi_cmp_prop_override);

	ufbx_const_prop_override_list result;
	result.data = overrides;
	result.count = num_overrides;
	return result;
}

ufbx_scene *ufbx_evaluate_scene(ufbx_scene *scene, const ufbx_anim *anim, double time, const ufbx_evaluate_opts *opts, ufbx_error *error)
{
	ufbxi_eval_context ec = { 0 };
	return ufbxi_evaluate_scene(&ec, scene, anim, time, opts, error);
}

ufbx_texture *ufbx_find_prop_texture_len(const ufbx_material *material, const char *name, size_t name_len)
{
	ufbx_string name_str = { name, name_len };
	if (!material) return NULL;

	size_t index = SIZE_MAX;
	ufbxi_macro_lower_bound_eq(ufbx_material_texture, 4, &index, material->textures.data, 0, material->textures.count, 
		( ufbxi_str_less(a->material_prop, name_str) ), ( ufbxi_str_equal(a->material_prop, name_str) ));
	return index < SIZE_MAX ? material->textures.data[index].texture : NULL;
}

ufbx_string ufbx_find_shader_prop_len(const ufbx_shader *shader, const char *name, size_t name_len)
{
	ufbx_string name_str = { name, name_len };
	if (!shader) return name_str;

	ufbxi_for_ptr_list(ufbx_shader_binding, p_bind, shader->bindings) {
		ufbx_shader_binding *bind = *p_bind;

		size_t index = SIZE_MAX;
		ufbxi_macro_lower_bound_eq(ufbx_shader_prop_binding, 4, &index, bind->prop_bindings.data, 0, bind->prop_bindings.count, 
			( ufbxi_str_less(a->shader_prop, name_str) ), ( ufbxi_str_equal(a->shader_prop, name_str) ));
		if (index != SIZE_MAX) {
			return bind->prop_bindings.data[index].material_prop;
		}
	}

	return name_str;
}

bool ufbx_coordinate_axes_valid(ufbx_coordinate_axes axes)
{
	if (axes.right < UFBX_COORDINATE_AXIS_POSITIVE_X || axes.right > UFBX_COORDINATE_AXIS_NEGATIVE_Z) return false;
	if (axes.up < UFBX_COORDINATE_AXIS_POSITIVE_X || axes.up > UFBX_COORDINATE_AXIS_NEGATIVE_Z) return false;
	if (axes.front < UFBX_COORDINATE_AXIS_POSITIVE_X || axes.front > UFBX_COORDINATE_AXIS_NEGATIVE_Z) return false;

	// Check that all the positive/negative axes are used
	uint32_t mask = 0;
	mask |= 1u << ((uint32_t)axes.right >> 1);
	mask |= 1u << ((uint32_t)axes.up >> 1);
	mask |= 1u << ((uint32_t)axes.front >> 1);
	return (mask & 0x7u) == 0x7u;
}

ufbx_quat ufbx_quat_mul(ufbx_quat a, ufbx_quat b)
{
	return ufbxi_mul_quat(a, b);
}

ufbx_real ufbx_quat_dot(ufbx_quat a, ufbx_quat b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

ufbx_quat ufbx_quat_normalize(ufbx_quat q)
{
	ufbx_real norm = ufbx_quat_dot(q, q);
	if (norm == 0.0) return ufbx_identity_quat;
	q.x /= norm;
	q.y /= norm;
	q.z /= norm;
	q.w /= norm;
	return q;
}

ufbx_quat ufbx_quat_fix_antipodal(ufbx_quat q, ufbx_quat reference)
{
	if (ufbx_quat_dot(q, reference) < 0.0f) {
		q.x = -q.x; q.y = -q.y; q.z = -q.z; q.w = -q.w;
	}
	return q;
}

ufbx_quat ufbx_quat_slerp(ufbx_quat a, ufbx_quat b, ufbx_real t)
{
	double dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
	if (dot < 0.0) {
		dot = -dot;
		b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w;
	}
	double omega = acos(fmin(fmax(dot, 0.0), 1.0));
	if (omega <= 1.175494351e-38f) return a;
	double rcp_so = 1.0 / sin(omega);
	double af = sin((1.0 - t) * omega) * rcp_so;
	double bf = sin(t * omega) * rcp_so;

	double x = af*a.x + bf*b.x;
	double y = af*a.y + bf*b.y;
	double z = af*a.z + bf*b.z;
	double w = af*a.w + bf*b.w;
	double rcp_len = 1.0 / sqrt(x*x + y*y + z*z + w*w);

	ufbx_quat ret;
	ret.x = (ufbx_real)(x * rcp_len);
	ret.y = (ufbx_real)(y * rcp_len);
	ret.z = (ufbx_real)(z * rcp_len);
	ret.w = (ufbx_real)(w * rcp_len);
	return ret;
}

ufbx_vec3 ufbx_quat_rotate_vec3(ufbx_quat q, ufbx_vec3 v)
{
	ufbx_real xy = q.x*v.y - q.y*v.x;
	ufbx_real xz = q.x*v.z - q.z*v.x;
	ufbx_real yz = q.y*v.z - q.z*v.y;
	ufbx_vec3 r;
	r.x = 2.0 * (+ q.w*yz + q.y*xy + q.z*xz) + v.x;
	r.y = 2.0 * (- q.x*xy - q.w*xz + q.z*yz) + v.y;
	r.z = 2.0 * (- q.x*xz - q.y*yz + q.w*xy) + v.z;
	return r;
}

ufbx_quat ufbx_euler_to_quat(ufbx_vec3 v, ufbx_rotation_order order)
{
	v.x *= UFBXI_DEG_TO_RAD * 0.5;
	v.y *= UFBXI_DEG_TO_RAD * 0.5;
	v.z *= UFBXI_DEG_TO_RAD * 0.5;
	ufbx_real cx = (ufbx_real)cos(v.x), sx = (ufbx_real)sin(v.x);
	ufbx_real cy = (ufbx_real)cos(v.y), sy = (ufbx_real)sin(v.y);
	ufbx_real cz = (ufbx_real)cos(v.z), sz = (ufbx_real)sin(v.z);
	ufbx_quat q;

	// Generated by `misc/gen_rotation_order.py`
	switch (order) {
	case UFBX_ROTATION_XYZ:
		q.x = -cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy + cy*sx*sz;
		q.z = cx*cy*sz - cz*sx*sy;
		q.w = cx*cy*cz + sx*sy*sz;
		break;
	case UFBX_ROTATION_XZY:
		q.x = cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy + cy*sx*sz;
		q.z = cx*cy*sz - cz*sx*sy;
		q.w = cx*cy*cz - sx*sy*sz;
		break;
	case UFBX_ROTATION_YZX:
		q.x = -cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy - cy*sx*sz;
		q.z = cx*cy*sz + cz*sx*sy;
		q.w = cx*cy*cz + sx*sy*sz;
		break;
	case UFBX_ROTATION_YXZ:
		q.x = -cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy + cy*sx*sz;
		q.z = cx*cy*sz + cz*sx*sy;
		q.w = cx*cy*cz - sx*sy*sz;
		break;
	case UFBX_ROTATION_ZXY:
		q.x = cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy - cy*sx*sz;
		q.z = cx*cy*sz - cz*sx*sy;
		q.w = cx*cy*cz + sx*sy*sz;
		break;
	case UFBX_ROTATION_ZYX:
		q.x = cx*sy*sz + cy*cz*sx;
		q.y = cx*cz*sy - cy*sx*sz;
		q.z = cx*cy*sz + cz*sx*sy;
		q.w = cx*cy*cz - sx*sy*sz;
		break;
	default:
		q.x = q.y = q.z = 0.0; q.w = 1.0;
		break;
	}

	return q;
}

ufbx_vec3 ufbx_quat_to_euler(ufbx_quat q, ufbx_rotation_order order)
{
	const double eps = 0.999999999;

	ufbx_vec3 v;
	double t;

	// Generated by `misc/gen_quat_to_euler.py`
	switch (order) {
	case UFBX_ROTATION_XYZ:
		t = 2.0f*(q.w*q.y - q.x*q.z);
		if (fabs(t) < eps) {
			v.y = (ufbx_real)asin(t);
			v.z = (ufbx_real)atan2(2.0f*(q.w*q.z + q.x*q.y), 2.0f*(q.w*q.w + q.x*q.x) - 1.0f);
			v.x = (ufbx_real)-atan2(-2.0f*(q.w*q.x + q.y*q.z), 2.0f*(q.w*q.w + q.z*q.z) - 1.0f);
		} else {
			v.y = (ufbx_real)copysign(UFBXI_PI*0.5f, t);
			v.z = (ufbx_real)(atan2(-2.0f*t*(q.w*q.x - q.y*q.z), t*(2.0f*q.w*q.y + 2.0f*q.x*q.z)));
			v.x = 0.0f;
		}
		break;
	case UFBX_ROTATION_XZY:
		t = 2.0f*(q.w*q.z + q.x*q.y);
		if (fabs(t) < eps) {
			v.z = (ufbx_real)asin(t);
			v.y = (ufbx_real)atan2(2.0f*(q.w*q.y - q.x*q.z), 2.0f*(q.w*q.w + q.x*q.x) - 1.0f);
			v.x = (ufbx_real)-atan2(-2.0f*(q.w*q.x - q.y*q.z), 2.0f*(q.w*q.w + q.y*q.y) - 1.0f);
		} else {
			v.z = (ufbx_real)copysign(UFBXI_PI*0.5f, t);
			v.y = (ufbx_real)(atan2(2.0f*t*(q.w*q.x + q.y*q.z), -t*(2.0f*q.x*q.y - 2.0f*q.w*q.z)));
			v.x = 0.0f;
		}
		break;
	case UFBX_ROTATION_YZX:
		t = 2.0f*(q.w*q.z - q.x*q.y);
		if (fabs(t) < eps) {
			v.z = (ufbx_real)asin(t);
			v.x = (ufbx_real)atan2(2.0f*(q.w*q.x + q.y*q.z), 2.0f*(q.w*q.w + q.y*q.y) - 1.0f);
			v.y = (ufbx_real)-atan2(-2.0f*(q.w*q.y + q.x*q.z), 2.0f*(q.w*q.w + q.x*q.x) - 1.0f);
		} else {
			v.z = (ufbx_real)copysign(UFBXI_PI*0.5f, t);
			v.x = (ufbx_real)(atan2(-2.0f*t*(q.w*q.y - q.x*q.z), t*(2.0f*q.w*q.z + 2.0f*q.x*q.y)));
			v.y = 0.0f;
		}
		break;
	case UFBX_ROTATION_YXZ:
		t = 2.0f*(q.w*q.x + q.y*q.z);
		if (fabs(t) < eps) {
			v.x = (ufbx_real)asin(t);
			v.z = (ufbx_real)atan2(2.0f*(q.w*q.z - q.x*q.y), 2.0f*(q.w*q.w + q.y*q.y) - 1.0f);
			v.y = (ufbx_real)-atan2(-2.0f*(q.w*q.y - q.x*q.z), 2.0f*(q.w*q.w + q.z*q.z) - 1.0f);
		} else {
			v.x = (ufbx_real)copysign(UFBXI_PI*0.5f, t);
			v.z = (ufbx_real)(atan2(2.0f*t*(q.w*q.y + q.x*q.z), -t*(2.0f*q.y*q.z - 2.0f*q.w*q.x)));
			v.y = 0.0f;
		}
		break;
	case UFBX_ROTATION_ZXY:
		t = 2.0f*(q.w*q.x - q.y*q.z);
		if (fabs(t) < eps) {
			v.x = (ufbx_real)asin(t);
			v.y = (ufbx_real)atan2(2.0f*(q.w*q.y + q.x*q.z), 2.0f*(q.w*q.w + q.z*q.z) - 1.0f);
			v.z = (ufbx_real)-atan2(-2.0f*(q.w*q.z + q.x*q.y), 2.0f*(q.w*q.w + q.y*q.y) - 1.0f);
		} else {
			v.x = (ufbx_real)copysign(UFBXI_PI*0.5f, t);
			v.y = (ufbx_real)(atan2(-2.0f*t*(q.w*q.z - q.x*q.y), t*(2.0f*q.w*q.x + 2.0f*q.y*q.z)));
			v.z = 0.0f;
		}
		break;
	case UFBX_ROTATION_ZYX:
		t = 2.0f*(q.w*q.y + q.x*q.z);
		if (fabs(t) < eps) {
			v.y = (ufbx_real)asin(t);
			v.x = (ufbx_real)atan2(2.0f*(q.w*q.x - q.y*q.z), 2.0f*(q.w*q.w + q.z*q.z) - 1.0f);
			v.z = (ufbx_real)-atan2(-2.0f*(q.w*q.z - q.x*q.y), 2.0f*(q.w*q.w + q.x*q.x) - 1.0f);
		} else {
			v.y = (ufbx_real)copysign(UFBXI_PI*0.5f, t);
			v.x = (ufbx_real)(atan2(2.0f*t*(q.w*q.z + q.x*q.y), -t*(2.0f*q.x*q.z - 2.0f*q.w*q.y)));
			v.z = 0.0f;
		}
		break;
	default:
		v.x = v.y = v.z = 0.0;
		break;
	}

	v.x *= UFBXI_RAD_TO_DEG;
	v.y *= UFBXI_RAD_TO_DEG;
	v.z *= UFBXI_RAD_TO_DEG;
	return v;
}

size_t ufbx_format_error(char *dst, size_t dst_size, const ufbx_error *error)
{
	if (!dst || !dst_size) return 0;
	if (!error) {
		*dst = '\0';
		return 0;
	}

	size_t offset = 0;

	{
		int num = snprintf(dst + offset, dst_size - offset, "ufbx v%u.%u.%u error: %s\n",
			UFBX_SOURCE_VERSION/1000000, UFBX_SOURCE_VERSION/1000%1000, UFBX_SOURCE_VERSION%1000,
			error->description ? error->description : "Unknown error");
		if (num > 0) offset = ufbxi_min_sz(offset + (size_t)num, dst_size - 1);
	}

	size_t stack_size = ufbxi_min_sz(error->stack_size, UFBX_ERROR_STACK_MAX_DEPTH);
	for (size_t i = 0; i < stack_size; i++) {
		const ufbx_error_frame *frame = &error->stack[i];
		int num = snprintf(dst + offset, dst_size - offset, "%6u:%s: %s\n", frame->source_line, frame->function, frame->description);
		if (num > 0) offset = ufbxi_min_sz(offset + (size_t)num, dst_size - 1);
	}

	// HACK: On some MSYS/MinGW implementations `snprintf` is broken and does
	// not write the null terminator on trunctation, it's always safe to do so
	// let's just do it unconditionally here...
	dst[offset] = '\0';

	return offset;
}

ufbx_matrix ufbx_matrix_mul(const ufbx_matrix *a, const ufbx_matrix *b)
{
	ufbx_matrix dst;

	dst.m03 = a->m00*b->m03 + a->m01*b->m13 + a->m02*b->m23 + a->m03;
	dst.m13 = a->m10*b->m03 + a->m11*b->m13 + a->m12*b->m23 + a->m13;
	dst.m23 = a->m20*b->m03 + a->m21*b->m13 + a->m22*b->m23 + a->m23;

	dst.m00 = a->m00*b->m00 + a->m01*b->m10 + a->m02*b->m20;
	dst.m10 = a->m10*b->m00 + a->m11*b->m10 + a->m12*b->m20;
	dst.m20 = a->m20*b->m00 + a->m21*b->m10 + a->m22*b->m20;

	dst.m01 = a->m00*b->m01 + a->m01*b->m11 + a->m02*b->m21;
	dst.m11 = a->m10*b->m01 + a->m11*b->m11 + a->m12*b->m21;
	dst.m21 = a->m20*b->m01 + a->m21*b->m11 + a->m22*b->m21;

	dst.m02 = a->m00*b->m02 + a->m01*b->m12 + a->m02*b->m22;
	dst.m12 = a->m10*b->m02 + a->m11*b->m12 + a->m12*b->m22;
	dst.m22 = a->m20*b->m02 + a->m21*b->m12 + a->m22*b->m22;

	return dst;
}

ufbx_real ufbx_matrix_determinant(const ufbx_matrix *m)
{
	return
		- m->m02*m->m11*m->m20 + m->m01*m->m12*m->m20 + m->m02*m->m10*m->m21
		- m->m00*m->m12*m->m21 - m->m01*m->m10*m->m22 + m->m00*m->m11*m->m22;
}

ufbx_matrix ufbx_matrix_invert(const ufbx_matrix *m)
{
	ufbx_real det = ufbx_matrix_determinant(m);

	ufbx_matrix r;
	if (det == 0.0) {
		memset(&r, 0, sizeof(r));
		return r;
	}

	ufbx_real rcp_det = 1.0 / det;

	r.m00 = ( - m->m12*m->m21 + m->m11*m->m22) * rcp_det;
	r.m10 = ( + m->m12*m->m20 - m->m10*m->m22) * rcp_det;
	r.m20 = ( - m->m11*m->m20 + m->m10*m->m21) * rcp_det;
	r.m01 = ( + m->m02*m->m21 - m->m01*m->m22) * rcp_det;
	r.m11 = ( - m->m02*m->m20 + m->m00*m->m22) * rcp_det;
	r.m21 = ( + m->m01*m->m20 - m->m00*m->m21) * rcp_det;
	r.m02 = ( - m->m02*m->m11 + m->m01*m->m12) * rcp_det;
	r.m12 = ( + m->m02*m->m10 - m->m00*m->m12) * rcp_det;
	r.m22 = ( - m->m01*m->m10 + m->m00*m->m11) * rcp_det;
	r.m03 = (m->m03*m->m12*m->m21 - m->m02*m->m13*m->m21 - m->m03*m->m11*m->m22 + m->m01*m->m13*m->m22 + m->m02*m->m11*m->m23 - m->m01*m->m12*m->m23) * rcp_det;
	r.m13 = (m->m02*m->m13*m->m20 - m->m03*m->m12*m->m20 + m->m03*m->m10*m->m22 - m->m00*m->m13*m->m22 - m->m02*m->m10*m->m23 + m->m00*m->m12*m->m23) * rcp_det;
	r.m23 = (m->m03*m->m11*m->m20 - m->m01*m->m13*m->m20 - m->m03*m->m10*m->m21 + m->m00*m->m13*m->m21 + m->m01*m->m10*m->m23 - m->m00*m->m11*m->m23) * rcp_det;

	return r;
}

ufbx_matrix ufbx_matrix_for_normals(const ufbx_matrix *m)
{
	ufbx_real det = ufbx_matrix_determinant(m);

	ufbx_matrix r;
	if (det == 0.0) {
		memset(&r, 0, sizeof(r));
		return r;
	}

	ufbx_real rcp_det = 1.0 / det;

	r.m00 = ( - m->m12*m->m21 + m->m11*m->m22) * rcp_det;
	r.m01 = ( + m->m12*m->m20 - m->m10*m->m22) * rcp_det;
	r.m02 = ( - m->m11*m->m20 + m->m10*m->m21) * rcp_det;
	r.m10 = ( + m->m02*m->m21 - m->m01*m->m22) * rcp_det;
	r.m11 = ( - m->m02*m->m20 + m->m00*m->m22) * rcp_det;
	r.m12 = ( + m->m01*m->m20 - m->m00*m->m21) * rcp_det;
	r.m20 = ( - m->m02*m->m11 + m->m01*m->m12) * rcp_det;
	r.m21 = ( + m->m02*m->m10 - m->m00*m->m12) * rcp_det;
	r.m22 = ( - m->m01*m->m10 + m->m00*m->m11) * rcp_det;
	r.m03 = r.m13 = r.m23 = 0.0;

	return r;
}

ufbx_vec3 ufbx_transform_position(const ufbx_matrix *m, ufbx_vec3 v)
{
	ufbx_vec3 r;
	r.x = m->m00*v.x + m->m01*v.y + m->m02*v.z + m->m03;
	r.y = m->m10*v.x + m->m11*v.y + m->m12*v.z + m->m13;
	r.z = m->m20*v.x + m->m21*v.y + m->m22*v.z + m->m23;
	return r;
}

ufbx_vec3 ufbx_transform_direction(const ufbx_matrix *m, ufbx_vec3 v)
{
	ufbx_vec3 r;
	r.x = m->m00*v.x + m->m01*v.y + m->m02*v.z;
	r.y = m->m10*v.x + m->m11*v.y + m->m12*v.z;
	r.z = m->m20*v.x + m->m21*v.y + m->m22*v.z;
	return r;
}

ufbx_matrix ufbx_transform_to_matrix(const ufbx_transform *t)
{
	ufbx_quat q = t->rotation;
	ufbx_real sx = 2.0 * t->scale.x, sy = 2.0 * t->scale.y, sz = 2.0 * t->scale.z;
	ufbx_real xx = q.x*q.x, xy = q.x*q.y, xz = q.x*q.z, xw = q.x*q.w;
	ufbx_real yy = q.y*q.y, yz = q.y*q.z, yw = q.y*q.w;
	ufbx_real zz = q.z*q.z, zw = q.z*q.w;
	ufbx_matrix m;
	m.m00 = sx * (- yy - zz + 0.5);
	m.m10 = sx * (+ xy + zw);
	m.m20 = sx * (- yw + xz);
	m.m01 = sy * (- zw + xy);
	m.m11 = sy * (- xx - zz + 0.5);
	m.m21 = sy * (+ xw + yz);
	m.m02 = sz * (+ xz + yw);
	m.m12 = sz * (- xw + yz);
	m.m22 = sz * (- xx - yy + 0.5);
	m.m03 = t->translation.x;
	m.m13 = t->translation.y;
	m.m23 = t->translation.z;
	return m;
}

ufbx_transform ufbx_matrix_to_transform(const ufbx_matrix *m)
{
	ufbx_real det = ufbx_matrix_determinant(m);

	ufbx_transform t;
	t.translation = m->cols[3];
	t.scale.x = ufbxi_length3(m->cols[0]);
	t.scale.y = ufbxi_length3(m->cols[1]);
	t.scale.z = ufbxi_length3(m->cols[2]);

	// Flip a single non-zero axis if negative determinant
	ufbx_real sign_x = 1.0f;
	ufbx_real sign_y = 1.0f;
	ufbx_real sign_z = 1.0f;
	if (det < 0.0f) {
		if (t.scale.x > 0.0f) sign_x = -1.0f;
		else if (t.scale.y > 0.0f) sign_y = -1.0f;
		else if (t.scale.z > 0.0f) sign_z = -1.0f;
	}

	ufbx_vec3 x = ufbxi_mul3(m->cols[0], t.scale.x > 0.0f ? sign_x / t.scale.x : 0.0f);
	ufbx_vec3 y = ufbxi_mul3(m->cols[1], t.scale.y > 0.0f ? sign_y / t.scale.y : 0.0f);
	ufbx_vec3 z = ufbxi_mul3(m->cols[2], t.scale.z > 0.0f ? sign_z / t.scale.z : 0.0f);
	ufbx_real trace = x.x + y.y + z.z;
	if (trace > 0.0f) {
		ufbx_real a = (ufbx_real)sqrt(fmax(0.0, trace + 1.0)), b = (a != 0.0f) ? 0.5f / a : 0.0f;
		t.rotation.x = (y.z - z.y) * b;
		t.rotation.y = (z.x - x.z) * b;
		t.rotation.z = (x.y - y.x) * b;
		t.rotation.w = 0.5f * a;
	} else if (x.x > y.y && x.x > z.z) {
		ufbx_real a = (ufbx_real)sqrt(fmax(0.0, 1.0 + x.x - y.y - z.z)), b = (a != 0.0f) ? 0.5f / a : 0.0f;
		t.rotation.x = 0.5f * a;
		t.rotation.y = (y.x + x.y) * b;
		t.rotation.z = (z.x + x.z) * b;
		t.rotation.w = (y.z - z.y) * b;
	}
	else if (y.y > z.z) {
		ufbx_real a = (ufbx_real)sqrt(fmax(0.0, 1.0 - x.x + y.y - z.z)), b = (a != 0.0f) ? 0.5f / a : 0.0f;
		t.rotation.x = (y.x + x.y) * b;
		t.rotation.y = 0.5f * a;
		t.rotation.z = (z.y + y.z) * b;
		t.rotation.w = (z.x - x.z) * b;
	}
	else {
		ufbx_real a = (ufbx_real)sqrt(fmax(0.0, 1.0 - x.x - y.y + z.z)), b = (a != 0.0f) ? 0.5f / a : 0.0f;
		t.rotation.x = (z.x + x.z) * b;
		t.rotation.y = (z.y + y.z) * b;
		t.rotation.z = 0.5f * a;
		t.rotation.w = (x.y - y.x) * b;
	}

	ufbx_real len = t.rotation.x*t.rotation.x + t.rotation.y*t.rotation.y + t.rotation.z*t.rotation.z + t.rotation.w*t.rotation.w;
	if (fabs(len - 1.0f) > (ufbx_real)1e-20) {
		if (len == 0.0f) {
			t.rotation = ufbx_identity_quat;
		} else {
			t.rotation.x /= len;
			t.rotation.y /= len;
			t.rotation.z /= len;
			t.rotation.w /= len;
		}
	}

	t.scale.x *= sign_x;
	t.scale.y *= sign_y;
	t.scale.z *= sign_z;

	return t;
}

ufbx_matrix ufbx_get_skin_vertex_matrix(const ufbx_skin_deformer *skin, size_t vertex, const ufbx_matrix *fallback)
{
	if (!skin || vertex >= skin->vertices.count) return ufbx_identity_matrix;
	ufbx_skin_vertex skin_vertex = skin->vertices.data[vertex];

	ufbx_matrix mat = { 0.0f };
	ufbx_quat q0 = { 0.0f }, qe = { 0.0f };
	ufbx_quat first_q0 = { 0.0f };
	ufbx_vec3 qs = { 0.0f, 0.0f, 0.0f };
	ufbx_real total_weight = 0.0f;

	for (uint32_t i = 0; i < skin_vertex.num_weights; i++) {
		ufbx_skin_weight weight = skin->weights.data[skin_vertex.weight_begin + i];
		ufbx_skin_cluster *cluster = skin->clusters.data[weight.cluster_index];
		const ufbx_node *node = cluster->bone_node;
		if (!node) continue;

		total_weight += weight.weight;
		if (skin_vertex.dq_weight > 0.0f) {
			ufbx_transform t = cluster->geometry_to_world_transform;
			ufbx_quat vq0 = t.rotation;
			if (i == 0) first_q0 = vq0;

			if (ufbx_quat_dot(first_q0, vq0) < 0.0f) {
				vq0.x = -vq0.x;
				vq0.y = -vq0.y;
				vq0.z = -vq0.z;
				vq0.w = -vq0.w;
			}

			ufbx_quat vqt = { 0.5f * t.translation.x, 0.5f * t.translation.y, 0.5f * t.translation.z };
			ufbx_quat vqe = ufbxi_mul_quat(vqt, vq0);
			ufbxi_add_weighted_quat(&q0, vq0, weight.weight);
			ufbxi_add_weighted_quat(&qe, vqe, weight.weight);
			ufbxi_add_weighted_vec3(&qs, t.scale, weight.weight);
		}

		if (skin_vertex.dq_weight < 1.0f) {
			ufbxi_add_weighted_mat(&mat, &cluster->geometry_to_world, (1.0f-skin_vertex.dq_weight) * weight.weight);
		}
	}

	if (total_weight <= 0.0f) {
		if (fallback) {
			return *fallback;
		} else {
			return ufbx_identity_matrix;
		}
	}

	if (fabs(total_weight - 1.0f) > (ufbx_real)1e-20) {
		ufbx_real rcp_weight = 1.0f / total_weight;
		if (skin_vertex.dq_weight > 0.0f) {
			q0.x *= rcp_weight; q0.y *= rcp_weight; q0.z *= rcp_weight; q0.w *= rcp_weight;
			qe.x *= rcp_weight; qe.y *= rcp_weight; qe.z *= rcp_weight; qe.w *= rcp_weight;
			qs.x *= rcp_weight; qs.y *= rcp_weight; qs.z *= rcp_weight;
		}
		if (skin_vertex.dq_weight < 1.0f) {
			mat.m00 *= rcp_weight; mat.m01 *= rcp_weight; mat.m02 *= rcp_weight; mat.m03 *= rcp_weight;
			mat.m10 *= rcp_weight; mat.m11 *= rcp_weight; mat.m12 *= rcp_weight; mat.m13 *= rcp_weight;
			mat.m20 *= rcp_weight; mat.m21 *= rcp_weight; mat.m22 *= rcp_weight; mat.m23 *= rcp_weight;
		}
	}

	if (skin_vertex.dq_weight > 0.0f) {
		ufbx_transform dqt;
		ufbx_real rcp_len = (ufbx_real)(1.0 / sqrt(q0.x*q0.x + q0.y*q0.y + q0.z*q0.z + q0.w*q0.w));
		ufbx_real rcp_len2x2 = 2.0f * rcp_len * rcp_len;
		dqt.rotation.x = q0.x * rcp_len;
		dqt.rotation.y = q0.y * rcp_len;
		dqt.rotation.z = q0.z * rcp_len;
		dqt.rotation.w = q0.w * rcp_len;
		dqt.scale.x = qs.x;
		dqt.scale.y = qs.y;
		dqt.scale.z = qs.z;
		dqt.translation.x = rcp_len2x2 * (- qe.w*q0.x + qe.x*q0.w - qe.y*q0.z + qe.z*q0.y);
		dqt.translation.y = rcp_len2x2 * (- qe.w*q0.y + qe.x*q0.z + qe.y*q0.w - qe.z*q0.x);
		dqt.translation.z = rcp_len2x2 * (- qe.w*q0.z - qe.x*q0.y + qe.y*q0.x + qe.z*q0.w);
		ufbx_matrix dqm = ufbx_transform_to_matrix(&dqt);
		if (skin_vertex.dq_weight < 1.0f) {
			ufbxi_add_weighted_mat(&mat, &dqm, skin_vertex.dq_weight);
		} else {
			mat = dqm;
		}
	}

	return mat;
}

ufbx_vec3 ufbx_get_blend_shape_vertex_offset(const ufbx_blend_shape *shape, size_t vertex)
{
	if (!shape) return ufbx_zero_vec3;

	size_t index = SIZE_MAX;
	int32_t vertex_ix = (int32_t)vertex;

	ufbxi_macro_lower_bound_eq(int32_t, 16, &index, shape->offset_vertices, 0, shape->num_offsets,
		( *a < vertex_ix ), ( *a == vertex_ix ));
	if (index == SIZE_MAX) return ufbx_zero_vec3;

	return shape->position_offsets[index];
}

ufbx_vec3 ufbx_get_blend_vertex_offset(const ufbx_blend_deformer *blend, size_t vertex)
{
	ufbx_vec3 offset = ufbx_zero_vec3;

	ufbxi_for_ptr_list(ufbx_blend_channel, p_chan, blend->channels) {
		ufbx_blend_channel *chan = *p_chan;
		ufbxi_for_list(ufbx_blend_keyframe, key, chan->keyframes) {
			if (key->effective_weight == 0.0f) continue;

			ufbx_vec3 key_offset = ufbx_get_blend_shape_vertex_offset(key->shape, vertex);
			ufbxi_add_weighted_vec3(&offset, key_offset, key->effective_weight);
		}
	}

	return offset;
}

void ufbx_add_blend_shape_vertex_offsets(const ufbx_blend_shape *shape, ufbx_vec3 *vertices, size_t num_vertices, ufbx_real weight)
{
	if (weight == 0.0f) return;
	if (!vertices) return;

	size_t num_offsets = shape->num_offsets;
	int32_t *vertex_indices = shape->offset_vertices;
	ufbx_vec3 *offsets = shape->position_offsets;
	for (size_t i = 0; i < num_offsets; i++) {
		int32_t index = vertex_indices[i];
		if (index >= 0 && (size_t)index < num_vertices) {
			ufbxi_add_weighted_vec3(&vertices[index], offsets[i], weight);
		}
	}
}

void ufbx_add_blend_vertex_offsets(const ufbx_blend_deformer *blend, ufbx_vec3 *vertices, size_t num_vertices, ufbx_real weight)
{
	ufbxi_for_ptr_list(ufbx_blend_channel, p_chan, blend->channels) {
		ufbx_blend_channel *chan = *p_chan;
		ufbxi_for_list(ufbx_blend_keyframe, key, chan->keyframes) {
			if (key->effective_weight == 0.0f) continue;
			ufbx_add_blend_shape_vertex_offsets(key->shape, vertices, num_vertices, weight * key->effective_weight);
		}
	}
}

size_t ufbx_evaluate_nurbs_basis(const ufbx_nurbs_basis *basis, ufbx_real u, size_t num_weights, ufbx_real *weights, ufbx_real *derivatives)
{
	ufbx_assert(basis);
	if (!basis) return SIZE_MAX;
	if (basis->order == 0) return SIZE_MAX;
	if (!basis->valid) return SIZE_MAX;

	size_t degree = basis->order - 1;

	// Binary search for the knot span `[min_u, max_u]` where `min_u <= u < max_u`
	ufbx_real_list knots = basis->knot_vector;
	size_t knot = SIZE_MAX;

	if (u <= basis->t_min) {
		knot = degree;
		u = basis->t_min;
	} else if (u >= basis->t_max) {
		knot = basis->knot_vector.count - degree - 2;
		u = basis->t_max;
	} else {
		ufbxi_macro_lower_bound_eq(ufbx_real, 8, &knot, knots.data, 0, knots.count - 1,
			( a[1] <= u ), ( a[0] <= u && u < a[1] ));
	}

	// The found effective control points are found left from `knot`, locally
	// we use `knot - ix` here as it's more convenient for the following algorithm
	// but we return it as `knot - degree` so that users can find the control points
	// at `points[knot], points[knot+1], ..., points[knot+degree]`
	if (knot < degree) return SIZE_MAX;

	if (num_weights < basis->order) return knot - degree;
	if (!weights) return knot - degree;

	weights[0] = 1.0f;
	for (size_t p = 1; p <= degree; p++) {

		ufbx_real prev = 0.0f;
		ufbx_real g = 1.0f - ufbxi_nurbs_weight(&knots, knot - p + 1, p, u);
		ufbx_real dg = 0.0f;
		if (derivatives && p == degree) {
			dg = ufbxi_nurbs_deriv(&knots, knot - p + 1, p);
		}

		for (size_t i = p; i > 0; i--) {
			ufbx_real f = ufbxi_nurbs_weight(&knots, knot - p + i, p, u);
			ufbx_real weight = weights[i - 1];
			weights[i] = f*weight + g*prev;

			if (derivatives && p == degree) {
				ufbx_real df = ufbxi_nurbs_deriv(&knots, knot - p + i, p);
				derivatives[i] = df*weight - dg*prev;
				dg = df;
			}
			
			prev = weight;
			g = 1.0f - f;
		}

		weights[0] = g*prev;
		if (derivatives && p == degree) {
			derivatives[0] = -dg*prev;
		}
	}

	return knot - degree;
}

ufbx_curve_point ufbx_evaluate_nurbs_curve(const ufbx_nurbs_curve *curve, ufbx_real u)
{
	ufbx_curve_point result = { false };

	ufbx_assert(curve);
	if (!curve) return result;

	ufbx_real weights[UFBXI_MAX_NURBS_ORDER];
	ufbx_real derivs[UFBXI_MAX_NURBS_ORDER];
	size_t base = ufbx_evaluate_nurbs_basis(&curve->basis, u, UFBXI_MAX_NURBS_ORDER, weights, derivs);
	if (base == SIZE_MAX) return result;

	ufbx_vec4 p = { 0 };
	ufbx_vec4 d = { 0 };

	size_t order = curve->basis.order;
	for (size_t i = 0; i < order; i++) {
		size_t ix = (base + i) % curve->control_points.count;
		ufbx_vec4 cp = curve->control_points.data[ix];
		ufbx_real weight = weights[i] * cp.w, deriv = derivs[i] * cp.w;

		p.x += cp.x * weight;
		p.y += cp.y * weight;
		p.z += cp.z * weight;
		p.w += weight;

		d.x += cp.x * deriv;
		d.y += cp.y * deriv;
		d.z += cp.z * deriv;
		d.w += deriv;
	}

	ufbx_real rcp_w = 1.0f / p.w;
	result.valid = true;
	result.position.x = p.x * rcp_w;
	result.position.y = p.y * rcp_w;
	result.position.z = p.z * rcp_w;
	result.derivative.x = (d.x - d.w*result.position.x) * rcp_w;
	result.derivative.y = (d.y - d.w*result.position.y) * rcp_w;
	result.derivative.z = (d.z - d.w*result.position.z) * rcp_w;
	return result;
}

ufbx_surface_point ufbx_evaluate_nurbs_surface(const ufbx_nurbs_surface *surface, ufbx_real u, ufbx_real v)
{
	ufbx_surface_point result = { false };

	ufbx_assert(surface);
	if (!surface) return result;

	ufbx_real weights_u[UFBXI_MAX_NURBS_ORDER], weights_v[UFBXI_MAX_NURBS_ORDER];
	ufbx_real derivs_u[UFBXI_MAX_NURBS_ORDER], derivs_v[UFBXI_MAX_NURBS_ORDER];
	size_t base_u = ufbx_evaluate_nurbs_basis(&surface->basis_u, u, UFBXI_MAX_NURBS_ORDER, weights_u, derivs_u);
	size_t base_v = ufbx_evaluate_nurbs_basis(&surface->basis_v, v, UFBXI_MAX_NURBS_ORDER, weights_v, derivs_v);
	if (base_u == SIZE_MAX || base_v == SIZE_MAX) return result;

	ufbx_vec4 p = { 0 };
	ufbx_vec4 du = { 0 };
	ufbx_vec4 dv = { 0 };

	size_t num_u = surface->num_control_points_u;
	size_t num_v = surface->num_control_points_v;
	size_t order_u = surface->basis_u.order;
	size_t order_v = surface->basis_v.order;
	for (size_t vi = 0; vi < order_v; vi++) {
		size_t vix = (base_v + vi) % num_v;
		ufbx_real weight_v = weights_v[vi], deriv_v = derivs_v[vi];

		for (size_t ui = 0; ui < order_u; ui++) {
			size_t uix = (base_u + ui) % num_u;
			ufbx_real weight_u = weights_u[ui], deriv_u = derivs_u[ui];
			ufbx_vec4 cp = surface->control_points.data[vix * num_u + uix];

			ufbx_real weight = weight_u * weight_v * cp.w;
			ufbx_real wderiv_u = deriv_u * weight_v * cp.w;
			ufbx_real wderiv_v = deriv_v * weight_u * cp.w;

			p.x += cp.x * weight;
			p.y += cp.y * weight;
			p.z += cp.z * weight;
			p.w += weight;

			du.x += cp.x * wderiv_u;
			du.y += cp.y * wderiv_u;
			du.z += cp.z * wderiv_u;
			du.w += wderiv_u;

			dv.x += cp.x * wderiv_v;
			dv.y += cp.y * wderiv_v;
			dv.z += cp.z * wderiv_v;
			dv.w += wderiv_v;
		}
	}

	ufbx_real rcp_w = 1.0f / p.w;
	result.valid = true;
	result.position.x = p.x * rcp_w;
	result.position.y = p.y * rcp_w;
	result.position.z = p.z * rcp_w;
	result.derivative_u.x = (du.x - du.w*result.position.x) * rcp_w;
	result.derivative_u.y = (du.y - du.w*result.position.y) * rcp_w;
	result.derivative_u.z = (du.z - du.w*result.position.z) * rcp_w;
	result.derivative_v.x = (dv.x - dv.w*result.position.x) * rcp_w;
	result.derivative_v.y = (dv.y - dv.w*result.position.y) * rcp_w;
	result.derivative_v.z = (dv.z - dv.w*result.position.z) * rcp_w;
	return result;
}

ufbx_mesh *ufbx_tessellate_nurbs_surface(const ufbx_nurbs_surface *surface, const ufbx_tessellate_opts *opts, ufbx_error *error)
{
	ufbx_assert(surface);
	if (!surface) return NULL;

	ufbxi_tessellate_context tc = { UFBX_ERROR_NONE };
	if (opts) {
		tc.opts = *opts;
	}

	tc.surface = surface;

	int ok = ufbxi_tessellate_nurbs_surface_imp(&tc);

	ufbxi_buf_free(&tc.tmp);
	ufbxi_map_free(&tc.position_map);
	ufbxi_free_ator(&tc.ator_tmp);

	if (ok) {
		if (error) {
			error->type = UFBX_ERROR_NONE;
			error->description = NULL;
			error->stack_size = 0;
		}
		return &tc.imp->mesh;
	} else {
		ufbxi_fix_error_type(&tc.error, "Failed to tessellate");
		if (error) *error = tc.error;
		ufbxi_buf_free(&tc.result);
		ufbxi_free_ator(&tc.ator_result);
		return NULL;
	}
}

uint32_t ufbx_triangulate_face(uint32_t *indices, size_t num_indices, const ufbx_mesh *mesh, ufbx_face face)
{
	if (face.num_indices < 3 || num_indices < ((size_t)face.num_indices - 2) * 3) return 0;

	if (face.num_indices == 3) {
		// Fast case: Already a triangle
		indices[0] = face.index_begin + 0;
		indices[1] = face.index_begin + 1;
		indices[2] = face.index_begin + 2;
		return 1;
	} else if (face.num_indices == 4) {
		// Quad: Split along the shortest axis unless a vertex crosses the axis
		uint32_t i0 = face.index_begin + 0;
		uint32_t i1 = face.index_begin + 1;
		uint32_t i2 = face.index_begin + 2;
		uint32_t i3 = face.index_begin + 3;
		ufbx_vec3 v0 = mesh->vertex_position.data[mesh->vertex_position.indices[i0]];
		ufbx_vec3 v1 = mesh->vertex_position.data[mesh->vertex_position.indices[i1]];
		ufbx_vec3 v2 = mesh->vertex_position.data[mesh->vertex_position.indices[i2]];
		ufbx_vec3 v3 = mesh->vertex_position.data[mesh->vertex_position.indices[i3]];

		ufbx_vec3 a = ufbxi_sub3(v2, v0);
		ufbx_vec3 b = ufbxi_sub3(v3, v1);

		ufbx_vec3 na1 = ufbxi_normalize3(ufbxi_cross3(a, ufbxi_sub3(v1, v0)));
		ufbx_vec3 na3 = ufbxi_normalize3(ufbxi_cross3(a, ufbxi_sub3(v0, v3)));
		ufbx_vec3 nb0 = ufbxi_normalize3(ufbxi_cross3(b, ufbxi_sub3(v1, v0)));
		ufbx_vec3 nb2 = ufbxi_normalize3(ufbxi_cross3(b, ufbxi_sub3(v2, v1)));

		ufbx_real dot_aa = ufbxi_dot3(a, a);
		ufbx_real dot_bb = ufbxi_dot3(b, b);
		ufbx_real dot_na = ufbxi_dot3(na1, na3);
		ufbx_real dot_nb = ufbxi_dot3(nb0, nb2);

		bool split_a = dot_aa <= dot_bb;

		if (dot_na < 0.0f || dot_nb < 0.0f) {
			split_a = dot_na >= dot_nb;
		}

		if (split_a) {
			indices[0] = i0;
			indices[1] = i1;
			indices[2] = i2;
			indices[3] = i2;
			indices[4] = i3;
			indices[5] = i0;
		} else {
			indices[0] = i1;
			indices[1] = i2;
			indices[2] = i3;
			indices[3] = i3;
			indices[4] = i0;
			indices[5] = i1;
		}

		return 2;
	} else {
		ufbxi_ngon_context nc = { 0 };
		nc.mesh = mesh;
		nc.face = face;

		uint32_t num_indices_u32 = num_indices < UINT32_MAX ? (uint32_t)num_indices : UINT32_MAX;

		uint32_t local_indices[12];
		if (num_indices_u32 < 12) {
			uint32_t num_tris = ufbxi_triangulate_ngon(&nc, local_indices, 12);
			memcpy(indices, local_indices, num_tris * 3 * sizeof(uint32_t));
			return num_tris;
		} else {
			return ufbxi_triangulate_ngon(&nc, indices, num_indices_u32);
		}
	}
}

void ufbx_compute_topology(const ufbx_mesh *mesh, ufbx_topo_edge *indices)
{
	ufbxi_compute_topology(mesh, indices);
}

int32_t ufbx_topo_next_vertex_edge(const ufbx_topo_edge *indices, int32_t index)
{
	if (index < 0) return -1;
	int32_t twin = indices[index].twin;
	if (twin < 0) return -1;
	return indices[twin].next;
}

int32_t ufbx_topo_prev_vertex_edge(const ufbx_topo_edge *indices, int32_t index)
{
	if (index < 0) return -1;
	return indices[indices[index].prev].twin;
}

ufbx_vec3 ufbx_get_weighted_face_normal(const ufbx_vertex_vec3 *positions, ufbx_face face)
{
	if (face.num_indices < 3) {
		return ufbx_zero_vec3;
	} else if (face.num_indices == 3) {
		ufbx_vec3 a = ufbx_get_vertex_vec3(positions, face.index_begin + 0);
		ufbx_vec3 b = ufbx_get_vertex_vec3(positions, face.index_begin + 1);
		ufbx_vec3 c = ufbx_get_vertex_vec3(positions, face.index_begin + 2);
		return ufbxi_cross3(ufbxi_sub3(b, a), ufbxi_sub3(c, a));
	} else if (face.num_indices == 4) {
		ufbx_vec3 a = ufbx_get_vertex_vec3(positions, face.index_begin + 0);
		ufbx_vec3 b = ufbx_get_vertex_vec3(positions, face.index_begin + 1);
		ufbx_vec3 c = ufbx_get_vertex_vec3(positions, face.index_begin + 2);
		ufbx_vec3 d = ufbx_get_vertex_vec3(positions, face.index_begin + 3);
		return ufbxi_cross3(ufbxi_sub3(c, a), ufbxi_sub3(d, b));
	} else {
		// Newell's Method
		ufbx_vec3 result = ufbx_zero_vec3;
		for (size_t i = 0; i < face.num_indices; i++) {
			size_t next = i + 1 < face.num_indices ? i + 1 : 0;
			ufbx_vec3 a = ufbx_get_vertex_vec3(positions, face.index_begin + i);
			ufbx_vec3 b = ufbx_get_vertex_vec3(positions, face.index_begin + next);
			result.x += (a.y - b.y) * (a.z + b.z);
			result.y += (a.z - b.z) * (a.x + b.x);
			result.z += (a.x - b.x) * (a.y + b.y);
		}
		return result;
	}
}

size_t ufbx_generate_normal_mapping(const ufbx_mesh *mesh, ufbx_topo_edge *topo, int32_t *normal_indices, bool assume_smooth)
{
	int32_t next_index = 0;

	for (size_t i = 0; i < mesh->num_indices; i++) {
		normal_indices[i] = -1;
	}

	// Walk around vertices and merge around smooth edges
	for (size_t vi = 0; vi < mesh->num_vertices; vi++) {
		int32_t original_start = mesh->vertex_first_index[vi];
		if (original_start < 0) continue;
		int32_t start = original_start, cur = start;

		for (;;) {
			int32_t prev = ufbx_topo_next_vertex_edge(topo, cur);
			if (!ufbxi_is_edge_smooth(mesh, topo, cur, assume_smooth)) start = cur;
			if (prev < 0) { start = cur; break; }
			if (prev == original_start) break;
			cur = prev;
		}

		normal_indices[start] = next_index++;
		int32_t next = start;
		for (;;) {
			next = ufbx_topo_prev_vertex_edge(topo, next);
			if (next == -1 || next == start) break;

			if (!ufbxi_is_edge_smooth(mesh, topo, next, assume_smooth)) {
				++next_index;
			}
			normal_indices[next] = next_index - 1;
		}
	}

	// Assign non-manifold indices
	for (size_t i = 0; i < mesh->num_indices; i++) {
		if (normal_indices[i] < 0) {
			normal_indices[i] = next_index++;
		}
	}

	return (size_t)next_index;
}

void ufbx_compute_normals(const ufbx_mesh *mesh, const ufbx_vertex_vec3 *positions, int32_t *normal_indices, ufbx_vec3 *normals, size_t num_normals)
{
	memset(normals, 0, sizeof(ufbx_vec3)*num_normals);

	for (size_t fi = 0; fi < mesh->num_faces; fi++) {
		ufbx_face face = mesh->faces[fi];
		ufbx_vec3 normal = ufbx_get_weighted_face_normal(positions, face);
		for (size_t ix = 0; ix < face.num_indices; ix++) {
			ufbx_vec3 *n = &normals[normal_indices[face.index_begin + ix]];
			*n = ufbxi_add3(*n, normal);
		}
	}

	for (size_t i = 0; i < num_normals; i++) {
		ufbx_real len = ufbxi_length3(normals[i]);
		if (len > 0.0f) {
			normals[i].x /= len;
			normals[i].y /= len;
			normals[i].z /= len;
		}
	}
}

ufbx_mesh *ufbx_subdivide_mesh(const ufbx_mesh *mesh, size_t level, const ufbx_subdivide_opts *opts, ufbx_error *error)
{
	if (!mesh) return NULL;
	if (level == 0) return (ufbx_mesh*)mesh;
	return ufbxi_subdivide_mesh(mesh, level, opts, error);
}

void ufbx_free_mesh(ufbx_mesh *mesh)
{
	if (!mesh) return;
	if (!mesh->subdivision_evaluated && !mesh->from_tessellated_nurbs) return;

	ufbxi_mesh_imp *imp = (ufbxi_mesh_imp*)mesh;
	ufbx_assert(imp->magic == UFBXI_MESH_IMP_MAGIC);
	if (imp->magic != UFBXI_MESH_IMP_MAGIC) return;
	imp->magic = 0;

	// See `ufbx_free_scene()` for more information
	ufbxi_allocator ator = imp->ator;
	ufbxi_buf result = imp->result_buf;
	result.ator = &ator;
	ufbxi_buf_free(&result);
	ufbxi_free_ator(&ator);
}

ufbx_geometry_cache *ufbx_load_geometry_cache(
	const char *filename,
	const ufbx_geometry_cache_opts *opts, ufbx_error *error)
{
	return ufbx_load_geometry_cache_len(filename, strlen(filename),
		opts, error);
}

ufbx_geometry_cache *ufbx_load_geometry_cache_len(
	const char *filename, size_t filename_len,
	const ufbx_geometry_cache_opts *opts, ufbx_error *error)
{
	ufbx_string str = { filename, filename_len };
	return ufbxi_load_geometry_cache(str, opts, error);
}

void ufbx_free_geometry_cache(ufbx_geometry_cache *cache)
{
	if (!cache) return;

	ufbxi_geometry_cache_imp *imp = (ufbxi_geometry_cache_imp*)cache;
	ufbx_assert(imp->magic == UFBXI_CACHE_IMP_MAGIC);
	if (imp->magic != UFBXI_CACHE_IMP_MAGIC) return;
	if (imp->owned_by_scene) return;
	imp->magic = 0;

	ufbxi_buf_free(&imp->string_buf);

	// We need to free `result_buf` last and be careful to copy it to
	// the stack since the `ufbxi_scene_imp` that contains it is allocated
	// from the same result buffer!
	ufbxi_allocator ator = imp->ator;
	ufbxi_buf result = imp->result_buf;
	result.ator = &ator;
	ufbxi_buf_free(&result);
	ufbxi_free_ator(&ator);
}

ufbxi_noinline size_t ufbx_read_geometry_cache_real(const ufbx_cache_frame *frame, ufbx_real *data, size_t count, ufbx_geometry_cache_data_opts *user_opts)
{
	if (!frame || count == 0) return 0;
	ufbx_assert(data);
	if (!data) return 0;

	ufbx_geometry_cache_data_opts opts;
	if (user_opts) {
		opts = *user_opts;
	} else {
		memset(&opts, 0, sizeof(opts));
	}

	if (!opts.open_file_fn) {
		opts.open_file_fn = ufbx_open_file;
	}

	// `ufbx_geometry_cache_data_opts` must be cleared to zero first!
	ufbx_assert(opts._begin_zero == 0 && opts._end_zero == 0);
	if (!(opts._begin_zero == 0 && opts._end_zero == 0)) return 0;

	bool use_double = false;

	size_t src_count = 0;

	switch (frame->data_format) {
	case UFBX_CACHE_DATA_UNKNOWN: src_count = 0; break;
	case UFBX_CACHE_DATA_REAL_FLOAT: src_count = frame->data_count; break;
	case UFBX_CACHE_DATA_VEC3_FLOAT: src_count = frame->data_count * 3; break;
	case UFBX_CACHE_DATA_REAL_DOUBLE: src_count = frame->data_count; use_double = true; break;
	case UFBX_CACHE_DATA_VEC3_DOUBLE: src_count = frame->data_count * 3; use_double = true; break;
	}

	bool src_big_endian = false;
	switch (frame->data_encoding) {
	case UFBX_CACHE_DATA_ENCODING_UNKNOWN: return 0;
	case UFBX_CACHE_DATA_ENCODING_LITTLE_ENDIAN: src_big_endian = false; break;
	case UFBX_CACHE_DATA_ENCODING_BIG_ENDIAN: src_big_endian = true; break;
	}

	// Test endianness
	bool dst_big_endian;
	{
		uint8_t buf[2];
		uint16_t val = 0xbbaa;
		memcpy(buf, &val, 2);
		dst_big_endian = buf[0] == 0xbb;
	}

	if (src_count == 0) return 0;
	src_count = ufbxi_min_sz(src_count, count);

	ufbx_stream stream = { 0 };
	if (!opts.open_file_fn(opts.open_file_user, &stream, frame->filename.data, frame->filename.length)) {
		return 0;
	}

	// Skip to the correct point in the file
	uint64_t offset = frame->data_offset;
	if (stream.skip_fn) {
		while (offset > 0) {
			size_t to_skip = (size_t)ufbxi_min64(offset, UFBXI_MAX_SKIP_SIZE);
			if (!stream.skip_fn(stream.user, to_skip)) break;
			offset -= to_skip;
		}
	} else {
		char buffer[4096];
		while (offset > 0) {
			size_t to_skip = (size_t)ufbxi_min64(offset, sizeof(buffer));
			size_t num_read = stream.read_fn(stream.user, buffer, to_skip);
			if (num_read != to_skip) break;
			offset -= to_skip;
		}
	}

	// Failed to skip all the way
	if (offset > 0) {
		if (stream.close_fn) {
			stream.close_fn(stream.user);
		}
		return 0;
	}

	ufbx_real *dst = data;
	if (use_double) {
		double buffer[512];
		while (src_count > 0) {
			size_t to_read = ufbxi_min_sz(src_count, ufbxi_arraycount(buffer));
			src_count -= to_read;
			size_t bytes_read = stream.read_fn(stream.user, buffer, to_read * sizeof(double));
			if (bytes_read == SIZE_MAX) bytes_read = 0;
			size_t num_read = bytes_read / sizeof(double);

			if (src_big_endian != dst_big_endian) {
				for (size_t i = 0; i < num_read; i++) {
					char t, *v = (char*)&buffer[i];
					t = v[0]; v[0] = v[7]; v[7] = t;
					t = v[1]; v[1] = v[6]; v[6] = t;
					t = v[2]; v[2] = v[5]; v[5] = t;
					t = v[3]; v[3] = v[4]; v[4] = t;
				}
			}

			ufbx_real weight = opts.weight;
			if (opts.additive && opts.use_weight) {
				for (size_t i = 0; i < num_read; i++) {
					dst[i] += (ufbx_real)buffer[i] * weight;
				}
			} else if (opts.additive) {
				for (size_t i = 0; i < num_read; i++) {
					dst[i] += (ufbx_real)buffer[i];
				}
			} else if (opts.use_weight) {
				for (size_t i = 0; i < num_read; i++) {
					dst[i] = (ufbx_real)buffer[i] * weight;
				}
			} else {
				for (size_t i = 0; i < num_read; i++) {
					dst[i] = (ufbx_real)buffer[i];
				}
			}

			dst += num_read;
			if (num_read != to_read) break;
		}
	} else {
		float buffer[1024];
		while (src_count > 0) {
			size_t to_read = ufbxi_min_sz(src_count, ufbxi_arraycount(buffer));
			src_count -= to_read;
			size_t bytes_read = stream.read_fn(stream.user, buffer, to_read * sizeof(float));
			if (bytes_read == SIZE_MAX) bytes_read = 0;
			size_t num_read = bytes_read / sizeof(float);

			if (src_big_endian != dst_big_endian) {
				for (size_t i = 0; i < num_read; i++) {
					char t, *v = (char*)&buffer[i];
					t = v[0]; v[0] = v[3]; v[3] = t;
					t = v[1]; v[1] = v[2]; v[2] = t;
				}
			}

			ufbx_real weight = opts.weight;
			if (opts.additive && opts.use_weight) {
				for (size_t i = 0; i < num_read; i++) {
					dst[i] += (ufbx_real)buffer[i] * weight;
				}
			} else if (opts.additive) {
				for (size_t i = 0; i < num_read; i++) {
					dst[i] += (ufbx_real)buffer[i];
				}
			} else if (opts.use_weight) {
				for (size_t i = 0; i < num_read; i++) {
					dst[i] = (ufbx_real)buffer[i] * weight;
				}
			} else {
				for (size_t i = 0; i < num_read; i++) {
					dst[i] = (ufbx_real)buffer[i];
				}
			}

			dst += num_read;
			if (num_read != to_read) break;
		}
	}

	if (stream.close_fn) {
		stream.close_fn(stream.user);
	}

	return (size_t)(dst - data);
}

ufbxi_noinline size_t ufbx_sample_geometry_cache_real(const ufbx_cache_channel *channel, double time, ufbx_real *data, size_t count, ufbx_geometry_cache_data_opts *user_opts)
{
	if (!channel || count == 0) return 0;
	ufbx_assert(data);
	if (!data) return 0;
	if (channel->frames.count == 0) return 0;

	ufbx_geometry_cache_data_opts opts;
	if (user_opts) {
		opts = *user_opts;
	} else {
		memset(&opts, 0, sizeof(opts));
	}

	// `ufbx_geometry_cache_data_opts` must be cleared to zero first!
	ufbx_assert(opts._begin_zero == 0 && opts._end_zero == 0);
	if (!(opts._begin_zero == 0 && opts._end_zero == 0)) return 0;

	size_t begin = 0;
	size_t end = channel->frames.count;
	const ufbx_cache_frame *frames = channel->frames.data;
	while (end - begin >= 8) {
		size_t mid = (begin + end) >> 1;
		if (frames[mid].time < time) {
			begin = mid + 1;
		} else { 
			end = mid;
		}
	}

	const double eps = 0.00000001;

	end = channel->frames.count;
	for (; begin < end; begin++) {
		const ufbx_cache_frame *next = &frames[begin];
		if (next->time < time) continue;

		// First keyframe
		if (begin == 0) {
			return ufbx_read_geometry_cache_real(next, data, count, &opts);
		}

		const ufbx_cache_frame *prev = next - 1;

		// Snap to exact frames if near
		if (fabs(next->time - time) < eps) {
			return ufbx_read_geometry_cache_real(next, data, count, &opts);
		}
		if (fabs(prev->time - time) < eps) {
			return ufbx_read_geometry_cache_real(prev, data, count, &opts);
		}

		double rcp_delta = 1.0 / (next->time - prev->time);
		double t = (time - prev->time) * rcp_delta;

		ufbx_real original_weight = opts.use_weight ? opts.weight : 1.0f;

		opts.use_weight = true;
		opts.weight = (ufbx_real)(original_weight * (1.0 - t));
		size_t num_prev = ufbx_read_geometry_cache_real(prev, data, count, &opts);

		opts.additive = true;
		opts.weight = (ufbx_real)(original_weight * t);
		return ufbx_read_geometry_cache_real(next, data, num_prev, &opts);
	}

	// Last frame
	const ufbx_cache_frame *last = &frames[end - 1];
	return ufbx_read_geometry_cache_real(last, data, count, &opts);
}

ufbxi_noinline size_t ufbx_read_geometry_cache_vec3(const ufbx_cache_frame *frame, ufbx_vec3 *data, size_t count, ufbx_geometry_cache_data_opts *opts)
{
	if (!frame || count == 0) return 0;
	ufbx_assert(data);
	if (!data) return 0;
	return ufbx_read_geometry_cache_real(frame, (ufbx_real*)data, count * 3, opts) / 3;
}

ufbxi_noinline size_t ufbx_sample_geometry_cache_vec3(const ufbx_cache_channel *channel, double time, ufbx_vec3 *data, size_t count, ufbx_geometry_cache_data_opts *opts)
{
	if (!channel || count == 0) return 0;
	ufbx_assert(data);
	if (!data) return 0;
	return ufbx_sample_geometry_cache_real(channel, time, (ufbx_real*)data, count * 3, opts) / 3;
}

size_t ufbx_generate_indices(const ufbx_vertex_stream *streams, size_t num_streams, uint32_t *indices, size_t num_indices, const ufbx_allocator *allocator, ufbx_error *error)
{
	ufbx_error local_error;
	return ufbxi_generate_indices(streams, num_streams, indices, num_indices, allocator, error ? error : &local_error);
}

#ifdef __cplusplus
}
#endif

#endif

#if defined(_MSC_VER)
	#pragma warning(pop)
#elif defined(__clang__)
	#pragma clang diagnostic pop
#elif defined(__GNUC__)
	#pragma GCC diagnostic pop
#endif
