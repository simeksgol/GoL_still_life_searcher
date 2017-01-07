// Note that we're assuming two's complement representation in some places

#define FALSE 0
#define TRUE 1

#define s8 int8_t
#define u8 uint8_t
#define s16 int16_t
#define u16 uint16_t
#define s32 int32_t
#define u32 uint32_t
#define s64 int64_t
#define u64 uint64_t

#define s32_MIN INT32_MIN
#define s32_MAX INT32_MAX
#define u32_MAX UINT32_MAX
#define u64_MAX UINT64_MAX

#ifdef __INTEL_COMPILER
	#define __not_inline __declspec(noinline)
	#define __may_inline
	#define __force_inline inline __forceinline
#else
	#ifdef __GNUC__
		#define __not_inline __attribute__((noinline))
		#define __may_inline
		#define __force_inline inline __attribute__((always_inline))
	#else
		#define __not_inline
		#define __may_inline
		#define __force_inline inline
	#endif
#endif

#ifdef __HAS_AVX_512F
	#ifndef __ALLOW_AVX_512F
		#define __ALLOW_AVX_512F
	#endif
#endif

#define MAX_SUPPORTED_VECTOR_BYTE_SIZE 64

// Vectorization is possible even when compiling for 32-bit x86 on Pentium 4, so hardly any reason to have a symbol for no vectorization at all
#ifdef __HAS_AVX_512F
	#define PREFERRED_VECTOR_BYTE_SIZE 64
#else
	#ifdef __NO_AVX2
		#define PREFERRED_VECTOR_BYTE_SIZE 16
	#else
		#define PREFERRED_VECTOR_BYTE_SIZE 32
	#endif
#endif

static __not_inline int verify_cpu_type_step_up (const char *feature)
{
	fprintf (stderr, "Note: This executable was compiled for CPUs without support for %s,\n", feature);
	fprintf (stderr, "      but this CPU does support %s. The program will run faster\n", feature);
	fprintf (stderr, "      if the %s executable is used instead.\n", feature);
	return TRUE;
}

static __not_inline int verify_cpu_type_step_down (const char *feature)
{
	fprintf (stderr, "Error: This executable was compiled for CPUs with support for %s,\n", feature);
	fprintf (stderr, "       but this CPU doesn't support that. Please use the executable\n");
	fprintf (stderr, "       that doesn't make use of %s instead.\n", feature);
	return FALSE;
}

static __not_inline int verify_cpu_type_unsupported (const char *feature)
{
	fprintf (stderr, "Error: This CPU doesn't support %s, but all executable versions of this\n", feature);
	fprintf (stderr, "       program require that.\n");
	return FALSE;
}

static __not_inline int verify_cpu_type_unknown (const char *feature)
{
	fprintf (stderr, "Warning: This executable was compiled for CPUs with support for %s,\n", feature);
	fprintf (stderr, "         but it is not checked if this CPU supports that. If the program\n");
	fprintf (stderr, "         crashes, please use the executable that doesn't make use of\n");
	fprintf (stderr, "         %s instead.\n", feature);
	return TRUE;
}

static __not_inline int verify_cpu_type (void)
{
	#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
		#ifdef __HAS_AVX_512F
			if (!__builtin_cpu_supports ("avx512f"))
				return verify_cpu_type_step_down ("AVX-512F");
		#else
			#ifdef __NO_AVX2
				#ifdef __REQUIRE_SSSE3
					if (!__builtin_cpu_supports ("ssse3"))
						return verify_cpu_type_unsupported ("SSSE3");
				#endif
				if (__builtin_cpu_supports ("avx2"))
					return verify_cpu_type_step_up ("AVX2");
			#else
				if (!__builtin_cpu_supports ("avx2"))
					return verify_cpu_type_step_down ("AVX2");
				#ifdef __ALLOW_AVX_512F
					if (__builtin_cpu_supports ("avx512f"))
						return verify_cpu_type_step_up ("AVX-512F");
				#endif
			#endif
		#endif
	#else
		#ifdef __HAS_AVX_512F
			return verify_cpu_type_unknown ("AVX-512F");
		#else
			#ifndef __NO_AVX2
				return verify_cpu_type_unknown ("AVX2");
			#endif
		#endif
	#endif
	
	return TRUE;
}

