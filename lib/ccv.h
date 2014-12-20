/**********************************************************
 * C-based/Cached/Core Computer Vision Library
 * Liu Liu, 2010-02-01
 **********************************************************/

#ifndef GUARD_ccv_h
#define GUARD_ccv_h

#include <unistd.h>
#include <stdint.h>
#define _WITH_GETLINE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <assert.h>
#if !defined(__OpenBSD__) && !defined(__FreeBSD__)
#include <alloca.h>
#endif

#define CCV_PI (3.141592653589793)
#define ccmalloc malloc
#define cccalloc calloc
#define ccrealloc realloc
#define ccfree free

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/param.h>
#if defined(__APPLE__) || defined(BSD) || _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600
#define ccmemalign posix_memalign
#else
#define ccmemalign(memptr, alignment, size) (*memptr = memalign(alignment, size))
#endif
#else
#define ccmemalign(memptr, alignment, size) (*memptr = ccmalloc(size))
#endif

/* Doxygen will ignore there, otherwise it has problem to process warn_unused_result directly. */
#define CCV_WARN_UNUSED(x) x __attribute__((warn_unused_result))

enum {
	CCV_8U  = 0x01000,
	CCV_32S = 0x02000,
	CCV_32F = 0x04000,
	CCV_64S = 0x08000,
	CCV_64F = 0x10000,
};

enum {
	CCV_C1 = 0x001,
	CCV_C2 = 0x002,
	CCV_C3 = 0x003,
	CCV_C4 = 0x004,
};

static const int _ccv_get_data_type_size[] = { -1, 1, 4, -1, 4, -1, -1, -1, 8, -1, -1, -1, -1, -1, -1, -1, 8 };

#define CCV_GET_DATA_TYPE(x) ((x) & 0xFF000)
#define CCV_GET_DATA_TYPE_SIZE(x) _ccv_get_data_type_size[CCV_GET_DATA_TYPE(x) >> 12]
#define CCV_MAX_CHANNEL (0xFFF)
#define CCV_GET_CHANNEL(x) ((x) & 0xFFF)
#define CCV_ALL_DATA_TYPE (CCV_8U | CCV_32S | CCV_32F | CCV_64S | CCV_64F)

enum {
	CCV_MATRIX_DENSE  = 0x0100000,
	CCV_MATRIX_SPARSE = 0x0200000,
	CCV_MATRIX_CSR    = 0x0400000,
	CCV_MATRIX_CSC    = 0x0800000,
};

enum {
	CCV_GARBAGE       = 0x80000000, // matrix is in cache (not used by any functions)
	CCV_REUSABLE      = 0x40000000, // matrix can be recycled
	CCV_UNMANAGED     = 0x20000000, // matrix is allocated by user, therefore, cannot be freed by ccv_matrix_free/ccv_matrix_free_immediately
	CCV_NO_DATA_ALLOC = 0x10000000, // matrix is allocated as header only, but with no data section, therefore, you have to free the data section separately
};

typedef union {
	unsigned char* u8;
	int* i32;
	float* f32;
	int64_t* i64;
	double* f64;
} ccv_matrix_cell_t;

typedef struct {
	int type;
	uint64_t sig;
	int refcount;
	int rows;
	int cols;
	int step;
	union {
		unsigned char u8;
		int i32;
		float f32;
		int64_t i64;
		double f64;
		void* p;
	} tag;
	ccv_matrix_cell_t data;
} ccv_dense_matrix_t;

enum {
	CCV_SPARSE_VECTOR = 0x01000000,
	CCV_DENSE_VECTOR  = 0x02000000,
};

typedef struct ccv_dense_vector_t {
	int step;
	int length;
	int index;
	int prime;
	int load_factor;
	ccv_matrix_cell_t data;
	int* indice;
	struct ccv_dense_vector_t* next;
} ccv_dense_vector_t;

enum {
	CCV_SPARSE_ROW_MAJOR = 0x00,
	CCV_SPARSE_COL_MAJOR = 0x01,
};

typedef struct {
	int type;
	uint64_t sig;
	int refcount;
	int rows;
	int cols;
	int major;
	int prime;
	int load_factor;
	union {
		unsigned char chr;
		int i;
		float fl;
		int64_t l;
		double db;
	} tag;
	ccv_dense_vector_t* vector;
} ccv_sparse_matrix_t;

extern int _ccv_get_sparse_prime[];
#define CCV_GET_SPARSE_PRIME(x) _ccv_get_sparse_prime[(x)]

typedef void ccv_matrix_t;

/**
 * @defgroup ccv_cache cache mechanism
 * This class implements a trie-based LRU cache that is then used for ccv application-wide cache in [ccv\_memory.c](/lib/ccv-memory).
 * @{
 */

typedef void(*ccv_cache_index_free_f)(void*);

typedef union {
	struct {
		uint64_t bitmap;
		uint64_t set;
		uint64_t age;
	} branch;
	struct {
		uint64_t sign;
		uint64_t off;
		uint64_t type;
	} terminal;
} ccv_cache_index_t;

typedef struct {
	ccv_cache_index_t origin;
	uint32_t rnum;
	uint32_t age;
	size_t up;
	size_t size;
	ccv_cache_index_free_f ffree[16];
} ccv_cache_t;

/* I made it as generic as possible */

/**
 * Setup a cache strcture to work with.
 * @param cache The allocated cache.
 * @param up The upper limit of cache size in bytes.
 * @param cache_types The number of cache types in this one cache instance.
 * @param ffree The function that will be used to free cached object.
 */
void ccv_cache_init(ccv_cache_t* cache, size_t up, int cache_types, ccv_cache_index_free_f ffree, ...);
/**
 * Get an object from cache for its signature. 0 if cannot find the object.
 * @param cache The cache.
 * @param sign The signature.
 * @param type The type of the object.
 * @return The pointer to the object.
 */
void* ccv_cache_get(ccv_cache_t* cache, uint64_t sign, uint8_t* type);
/**
 * Put an object to cache with its signature, size, and type
 * @param cache The cache.
 * @param sign The signature.
 * @param x The pointer to the object.
 * @param size The size of the object.
 * @param type The type of the object.
 * @return 0 - success, 1 - replace, -1 - failure.
 */
int ccv_cache_put(ccv_cache_t* cache, uint64_t sign, void* x, uint32_t size, uint8_t type);
/**
 * Get an object from cache for its signature and then remove that object from the cache. 0 if cannot find the object.
 * @param cache The cache.
 * @param sign The signature.
 * @param type The type of the object.
 * @return The pointer to the object.
 */
void* ccv_cache_out(ccv_cache_t* cache, uint64_t sign, uint8_t* type);
/**
 * Delete an object from cache for its signature and free it.
 * @param cache The cache.
 * @param sign The signature.
 * @return -1 if cannot find the object, otherwise return 0.
 */
int ccv_cache_delete(ccv_cache_t* cache, uint64_t sign);
/**
 * Clean up the cache, free all objects inside and other memory space occupied.
 * @param cache The cache.
 */
void ccv_cache_cleanup(ccv_cache_t* cache);
/**
 * For current implementation (trie-based LRU cache), it is an alias for ccv_cache_cleanup.
 * @param cache The cache.
 */
void ccv_cache_close(ccv_cache_t* cache);
/** @} */

/* deprecated methods, often these implemented in another way and no longer suitable for newer computer architecture */
/* 0 */

typedef struct {
	int type;
	uint64_t sig;
	int refcount;
	int rows;
	int cols;
	int nnz;
	union {
		unsigned char chr;
		int i;
		float fl;
		int64_t l;
		double db;
	} tag;
	int* index;
	int* offset;
	ccv_matrix_cell_t data;
} ccv_compressed_sparse_matrix_t;

#define ccv_clamp(x, a, b) (((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))
#define ccv_min(a, b) (((a) < (b)) ? (a) : (b))
#define ccv_max(a, b) (((a) > (b)) ? (a) : (b))

/* matrix memory operations ccv_memory.c */
#define ccv_compute_dense_matrix_size(rows, cols, type) (sizeof(ccv_dense_matrix_t) + (((cols) * CCV_GET_DATA_TYPE_SIZE(type) * CCV_GET_CHANNEL(type) + 3) & -4) * (rows))
CCV_WARN_UNUSED(ccv_dense_matrix_t*) ccv_dense_matrix_renew(ccv_dense_matrix_t* x, int rows, int cols, int types, int prefer_type, uint64_t sig);
CCV_WARN_UNUSED(ccv_dense_matrix_t*) ccv_dense_matrix_new(int rows, int cols, int type, void* data, uint64_t sig);
ccv_dense_matrix_t ccv_dense_matrix(int rows, int cols, int type, void* data, uint64_t sig);
void ccv_make_matrix_mutable(ccv_matrix_t* mat);
/**
 * Mark the current matrix as immutable. Under the hood, it will generate a signature for the matrix, and mark it as non-collectable. For the convention, if the matrix is marked as immutable, you shouldn't change the content of the matrix, otherwise it will cause surprising behavior. If you want to change the content of the matrix, mark it as mutable first.
 * @param mat the supplied matrix that will be marked as immutable.
 **/
void ccv_make_matrix_immutable(ccv_matrix_t* mat);
CCV_WARN_UNUSED(ccv_sparse_matrix_t*) ccv_sparse_matrix_new(int rows, int cols, int type, int major, uint64_t sig);
void ccv_matrix_free_immediately(ccv_matrix_t* mat);
void ccv_matrix_free(ccv_matrix_t* mat);

uint64_t ccv_cache_generate_signature(const char* msg, int len, uint64_t sig_start, ...);

