// A correct ObjCellList should list on-cells in an object, row by row, and from left to right within each row, without duplications
// There are pros and cons of keeping cells sorted at all times, but the intended purpuse of enforcing it is to facilitate fast comparison och hashing of objects

typedef struct
{
	u8 x;
	u8 y;
} ObjCellList_Cell;

typedef struct
{
	Rect obj_rect;
	s32 cell_cnt;
	s32 max_cells;
	ObjCellList_Cell *cell;
} ObjCellList;

static __force_inline void ObjCellList_make_empty (ObjCellList *obj, ObjCellList_Cell *cell_array, s32 max_cells)
{
	if (!obj)
		return (void) ffsc (__func__);
	
	Rect_make (&obj->obj_rect, 0, 0, 0, 0);
	obj->cell_cnt = 0;
	obj->max_cells = 0;
	obj->cell = NULL;
	
	if (max_cells < 0 || (max_cells > 0 && !cell_array))
		return (void) ffsc (__func__);
	
	obj->max_cells = max_cells;
	obj->cell = cell_array;
}

static __not_inline void ObjCellList_make_zero_size (ObjCellList *obj)
{
	if (!obj)
		return (void) ffsc (__func__);
	
	return ObjCellList_make_empty (obj, NULL, 0);
}

static __force_inline void ObjCellList_clear (ObjCellList *obj)
{
	if (!obj || (!obj->cell && obj->max_cells != 0))
		return (void) ffsc (__func__);
	
	Rect_make (&obj->obj_rect, 0, 0, 0, 0);
	obj->cell_cnt = 0;
}

static __not_inline int ObjCellList_add_on_cell (ObjCellList *obj, s32 x, s32 y)
{
	if (!obj || (!obj->cell && obj->max_cells != 0))
		return ffsc (__func__);
	
	if (obj->cell_cnt == 0)
	{
		if (obj->max_cells <= 0)
			return FALSE;
		
		Rect_make (&obj->obj_rect, x, y, 1, 1);
		obj->cell_cnt = 1;
		obj->cell [0].x = 0;
		obj->cell [0].y = 0;
		
		return TRUE;
	}
	
	Rect new_rect;
	Rect_include (&obj->obj_rect, &new_rect, x, y);
	
	if (new_rect.width > 256 || new_rect.height > 256)
		return FALSE;
	
	s32 cell_ix;
	if (new_rect.left_x < obj->obj_rect.left_x || new_rect.top_y < obj->obj_rect.top_y)
		for (cell_ix = 0; cell_ix < obj->cell_cnt; cell_ix++)
		{
			obj->cell [cell_ix].x += (u8) (obj->obj_rect.left_x - new_rect.left_x);
			obj->cell [cell_ix].y += (u8) (obj->obj_rect.top_y - new_rect.top_y);
		}
	
	s32 rel_x = x - new_rect.left_x;
	s32 rel_y = y - new_rect.top_y;
	
	s32 insertion_ix = obj->cell_cnt;
	if (obj->cell [obj->cell_cnt - 1].y > rel_y || (obj->cell [obj->cell_cnt - 1].y == rel_y && obj->cell [obj->cell_cnt - 1].x >= rel_x))
	{
		for (cell_ix = 0; cell_ix < obj->cell_cnt; cell_ix++)
			if (obj->cell [cell_ix].y > rel_y || (obj->cell [cell_ix].y == rel_y && obj->cell [cell_ix].x >= rel_x))
				break;
		
		insertion_ix = cell_ix;
		if (insertion_ix < obj->cell_cnt)
			if (obj->cell [insertion_ix].y == rel_y && obj->cell [insertion_ix].x == rel_x)
				return TRUE;
		
		if (obj->cell_cnt >= obj->max_cells)
			return FALSE;
		
		for (cell_ix = obj->cell_cnt; cell_ix > insertion_ix; cell_ix--)
		{
			obj->cell [cell_ix].x = obj->cell [cell_ix - 1].x;
			obj->cell [cell_ix].y = obj->cell [cell_ix - 1].y;
		}
		
	}
	else
		if (obj->cell_cnt >= obj->max_cells)
			return FALSE;
	
	Rect_copy (&new_rect, &obj->obj_rect);
	obj->cell [insertion_ix].x = (u8) rel_x;
	obj->cell [insertion_ix].y = (u8) rel_y;
	obj->cell_cnt++;
	
	return TRUE;
}