static __not_inline int ffsc (const char *fname)
{
	fprintf (stderr, "%s failed in function entry sanity check\n", (fname ? fname : "<unknown function namn>"));
	return FALSE;
}

static __not_inline void *ffsc_p (const char *fname)
{
	fprintf (stderr, "%s failed in function entry sanity check\n", (fname ? fname : "<unknown function namn>"));
	return NULL;
}

static __force_inline s32 abs_s32 (s32 arg)
{
	if (arg >= 0)
		return arg;
	else
		return -arg;
}

static __force_inline s32 lower_of_s32 (s32 arg1, s32 arg2)
{
	if (arg1 <= arg2)
		return arg1;
	else
		return arg2;
}

static __force_inline s32 higher_of_s32 (s32 arg1, s32 arg2)
{
	if (arg1 >= arg2)
		return arg1;
	else
		return arg2;
}

static __force_inline s32 lower_of_u32 (u32 arg1, u32 arg2)
{
	if (arg1 <= arg2)
		return arg1;
	else
		return arg2;
}

static __force_inline u32 higher_of_u32 (u32 arg1, u32 arg2)
{
	if (arg1 >= arg2)
		return arg1;
	else
		return arg2;
}

static __force_inline u64 lower_of_u64 (u64 arg1, u64 arg2)
{
	if (arg1 <= arg2)
		return arg1;
	else
		return arg2;
}

static __force_inline u64 higher_of_u64 (u64 arg1, u64 arg2)
{
	if (arg1 >= arg2)
		return arg1;
	else
		return arg2;
}

static __force_inline void swap_pointers (void **arg1, void **arg2)
{
	void *temp = *arg1;
	*arg1 = *arg2;
	*arg2 = temp;
}

static __force_inline int bit_count_u64 (u64 arg)
{
	arg = (arg & 0x5555555555555555u) + ((arg >> 1) & 0x5555555555555555u);
	arg = (arg & 0x3333333333333333u) + ((arg >> 2) & 0x3333333333333333u);
	arg = (arg & 0x0f0f0f0f0f0f0f0fu) + ((arg >> 4) & 0x0f0f0f0f0f0f0f0fu);
	return (arg * 0x0101010101010101u) >> 56;
}

static const s8 significant_bit_u64_table [116] = 
	{64, -1, 36, -1, -1, -1, 55, -1, -1, -1, 18, -1,  0, -1,  4, 58, -1, 44, 37, -1, -1, -1, -1, -1, 53, -1, 56, -1, -1, -1, 25, -1,
	 27, 19, -1,  8, -1, -1,  1, 50, -1,  5, 29, 59, 32, 21, -1, -1, 45, 38, 10, 62, -1, -1, -1, -1, -1, -1, 35, -1, 54, -1, 17, -1,
	  3, 57, 43, -1, -1, 52, -1, -1, 24, -1, 26,  7, -1, 49, 28, 31, 20, -1,  9, 61, -1, -1, -1, 34, 16,  2, 42, -1, 51, -1, 23,  6,
	 48, 30, -1, 60, -1, 33, 15, 41, 22, 47, -1, -1, 14, 40, 46, 13, 39, 12, 11, 63};

static __force_inline int least_significant_bit_u64 (u64 arg)
{
	if (arg != 0)
		arg = (arg ^ (arg - 1));
	
	return (int) significant_bit_u64_table [(arg * 0x19afe5d5b8f9ed27u) >> 57];
}

static __force_inline int most_significant_bit_u64 (u64 arg)
{
	arg |= (arg >> 1);
	arg |= (arg >> 2);
	arg |= (arg >> 4);
	arg |= (arg >> 8);
	arg |= (arg >> 16);
	arg |= (arg >> 32);
	
	return (int) significant_bit_u64_table [(arg * 0x19afe5d5b8f9ed27u) >> 57];
}