#define CCV_DEFAULT_CACHE_SIZE (1024 * 1024 * 64)

void ccv_drain_cache(void);
void ccv_disable_cache(void);
void ccv_enable_default_cache(void);
void ccv_enable_cache(size_t size);

#define ccv_get_dense_matrix_cell_by(type, x, row, col, ch) \
	(((type) & CCV_32S) ? (void*)((x)->data.i32 + ((row) * (x)->cols + (col)) * CCV_GET_CHANNEL(type) + (ch)) : \
	(((type) & CCV_32F) ? (void*)((x)->data.f32+ ((row) * (x)->cols + (col)) * CCV_GET_CHANNEL(type) + (ch)) : \
	(((type) & CCV_64S) ? (void*)((x)->data.i64+ ((row) * (x)->cols + (col)) * CCV_GET_CHANNEL(type) + (ch)) : \
	(((type) & CCV_64F) ? (void*)((x)->data.f64 + ((row) * (x)->cols + (col)) * CCV_GET_CHANNEL(type) + (ch)) : \
	(void*)((x)->data.u8 + (row) * (x)->step + (col) * CCV_GET_CHANNEL(type) + (ch))))))

#define ccv_get_dense_matrix_cell(x, row, col, ch) ccv_get_dense_matrix_cell_by((x)->type, x, row, col, ch)

/* this is for simplicity in code, I am sick of x->data.f64[i * x->cols + j] stuff, this is clearer, and compiler
 * can optimize away the if structures */
#define ccv_get_dense_matrix_cell_value_by(type, x, row, col, ch) \
	(((type) & CCV_32S) ? (x)->data.i32[((row) * (x)->cols + (col)) * CCV_GET_CHANNEL(type) + (ch)] : \
	(((type) & CCV_32F) ? (x)->data.f32[((row) * (x)->cols + (col)) * CCV_GET_CHANNEL(type) + (ch)] : \
	(((type) & CCV_64S) ? (x)->data.i64[((row) * (x)->cols + (col)) * CCV_GET_CHANNEL(type) + (ch)] : \
	(((type) & CCV_64F) ? (x)->data.f64[((row) * (x)->cols + (col)) * CCV_GET_CHANNEL(type) + (ch)] : \
	(x)->data.u8[(row) * (x)->step + (col) * CCV_GET_CHANNEL(type) + (ch)]))))

#define ccv_get_dense_matrix_cell_value(x, row, col, ch) ccv_get_dense_matrix_cell_value_by((x)->type, x, row, col, ch)

#define ccv_get_value(type, ptr, i) \
	(((type) & CCV_32S) ? ((int*)(ptr))[(i)] : \
	(((type) & CCV_32F) ? ((float*)(ptr))[(i)] : \
	(((type) & CCV_64S) ? ((int64_t*)(ptr))[(i)] : \
	(((type) & CCV_64F) ? ((double*)(ptr))[(i)] : \
	((unsigned char*)(ptr))[(i)]))))

#define ccv_set_value(type, ptr, i, value, factor) switch (CCV_GET_DATA_TYPE((type))) { \
	case CCV_32S: ((int*)(ptr))[(i)] = (int)(value) >> factor; break; \
	case CCV_32F: ((float*)(ptr))[(i)] = (float)value; break; \
	case CCV_64S: ((int64_t*)(ptr))[(i)] = (int64_t)(value) >> factor; break; \
	case CCV_64F: ((double*)(ptr))[(i)] = (double)value; break; \
	default: ((unsigned char*)(ptr))[(i)] = ccv_clamp((int)(value) >> factor, 0, 255); }

/* basic io ccv_io.c */

enum {
	// modifier for converting to gray-scale
	CCV_IO_GRAY      = 0x100,
	// modifier for converting to color
	CCV_IO_RGB_COLOR = 0x300,
};

enum {
	// modifier for not copy the data over when read raw in-memory data
	CCV_IO_NO_COPY = 0x10000,
};

enum {
	// read self-describe in-memory data
	CCV_IO_ANY_STREAM     = 0x010,
	CCV_IO_BMP_STREAM     = 0x011,
	CCV_IO_JPEG_STREAM    = 0x012,
	CCV_IO_PNG_STREAM     = 0x013,
	CCV_IO_PLAIN_STREAM   = 0x014,
	CCV_IO_DEFLATE_STREAM = 0x015,
	// read self-describe on-disk data
	CCV_IO_ANY_FILE       = 0x020,
	CCV_IO_BMP_FILE       = 0x021,
	CCV_IO_JPEG_FILE      = 0x022,
	CCV_IO_PNG_FILE       = 0x023,
	CCV_IO_BINARY_FILE    = 0x024,
	// read not-self-describe in-memory data (a.k.a. raw data)
	// you need to specify rows, cols, or scanline for these data
	CCV_IO_ANY_RAW        = 0x040,
	CCV_IO_RGB_RAW        = 0x041,
	CCV_IO_RGBA_RAW       = 0x042,
	CCV_IO_ARGB_RAW       = 0x043,
	CCV_IO_BGR_RAW        = 0x044,
	CCV_IO_BGRA_RAW       = 0x045,
	CCV_IO_ABGR_RAW       = 0x046,
	CCV_IO_GRAY_RAW       = 0x047,
};

enum {
	CCV_IO_FINAL = 0x00,
	CCV_IO_CONTINUE,
	CCV_IO_ERROR,
	CCV_IO_ATTEMPTED,
	CCV_IO_UNKNOWN,
};

int ccv_read_impl(const void* in, ccv_dense_matrix_t** x, int type, int rows, int cols, int scanline);
#define ccv_read_n(in, x, type, rows, cols, scanline, ...) \
	ccv_read_impl(in, x, type, rows, cols, scanline)
