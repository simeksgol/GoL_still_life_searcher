// A datatype for a pre-generated array of random u64 data
// The typical way to make use of this is to verify just once that the size is enough with RandomDataArray_verify_size, then access the array directly with rda_object->random_data [index]

typedef struct
{
	u64 size;
	u64 *data_alloc;
	u64 *random_data;
} RandomDataArray;

static __may_inline void RandomDataArray_preinit (RandomDataArray *rda)
{
	if (!rda)
		return (void) ffsc (__func__);
	
	rda->size = 0;
	rda->data_alloc = NULL;
	rda->random_data = NULL;
}

static __not_inline void RandomDataArray_free (RandomDataArray *rda)
{
	if (!rda)
		return (void) ffsc (__func__);
	
	if (rda->data_alloc)
		free (rda->data_alloc);
	
	RandomDataArray_preinit (rda);
}

static __not_inline int RandomDataArray_create (RandomDataArray *rda, u64 size)
{
	if (!rda)
		return ffsc (__func__);
	
	RandomDataArray_preinit (rda);
	
	rda->size = size;
	
	if (!allocate_aligned (size * sizeof (u64), MAX_SUPPORTED_VECTOR_BYTE_SIZE, 0, FALSE, (void **) &rda->data_alloc, (void **) &rda->random_data))
	{
		fprintf (stderr, "Out of memory in %s\n", __func__);
		RandomDataArray_free (rda);
		return FALSE;
	}
	
	u64 data_ix;
	for (data_ix = 0; data_ix < size; data_ix++)
		rda->random_data [data_ix] = random_u64 ();
	
	return TRUE;
}

static __force_inline int RandomDataArray_verify_size (const RandomDataArray *rda, u64 needed_size)
{
	if (!rda || !rda->random_data)
		return ffsc (__func__);
	
	return rda->size >= needed_size;
}