// This strange order of the iterative bit reversal is enough to fool GCC into not using its __builtin_bswap64 for the three high-order swaps (bswap64 swaps bytes)
// If GCC finds a match for using __builtin_bswap64, that currently prevents vectorization
static __force_inline u64 bit_reverse_u64 (u64 arg)
{
	arg = ((arg & 0xaaaaaaaaaaaaaaaau) >>  1) | ((arg & 0x5555555555555555u) <<  1);
	arg = ((arg & 0xff00ff00ff00ff00u) >>  8) | ((arg & 0x00ff00ff00ff00ffu) <<  8);
	arg = ((arg & 0xccccccccccccccccu) >>  2) | ((arg & 0x3333333333333333u) <<  2);
	arg = ((arg & 0xffff0000ffff0000u) >> 16) | ((arg & 0x0000ffff0000ffffu) << 16);
	arg = ((arg & 0xf0f0f0f0f0f0f0f0u) >>  4) | ((arg & 0x0f0f0f0f0f0f0f0fu) <<  4);
	
	return (arg << 32) | (arg >> 32);
}

static __not_inline u64 combinations_u64 (u64 n, u64 r)
{
	if (r > n)
		return 0;
	
	r = lower_of_u64 (r, n - r);
	
	// This would always overflow
	if (r >= 34)
		return 0;
	
	u64 comb = 1;
	u64 it_ix;
	for (it_ix = 0; it_ix < r; it_ix++)
	{
		u64 rem = comb % (it_ix + 1);
		comb = ((comb / (it_ix + 1)) * (n - it_ix)) + ((rem * (n - it_ix)) / (it_ix + 1));
	}	
	
	return comb;
}

static __force_inline u64 next_higher_with_same_bit_count_u64 (u64 arg)
{
	int ls_one = least_significant_bit_u64 (arg);
	if (ls_one >= 63)
		return 0;
	
	int ls_ones_in_a_row = 1 + least_significant_bit_u64 (~(arg >> (ls_one + 1)));
	int flip_bit = ls_one + ls_ones_in_a_row;
	if (flip_bit > 63)
		return 0;
	
	return (((arg >> flip_bit) + 1) << flip_bit) | ((((u64) 1) << (ls_ones_in_a_row - 1)) - 1);
}

static __not_inline int digits_in_u32 (u32 arg)
{
	int digits = 1;
	
	while (arg > 9)
	{
		arg /= 10;
		digits++;
	}
	
	return digits;
}

static __force_inline s32 round_double (double arg)
{
	if (arg < 0.0)
		return (s32) (arg - 0.5);
	else
		return (s32) (arg + 0.5);
}

// This is the public domain "xorshift128plus" PRNG by Sebastiano Vigna

static u64 random_u64_state_0 = 0x19803c70561f8414u;
static u64 random_u64_state_1 = 0xcaca61eeae213995u;

static __may_inline void random_u64_set_seed (u64 seed_1, u64 seed_2, int xor_with_current_time)
{
	u64 cur_time = 0;
	if (xor_with_current_time)
		cur_time = (u64) time (0);
	
	random_u64_state_0 = seed_1 ^ cur_time;
	random_u64_state_1 = seed_2 ^ cur_time;
}

static __force_inline u64 random_u64 (void)
{
	u64 x = random_u64_state_0;
	u64 y = random_u64_state_1;
	random_u64_state_0 = y;
	x ^= x << 23;
	random_u64_state_1 = x ^ y ^ (x >> 17) ^ (y >> 26);
	return random_u64_state_1 + y;
}

static __not_inline void print_hex_u64 (char *text, u64 arg)
{
	printf ("%s%08x%08x\n", (text ? text : ""), (u32) (arg >> 32), (u32) arg);
}