static __not_inline int ObjCellList_parse_rle (const char *spec, ObjCellList *obj)
{
	if (!spec || !obj || (!obj->cell && obj->max_cells != 0))
		return ffsc (__func__);
	
	s32 cell_ix = 0;
	s32 y = 0;
	s32 x = 0;
	
	s32 pop_x_on = 256;
	s32 pop_x_off = 0;
	s32 pop_y_on = 256;
	s32 pop_y_off = 0;
	
	while (TRUE)
	{
		char c = *spec++;
		
		s32 reps = 1;
		
		if (c >= '0' && c <= '9')
		{
			spec--;
			
			u64 number;
			int success = parse_u64 (&spec, &number);
			
			if (!success || number == 0 || number > 256)
			{
				ObjCellList_clear (obj);
				return FALSE;
			}
			
			reps = (s32) number;
			c = *spec++;
		}
		
		s32 rep_ix;
		if (c == 'o' || c == 'A' || c == 'C' || c == 'E' || c == 'O' || c == '*' || c == '@')
			for (rep_ix = 0; rep_ix < reps; rep_ix++)
			{
				if (cell_ix >= obj->max_cells)
				{
					ObjCellList_clear (obj);
					return FALSE;
				}
				
				if (x >= 256 || y >= 256)
				{
					ObjCellList_clear (obj);
					return FALSE;
				}
				
				obj->cell [cell_ix].x = (u8) x;
				obj->cell [cell_ix].y = (u8) y;
				cell_ix++;
				
				if (pop_x_on > x)
					pop_x_on = x;
				if (pop_x_off < x + 1)
					pop_x_off = x + 1;
				if (pop_y_on > y)
					pop_y_on = y;
				if (pop_y_off < y + 1)
					pop_y_off = y + 1;
				
				x++;
			}
		else if (c == '$')
		{
			y += reps;
			x = 0;
		}
		else if (c == '!' || c == '\0')
			break;
		else if (c == '\n' || c == '\r')
			continue;
		else
			x += reps;
	}
	
	if (cell_ix == 0)
		ObjCellList_clear (obj);
	else
	{
		obj->cell_cnt = cell_ix;
		for (cell_ix = 0; cell_ix < obj->cell_cnt; cell_ix++)
		{
			obj->cell [cell_ix].x = (u8) (((s32) obj->cell [cell_ix].x) - pop_x_on);
			obj->cell [cell_ix].y = (u8) (((s32) obj->cell [cell_ix].y) - pop_y_on);
		}
		
		Rect_make (&obj->obj_rect, 0, 0, pop_x_off - pop_x_on, pop_y_off - pop_y_on);
	}
	
	return TRUE;
}

static __force_inline int ObjCellList_copy (const ObjCellList *src_obj, ObjCellList *dst_obj)
{
	if (!src_obj || (!src_obj->cell && src_obj->max_cells != 0) || !dst_obj || (!dst_obj->cell && dst_obj->max_cells != 0))
		return ffsc (__func__);
	
	if (dst_obj->max_cells < src_obj->cell_cnt)
	{
		ObjCellList_clear (dst_obj);
		return FALSE;
	}
	
	s32 cell_ix;
	for (cell_ix = 0; cell_ix < src_obj->cell_cnt; cell_ix++)
	{
		dst_obj->cell [cell_ix].x = src_obj->cell [cell_ix].x;
		dst_obj->cell [cell_ix].y = src_obj->cell [cell_ix].y;
	}
	
	Rect_copy (&src_obj->obj_rect, &dst_obj->obj_rect);
	dst_obj->cell_cnt = src_obj->cell_cnt;
	
	return TRUE;
}

