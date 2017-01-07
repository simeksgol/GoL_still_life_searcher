static __force_inline int GoLGrid_get_cell_64_wide_zero_org_unchecked (const GoLGrid *gg, s32 x, s32 y)
{
	return (gg->grid [y] >> (63 - x)) & 1;
}

static __force_inline void GoLGrid_set_cell_on_64_wide_zero_org_unchecked (GoLGrid *gg, s32 x, s32 y)
{
	gg->grid [y] |= (((u64) 1) << (63 - x));
	GoLGrid_int_adjust_pop_rect_new_on_cell (gg, x, y);
}

static __force_inline void GoLGrid_set_cell_on_64_wide_zero_org_unchanged_bb_unchecked (GoLGrid *gg, s32 x, s32 y)
{
	gg->grid [y] |= (((u64) 1) << (63 - x));
}

static __not_inline void GoLGrid_set_cell_off_64_wide_zero_org_unchecked (GoLGrid *gg, s32 x, s32 y)
{
	gg->grid [y] &= ~(((u64) 1) << (63 - x));
	GoLGrid_int_adjust_pop_rect_new_off_cell_64_wide (gg, x, y);
}

static __not_inline void GoLGrid_set_cell_off_64_wide_zero_org_unchanged_bb_unchecked (GoLGrid *gg, s32 x, s32 y)
{
	gg->grid [y] &= ~(((u64) 1) << (63 - x));
}
