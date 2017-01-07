static __not_inline GoLGrid *GoLUtils_alloc_std_grid (s32 left_x, s32 top_y, s32 width, s32 height)
{
	GoLGrid *gg = malloc (sizeof (GoLGrid));
	if (!gg)
	{
		fprintf (stderr, "Out of memory allocating GoLGrid object");
		return NULL;
	}
	
	Rect grid_rect;
	Rect_make (&grid_rect, left_x, top_y, width, height);
	
	if (!GoLGrid_create (gg, &grid_rect))
	{
		free (gg);
		return NULL;
	}
	
	return gg;
}

static __not_inline void GoLUtils_free_std_grid (GoLGrid **gg)
{
	if (!gg || !*gg)
		return (void) ffsc (__func__);
	
	GoLGrid_free (*gg);
	free (*gg);
	
	*gg = NULL;
}

static __not_inline int GoLUtils_or_glider (GoLGrid *gg, const Glider *glider, int consider_grid_generation)
{
	if (!gg || !glider)
		return ffsc (__func__);
	
	Glider adj_glider;
	const Glider *glider_to_use = glider;
	
	if (consider_grid_generation)
	{
		adj_glider.dir = glider->dir;
		adj_glider.lane = glider->lane;
		adj_glider.timing = glider->timing - gg->generation;
		
		glider_to_use = &adj_glider;
	}
	
	ObjCellList ocl;
	Objects_make_glider_obj_cell_list (&ocl, glider_to_use);
	
	return GoLGrid_or_obj_cell_list (gg, &ocl, 0, 0);
}

static __not_inline s32 GoLUtils_get_safe_glider_progression (const GoLGrid *target_area, s32 glider_dir, u64 *projection, s32 projection_size)
{
	if (!target_area || GoLGrid_is_empty (target_area) || glider_dir < 0 || glider_dir >= 4 || !projection)
		return ffsc (__func__);
	
	if (glider_dir == 0 || glider_dir == 2)
		GoLGrid_make_rightup_projection_noinline (target_area, projection, projection_size);
	else
		GoLGrid_make_rightdown_projection_noinline (target_area, projection, projection_size);
	
	print_bin_u64 ("word 2: ", projection [2]);
	print_bin_u64 ("word 3: ", projection [3]);
	
	if (glider_dir == 0)
		return -9 - (2 * GoLGrid_get_rightdown_pop_off (target_area, projection, projection_size));
	else if (glider_dir == 1)
		return -11 + (2 * GoLGrid_get_rightup_pop_on (target_area, projection, projection_size));
	else if (glider_dir == 2)
		return -11 + (2 * GoLGrid_get_rightdown_pop_on (target_area, projection, projection_size));
	else
		return -9 - (2 * GoLGrid_get_rightup_pop_off (target_area, projection, projection_size));
}

// src_gg and dst_gg may mave different sizes. The virtual position of dst_gg is set to (0, 0) by this function. Returns FALSE if clipping occurred
static __not_inline int GoLUtils_copy_to_top_left (const GoLGrid *src_gg, GoLGrid *dst_gg)
{
	if (!src_gg || !dst_gg)
		return ffsc (__func__);
	
	Rect src_bb;
	GoLGrid_get_bounding_box (src_gg, &src_bb);
	
	GoLGrid_set_grid_coords (dst_gg, 0, 0);
	return GoLGrid_copy_unmatched_noinline (src_gg, dst_gg, -src_bb.left_x, -src_bb.top_y);
}

// Only temp_gg_1 and temp_gg_2 need to be square and have the same grid rects. Returns FALSE if clipping occurred
static __not_inline int GoLUtils_flip_diagonally_virtual (const GoLGrid *src_gg, GoLGrid *dst_gg, GoLGrid *temp_gg_1, GoLGrid *temp_gg_2)
{
	if (!src_gg || !dst_gg || !temp_gg_1 || temp_gg_1->grid_rect.height != temp_gg_1->grid_rect.width || !temp_gg_2 ||
			temp_gg_2->grid_rect.width != temp_gg_1->grid_rect.width || temp_gg_2->grid_rect.height != temp_gg_1->grid_rect.height)
		return ffsc (__func__);
	
	Rect src_grid_rect;
	GoLGrid_get_grid_rect (src_gg, &src_grid_rect);
	
	Rect src_bb;
	GoLGrid_get_bounding_box (src_gg, &src_bb);
	
	Rect temp_1_grid_rect;
	GoLGrid_get_grid_rect (temp_gg_1, &temp_1_grid_rect);
	
	int clipped = GoLGrid_copy_unmatched_noinline (src_gg, temp_gg_1, temp_1_grid_rect.left_x - src_bb.left_x, temp_1_grid_rect.top_y - src_bb.top_y);
	GoLGrid_flip_diagonally_noinline (temp_gg_1, temp_gg_2);
	clipped |= GoLGrid_copy_unmatched_noinline (temp_gg_2, dst_gg, src_bb.top_y - temp_1_grid_rect.left_x, src_bb.left_x - temp_1_grid_rect.top_y);
	
	return clipped;
}