static __force_inline void ObjCellList_set_top_left (ObjCellList *obj, s32 left_x, s32 top_y)
{
	if (!obj || (!obj->cell && obj->max_cells != 0))
		return (void) ffsc (__func__);
	
	obj->obj_rect.left_x = left_x;
	obj->obj_rect.top_y = top_y;
}

static __force_inline int ObjCellList_sort_are_sorted (ObjCellList_Cell heap [], s32 a_ix, s32 b_ix)
{
	if (heap [a_ix].y < heap [b_ix].y)
		return TRUE;
	if (heap [a_ix].y > heap [b_ix].y)
		return FALSE;
	
	return (heap [a_ix].x < heap [b_ix].x);
}

static __force_inline void ObjCellList_sort_swap (ObjCellList_Cell heap [], s32 a_ix, s32 b_ix)
{
	s32 temp_x = heap [a_ix].x;
	s32 temp_y = heap [a_ix].y;
	
	heap [a_ix].x = heap [b_ix].x;
	heap [a_ix].y = heap [b_ix].y;
	
	heap [b_ix].x = temp_x;
	heap [b_ix].y = temp_y;
}

static __force_inline void ObjCellList_sort_sift_down (ObjCellList_Cell heap [], s32 heap_size, s32 parent_ix)
{
	s32 ix_of_highest = parent_ix;
	while (TRUE)
	{
		s32 left_child_ix = (2 * parent_ix) + 1;
		s32 right_child_ix = (2 * parent_ix) + 2;
		
		if (left_child_ix < heap_size)
			if (!ObjCellList_sort_are_sorted (heap, left_child_ix, ix_of_highest))
				ix_of_highest = left_child_ix;
		
		if (right_child_ix < heap_size)
			if (!ObjCellList_sort_are_sorted (heap, right_child_ix, ix_of_highest))
				ix_of_highest = right_child_ix;
		
		if (ix_of_highest == parent_ix)
			break;
		
		ObjCellList_sort_swap (heap, parent_ix, ix_of_highest);
		parent_ix = ix_of_highest;
	}
}

static __force_inline void ObjCellList_sort_heapify (ObjCellList_Cell cell [], s32 cell_cnt)
{
	s32 parent_ix = (cell_cnt / 2) - 1;
	while (parent_ix >= 0)
	{
		ObjCellList_sort_sift_down (cell, cell_cnt, parent_ix);
		parent_ix--;
	}
}

static __not_inline void ObjCellList_sort (ObjCellList *obj)
{
	if (!obj || (!obj->cell && obj->max_cells != 0))
		return (void) ffsc (__func__);
	
	ObjCellList_sort_heapify (obj->cell, obj->cell_cnt);
	
	s32 heap_size = obj->cell_cnt;
	while (heap_size > 1)
	{
		ObjCellList_sort_swap (obj->cell, 0, heap_size - 1);
		ObjCellList_sort_sift_down (obj->cell, heap_size - 1, 0);
		heap_size--;
	}
}

static __not_inline void ObjCellList_flip_horizontally (ObjCellList *obj)
{
	if (!obj || (!obj->cell && obj->max_cells != 0))
		return (void) ffsc (__func__);
	
	s32 cell_ix;
	for (cell_ix = 0; cell_ix < obj->cell_cnt; cell_ix++)
		obj->cell [cell_ix].x = (u8) ((obj->obj_rect.width - 1) - (s32) obj->cell [cell_ix].x);
	
	ObjCellList_sort (obj);
}

static __not_inline void ObjCellList_flip_vertically (ObjCellList *obj)
{
	if (!obj || (!obj->cell && obj->max_cells != 0))
		return (void) ffsc (__func__);
	
	s32 cell_ix;
	for (cell_ix = 0; cell_ix < obj->cell_cnt; cell_ix++)
		obj->cell [cell_ix].y = (u8) ((obj->obj_rect.height - 1) - (s32) obj->cell [cell_ix].y);
	
	ObjCellList_sort (obj);
}