static __not_inline void print_bin_u64 (char *text, u64 arg)
{
	printf ("%s", (text ? text : ""));
	int bit;
	for (bit = 63; bit >= 0; bit--)
	{
		printf ("%c", ((arg & (((u64) 1) << bit)) != 0) ? '1' : '0');
		if (bit % 8 == 0)
			printf ("%s", (bit == 0 ? "\n" : "."));
	}
}

static __force_inline const void *align_down_const_pointer (const void *p, u64 alignment)
{
	return (const void *) (((uintptr_t) p) & (uintptr_t) ~(alignment - 1));
}

static __force_inline void *align_down_pointer (void *p, u64 alignment)
{
	return (void *) (((uintptr_t) p) & (uintptr_t) ~(alignment - 1));
}

// We cast alignment to an s32 so that the and operation is performed on signed numbers. This avoids the final implicit conversion of an unsigned number
// to a possibly negative signed number which is not well defined by the C standard. We assume two's complement representation here
static __force_inline s32 align_down_s32 (s32 arg, u32 alignment)
{
	return arg & ~(((s32) alignment) - 1);
}

static __force_inline s32 align_up_s32 (s32 arg, u32 alignment)
{
	return (arg + (((s32) alignment) - 1)) & ~(((s32) alignment) - 1);
}

// Conforms to the C standard even for negative numbers, for 1 <= shift <= 31
static __force_inline s32 arithmetic_shift_right_s32 (s32 arg, int shift)
{
	return ((s32) ((((u32) arg) + ((u32) 0x80000000u)) >> shift)) - ((s32) (((u32) 0x80000000u) >> shift));
}

static __force_inline u64 align_down_u64 (u64 arg, u64 alignment)
{
	return arg & ~(alignment - 1);
}

static __not_inline int allocate_aligned (u64 size, u64 alignment, u64 alignment_offset, int clear, void **allocated_buffer, void **aligned_buffer)
{
	if (aligned_buffer)
		*aligned_buffer = NULL;
	if (allocated_buffer)
		*allocated_buffer = NULL;
	
	if (size == 0 || bit_count_u64 (alignment) != 1 || alignment_offset >= alignment || !allocated_buffer || !aligned_buffer)
		return ffsc (__func__);
	
	void *buffer = malloc (size + alignment);
	if (!buffer)
		return FALSE;
	
	if (clear)
		memset (buffer, 0, size + alignment);
	
	*allocated_buffer = buffer;
	*aligned_buffer = (void *) (((((uintptr_t) buffer) + (uintptr_t) (alignment - (1 + alignment_offset))) & (uintptr_t) ~(alignment - 1)) + (uintptr_t) alignment_offset);
	
	return TRUE;
}

static __not_inline int parse_u64 (const char **s, u64 *num)
{
	if (num)
		*num = 0;
	
	if (!s || !*s || !num)
		return ffsc (__func__);
	
	int started = FALSE;
	int overflow = FALSE;
	u64 n = 0;
	
	while (TRUE)
	{
		char c = **s;
		
		if (c >= '0' && c <= '9')
		{
			(*s)++;
			started = TRUE;
			
			u64 digit = c - '0';
			if (n > u64_MAX / 10 || (n == u64_MAX / 10 && digit > u64_MAX % 10))
				overflow = TRUE;
			
			n = 10 * n + digit;
		}
		else
			break;
	}
	
	*num = n;
	return (started && !overflow);
}

static __not_inline int str_to_u32 (const char *s, u32 *num)
{
	if (num)
		 *num = 0;
	
	if (!s || !num)
		return ffsc (__func__);
	
	u64 n;
	if (!parse_u64 (&s, &n))
		return FALSE;
	
	if (*s != '\0' || n > u32_MAX)
		return FALSE;
	
	*num = n;
	return TRUE;
}

static __not_inline int str_to_u64 (const char *s, u64 *num)
{
	if (num)
		 *num = 0;
	
	if (!s || !num)
		return ffsc (__func__);
	
	u64 n;
	if (!parse_u64 (&s, &n))
		return FALSE;
	
	if (*s != '\0')
		return FALSE;
	
	*num = n;
	return TRUE;
}