static __force_inline void GoLUtils_try_canonical (const GoLGrid *src_gg, GoLGrid *dst_gg, u64 *lowest_hash, const RandomDataArray *rda)
{
	u64 hash = GoLGrid_get_hash_noinline (src_gg, rda);
	if (hash < *lowest_hash)
	{
		*lowest_hash = hash;
		GoLGrid_copy_noinline (src_gg, dst_gg);
	}
}

// dst_gg, temp_gg1_1 and temp_gg2 must be square and have the same size, but may be of a different size than src_gg. The virtual position of dst_gg is set to (0, 0) by this function
// Returns the hash value of the canonical pattern
static __not_inline u64 GoLUtils_make_canonical (const GoLGrid *src_gg, GoLGrid *dst_gg, const RandomDataArray *rda, GoLGrid *temp_gg_1, GoLGrid *temp_gg_2)
{
	if (!src_gg || !dst_gg || dst_gg->grid_rect.height != dst_gg->grid_rect.width || !rda ||
			!temp_gg_1 || temp_gg_1->grid_rect.height != temp_gg_1->grid_rect.width || temp_gg_1->grid_rect.height != dst_gg->grid_rect.height ||
			!temp_gg_2 || temp_gg_2->grid_rect.height != temp_gg_2->grid_rect.width || temp_gg_2->grid_rect.height != dst_gg->grid_rect.height)
		return ffsc (__func__);
	
	if (GoLGrid_is_empty (src_gg))
	{
		GoLUtils_copy_to_top_left (src_gg, dst_gg);
		return GoLGrid_get_hash_noinline (dst_gg, rda);
	}
	
	GoLUtils_copy_to_top_left (src_gg, temp_gg_1);
	
	Rect bb;
	GoLGrid_get_bounding_box (temp_gg_1, &bb);
	
	if (bb.height > bb.width)
	{
		GoLGrid_flip_diagonally_noinline (temp_gg_1, temp_gg_2);
		swap_pointers ((void **) &temp_gg_1, (void **) &temp_gg_2);
	}
	
	u64 lowest_hash = GoLGrid_get_hash_noinline (temp_gg_1, rda);
	GoLGrid_copy_noinline (temp_gg_1, dst_gg);
	
	if (bb.height == bb.width)
	{
		GoLGrid_flip_horizontally_noinline (temp_gg_1, temp_gg_2);
		GoLUtils_try_canonical (temp_gg_2, dst_gg, &lowest_hash, rda);
		GoLGrid_flip_vertically_noinline (temp_gg_2, temp_gg_1);
		GoLUtils_try_canonical (temp_gg_1, dst_gg, &lowest_hash, rda);
		GoLGrid_flip_horizontally_noinline (temp_gg_1, temp_gg_2);
		GoLUtils_try_canonical (temp_gg_2, dst_gg, &lowest_hash, rda);
		
		GoLGrid_flip_diagonally_noinline (temp_gg_2, temp_gg_1);
		GoLUtils_try_canonical (temp_gg_1, dst_gg, &lowest_hash, rda);
	}
	
	GoLGrid_flip_horizontally_noinline (temp_gg_1, temp_gg_2);
	GoLUtils_try_canonical (temp_gg_2, dst_gg, &lowest_hash, rda);
	GoLGrid_flip_vertically_noinline (temp_gg_2, temp_gg_1);
	GoLUtils_try_canonical (temp_gg_1, dst_gg, &lowest_hash, rda);
	GoLGrid_flip_horizontally_noinline (temp_gg_1, temp_gg_2);
	GoLUtils_try_canonical (temp_gg_2, dst_gg, &lowest_hash, rda);
	
	return lowest_hash;
}