static __not_inline void ObjCellList_flip_diagonally (ObjCellList *obj)
{
	if (!obj || (!obj->cell && obj->max_cells != 0))
		return (void) ffsc (__func__);
	
	s32 cell_ix;
	for (cell_ix = 0; cell_ix < obj->cell_cnt; cell_ix++)
	{
		u8 prev_x = obj->cell [cell_ix].x;
		obj->cell [cell_ix].x = obj->cell [cell_ix].y;
		obj->cell [cell_ix].y = prev_x;
	}
	
	s32 prev_width = obj->obj_rect.width;
	obj->obj_rect.width = obj->obj_rect.height;
	obj->obj_rect.height = prev_width;
	
	ObjCellList_sort (obj);
}

static __not_inline int ObjCellList_evolve_slow (const ObjCellList *in_obj, ObjCellList *out_obj)
{
	if (!in_obj || (!in_obj->cell && in_obj->max_cells != 0) || in_obj->obj_rect.width > 254 ||in_obj->obj_rect.height > 254 || !out_obj || (!out_obj->cell && out_obj->max_cells != 0))
		return ffsc (__func__);
	
	s32 out_cell_ix = 0;
	s32 out_pop_x_on = in_obj->obj_rect.height + 1;
	s32 out_pop_x_off = -1;
	
	s32 mid_y;
	s32 mid_x;
	for (mid_y = -1; mid_y < in_obj->obj_rect.height + 1; mid_y++)
		for (mid_x = -1; mid_x < in_obj->obj_rect.width + 1; mid_x++)
		{
			int mid_cnt = 0;
			int neighbour_cnt = 0;
			
			s32 in_cell_ix;
			for (in_cell_ix = 0; in_cell_ix < in_obj->cell_cnt; in_cell_ix++)
			{
				s32 cell_x = (s32) in_obj->cell [in_cell_ix].x;
				s32 cell_y = (s32) in_obj->cell [in_cell_ix].y;
				
				if (cell_x == mid_x && cell_y == mid_y)
					mid_cnt++;
				else if (cell_x >= mid_x - 1 && cell_x <= mid_x + 1 && cell_y >= mid_y - 1 && cell_y <= mid_y + 1)
					neighbour_cnt++;
			}
			
			if (neighbour_cnt == 3 || (mid_cnt == 1 && neighbour_cnt == 2))
			{
				if (out_cell_ix >= out_obj->max_cells)
				{
					ObjCellList_clear (out_obj);
					return FALSE;
				}
				
				if (out_pop_x_on > mid_x)
					out_pop_x_on = mid_x;
				if (out_pop_x_off < mid_x + 1)
					out_pop_x_off = mid_x + 1;
				
				out_obj->cell [out_cell_ix].x = (u8) (mid_x + 1);
				out_obj->cell [out_cell_ix].y = (u8) (mid_y + 1);
				out_cell_ix++;
			}
		}
	
	out_obj->cell_cnt = out_cell_ix;
	if (out_obj->cell_cnt == 0)
	{
		ObjCellList_clear (out_obj);
		return TRUE;
	}
	
	s32 out_pop_y_on = ((s32) out_obj->cell [0].y) - 1;
	s32 out_pop_y_off = (((s32) out_obj->cell [out_obj->cell_cnt - 1].y) + 1) - 1;
	
	for (out_cell_ix = 0; out_cell_ix < out_obj->cell_cnt; out_cell_ix++)
	{
		out_obj->cell [out_cell_ix].x = (u8) (((s32) out_obj->cell [out_cell_ix].x) - (out_pop_x_on + 1));
		out_obj->cell [out_cell_ix].y = (u8) (((s32) out_obj->cell [out_cell_ix].y) - (out_pop_y_on + 1));
	}
	
	Rect_make (&out_obj->obj_rect, in_obj->obj_rect.left_x + out_pop_x_on, in_obj->obj_rect.top_y + out_pop_y_on, out_pop_x_off - out_pop_x_on, out_pop_y_off - out_pop_y_on);
	return TRUE;
}