#define ccv_read(in, x, type, ...) \
	ccv_read_n(in, x, type, ##__VA_ARGS__, 0, 0, 0)
// this is a way to implement function-signature based dispatch, you can call either
// ccv_read(in, x, type) or ccv_read(in, x, type, rows, cols, scanline)
// notice that you can implement this with va_* functions, but that is not type-safe
int ccv_write(ccv_dense_matrix_t* mat, char* out, int* len, int type, void* conf);

/**
 * @defgroup ccv_algebra linear algebra
 * @{
 */

double ccv_trace(ccv_matrix_t* mat);

enum {
	CCV_L1_NORM  = 0x01, // |dx| + |dy|
	CCV_L2_NORM  = 0x02, // sqrt(dx^2 + dy^2)
	CCV_GSEDT    = 0x04, // Generalized Squared Euclidean Distance Transform:
						 // a * dx + b * dy + c * dx^2 + d * dy^2, when combined with CCV_L1_NORM:
						 // a * |dx| + b * |dy| + c * dx^2 + d * dy^2
	CCV_NEGATIVE = 0x08, // negative distance computation (from positive (min) to negative (max))
	CCV_POSITIVE = 0x00, // positive distance computation (the default)
};

enum {
	CCV_NO_PADDING = 0x00,
	CCV_PADDING_ZERO = 0x01,
	CCV_PADDING_EXTEND = 0x02,
	CCV_PADDING_MIRROR = 0x04,
};

enum {
	CCV_SIGNED = 0x00,
	CCV_UNSIGNED = 0x01,
};

double ccv_norm(ccv_matrix_t* mat, int type);
/**
 * Normalize a matrix and return the normalize factor.
 * @param a The input matrix.
 * @param b The output matrix.
 * @param btype The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param flag CCV_L1 or CCV_L2, for L1 or L2 normalization.
 * @return L1 or L2 sum.
 */
double ccv_normalize(ccv_matrix_t* a, ccv_matrix_t** b, int btype, int flag);
/**
 * Generate the [Summed Area Table](https://en.wikipedia.org/wiki/Summed_area_table).
 * @param a The input matrix.
 * @param b The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param padding_pattern CCV_NO_PADDING - the first row and the first column in the output matrix is the same as the input matrix. CCV_PADDING_ZERO - the first row and the first column in the output matrix is zero, thus, the output matrix size is 1 larger than the input matrix.
 */
void ccv_sat(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, int padding_pattern);
/**
 * Dot product of two matrix.
 * @param a The input matrix.
 * @param b The other input matrix.
 * @return Dot product.
 */
double ccv_dot(ccv_matrix_t* a, ccv_matrix_t* b);
/**
 * Return the sum of all elements in the matrix.
 * @param mat The input matrix.
 * @param flag CCV_UNSIGNED - compute fabs(x) of the elements first and then sum up. CCV_SIGNED - compute the sum normally.
 */
double ccv_sum(ccv_matrix_t* mat, int flag);
/**
 * Return the sum of all elements in the matrix.
 * @param mat The input matrix.
 * @return Element variance of the input matrix.
 */
double ccv_variance(ccv_matrix_t* mat);
/**
 * Do element-wise matrix multiplication.
 * @param a The input matrix.
 * @param b The input matrix.
 * @param c The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 */
void ccv_multiply(ccv_matrix_t* a, ccv_matrix_t* b, ccv_matrix_t** c, int type);
/**
 * Matrix addition.
 * @param a The input matrix.
 * @param b The input matrix.
 * @param c The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 */
void ccv_add(ccv_matrix_t* a, ccv_matrix_t* b, ccv_matrix_t** c, int type);
/**
 * Matrix subtraction.
 * @param a The input matrix.
 * @param b The input matrix.
 * @param c The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 */
void ccv_subtract(ccv_matrix_t* a, ccv_matrix_t* b, ccv_matrix_t** c, int type);
/**
 * Scale given matrix by factor of **ds**.
 * @param a The input matrix.
 * @param b The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param ds The scale factor, `b = a * ds`
 */
void ccv_scale(ccv_matrix_t* a, ccv_matrix_t** b, int type, double ds);

enum {
	CCV_A_TRANSPOSE = 0x01,
	CCV_B_TRANSPOSE = 0X02,
	CCV_C_TRANSPOSE = 0X04,
};

/**
 * General purpose matrix multiplication. This function has a hard dependency on [cblas](http://www.netlib.org/blas/) library.
 *
 * As general as it is, it computes:
 *
 *   alpha * A * B + beta * C
 *
 * whereas A, B, C are matrix, and alpha, beta are scalar.
 *
 * @param a The input matrix.
 * @param b The input matrix.
 * @param alpha The multiplication factor.
 * @param c The input matrix.
 * @param beta The multiplication factor.
 * @param transpose CCV_A_TRANSPOSE, CCV_B_TRANSPOSE to indicate if matrix A or B need to be transposed first before multiplication.
 * @param d The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 */
void ccv_gemm(ccv_matrix_t* a, ccv_matrix_t* b, double alpha, ccv_matrix_t* c, double beta, int transpose, ccv_matrix_t** d, int type);
/** @} */

typedef struct {
	int left;
	int top;
	int right;
	int bottom;
} ccv_margin_t;

inline static ccv_margin_t ccv_margin(int left, int top, int right, int bottom)
{
	ccv_margin_t margin;
	margin.left = left;
	margin.top = top;
	margin.right = right;
	margin.bottom = bottom;
	return margin;
}

/* matrix build blocks / utility functions ccv_util.c */

ccv_dense_matrix_t* ccv_get_dense_matrix(ccv_matrix_t* mat);
ccv_sparse_matrix_t* ccv_get_sparse_matrix(ccv_matrix_t* mat);
ccv_dense_vector_t* ccv_get_sparse_matrix_vector(ccv_sparse_matrix_t* mat, int index);
ccv_matrix_cell_t ccv_get_sparse_matrix_cell(ccv_sparse_matrix_t* mat, int row, int col);
void ccv_set_sparse_matrix_cell(ccv_sparse_matrix_t* mat, int row, int col, void* data);
void ccv_compress_sparse_matrix(ccv_sparse_matrix_t* mat, ccv_compressed_sparse_matrix_t** csm);
void ccv_decompress_sparse_matrix(ccv_compressed_sparse_matrix_t* csm, ccv_sparse_matrix_t** smt);

void ccv_move(ccv_matrix_t* a, ccv_matrix_t** b, int btype, int y, int x);
int ccv_matrix_eq(ccv_matrix_t* a, ccv_matrix_t* b);
void ccv_slice(ccv_matrix_t* a, ccv_matrix_t** b, int btype, int y, int x, int rows, int cols);
void ccv_border(ccv_matrix_t* a, ccv_matrix_t** b, int type, ccv_margin_t margin);
void ccv_visualize(ccv_matrix_t* a, ccv_matrix_t** b, int type);
void ccv_flatten(ccv_matrix_t* a, ccv_matrix_t** b, int type, int flag);
void ccv_zero(ccv_matrix_t* mat);
void ccv_shift(ccv_matrix_t* a, ccv_matrix_t** b, int type, int lr, int rr);
int ccv_any_nan(ccv_matrix_t *a);
ccv_dense_matrix_t ccv_reshape(ccv_dense_matrix_t* a, int y, int x, int rows, int cols);

// 32-bit float to 16-bit float
void ccv_float_to_half_precision(float* f, uint16_t* h, size_t len);
void ccv_half_precision_to_float(uint16_t* h, float* f, size_t len);

/* basic data structures ccv_util.c */

typedef struct {
	int width;
	int height;
} ccv_size_t;

inline static ccv_size_t ccv_size(int width, int height)
{
	ccv_size_t size;
	size.width = width;
	size.height = height;
	return size;
}

inline static int ccv_size_is_zero(ccv_size_t size)
{
	return size.width == 0 && size.height == 0;
}

typedef struct {
	int x;
	int y;
	int width;
	int height;
} ccv_rect_t;

inline static ccv_rect_t ccv_rect(int x, int y, int width, int height)
{
	ccv_rect_t rect;
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;
	return rect;
}

inline static int ccv_rect_is_zero(ccv_rect_t rect)
{
	return rect.x == 0 && rect.y == 0 && rect.width == 0 && rect.height == 0;
}

typedef struct {
	int type;
	uint64_t sig;
	int refcount;
	int rnum;
	int size;
	int rsize;
	void* data;
} ccv_array_t;

CCV_WARN_UNUSED(ccv_array_t*) ccv_array_new(int rsize, int rnum, uint64_t sig);
void ccv_array_push(ccv_array_t* array, const void* r);
typedef int(*ccv_array_group_f)(const void*, const void*, void*);
int ccv_array_group(ccv_array_t* array, ccv_array_t** index, ccv_array_group_f gfunc, void* data);
void ccv_make_array_immutable(ccv_array_t* array);
void ccv_make_array_mutable(ccv_array_t* array);
void ccv_array_zero(ccv_array_t* array);
void ccv_array_clear(ccv_array_t* array);
void ccv_array_free_immediately(ccv_array_t* array);
void ccv_array_free(ccv_array_t* array);

#define ccv_array_get(a, i) (((char*)((a)->data)) + (size_t)(a)->rsize * (size_t)(i))

typedef struct {
	int x, y;
} ccv_point_t;

inline static ccv_point_t ccv_point(int x, int y)
{
	ccv_point_t point;
	point.x = x;
	point.y = y;
	return point;
}

typedef struct {
	float x, y;
} ccv_decimal_point_t;

inline static ccv_decimal_point_t ccv_decimal_point(float x, float y)
{
	ccv_decimal_point_t point;
	point.x = x;
	point.y = y;
	return point;
}

typedef struct {
	float x, y, a, b;
	float roll, pitch, yaw;
} ccv_decimal_pose_t;

inline static ccv_decimal_pose_t ccv_decimal_pose(float x, float y, float a, float b, float roll, float pitch, float yaw)
{
	ccv_decimal_pose_t pose;
	pose.x = x;
	pose.y = y;
	pose.a = a;
	pose.b = b;
	pose.roll = roll;
	pose.pitch = pitch;
	pose.yaw = yaw;
	return pose;
}

typedef struct {
	ccv_rect_t rect;
	int size;
	ccv_array_t* set;
	long m10, m01, m11, m20, m02;
} ccv_contour_t;

ccv_contour_t* ccv_contour_new(int set);
void ccv_contour_push(ccv_contour_t* contour, ccv_point_t point);
void ccv_contour_free(ccv_contour_t* contour);

/* numerical algorithms ccv_numeric.c */

/* clarification about algebra and numerical algorithms:
 * when using the word "algebra", I assume the operation is well established in Mathematic sense
 * and can be calculated with a straight-forward, finite sequence of operation. The "numerical"
 * in other word, refer to a class of algorithm that can only approximate/or iteratively found the
 * solution. Thus, "invert" would be classified as numerical because of the sense that in some case,
 * it can only be "approximate" (in least-square sense), so to "solve". */

void ccv_invert(ccv_matrix_t* a, ccv_matrix_t** b, int type);
void ccv_solve(ccv_matrix_t* a, ccv_matrix_t* b, ccv_matrix_t** d, int type);
void ccv_eigen(ccv_dense_matrix_t* a, ccv_dense_matrix_t** vector, ccv_dense_matrix_t** lambda, int type, double epsilon);

typedef struct {
	double interp;
	double extrap;
	int max_iter;
	double ratio;
	double rho;
	double sig;
} ccv_minimize_param_t;

extern const ccv_minimize_param_t ccv_minimize_default_params;

typedef int(*ccv_minimize_f)(const ccv_dense_matrix_t* x, double* f, ccv_dense_matrix_t* df, void*);
void ccv_minimize(ccv_dense_matrix_t* x, int length, double red, ccv_minimize_f func, ccv_minimize_param_t params, void* data);

void ccv_filter(ccv_dense_matrix_t* a, ccv_dense_matrix_t* b, ccv_dense_matrix_t** d, int type, int padding_pattern);
typedef double(*ccv_filter_kernel_f)(double x, double y, void*);
void ccv_filter_kernel(ccv_dense_matrix_t* x, ccv_filter_kernel_f func, void* data);

/* modern numerical algorithms */

void ccv_distance_transform(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, ccv_dense_matrix_t** x, int x_type, ccv_dense_matrix_t** y, int y_type, double dx, double dy, double dxx, double dyy, int flag);
void ccv_sparse_coding(ccv_matrix_t* x, int k, ccv_matrix_t** A, int typeA, ccv_matrix_t** y, int typey);
void ccv_compressive_sensing_reconstruct(ccv_matrix_t* a, ccv_matrix_t* x, ccv_matrix_t** y, int type);

/**
 * @defgroup ccv_basic basic image pre-processing utilities
 * The utilities in this file provides basic pre-processing which, most-likely, are the first steps for computer vision algorithms.
 * @{
 */

/**
 * Compute image with [Sobel operator](https://en.wikipedia.org/wiki/Sobel_operator).
 * @param a The input matrix.
 * @param b The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param dx The window size of Sobel operator on x-axis, specially optimized for 1, 3
 * @param dy The window size of Sobel operator on y-axis, specially optimized for 1, 3
 */
void ccv_sobel(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, int dx, int dy);
/**
 * Compute the gradient (angle and magnitude) at each pixel.
 * @param a The input matrix.
 * @param theta The output matrix of angle at each pixel.
 * @param ttype The type of output matrix, if 0, ccv will defaults to CCV_32F.
 * @param m The output matrix of magnitude at each pixel.
 * @param mtype The type of output matrix, if 0, ccv will defaults to CCV_32F.
 * @param dx The window size of the underlying Sobel operator used on x-axis, specially optimized for 1, 3
 * @param dy The window size of the underlying Sobel operator used on y-axis, specially optimized for 1, 3
 */
void ccv_gradient(ccv_dense_matrix_t* a, ccv_dense_matrix_t** theta, int ttype, ccv_dense_matrix_t** m, int mtype, int dx, int dy);

enum {
	CCV_FLIP_X = 0x01,
	CCV_FLIP_Y = 0x02,
};
/**
 * Flip the matrix by x-axis, y-axis or both.
 * @param a The input matrix.
 * @param b The output matrix (it is in-place safe).
 * @param btype The type of output matrix, if 0, ccv will use the sample type as the input matrix.
 * @param type CCV_FLIP_X - flip around x-axis, CCV_FLIP_Y - flip around y-axis.
 */
void ccv_flip(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int btype, int type);
/**
 * Using [Gaussian blur](https://en.wikipedia.org/wiki/Gaussian_blur) on a given matrix. It implements a O(n * sqrt(m)) algorithm, n is the size of input matrix, m is the size of Gaussian filtering kernel.
 * @param a The input matrix.
 * @param b The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param sigma The sigma factor in Gaussian filtering kernel.
 */
void ccv_blur(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, double sigma);
/** @} */

/**
 * @defgroup ccv_image_processing image processing utilities
 * The utilities in this file provides image processing methods that are widely used for image enhancements.
 * @{
 */

enum {
	CCV_RGB_TO_YUV = 0x01,
};

/**
 * Convert matrix from one color space representation to another.
 * @param a The input matrix.
 * @param b The output matrix.
 * @param type The type of output matrix, if 0, ccv will use the sample type as the input matrix.
 * @param flag **CCV_RGB_TO_YUV** to convert from RGB color space to YUV color space.
 */
void ccv_color_transform(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, int flag);
/**
 * Manipulate image's saturation.
 * @param a The input matrix.
 * @param b The output matrix (it is in-place safe).
 * @param type The type of output matrix, if 0, ccv will use the sample type as the input matrix.
 * @param ds The coefficient (0: grayscale, 1: original).
 */
void ccv_saturation(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, double ds);
/**
 * Manipulate image's contrast.
 * @param a The input matrix.
 * @param b The output matrix (it is in-place safe).
 * @param type The type of output matrix, if 0, ccv will use the sample type as the input matrix.
 * @param ds The coefficient (0: mean image, 1: original).
 */
void ccv_contrast(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, double ds);
/** @} */

/**
 * @defgroup ccv_resample image resampling utilities
 * @{
 */

enum {
	CCV_INTER_AREA    = 0x01,
	CCV_INTER_LINEAR  = 0X02,
	CCV_INTER_CUBIC   = 0X04,
	CCV_INTER_LANCZOS = 0X08,
};

/**
 * Resample a given matrix to different size, as for now, ccv only supports either downsampling (with CCV_INTER_AREA) or upsampling (with CCV_INTER_CUBIC).
 * @param a The input matrix.
 * @param b The output matrix.
 * @param btype The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param rows The new row.
 * @param cols The new column.
 * @param type For now, ccv supports CCV_INTER_AREA, which is an extension to [bilinear resampling](https://en.wikipedia.org/wiki/Bilinear_filtering) for downsampling and CCV_INTER_CUBIC [bicubic resampling](https://en.wikipedia.org/wiki/Bicubic_interpolation) for upsampling.
 */
void ccv_resample(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int btype, int rows, int cols, int type);
/**
 * Downsample a given matrix to exactly half size with a [Gaussian filter](https://en.wikipedia.org/wiki/Gaussian_filter). The half size is approximated by floor(rows * 0.5) x floor(cols * 0.5).
 * @param a The input matrix.
 * @param b The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param src_x Shift the start point by src_x.
 * @param src_y Shift the start point by src_y.
 */
void ccv_sample_down(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, int src_x, int src_y);
/**
 * Upsample a given matrix to exactly double size with a [Gaussian filter](https://en.wikipedia.org/wiki/Gaussian_filter).
 * @param a The input matrix.
 * @param b The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param src_x Shift the start point by src_x.
 * @param src_y Shift the start point by src_y.
 */
void ccv_sample_up(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, int src_x, int src_y);
/** @} */

/**
 * @defgroup ccv_transform image transform utilities
 * @{
 */

/**
 * Similar to ccv_slice, it will slice a given matrix into required rows / cols, but it will interpolate the value with bilinear filter if x and y is non-integer.
 * @param a The given matrix that will be sliced
 * @param b The output matrix
 * @param type The type of output matrix
 * @param y The top point to slice
 * @param x The left point to slice
 * @param rows The number of rows for destination matrix
 * @param cols The number of cols for destination matrix
 */
void ccv_decimal_slice(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, float y, float x, int rows, int cols);
/**
 * Apply a [3D transform](https://en.wikipedia.org/wiki/Perspective_transform#Perspective_projection) against the given point in a given image size, assuming field of view is 60 (in degree).
 * @param point The point to be transformed in decimal
 * @param size The image size
 * @param m00, m01, m02, m10, m11, m12, m20, m21, m22 The transformation matrix
 */
ccv_decimal_point_t ccv_perspective_transform_apply(ccv_decimal_point_t point, ccv_size_t size, float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22);
/**
 * Apply a [3D transform](https://en.wikipedia.org/wiki/Perspective_transform#Perspective_projection) on a given matrix, assuming field of view is 60 (in degree).
 * @param a The given matrix to be transformed
 * @param b The output matrix
 * @param type The type of output matrix
 * @param m00, m01, m02, m10, m11, m12, m20, m21, m22 The transformation matrix
 */
void ccv_perspective_transform(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, float m00, float m01, float m02, float m10, float m11, float m12, float m20, float m21, float m22);
/** @} */

/* classic computer vision algorithms ccv_classic.c */
/**
 * @defgroup ccv_classic classic computer vision algorithms
 * @{
 */

/**
 * [Histogram-of-Oriented-Gradients](https://en.wikipedia.org/wiki/Histogram_of_oriented_gradients) implementation, specifically, it implements the HOG described in *Object Detection with Discriminatively Trained Part-Based Models, Pedro F. Felzenszwalb, Ross B. Girshick, David McAllester and Deva Ramanan*.
 * @param a The input matrix.
 * @param b The output matrix.
 * @param b_type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param sbin The number of bins for orientation (default to 9, thus, for **b**, it will have 9 * 2 + 9 + 4 = 31 channels).
 * @param size The window size for HOG (default to 8)
 */
void ccv_hog(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int b_type, int sbin, int size);
/**
 * [Canny edge detector](https://en.wikipedia.org/wiki/Canny_edge_detector) implementation. For performance reason, this is a clean-up reimplementation of OpenCV's Canny edge detector, it has very similar performance characteristic as the OpenCV one. As of today, ccv's Canny edge detector only works with CCV_8U or CCV_32S dense matrix type.
 * @param a The input matrix.
 * @param b The output matrix.
 * @param type The type of output matrix, if 0, ccv will create a CCV_8U | CCV_C1 matrix.
 * @param size The underlying Sobel filter size.
 * @param low_thresh The low threshold that makes the point interesting.
 * @param high_thresh The high threshold that makes the point acceptable.
 */
void ccv_canny(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, int size, double low_thresh, double high_thresh);
void ccv_close_outline(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type);
/* range: exclusive, return value: inclusive (i.e., threshold = 5, 0~5 is background, 6~range-1 is foreground */
/**
 * [OTSU](https://en.wikipedia.org/wiki/Otsu%27s_method) implementation.
 * @param a The input matrix.
 * @param outvar The inter-class variance.
 * @param range The maximum range of data in the input matrix.
 * @return The threshold, inclusively. e.g. 5 means 0~5 is in the background, and 6~255 is in the foreground.
 */
int ccv_otsu(ccv_dense_matrix_t* a, double* outvar, int range);

typedef struct {
	ccv_decimal_point_t point;
	uint8_t status;
} ccv_decimal_point_with_status_t;

/**
 * [Lucas Kanade](https://en.wikipedia.org/wiki/Lucas%E2%80%93Kanade_Optical_Flow_Method) optical flow implementation with image pyramid extension.
 * @param a The first frame
 * @param b The next frame
 * @param point_a The points in first frame, of **ccv_decimal_point_t** type
 * @param point_b The output points in the next frame, of **ccv_decimal_point_with_status_t** type
 * @param win_size The window size to compute each optical flow, it must be a odd number
 * @param level How many image pyramids to be used for the computation
 * @param min_eigen The minimal eigen-value to pass optical flow computation
 */
void ccv_optical_flow_lucas_kanade(ccv_dense_matrix_t* a, ccv_dense_matrix_t* b, ccv_array_t* point_a, ccv_array_t** point_b, ccv_size_t win_size, int level, double min_eigen);
/** @} */

/* modern computer vision algorithms */
/* SIFT, DAISY, SWT, MSER, DPM, BBF, SGF, SSD, FAST */

/**
 * @defgroup ccv_daisy DAISY
 * @{
 */

/* daisy related methods */
typedef struct {
	double radius; /** the Gaussian radius. */
	int rad_q_no;
	int th_q_no;
	int hist_th_q_no;
	float normalize_threshold;
	int normalize_method;
} ccv_daisy_param_t;

enum {
	CCV_DAISY_NORMAL_PARTIAL = 0x01,
	CCV_DAISY_NORMAL_FULL    = 0x02,
	CCV_DAISY_NORMAL_SIFT    = 0x03,
};

/**
 * [DAISY](http://cvlab.epfl.ch/publications/publications/2010/TolaLF10a.pdf) implementation. For more details - DAISY: An Efficient Dense Descriptor Applied to Wide-Baseline Stereo.
 * @param a The input matrix.
 * @param b The output matrix.
 * @param type The type of output matrix, if 0, ccv will try to match the input matrix for appropriate type.
 * @param params A **ccv_daisy_param_t** structure that defines various aspect of the feature extractor.
 */
void ccv_daisy(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, ccv_daisy_param_t params);
/** @} */

/* sift related methods */
typedef struct {
	float x, y;
	int octave;
	int level;
	union {
		struct {
			double a, b;
			double c, d;
		} affine;
		struct {
			double scale;
			double angle;
		} regular;
	};
} ccv_keypoint_t;

typedef struct {
	int up2x;
	int noctaves;
	int nlevels;
	float edge_threshold;
	float peak_threshold;
	float norm_threshold;
} ccv_sift_param_t;

extern const ccv_sift_param_t ccv_sift_default_params;

void ccv_sift(ccv_dense_matrix_t* a, ccv_array_t** keypoints, ccv_dense_matrix_t** desc, int type, ccv_sift_param_t params);

/* mser related method */

typedef struct {
	/* parameters for MSER */
	int delta;
	int min_area; /* default: 60 */
	int direction; /* default: 0, 0 for both, -1 for bright to dark, 1 for dark to bright */
	int max_area;
	double max_variance;
	double min_diversity;
	int range; /* from 0 to range, inclusive */
	/* parameters for MSCR */
	double area_threshold; /* default: 1.01 */
	double min_margin; /* default: 0.003 */
	int max_evolution;
	double edge_blur_sigma; /* default: 1.0 */
} ccv_mser_param_t;

typedef struct {
	ccv_rect_t rect;
	int size;
	long m10, m01, m11, m20, m02;
	ccv_point_t keypoint;
} ccv_mser_keypoint_t;

enum {
	CCV_BRIGHT_TO_DARK = -1,
	CCV_DARK_TO_BRIGHT = 1,
};

CCV_WARN_UNUSED(ccv_array_t*) ccv_mser(ccv_dense_matrix_t* a, ccv_dense_matrix_t* h, ccv_dense_matrix_t** b, int type, ccv_mser_param_t params);

/* swt related method: stroke width transform is relatively new, typically used in text detection */
typedef struct {
	int interval; // for scale invariant option
	int min_neighbors; // minimal neighbors to make a detection valid, this is for scale-invariant version
	int scale_invariant; // enable scale invariant swt (to scale to different sizes and then combine the results)
	int direction;
	double same_word_thresh[2]; // overlapping more than 0.1 of the bigger one (0), and 0.9 of the smaller one (1)
	/* canny parameters */
	int size;
	int low_thresh;
	int high_thresh;
	/* geometry filtering parameters */
	int max_height;
	int min_height;
	int min_area;
	int letter_occlude_thresh;
	double aspect_ratio;
	double std_ratio;
	/* grouping parameters */
	double thickness_ratio;
	double height_ratio;
	int intensity_thresh;
	double distance_ratio;
	double intersect_ratio;
	double elongate_ratio;
	int letter_thresh;
	/* break textline into words */
	int breakdown;
	double breakdown_ratio;
} ccv_swt_param_t;

extern const ccv_swt_param_t ccv_swt_default_params;

void ccv_swt(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type, ccv_swt_param_t params);
CCV_WARN_UNUSED(ccv_array_t*) ccv_swt_detect_words(ccv_dense_matrix_t* a, ccv_swt_param_t params);

/* it makes sense now to include a simple data structure that encapsulate the common idiom of
 * having file name with a bounding box */

typedef struct {
	char* filename;
	union {
		ccv_rect_t box;
		ccv_decimal_pose_t pose;
	};
} ccv_file_info_t;

/* I'd like to include Deformable Part Models as a general object detection method in here
 * The difference between BBF and DPM:
 * ~ BBF is for rigid object detection: banners, box, faces etc.
 * ~ DPM is more generalized, can detect people, car, bike (larger inner-class difference) etc.
 * ~ BBF is blazing fast (few milliseconds), DPM is relatively slow (around 1 seconds or so) */

/**
 * @defgroup ccv_dpm deformable parts model
 * @{
 */

#define CCV_DPM_PART_MAX (10)

typedef struct {
	int id;
	float confidence;
} ccv_classification_t;

typedef struct {
	ccv_rect_t rect;
	int neighbors;
	ccv_classification_t classification;
} ccv_comp_t;

typedef struct {
	ccv_rect_t rect;
	int neighbors;
	ccv_classification_t classification;
	int pnum;
	ccv_comp_t part[CCV_DPM_PART_MAX];
} ccv_root_comp_t;

typedef struct {
	ccv_dense_matrix_t* w;
	double dx, dy, dxx, dyy;
	int x, y, z;
	int counterpart;
	float alpha[6];
} ccv_dpm_part_classifier_t;

typedef struct {
	int count;
	ccv_dpm_part_classifier_t root;
	ccv_dpm_part_classifier_t* part;
	float alpha[3], beta;
} ccv_dpm_root_classifier_t;

typedef struct {
	int count;
	ccv_dpm_root_classifier_t* root;
} ccv_dpm_mixture_model_t;

typedef struct {
	int interval; /** Interval images between the full size image and the half size one. e.g. 2 will generate 2 images in between full size image and half size one: image with full size, image with 5/6 size, image with 2/3 size, image with 1/2 size. */
	int min_neighbors; /** 0: no grouping afterwards. 1: group objects that intersects each other. > 1: group objects that intersects each other, and only passes these that have at least **min_neighbors** intersected objects. */
	int flags; /** CCV_DPM_NO_NESTED, if one class of object is inside another class of object, this flag will reject the first object. */
	float threshold; /** The threshold the determines the acceptance of an object. */
} ccv_dpm_param_t;

typedef struct {
	int components; /** The number of root filters in the mixture model. */
	int parts; /** The number of part filters for each root filter. */
	int grayscale; /** Whether to exploit color in a given image. */
	int symmetric; /** Whether to exploit symmetric property of the object. */
	int min_area; /** The minimum area that one part classifier can occupy, 3000 is a reasonable number. */
	int max_area; /** The maximum area that one part classifier can occupy. 5000 is a reasonable number. */
	int iterations; /** How many iterations needed for stochastic gradient descent. */
	int data_minings; /** How many data mining procedures are needed for discovering hard examples. */
	int root_relabels; /** How many relabel procedures for root classifier are needed. */
	int relabels; /** How many relabel procedures are needed. */
	int discard_estimating_constant; // 1
	int negative_cache_size; /** The cache size for negative examples. 1000 is a reasonable number. */
	double include_overlap; /** The percentage of overlap between expected bounding box and the bounding box from detection. Beyond this threshold, it is ensured to be the same object. 0.7 is a reasonable number. */
	double alpha; /** The step size for stochastic gradient descent. */
	double alpha_ratio; /** Decrease the step size for each iteration. 0.85 is a reasonable number. */
	double balance; /** To balance the weight of positive examples and negative examples. 1.5 is a reasonable number. */
	double C; /** C in SVM. */
	double percentile_breakdown; /** The percentile use for breakdown threshold. 0.05 is the default. */
	ccv_dpm_param_t detector; /** A **ccv_dpm_params_t** structure that will be used to search positive examples and negative examples from background images. */
} ccv_dpm_new_param_t;

enum {
	CCV_DPM_NO_NESTED = 0x10000000,
};

extern const ccv_dpm_param_t ccv_dpm_default_params;

/**
 * Create a new DPM mixture model from given positive examples and background images. This function has hard dependencies on [GSL](http://www.gnu.org/software/gsl/) and [LibLinear](http://www.csie.ntu.edu.tw/~cjlin/liblinear/).
 * @param posfiles An array of positive images.
 * @param bboxes An array of bounding boxes for positive images.
 * @param posnum Number of positive examples.
 * @param bgfiles An array of background images.
 * @param bgnum Number of background images.
 * @param negnum Number of negative examples that is harvested from background images.
 * @param dir The working directory to store/retrieve intermediate data.
 * @param params A **ccv_dpm_new_param_t** structure that defines various aspects of the training function.
 */
void ccv_dpm_mixture_model_new(char** posfiles, ccv_rect_t* bboxes, int posnum, char** bgfiles, int bgnum, int negnum, const char* dir, ccv_dpm_new_param_t params);
/**
 * Using a DPM mixture model to detect objects in a given image. If you have several DPM mixture models, it is better to use them in one method call. In this way, ccv will try to optimize the overall performance.
 * @param a The input image.
 * @param model An array of mixture models.
 * @param count How many mixture models you've passed in.
 * @param params A **ccv_dpm_param_t** structure that defines various aspects of the detector.
 * @return A **ccv_array_t** of **ccv_root_comp_t** that contains the root bounding box as well as its parts.
 */
CCV_WARN_UNUSED(ccv_array_t*) ccv_dpm_detect_objects(ccv_dense_matrix_t* a, ccv_dpm_mixture_model_t** model, int count, ccv_dpm_param_t params);
/**
 * Read DPM mixture model from a model file.
 * @param directory The model file for DPM mixture model.
 * @return A DPM mixture model, 0 if no valid DPM mixture model available.
 */
CCV_WARN_UNUSED(ccv_dpm_mixture_model_t*) ccv_dpm_read_mixture_model(const char* directory);
/**
 * Free up the memory of DPM mixture model.
 * @param model The DPM mixture model.
 */
void ccv_dpm_mixture_model_free(ccv_dpm_mixture_model_t* model);
/** @} */

/**
 * @defgroup ccv_bbf binary brightness feature
 * this is open source implementation of object detection algorithm: brightness binary feature
 * it is an extension/modification of original HAAR-like feature with Adaboost, featured faster
 * computation and higher accuracy (current highest accuracy close-source face detector is based
 * on the same algorithm)
 * @{
 */

#define CCV_BBF_POINT_MAX (8)
#define CCV_BBF_POINT_MIN (3)

typedef struct {
	int size;
	int px[CCV_BBF_POINT_MAX];
	int py[CCV_BBF_POINT_MAX];
	int pz[CCV_BBF_POINT_MAX];
	int nx[CCV_BBF_POINT_MAX];
	int ny[CCV_BBF_POINT_MAX];
	int nz[CCV_BBF_POINT_MAX];
} ccv_bbf_feature_t;

typedef struct {
	int count;
	float threshold;
	ccv_bbf_feature_t* feature;
	float* alpha;
} ccv_bbf_stage_classifier_t;

typedef struct {
	int count;
	ccv_size_t size;
	ccv_bbf_stage_classifier_t* stage_classifier;
} ccv_bbf_classifier_cascade_t;

enum {
	CCV_BBF_GENETIC_OPT = 0x01,
	CCV_BBF_FLOAT_OPT = 0x02,
};

typedef struct {
	int interval; /** Interval images between the full size image and the half size one. e.g. 2 will generate 2 images in between full size image and half size one: image with full size, image with 5/6 size, image with 2/3 size, image with 1/2 size. */
	int min_neighbors; /** 0: no grouping afterwards. 1: group objects that intersects each other. > 1: group objects that intersects each other, and only passes these that have at least **min_neighbors** intersected objects. */
	int flags; /** CCV_BBF_NO_NESTED, if one class of object is inside another class of object, this flag will reject the first object. */
	int accurate; /** BBF will generates 4 spatial scale variations for better accuracy. Set this parameter to 0 will reduce to 1 scale variation, and thus 3 times faster but lower the general accuracy of the detector. */
	ccv_size_t size; /** The smallest object size that will be interesting to us. */
} ccv_bbf_param_t;

typedef struct {
	double pos_crit; /** Positive criteria or the targeted recall ratio, BBF classifier tries to adjust the constant to meet this criteria. */
	double neg_crit; /** Negative criteria or the targeted reject ratio, BBF classifier tries to include more weak features until meet this criteria. */
	double balance_k; /** Weight positive examples differently from negative examples. */
	int layer; /** The maximum layer trained for the classifier cascade. */
	int feature_number; /** The maximum feature number for each classifier. */
	int optimizer; /** CCV_BBF_GENETIC_OPT, using genetic algorithm to search the best weak feature; CCV_BBF_FLOAT_OPT, using float search to improve the found best weak feature. */
	ccv_bbf_param_t detector; /** A **ccv_bbf_params_t** structure that will be used to search negative examples from background images. */
} ccv_bbf_new_param_t;

enum {
	CCV_BBF_NO_NESTED = 0x10000000,
};

extern const ccv_bbf_param_t ccv_bbf_default_params;

/**
 * Create a new BBF classifier cascade from given positive examples and background images. This function has a hard dependency on [GSL](http://www.gnu.org/software/gsl/).
 * @param posimg An array of positive examples.
 * @param posnum Number of positive examples.
 * @param bgfiles An array of background images.
 * @param bgnum Number of background images.
 * @param negnum Number of negative examples that is harvested from background images.
 * @param size The image size of positive examples.
 * @param dir The working directory to store/retrieve intermediate data.
 * @param params A **ccv_bbf_new_param_t** structure that defines various aspects of the training function.
 */
void ccv_bbf_classifier_cascade_new(ccv_dense_matrix_t** posimg, int posnum, char** bgfiles, int bgnum, int negnum, ccv_size_t size, const char* dir, ccv_bbf_new_param_t params);
/**
 * Using a BBF classifier cascade to detect objects in a given image. If you have several classifier cascades, it is better to use them in one method call. In this way, ccv will try to optimize the overall performance.
 * @param a The input image.
 * @param cascade An array of classifier cascades.
 * @param count How many classifier cascades you've passed in.
 * @param params A **ccv_bbf_param_t** structure that defines various aspects of the detector.
 * @return A **ccv_array_t** of **ccv_comp_t** for detection results.
 */
CCV_WARN_UNUSED(ccv_array_t*) ccv_bbf_detect_objects(ccv_dense_matrix_t* a, ccv_bbf_classifier_cascade_t** cascade, int count, ccv_bbf_param_t params);
/**
 * Read BBF classifier cascade from working directory.
 * @param directory The working directory that trains a BBF classifier cascade.
 * @return A classifier cascade, 0 if no valid classifier cascade available.
 */
CCV_WARN_UNUSED(ccv_bbf_classifier_cascade_t*) ccv_bbf_read_classifier_cascade(const char* directory);
/**
 * Free up the memory of BBF classifier cascade.
 * @param cascade The BBF classifier cascade.
 */
void ccv_bbf_classifier_cascade_free(ccv_bbf_classifier_cascade_t* cascade);
/**
 * Load BBF classifier cascade from a memory region.
 * @param s The memory region of binarized BBF classifier cascade.
 * @return A classifier cascade, 0 if no valid classifier cascade available.
 */
CCV_WARN_UNUSED(ccv_bbf_classifier_cascade_t*) ccv_bbf_classifier_cascade_read_binary(char* s);
/**
 * Write BBF classifier cascade to a memory region.
 * @param cascade The BBF classifier cascade.
 * @param s The designated memory region.
 * @param slen The size of the designated memory region.
 * @return The actual size of the binarized BBF classifier cascade, if this size is larger than **slen**, please reallocate the memory region and do it again.
 */
int ccv_bbf_classifier_cascade_write_binary(ccv_bbf_classifier_cascade_t* cascade, char* s, int slen);
/** @} */

/* Ferns classifier: this is a fern implementation that specifically used for TLD
 * see: http://cvlab.epfl.ch/alumni/oezuysal/ferns.html for more about ferns */

typedef struct {
	int structs;
	int features;
	int scales;
	int posteriors;
	float threshold;
	int cnum[2];
	int* rnum;
	float* posterior;
	// decided to go flat organized fern so that we can profiling different memory layout impacts the performance
	ccv_point_t fern[1];
} ccv_ferns_t;

CCV_WARN_UNUSED(ccv_ferns_t*) ccv_ferns_new(int structs, int features, int scales, ccv_size_t* sizes);
void ccv_ferns_feature(ccv_ferns_t* ferns, ccv_dense_matrix_t* a, int scale, uint32_t* fern);
void ccv_ferns_correct(ccv_ferns_t* ferns, uint32_t* fern, int c, int repeat);
float ccv_ferns_predict(ccv_ferns_t* ferns, uint32_t* fern);
void ccv_ferns_free(ccv_ferns_t* ferns);

/* TLD: Track-Learn-Detection is a long-term object tracking framework, which achieved very high
 * tracking accuracy, this is the tracking algorithm of choice ccv implements */

typedef struct {
	/* short-term lucas-kanade tracking parameters */
	ccv_size_t win_size;
	int level;
	float min_eigen;
	float min_forward_backward_error;
	/* image pyramid (different resolution) generation parameters */
	int interval;
	float shift;
	/* samples generation parameters */
	int min_win;
	float include_overlap;
	float exclude_overlap;
	/* fern classifier setting */
	int structs;
	int features;
	/* nearest neighbor thresholds */
	float validate_set; // 0.5 for conservative confidence
	float nnc_same; // the same object
	float nnc_thres; // highly correlated
	float nnc_verify; // correlated with tracking
	float nnc_beyond; // this is the cap of nnc_thres
	float nnc_collect; // modest correlated, worth to collect as negative example
	int bad_patches; // number of bad patches
	/* deformation round */
	int new_deform;
	int track_deform;
	float new_deform_angle;
	float track_deform_angle;
	float new_deform_scale;
	float track_deform_scale;
	float new_deform_shift;
	float track_deform_shift;
	/* top detections */
	int top_n;
	/* speed up technique, instead of running slide window at
	 * every frame, we will rotate them, for example, slide window 1
	 * only gets examined at frame % rotation == 1 */
	int rotation;
} ccv_tld_param_t;

extern const ccv_tld_param_t ccv_tld_default_params;

typedef struct {
	ccv_tld_param_t params;
	ccv_comp_t box; // tracking comp
	ccv_ferns_t* ferns; // ferns classifier
	ccv_array_t* sv[2]; // example-based classifier
	ccv_size_t patch; // resized to patch for example-based classifier
	int found; // if the last time found a valid box
	int verified; // the last frame is verified, therefore, a successful tracking is verified too
	ccv_array_t* top; // top matches
	float ferns_thres; // computed dynamically from negative examples
	float nnc_thres; // computed dynamically from negative examples
	float nnc_verify_thres; // computed dynamically from negative examples
	double var_thres; // computed dynamically from the supplied same
	uint64_t frame_signature;
	int count;
	void* sfmt;
	void* dsfmt;
	uint32_t fern_buffer[1]; // fetched ferns from image, this is a buffer
} ccv_tld_t;

typedef struct {
	int perform_track;
	int perform_learn;
	int track_success;
	int ferns_detects;
	int nnc_detects;
	int clustered_detects;
	int confident_matches;
	int close_matches;
} ccv_tld_info_t;

CCV_WARN_UNUSED(ccv_tld_t*) ccv_tld_new(ccv_dense_matrix_t* a, ccv_rect_t box, ccv_tld_param_t params);
ccv_comp_t ccv_tld_track_object(ccv_tld_t* tld, ccv_dense_matrix_t* a, ccv_dense_matrix_t* b, ccv_tld_info_t* info);
void ccv_tld_free(ccv_tld_t* tld);

/* ICF: Integral Channels Features, this is a theorized framework that retrospectively incorporates the original
 * Viola-Jones detection method with various enhancement later. Specifically, this implementation is after:
 * Pedestrian detection at 100 frames per second, Rodrigo Benenson, Markus Mathias, Radu Timofte and Luc Van Gool
 * With WFS (width first search) tree from:
 * High-Performance Rotation Invariant Multiview Face Detection, Chang Huang, Haizhou Ai, Yuan Li and Shihong Lao */

#define CCV_ICF_SAT_MAX (2)

typedef struct {
	int count;
	int channel[CCV_ICF_SAT_MAX];
	ccv_point_t sat[CCV_ICF_SAT_MAX * 2];
	float alpha[CCV_ICF_SAT_MAX];
	float beta;
} ccv_icf_feature_t;

typedef struct {
	// we use depth-2 decision tree
	uint32_t pass;
	ccv_icf_feature_t features[3];
	float weigh[2];
	float threshold;
} ccv_icf_decision_tree_t;

enum {
	CCV_ICF_CLASSIFIER_TYPE_A = 0x1,
	CCV_ICF_CLASSIFIER_TYPE_B = 0x2,
};

typedef struct {
	int type;
	int count;
	int grayscale;
	ccv_margin_t margin;
	ccv_size_t size; // this is the size includes the margin
	ccv_icf_decision_tree_t* weak_classifiers;
} ccv_icf_classifier_cascade_t; // Type A, scale image

typedef struct {
	int type;
	int count;
	int octave;
	int grayscale;
	ccv_icf_classifier_cascade_t* cascade;
} ccv_icf_multiscale_classifier_cascade_t; // Type B, scale the classifier

typedef struct {
	int min_neighbors;
	int flags;
	int step_through;
	int interval;
	float threshold;
} ccv_icf_param_t;

extern const ccv_icf_param_t ccv_icf_default_params;

typedef struct {
	ccv_icf_param_t detector;
	int grayscale;
	int min_dimension;
	ccv_margin_t margin;
	ccv_size_t size; // this is the size excludes the margin
	int feature_size;
	int weak_classifier;
	int bootstrap;
	float deform_angle;
	float deform_scale;
	float deform_shift;
	double acceptance;
} ccv_icf_new_param_t;

void ccv_icf(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type);

/* ICF for single scale */
CCV_WARN_UNUSED(ccv_icf_classifier_cascade_t*) ccv_icf_classifier_cascade_new(ccv_array_t* posfiles, int posnum, ccv_array_t* bgfiles, int negnum, ccv_array_t* testfiles, const char* dir, ccv_icf_new_param_t params);
void ccv_icf_classifier_cascade_soft(ccv_icf_classifier_cascade_t* cascade, ccv_array_t* posfiles, double acceptance);
CCV_WARN_UNUSED(ccv_icf_classifier_cascade_t*) ccv_icf_read_classifier_cascade(const char* filename);
void ccv_icf_write_classifier_cascade(ccv_icf_classifier_cascade_t* classifier, const char* filename);
void ccv_icf_classifier_cascade_free(ccv_icf_classifier_cascade_t* classifier);

/* ICF for multiple scale */
CCV_WARN_UNUSED(ccv_icf_multiscale_classifier_cascade_t*) ccv_icf_multiscale_classifier_cascade_new(ccv_icf_classifier_cascade_t* cascades, int octave, int interval);
CCV_WARN_UNUSED(ccv_icf_multiscale_classifier_cascade_t*) ccv_icf_read_multiscale_classifier_cascade(const char* directory);
void ccv_icf_write_multiscale_classifier_cascade(ccv_icf_multiscale_classifier_cascade_t* classifier, const char* directory);
void ccv_icf_multiscale_classifier_cascade_free(ccv_icf_multiscale_classifier_cascade_t* classifier);

/* polymorph function to run ICF based detector */
CCV_WARN_UNUSED(ccv_array_t*) ccv_icf_detect_objects(ccv_dense_matrix_t* a, void* cascade, int count, ccv_icf_param_t params);

/* SCD: SURF-Cascade Detector
 * This is a variant of VJ framework for object detection
 * Read: Learning SURF Cascade for Fast and Accurate Object Detection
 */

typedef struct {
	int sx[4];
	int sy[4];
	int dx[4];
	int dy[4];
	float bias;
	float w[32];
} ccv_scd_stump_feature_t;

typedef struct {
	int count;
	ccv_scd_stump_feature_t* features;
	float threshold;
} ccv_scd_stump_classifier_t;

// this is simpler than ccv's icf feature, largely inspired
// by the latest implementation of doppia, it seems more restrictive
// approach can generate more robust feature due to the overfitting
// nature of decision tree
typedef struct {
	int channel;
	int sx;
	int sy;
	int dx;
	int dy;
	float bias;
} ccv_scd_tree_feature_t;

enum {
	CCV_SCD_STUMP_FEATURE = 0x01,
	CCV_SCD_TREE_FEATURE = 0x02,
};

typedef struct {
	int type;
	uint32_t pass;
	ccv_scd_stump_feature_t feature;
	ccv_scd_tree_feature_t node[3];
	float beta[6];
	float threshold;
} ccv_scd_decision_tree_t;

typedef struct {
	int count;
	ccv_margin_t margin;
	ccv_size_t size;
	ccv_scd_stump_classifier_t* classifiers;
	// the last stage classifier is a hybrid of scd feature with icf-like feature
	// this is trained as soft-cascade classifier, and select between a depth-2 decision tree
	// or the scd feature.
	struct {
		int count;
		ccv_scd_decision_tree_t* tree;
	} decision;
} ccv_scd_classifier_cascade_t;

typedef struct {
	int min_neighbors;
	int flags;
	int step_through;
	int interval;
	ccv_size_t size;
} ccv_scd_param_t;

typedef struct {
	int boosting;
	ccv_size_t size;
	struct {
		ccv_size_t base;
		int range_through;
		int step_through;
	} feature;
	struct {
		float hit_rate;
		float false_positive_rate;
		float accu_false_positive_rate;
		float auc_crit;
		int maximum_feature;
		int prune_stage;
		int prune_feature;
	} stop_criteria;
	double weight_trimming;
	double C;
	int grayscale;
} ccv_scd_train_param_t;

extern const ccv_scd_param_t ccv_scd_default_params;

CCV_WARN_UNUSED(ccv_scd_classifier_cascade_t*) ccv_scd_classifier_cascade_new(ccv_array_t* posfiles, ccv_array_t* hard_mine, int negative_count, const char* filename, ccv_scd_train_param_t params);
void ccv_scd_classifier_cascade_write(ccv_scd_classifier_cascade_t* cascade, const char* filename);
CCV_WARN_UNUSED(ccv_scd_classifier_cascade_t*) ccv_scd_classifier_cascade_read(const char* filename);
void ccv_scd_classifier_cascade_free(ccv_scd_classifier_cascade_t* cascade);

void ccv_scd(ccv_dense_matrix_t* a, ccv_dense_matrix_t** b, int type);
CCV_WARN_UNUSED(ccv_array_t*) ccv_scd_detect_objects(ccv_dense_matrix_t* a, ccv_scd_classifier_cascade_t** cascades, int count, ccv_scd_param_t params);

/* categorization types and methods for training */

enum {
	CCV_CATEGORIZED_DENSE_MATRIX = 0x01,
	CCV_CATEGORIZED_FILE = 0x02,
};

typedef struct {
	int c; // class / category label
	int type;
	union {
		ccv_dense_matrix_t* matrix;
		ccv_file_info_t file;
	};
} ccv_categorized_t;

inline static ccv_categorized_t ccv_categorized(int c, ccv_dense_matrix_t* matrix, ccv_file_info_t* file)
{
	assert((matrix && !file) || (!matrix && file));
	ccv_categorized_t categorized;
	categorized.c = c;
	if (matrix)
		categorized.type = CCV_CATEGORIZED_DENSE_MATRIX, categorized.matrix = matrix;
	else
		categorized.type = CCV_CATEGORIZED_FILE, categorized.file = *file;
	return categorized;
}

/**
 * @defgroup ccv_convnet deep convolutional networks
 * This is a implementation of deep convolutional networks mainly for image recognition and object detection.
 * @{
 */

enum {
	CCV_CONVNET_CONVOLUTIONAL = 0x01,
	CCV_CONVNET_FULL_CONNECT = 0x02,
	CCV_CONVNET_MAX_POOL = 0x03,
	CCV_CONVNET_AVERAGE_POOL = 0x04,
	CCV_CONVNET_LOCAL_RESPONSE_NORM = 0x05,
};

typedef union {
	struct {
		int count; /** The number of filters for convolutional layer. */
		int strides; /** The strides for convolutional filter. */
		int border; /** The padding border size for the input matrix. */
		int rows; /** The number of rows for convolutional filter. */
		int cols; /** The number of columns for convolutional filter. */
		int channels; /** The number of channels for convolutional filter. */
		int partition; /** The number of partitions for convolutional filter. */
	} convolutional;
	struct {
		int strides; /** The strides for pooling layer. */
		int size; /** The window size for pooling layer. */
		int border; /** The padding border size for the input matrix. */
	} pool;
	struct {
		int size; /** The size of local response normalization layer. */
		float kappa; /** As of b[i] = a[i] / (rnorm.kappa + rnorm.alpha * sum(a, i - rnorm.size / 2, i + rnorm.size / 2)) ^ rnorm.beta */
		float alpha; /** See **rnorm.kappa**. */
		float beta; /** See **rnorm.kappa**. */
	} rnorm;
	struct {
		int relu; /** 0 - ReLU, 1 - no ReLU */
		int count; /** The number of output nodes for full connect layer. */
	} full_connect;
} ccv_convnet_type_t;

typedef struct {
	struct {
		int rows; /** The number of rows of the input matrix. */
		int cols; /** The number of columns of the input matrix. */
		int channels; /** The number of channels of the input matrix. */
		int partition; /** The number of partitions of the input matrix, it must be dividable by the number of channels (it is partitioned by channels). */
	} matrix;
	struct {
		int count; /** The number of nodes. You should either use **node** or **matrix** to specify the input structure. */
	} node;
} ccv_convnet_input_t;

typedef struct {
	int type; /** One of following value to specify the network layer type, **CCV_CONVNET_CONVOLUTIONAL**, **CCV_CONVNET_FULL_CONNECT**, **CCV_CONVNET_MAX_POOL**, **CCV_CONVNET_AVERAGE_POOL**, **CCV_CONVNET_LOCAL_RESPONSE_NORM**. */
	float bias; /** The initialization value for bias if applicable (for convolutional layer and full connect layer). */
	float glorot; /** The truncated uniform distribution coefficients for weights if applicable (for convolutional layer and full connect layer, glorot / sqrt(in + out)). */
	ccv_convnet_input_t input; /** A **ccv_convnet_input_t** specifies the input structure. */
	ccv_convnet_type_t output; /** A **ccv_convnet_type_t** specifies the output parameters and structure. */
} ccv_convnet_layer_param_t;

typedef struct {
	int type;
	float* w; // weight
	float* bias; // bias
	size_t wnum; // the number of weights
	ccv_convnet_input_t input; // the input requirement
	ccv_convnet_type_t net; // network configuration
	void* reserved;
} ccv_convnet_layer_t;

typedef struct {
	int use_cwc_accel; // use "ccv with cuda" acceleration
	// this is redundant, but good to enforcing what the input should look like
	ccv_size_t input;
	int rows;
	int cols;
	int channels;
	// count and layer of the convnet
	int count;
	ccv_dense_matrix_t* mean_activity; // mean activity to subtract from
	ccv_convnet_layer_t* layers; // the layer configuration
	// these can be reused and we don't need to reallocate memory
	ccv_dense_matrix_t** denoms; // denominators
	ccv_dense_matrix_t** acts; // hidden layers and output layers
	void* reserved;
} ccv_convnet_t;

typedef struct {
	float decay; /** See **learn_rate**. */
	float learn_rate; /** New velocity = **momentum** * old velocity - **decay** * **learn_rate** * old value + **learn_rate** * delta, new value = old value + new velocity */
	float momentum; /** See **learn_rate**. */
} ccv_convnet_layer_sgd_param_t;

typedef struct {
	// the dropout rate, I find that dor is better looking than dropout_rate,
	// and drop out is happened on the input neuron (so that when the network
	// is used in real-world, I simply need to multiply its weights to 1 - dor
	// to get the real one)
	float dor; /** The dropout rate for this layer, it is only applicable for full connect layer. */
	ccv_convnet_layer_sgd_param_t w; /** A **ccv_convnet_layer_sgd_param_t** specifies the stochastic gradient descent update rule for weight, it is only applicable for full connect layer and convolutional layer. */
	ccv_convnet_layer_sgd_param_t bias; /** A **ccv_convnet_layer_sgd_param_t** specifies the stochastic gradient descent update rule for bias, it is only applicable for full connect layer and convolutional layer weight. */
} ccv_convnet_layer_train_param_t;

typedef struct {
	int max_epoch; /** The number of epoch (an epoch sweeps through all the examples) to go through before end the training. */
	int mini_batch; /** The number of examples for a batch in stochastic gradient descent. */
	int iterations; /** The number of iterations (an iteration is for one batch) before save the progress. */
	int sgd_frequency; /** After how many batches when we do a SGD update. */
	int symmetric; /** Whether to exploit the symmetric property of the provided examples. */
	int device_count; /** Use how many GPU devices, this is capped by available CUDA devices on your system. For now, ccv's implementation only support up to 4 GPUs */
	int peer_access; /** Enable peer access for cross device communications or not, this will enable faster multiple device training. */
	float image_manipulation; /** The value for image brightness / contrast / saturation manipulations. */
	float color_gain; /** The color variance for data augmentation (0 means no such augmentation). */
	struct {
		int min_dim; /** The minimum dimensions for random resize of training images. */
		int max_dim; /** The maximum dimensions for random resize of training images. */
	} input;
	ccv_convnet_layer_train_param_t* layer_params; /** An C-array of **ccv_convnet_layer_train_param_t** training parameters for each layer. */
} ccv_convnet_train_param_t;

typedef struct {
	int half_precision; /** Use half precision float point to represent network parameters. */
} ccv_convnet_write_param_t;

/**
 * Create a new (deep) convolutional network with specified parameters. ccv only supports convolutional layer (shared weights), max pooling layer, average pooling layer, full connect layer and local response normalization layer.
 * @param use_cwc_accel Whether use CUDA-enabled GPU to accelerate various computations for convolutional network.
 * @param input Ihe input size of the image, it is not necessarily the input size of the first convolutional layer.
 * @param params[] The C-array of **ccv_convnet_layer_param_t** that specifies the parameters for each layer.
 * @param count The size of params[] C-array.
 * @return A new deep convolutional network structs
 */
CCV_WARN_UNUSED(ccv_convnet_t*) ccv_convnet_new(int use_cwc_accel, ccv_size_t input, ccv_convnet_layer_param_t params[], int count);
/**
 * Verify the specified parameters make sense as a deep convolutional network.
 * @param convnet A deep convolutional network to verify.
 * @param output The output number of nodes (for the last full connect layer).
 * @return 0 if the given deep convolutional network making sense.
 */
int ccv_convnet_verify(ccv_convnet_t* convnet, int output);
/**
 * Start to train a deep convolutional network with given parameters and data.
 * @param convnet A deep convolutional network that is initialized.
 * @param categorizeds An array of images with its category information for training.
 * @param tests An array of images with its category information for validating.
 * @param filename The working file to save progress and the trained convolutional network.
 * @param params The training parameters.
 */
void ccv_convnet_supervised_train(ccv_convnet_t* convnet, ccv_array_t* categorizeds, ccv_array_t* tests, const char* filename, ccv_convnet_train_param_t params);
/**
 * Use a convolutional network to encode an image into a compact representation.
 * @param convnet**: The given convolutional network.
 * @param a A C-array of input images.
 * @param b A C-array of output matrix of compact representation.
 * @param batch The number of input images.
 */
void ccv_convnet_encode(ccv_convnet_t* convnet, ccv_dense_matrix_t** a, ccv_dense_matrix_t** b, int batch);
void ccv_convnet_input_formation(ccv_size_t input, ccv_dense_matrix_t* a, ccv_dense_matrix_t** b);
/**
 * Use a convolutional network to classify an image into categories.
 * @param convnet**: The given convolutional network.
 * @param a A C-array of input images.
 * @param symmetric Whether the input is symmetric.
 * @param ranks A C-array of **ccv_array_t** contains top categories by the convolutional network.
 * @param tops The number of top categories return for each image.
 * @param batch The number of input images.
 */
void ccv_convnet_classify(ccv_convnet_t* convnet, ccv_dense_matrix_t** a, int symmetric, ccv_array_t** ranks, int tops, int batch);
/**
 * Read a convolutional network that persisted on the disk.
 * @param use_cwc_accel Use CUDA-enabled GPU acceleration.
 * @param filename The file on the disk.
 */
CCV_WARN_UNUSED(ccv_convnet_t*) ccv_convnet_read(int use_cwc_accel, const char* filename);
/**
 * Write a convolutional network to a disk.
 * @param convnet A given convolutional network.
 * @param filename The file on the disk.
 * @param params A **ccv_convnet_write_param_t** to specify the write parameters.
 */
void ccv_convnet_write(ccv_convnet_t* convnet, const char* filename, ccv_convnet_write_param_t params);
/**
 * Free up temporary resources of a given convolutional network.
 * @param convnet A convolutional network.
 */
void ccv_convnet_compact(ccv_convnet_t* convnet);
/**
 * Free up the memory of a given convolutional network.
 * @param convnet A convolutional network.
 */
void ccv_convnet_free(ccv_convnet_t* convnet);
/** @} */

/* add for command-line outputs, b/c ccv's training steps has a ton of outputs,
 * and in the future, it can be piped into callback functions for critical information
 * (such as the on-going missing rate, or iterations etc.) */

enum {
	CCV_CLI_ERROR = 1 << 2,
	CCV_CLI_INFO = 1 << 1,
	CCV_CLI_VERBOSE = 1,
	CCV_CLI_NONE = 0,
};


int ccv_cli_output_level_and_above(int level);
void ccv_set_cli_output_levels(int level);
int ccv_get_cli_output_levels(void);

#define CCV_CLI_OUTPUT_LEVEL_IS(a) (a & ccv_get_cli_output_levels())

#endif
