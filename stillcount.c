#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>
#include <time.h>
#include <stdio.h>

#include "lib/lib.c"
#include "lib/rect.c"
#include "lib/celllist.c"
#include "lib/objects.c"
#include "lib/randomarray.c"
#include "lib/golgrid.c"
#include "lib/gridunsafe.c"
#include "lib/gridmisc.c"
#include "lib/golutils.c"

#ifdef _WIN32
	#define USE_PERF_TIMER
	#define USE_GRID_VISUALIZATION
#endif

#include "lib/perftimer.c"
#include "lib/visual.c"

#define GRID_HEIGHT 128
#define GRID_WIDTH 64
#define GRID_BORDER 4
#define SEED_ON_CELL_X GRID_BORDER
#define SEED_ON_CELL_Y (GRID_HEIGHT / 2)
#define MAX_ON_CELLS 64
#define MAX_BIT_CNT (MAX_ON_CELLS - 16)
#define OPEN_CELL_CNT (25 * MAX_ON_CELLS)
#define MAX_PARTITIONS 20
#define REMAINING_CELLS_THRESHOLD_FOR_UNCONNECTABLE_CHECK 5
#define FILE_BUFFER_SIZE (8192 * 1024)

#define TAG_SIZE 9
#define TAG_CNT_AT_TAG_SIZE_9 3006
#define SELECTED_SEARCH_SUBSETS 100
#define WANTED_SEARCH_SUBSETS 100
#define MAX_OPS_IN_SUBSET_LOW_ESTIMATE 12000000

typedef struct
{
	s32 x;
	s32 y;
	int state;
	int is_forced;
} TakenDefine;

typedef struct
{
	s8 defined_may_be_stabilized_table [9] [9] [2];
	s8 undefined_may_be_stabilized_table [9] [9];
	
	int write_files;
	
	s32 min_wanted_bit_cnt;
	s32 max_wanted_bit_cnt;
	
	s32 search_subset;
	s32 wanted_tag_on;
	s32 wanted_tag_off;
	
	GoLGrid *undef_cells;
	GoLGrid *on_cells;
	s32 on_cnt;
	
	u8 undef_shadow [GRID_WIDTH] [GRID_HEIGHT];
	u8 on_shadow [GRID_WIDTH] [GRID_HEIGHT];
	
	s32 taken_define_cnt;
	TakenDefine taken_define [OPEN_CELL_CNT];
	
	int new_on_cells_defined;
	int new_tag_cells_defined;
	
	s64 op_cnt;
	s32 cur_tag_ix;
	
	FILE *strict_file [MAX_BIT_CNT + 1];
	FILE *pseudo_file [MAX_BIT_CNT + 1];
	
	s64 not_stable_cnt;
	s64 not_canonical_cnt;
	s64 not_connected_cnt;
	s64 strict_sol_cnt [MAX_BIT_CNT + 1];
	s64 pseudo_sol_cnt [MAX_BIT_CNT + 1];
	
	GridVisualization *gv;
} SearchState;

#if SELECTED_SEARCH_SUBSETS == 16
	static s32 tag_size_9_in_subsets [SELECTED_SEARCH_SUBSETS + 1] = {0, 177, 364, 549, 716, 876, 1068, 1235, 1396, 1596, 1783, 2017, 2207, 2364, 2550, 2815, 3006};
#elif SELECTED_SEARCH_SUBSETS == 100
	static s32 tag_size_9_in_subsets [SELECTED_SEARCH_SUBSETS + 1] = {0, 29, 42, 56, 91, 130, 173, 207, 232, 266, 297, 320, 352, 376, 404, 431, 464, 489, 515, 555, 579, 601, 627, 669, 692, 716, 739, 754, 796, 820, 847, 875,
			897, 934, 962, 1004, 1032, 1058, 1083, 1100, 1130, 1163, 1178, 1216, 1240, 1266, 1289, 1326, 1354, 1371, 1397, 1417, 1450, 1476, 1512, 1551, 1589, 1620, 1648, 1673, 1699, 1734, 1773, 1802, 1846, 1884, 1916, 1961,
			2001, 2032, 2053, 2075, 2101, 2135, 2171, 2208, 2220, 2251, 2283, 2315, 2339, 2360, 2385, 2415, 2441, 2478, 2521, 2544, 2578, 2637, 2657, 2693, 2723, 2774, 2829, 2844, 2884, 2921, 2951, 2991, 3006};
#endif


static __not_inline GoLGrid *alloc_grid (void)
{
	return GoLUtils_alloc_std_grid (0, 0, GRID_WIDTH, GRID_HEIGHT);
}

static __not_inline void visualize_cur (SearchState *st)
{
	s32 y;
	s32 x;
	for (y = 0; y < GRID_HEIGHT; y++)
		for (x = 0; x < GRID_WIDTH; x++)
		{
			int undef = GoLGrid_get_cell (st->undef_cells, x, y);
			int on = GoLGrid_get_cell (st->on_cells, x, y);
			
			int state;
			if (on)
				state = 1;
			else if (undef)
				state = 5;
			else
				state = 0;
			
			GridVisualization_set_cell (st->gv, x, y, state);
		}
}

static __not_inline int get_connected_part (const GoLGrid *src, GoLGrid *dst)
{
	static int static_init = FALSE;
	static GoLGrid *all_bleed_3_or_more;
	static GoLGrid *cur_included;
	static GoLGrid *bleed_8;
	static GoLGrid *connecting_cells;
	static GoLGrid *new_included_mask;
	
	if (!static_init)
	{
		static_init = TRUE;
		all_bleed_3_or_more = alloc_grid ();
		cur_included = alloc_grid ();
		bleed_8 = alloc_grid ();
		connecting_cells = alloc_grid ();
		new_included_mask = alloc_grid ();
	}
	
	GoLGrid_bleed_3_or_more_neighbours_64_wide (src, all_bleed_3_or_more);
	
	GoLGrid_clear_64_wide (cur_included);
	GoLGrid_set_cell_on_64_wide_zero_org_unchecked (cur_included, SEED_ON_CELL_X, SEED_ON_CELL_Y);
	
	while (TRUE)
	{
		GoLGrid_bleed_8_64_wide (cur_included, bleed_8);
		GoLGrid_and_64_wide (bleed_8, all_bleed_3_or_more, connecting_cells);
		GoLGrid_bleed_8_64_wide (connecting_cells, new_included_mask);
		GoLGrid_and_64_wide (src, new_included_mask, dst);
		
		if (GoLGrid_is_equal_64_wide (dst, src))
			return TRUE;
		if (GoLGrid_is_equal_64_wide (dst, cur_included))
			return FALSE;
		
		GoLGrid_copy_64_wide (dst, cur_included);
	}
}

static __not_inline int is_connectable (SearchState *st)
{
	static int static_init = FALSE;
	static GoLGrid *first_connected;
	static GoLGrid *first_bleed_8;
	static GoLGrid *first_bleed_24;
	static GoLGrid *other_on_cells;
	static GoLGrid *undef_in_first_bleed_24;
	static GoLGrid *possibly_other_on;
	static GoLGrid *bleed_8_of_possibly_other_on;
	static GoLGrid *connection_cell_area;
	static GoLGrid *possibly_on_in_first_bleed_24;
	static GoLGrid *possible_connection_cells;
	
	if (!static_init)
	{
		static_init = TRUE;
		first_connected = alloc_grid ();
		first_bleed_8 = alloc_grid ();
		first_bleed_24 = alloc_grid ();
		other_on_cells = alloc_grid ();
		undef_in_first_bleed_24 = alloc_grid ();
		possibly_other_on = alloc_grid ();
		bleed_8_of_possibly_other_on = alloc_grid ();
		connection_cell_area = alloc_grid ();
		possibly_on_in_first_bleed_24 = alloc_grid ();
		possible_connection_cells = alloc_grid ();
	}
	
	get_connected_part (st->on_cells, first_connected);
	
	GoLGrid_bleed_8_64_wide (first_connected, first_bleed_8);
	if (!GoLGrid_are_disjoint_64_wide (first_bleed_8, st->undef_cells))
		return TRUE;
	
	GoLGrid_bleed_8_64_wide (first_bleed_8, first_bleed_24);
	GoLGrid_copy_64_wide (st->on_cells, other_on_cells);
	GoLGrid_subtract_64_wide (other_on_cells, first_connected);
	
	GoLGrid_and_64_wide (first_bleed_24, st->undef_cells, undef_in_first_bleed_24);
	GoLGrid_and_64_wide (first_bleed_24, other_on_cells, possibly_other_on);
	GoLGrid_or_64_wide (possibly_other_on, undef_in_first_bleed_24);
	
	GoLGrid_bleed_8_64_wide (possibly_other_on, bleed_8_of_possibly_other_on);
	GoLGrid_and_64_wide (bleed_8_of_possibly_other_on, first_bleed_8, connection_cell_area);
	
	GoLGrid_copy_64_wide (first_connected, possibly_on_in_first_bleed_24);
	GoLGrid_or_64_wide (possibly_on_in_first_bleed_24, possibly_other_on);
	
	GoLGrid_bleed_3_or_more_neighbours_64_wide (possibly_on_in_first_bleed_24, possible_connection_cells);
	return !(GoLGrid_are_disjoint_64_wide (connection_cell_area, possible_connection_cells));
}

static __force_inline void add_preliminary_define (SearchState *st, s32 cell_x, s32 cell_y, int state, int is_forced)
{
	TakenDefine *td = &st->taken_define [st->taken_define_cnt];
	st->taken_define_cnt++;
	
	td->x = cell_x;
	td->y = cell_y;
	td->state = state;
	td->is_forced = is_forced;
	
	st->undef_shadow [cell_x] [cell_y] = 0;
	
	if (state)
		st->on_shadow [cell_x] [cell_y] = 1;
}

static __force_inline int verify_cell_stability_and_take_forced (SearchState *st, s32 cell_x, s32 cell_y)
{
	s32 on_cnt = 0;
	s32 undef_cnt = 0;
	s32 y;
	s32 x;
	
	for (y = cell_y - 1; y <= cell_y + 1; y++)
		for (x = cell_x - 1; x <= cell_x + 1; x++)
			if (y != cell_y || x != cell_x)
			{
				on_cnt += st->on_shadow [x] [y];
				undef_cnt += st->undef_shadow [x] [y];
			}
	
	// We can sometimes define an undefined center cell to a particular value, or we can define the unknown neighbours of a defined center cell to a particular common value, but we can never do both in B3/S23
	if (st->undef_shadow [cell_x] [cell_y])
	{
		int stability = st->undefined_may_be_stabilized_table [undef_cnt] [on_cnt];
		if (stability == 2)
		{
			add_preliminary_define (st, cell_x, cell_y, stability - 2, TRUE);
			return TRUE;
		}
		else
			return stability;
	}
	else
	{
		int stability = st->defined_may_be_stabilized_table [undef_cnt] [on_cnt] [st->on_shadow [cell_x] [cell_y]];
		if (stability > 1)
		{
			for (y = cell_y - 1; y <= cell_y + 1; y++)
				for (x = cell_x - 1; x <= cell_x + 1; x++)
					if (y != cell_y || x != cell_x)
						if (st->undef_shadow [x] [y])
							add_preliminary_define (st, x, y, stability - 2, TRUE);
			
			return TRUE;
		}
		else
			return stability;
	}
}

static __force_inline int verify_suggested_define_and_take_forced (SearchState *st)
{
	s32 taken_define_ix = st->taken_define_cnt - 1;
	while (TRUE)
	{
		TakenDefine *td = &st->taken_define [taken_define_ix];
		
		// Test for consequences of the newly added cell first
		if (!(verify_cell_stability_and_take_forced (st, td->x, td->y)))
			return FALSE;
		
		// Then test for consequences for the neighbours of the newly added cell
		s32 y;
		s32 x;
		for (y = td->y - 1; y <= td->y + 1; y++)
			for (x = td->x - 1; x <= td->x + 1; x++)
				if (y != td->y || x != td->x)
					if (!(verify_cell_stability_and_take_forced (st, x, y)))
						return FALSE;
		
		if (taken_define_ix >= st->taken_define_cnt - 1)
			break;
		
		taken_define_ix++;
	}
	
	return TRUE;
}

static __force_inline void undo_preliminary_defines (SearchState *st, s32 prev_taken_define_cnt)
{
	while (st->taken_define_cnt > prev_taken_define_cnt)
	{
		st->taken_define_cnt--;
		TakenDefine *td = &st->taken_define [st->taken_define_cnt];
		
		st->undef_shadow [td->x] [td->y] = 1;
		
		if (td->state)
			st->on_shadow [td->x] [td->y] = 0;
	}
}

static __force_inline void finalize_preliminary_defines (SearchState *st, s32 prev_taken_define_cnt)
{
	s32 taken_define_ix = prev_taken_define_cnt;
	while (taken_define_ix < st->taken_define_cnt)
	{
		TakenDefine *td = &st->taken_define [taken_define_ix];
		
		GoLGrid_set_cell_off_64_wide_zero_org_unchanged_bb_unchecked (st->undef_cells, td->x, td->y);
		
		if (td->state == 1)
		{
			GoLGrid_set_cell_on_64_wide_zero_org_unchecked (st->on_cells, td->x, td->y);
			st->new_on_cells_defined = TRUE;
			st->on_cnt++;
			if (st->on_cnt <= TAG_SIZE)
				st->new_tag_cells_defined = TRUE;
		}
		
		taken_define_ix++;
	}
}

static __not_inline int try_define_cell (SearchState *st, s32 cell_x, s32 cell_y, int state)
{
	s32 cur_taken_define_cnt = st->taken_define_cnt;
	
	add_preliminary_define (st, cell_x, cell_y, state, FALSE);
	int could_be_stabilized = verify_suggested_define_and_take_forced (st);
	
	if (!could_be_stabilized)
		undo_preliminary_defines (st, cur_taken_define_cnt);
	else
		finalize_preliminary_defines (st, cur_taken_define_cnt);
	
	return could_be_stabilized;
}

static __force_inline const TakenDefine *undo_taken_define (SearchState *st)
{
	st->taken_define_cnt--;
	TakenDefine *td = &st->taken_define [st->taken_define_cnt];
	
	GoLGrid_set_cell_on_64_wide_zero_org_unchanged_bb_unchecked (st->undef_cells, td->x, td->y);
	st->undef_shadow [td->x] [td->y] = 1;
	
	if (td->state)
	{
		GoLGrid_set_cell_off_64_wide_zero_org_unchecked (st->on_cells, td->x, td->y);
		st->on_shadow [td->x] [td->y] = 0;
		st->on_cnt--;
	}
	
	return &st->taken_define [st->taken_define_cnt];
}

static __force_inline s32 distance_from_start_cell (s32 x, s32 y)
{
	return ((SEED_ON_CELL_X - x) * (SEED_ON_CELL_X - x)) + ((SEED_ON_CELL_Y - y) * (SEED_ON_CELL_Y - y));
}

// Find the on-cell in the grid that has the closest geometric distance to the first on-cell
static __force_inline int find_closest (const GoLGrid *obj_gg, s32 *closest_x, s32 *closest_y)
{
	int first = TRUE;
	s32 cell_x = 0;
	s32 cell_y = 0;
	s32 closest_dist = -1;
	
	while (TRUE)
	{
		if (!GoLGrid_find_next_on_cell_64_wide (obj_gg, first, &cell_x, &cell_y))
			break;
		
		first = FALSE;
		
		s32 dist = distance_from_start_cell (cell_x, cell_y);
		if (closest_dist == -1 || dist < closest_dist)
		{
			closest_dist = dist;
			*closest_x = cell_x;
			*closest_y = cell_y;
		}
	}
	
	return TRUE;
}

static __not_inline int find_cell_to_define (SearchState *st, s32 *cell_to_define_x, s32 *cell_to_define_y)
{
	static int static_init = FALSE;
	static GoLGrid *connected;
	static GoLGrid *not_stable;
	static GoLGrid *not_stable_bleed_8;
	static GoLGrid *not_stable_undef_neighbours;
	static GoLGrid *bleed_8;
	static GoLGrid *undef_in_bleed_8;
	static GoLGrid *bleed_20;
	static GoLGrid *undef_in_bleed_20;
	
	if (!static_init)
	{
		static_init = TRUE;
		connected = alloc_grid ();
		not_stable = alloc_grid ();
		not_stable_bleed_8 = alloc_grid ();
		not_stable_undef_neighbours = alloc_grid ();
		bleed_8 = alloc_grid ();
		undef_in_bleed_8 = alloc_grid ();
		bleed_20 = alloc_grid ();
		undef_in_bleed_20 = alloc_grid ();
	}
	
	*cell_to_define_x = 0;
	*cell_to_define_y = 0;
	
	// First we prioritize cells in the neighbourhood of unstable cell. If it's worth the extra effort we limit this to the first connected part
	
	int do_expensive_checks = (st->on_cnt <= st->max_wanted_bit_cnt - REMAINING_CELLS_THRESHOLD_FOR_UNCONNECTABLE_CHECK);
	int is_shown_unconnected = FALSE;
	
	if (do_expensive_checks)
	{
		is_shown_unconnected = !(get_connected_part (st->on_cells, connected));
		if (is_shown_unconnected)
			if (!is_connectable (st))
				return FALSE;
		
		GoLGrid_evolve_64_wide (connected, not_stable);
		GoLGrid_xor_64_wide (not_stable, connected);
	}
	else
	{
		GoLGrid_evolve_64_wide (st->on_cells, not_stable);
		GoLGrid_xor_64_wide (not_stable, st->on_cells);
	}
	
	GoLGrid_bleed_8_64_wide (not_stable, not_stable_bleed_8);
	GoLGrid_and_64_wide (not_stable_bleed_8, st->undef_cells, not_stable_undef_neighbours);
	
	if (!GoLGrid_is_empty (not_stable_undef_neighbours))
		return find_closest (not_stable_undef_neighbours, cell_to_define_x, cell_to_define_y);
	
	// Then we prioritize cells in the neighbourhood of cells already defined to on
	// Finally we allow cells up to a (2, 1) distance from cells already defined to on
	// This is enough for B3/S23, but some rules will also require cells at a (2, 2) distance to be considered
	// If we have taken the time to find the first connected part, we do these two steps for that part first
	
	if (is_shown_unconnected)
	{
		GoLGrid_bleed_8_64_wide (connected, bleed_8);
		GoLGrid_and_64_wide (bleed_8, st->undef_cells, undef_in_bleed_8);
		
		if (!GoLGrid_is_empty (undef_in_bleed_8))
			return find_closest (undef_in_bleed_8, cell_to_define_x, cell_to_define_y);
		
		GoLGrid_bleed_4_64_wide (bleed_8, bleed_20);
		GoLGrid_and_64_wide (bleed_20, st->undef_cells, undef_in_bleed_20);
		
		if (!GoLGrid_is_empty (undef_in_bleed_20))
			return find_closest (undef_in_bleed_20, cell_to_define_x, cell_to_define_y);
	}
	
	GoLGrid_bleed_8_64_wide (st->on_cells, bleed_8);
	GoLGrid_and_64_wide (bleed_8, st->undef_cells, undef_in_bleed_8);
	
	if (!GoLGrid_is_empty (undef_in_bleed_8))
		return find_closest (undef_in_bleed_8, cell_to_define_x, cell_to_define_y);
	
	GoLGrid_bleed_4_64_wide (bleed_8, bleed_20);
	GoLGrid_and_64_wide (bleed_20, st->undef_cells, undef_in_bleed_20);
	
	if (!GoLGrid_is_empty (undef_in_bleed_20))
		return find_closest (undef_in_bleed_20, cell_to_define_x, cell_to_define_y);
	
	return FALSE;
}

static __not_inline int compare_cell_lists (const ObjCellList *list_1, const ObjCellList *list_2)
{
	if (list_1->obj_rect.width != list_2->obj_rect.width || list_1->obj_rect.height != list_2->obj_rect.height || list_1->cell_cnt != list_2->cell_cnt)
		return ffsc (__func__);
	
	s32 cell_ix;
	for (cell_ix = 0; cell_ix < list_1->cell_cnt; cell_ix++)
	{
		s32 x1 = list_1->cell [cell_ix].x;
		s32 y1 = list_1->cell [cell_ix].y;
		s32 x2 = list_2->cell [cell_ix].x;
		s32 y2 = list_2->cell [cell_ix].y;
		
		if (y1 > y2 || (y1 == y2 && x1 > x2))
			return 1;
		if (y1 < y2 || (y1 == y2 && x1 < x2))
			return -1;
	}
	
	return 0;
}

static __not_inline int is_canonical (const GoLGrid *gg)
{
	static ObjCellList org;
	static ObjCellList_Cell org_cell [MAX_ON_CELLS];
	static ObjCellList trans;
	static ObjCellList_Cell trans_cell [MAX_ON_CELLS];
	
	ObjCellList_make_empty (&org, org_cell, MAX_ON_CELLS);
	ObjCellList_make_empty (&trans, trans_cell, MAX_ON_CELLS);
	
	GoLGrid_to_obj_cell_list (gg, &org);
	if (org.obj_rect.width < org.obj_rect.height)
		return FALSE;
	
	ObjCellList_copy (&org, &trans);
	
	if (org.obj_rect.width == org.obj_rect.height)
	{
		ObjCellList_flip_horizontally (&trans);
		if (compare_cell_lists (&org, &trans) < 0)
			return FALSE;
		ObjCellList_flip_vertically (&trans);
		if (compare_cell_lists (&org, &trans) < 0)
			return FALSE;
		ObjCellList_flip_horizontally (&trans);
		if (compare_cell_lists (&org, &trans) < 0)
			return FALSE;
		
		ObjCellList_flip_diagonally (&trans);
		if (compare_cell_lists (&org, &trans) < 0)
			return FALSE;
	}
	
	ObjCellList_flip_horizontally (&trans);
	if (compare_cell_lists (&org, &trans) < 0)
		return FALSE;
	ObjCellList_flip_vertically (&trans);
	if (compare_cell_lists (&org, &trans) < 0)
		return FALSE;
	ObjCellList_flip_horizontally (&trans);
	if (compare_cell_lists (&org, &trans) < 0)
		return FALSE;
	
	return TRUE;
}

static __not_inline int is_connected (const GoLGrid *gg)
{
	static int static_init = FALSE;
	static GoLGrid *connected;
	
	if (!static_init)
	{
		static_init = TRUE;
		connected = alloc_grid ();
	}
	
	return get_connected_part (gg, connected);
}

static __not_inline int verify_possible_solution (SearchState *st)
{
	static int static_init = FALSE;
	static GoLGrid *evolved;
	
	if (!static_init)
	{
		static_init = TRUE;
		evolved = alloc_grid ();
	}
	
	GoLGrid_evolve_64_wide (st->on_cells, evolved);
	if (!GoLGrid_is_equal_64_wide (st->on_cells, evolved))
	{
		st->not_stable_cnt++;
		return FALSE;
	}
	
	if (!is_canonical (st->on_cells))
	{
		st->not_canonical_cnt++;
		return FALSE;
	}
	
	if (!is_connected (st->on_cells))
	{
		st->not_connected_cnt++;
		return FALSE;
	}
	
	return TRUE;
}

static __not_inline s32 partition_into_islands (const GoLGrid *gg, ObjCellList *list)
{
	static int static_init = FALSE;
	static GoLGrid *remaining;
	static GoLGrid *cur_part;
	static GoLGrid *bleed_8;
	static GoLGrid *new_part;
	
	if (!static_init)
	{
		static_init = TRUE;
		remaining = alloc_grid ();
		cur_part = alloc_grid ();
		bleed_8 = alloc_grid ();
		new_part = alloc_grid ();
	}
	
	GoLGrid_copy_64_wide (gg, remaining);
	
	s32 part_cnt = 0;
	while (TRUE)
	{
		if (GoLGrid_is_empty (remaining))
			break;
		
		s32 x;
		s32 y;
		if (!GoLGrid_find_next_on_cell_64_wide (remaining, TRUE, &x, &y))
			break;
		
		GoLGrid_clear_64_wide (cur_part);
		GoLGrid_set_cell_on_64_wide_zero_org_unchecked (cur_part, x, y);
		
		while (TRUE)
		{
			GoLGrid_bleed_8_64_wide (cur_part, bleed_8);
			GoLGrid_and_64_wide (remaining, bleed_8, new_part);
			
			if (GoLGrid_is_equal_64_wide (new_part, cur_part))
				break;
			
			GoLGrid_copy_64_wide (new_part, cur_part);
		}
		
		GoLGrid_to_obj_cell_list (cur_part, &list [part_cnt]);
		GoLGrid_subtract_64_wide (remaining, cur_part);
		part_cnt++;
	}
	
	return part_cnt;
}

static __not_inline int is_stable_subset (const ObjCellList *list, u64 partition_mask)
{
	static int static_init = FALSE;
	static GoLGrid *subset;
	static GoLGrid *evolved;
	
	if (!static_init)
	{
		static_init = TRUE;
		subset = alloc_grid ();
		evolved = alloc_grid ();
	}
	
	GoLGrid_clear_noinline (subset);
	while (partition_mask)
	{
		s32 list_ix = least_significant_bit_u64 (partition_mask);
		GoLGrid_or_obj_cell_list_noinline (subset, &list [list_ix], 0, 0);
		partition_mask = partition_mask ^ (((u64) 1) << list_ix);
	}
	
	GoLGrid_evolve_noinline (subset, evolved);
	return GoLGrid_is_equal_noinline (subset, evolved);
}

static __not_inline int has_stable_partitioning (const ObjCellList *list, s32 list_cnt, int allow_more_than_two_parts, int is_already_a_partition)
{
	if (is_already_a_partition)
	{
		if (is_stable_subset (list, (((u64) 1) << list_cnt) - (u64) 1))
			return TRUE;
		else if (!allow_more_than_two_parts)
			return FALSE;
	}
	
	s32 subset_cnt = 1 << list_cnt;
	s32 subset_ix;
	
	// We test all possible subsets of the partitions where partition 0 is included, except for the one where all partitions are included
	for (subset_ix = 1; subset_ix < subset_cnt - 2; subset_ix += 2)
		if (is_stable_subset (list, subset_ix))
		{
			// We have found a stable subset, now recursively check if the part of the pattern not included in that subset is stable, either as a whole or any partitioning of it
			
			ObjCellList obj [MAX_PARTITIONS];
			ObjCellList_Cell obj_cell [MAX_PARTITIONS] [MAX_ON_CELLS];
			
			s32 remains_cnt = 0;
			s32 remains_ix;
			for (remains_ix = 0; remains_ix < list_cnt; remains_ix++)
				if (((1 << remains_ix) & subset_ix) == 0)
				{
					ObjCellList_make_empty (&obj [remains_cnt], obj_cell [remains_cnt], MAX_ON_CELLS);
					ObjCellList_copy (&list [remains_ix], &obj [remains_cnt]);
					remains_cnt++;
				}
			
			if (has_stable_partitioning (obj, remains_cnt, allow_more_than_two_parts, TRUE))
				return TRUE;
		}
	
	return FALSE;	
}

static __not_inline int is_pseudo_still (SearchState *st, int allow_more_than_two_parts)
{
	ObjCellList obj [MAX_PARTITIONS];
	ObjCellList_Cell obj_cell [MAX_PARTITIONS] [MAX_ON_CELLS];
	
	s32 obj_ix;
	for (obj_ix = 0; obj_ix < MAX_PARTITIONS; obj_ix++)
		ObjCellList_make_empty (&obj [obj_ix], obj_cell [obj_ix], MAX_ON_CELLS);
	
	s32 part_cnt = partition_into_islands (st->on_cells, obj);
	return has_stable_partitioning (obj, part_cnt, allow_more_than_two_parts, FALSE);
}

// This inefficient evolve function is only used to build the stability tables
static __not_inline int evolved_cell_state_with_unknown (int cur_state, s32 on_cnt, s32 unknown_cnt)
{
	if (on_cnt + unknown_cnt > 8)
		return -2;
	
	if (cur_state)
		if (on_cnt >= 2 && (on_cnt + unknown_cnt) <= 3)
			return 1;
		else if ((on_cnt + unknown_cnt) < 2 || on_cnt > 3)
			return 0;
		else
			return -1;
	else
		if (on_cnt == 3 && unknown_cnt == 0)
			return 1;
		if ((on_cnt + unknown_cnt) < 3 || on_cnt > 3)
			return 0;
		else
			return -1;
}

static __not_inline void make_stability_tables (SearchState *st)
{
	s32 unknown_cnt;
	s32 on_cnt;
	s32 cell_state;
	
	for (unknown_cnt = 0; unknown_cnt <= 8; unknown_cnt++)
		for (on_cnt = 0; on_cnt <= 8; on_cnt++)
			for (cell_state = 0; cell_state <= 1; cell_state++)
			{
				int table_entry = -2;
				if (unknown_cnt + on_cnt <= 8)
				{
					s32 option_cnt = 0;
					s32 last_working_unknown_on_cnt = -1;
					s32 unknown_on_cnt;
					
					for (unknown_on_cnt = 0; unknown_on_cnt <= unknown_cnt; unknown_on_cnt++)
 						if (evolved_cell_state_with_unknown (cell_state, on_cnt + unknown_on_cnt, 0) == cell_state)
						{
							option_cnt++;
							last_working_unknown_on_cnt = unknown_on_cnt;
						}
					
					if (unknown_cnt > 0 && option_cnt == 1 && ((last_working_unknown_on_cnt == 0) || (last_working_unknown_on_cnt == unknown_cnt)))
						table_entry = (last_working_unknown_on_cnt == 0) ? 2 : 3;
					else if (option_cnt > 0)
						table_entry = 1;
					else
						table_entry = 0;
				}
				
	 			st->defined_may_be_stabilized_table [unknown_cnt] [on_cnt] [cell_state] = table_entry;
			}
	
	for (unknown_cnt = 0; unknown_cnt <= 8; unknown_cnt++)
		for (on_cnt = 0; on_cnt <= 8; on_cnt++)
		{
			int table_entry = -2;
			if (unknown_cnt + on_cnt <= 8)
			{
				int stability_with_off = st->defined_may_be_stabilized_table [unknown_cnt] [on_cnt] [0];
				int stability_with_on = st->defined_may_be_stabilized_table [unknown_cnt] [on_cnt] [1];
				
				// Keep this test in case support for other rules than B3/S23 would be added in the future
				if ((stability_with_off == 0 && stability_with_on != 1) || (stability_with_on == 0 && stability_with_off != 1))
				{
					fprintf (stderr, "Invalid assumption about stabilization property of undefined cells\n");
					exit (EXIT_FAILURE);
				}
				
				if (stability_with_off == 0)
					table_entry = 3;
				else if (stability_with_on == 0)
					table_entry = 2;
				else
					table_entry = 1;
			}
			
	 		st->undefined_may_be_stabilized_table [unknown_cnt] [on_cnt] = table_entry;
		}
}

static __not_inline int open_files (SearchState *st)
{
	s32 bit_ix;
	for (bit_ix = st->min_wanted_bit_cnt; bit_ix <= st->max_wanted_bit_cnt; bit_ix++)
	{
		st->strict_file [bit_ix] = NULL;
		st->pseudo_file [bit_ix] = NULL;
	}
	
	char filename [64];
	for (bit_ix = st->min_wanted_bit_cnt; bit_ix <= st->max_wanted_bit_cnt; bit_ix++)
	{
		if (st->search_subset < 0)
			sprintf (filename, "%02d_bits_strict.txt", bit_ix);
		else
			sprintf (filename, "%02d_bits_strict_subset_%04d_of_%04d.txt", bit_ix, st->search_subset, SELECTED_SEARCH_SUBSETS);
		
		st->strict_file [bit_ix] = fopen (filename, "w");
		if (!st->strict_file [bit_ix])
			return FALSE;
		if (setvbuf (st->strict_file [bit_ix], NULL, _IOFBF, FILE_BUFFER_SIZE))
			return FALSE;
			
		if (st->search_subset < 0)
			sprintf (filename, "%02d_bits_pseudo.txt", bit_ix);
		else
			sprintf (filename, "%02d_bits_pseudo_subset_%04d_of_%04d.txt", bit_ix, st->search_subset, SELECTED_SEARCH_SUBSETS);
		
		st->pseudo_file [bit_ix] = fopen (filename, "w");
		if (!st->pseudo_file [bit_ix])
			return FALSE;
		if (setvbuf (st->pseudo_file [bit_ix], NULL, _IOFBF, FILE_BUFFER_SIZE))
			return FALSE;
	}
	
	return TRUE;
}

static __not_inline void write_result (SearchState *st, int is_pseudo)
{
	Rect bb;
	GoLGrid_get_bounding_box (st->on_cells, &bb);
	GoLGrid_print_life_history_full ((is_pseudo ? st->pseudo_file [st->on_cnt] : st->strict_file [st->on_cnt]), &bb, st->on_cells, NULL, NULL, NULL, FALSE, 1024);
}

static __not_inline void close_files (SearchState *st)
{
	s32 bit_ix;
	for (bit_ix = st->min_wanted_bit_cnt; bit_ix <= st->max_wanted_bit_cnt; bit_ix++)
	{
		if (st->strict_file [bit_ix])
			fclose (st->strict_file [bit_ix]);
		if (st->pseudo_file [bit_ix])
			fclose (st->pseudo_file [bit_ix]);
	}
}

static __not_inline void add_open_cells (SearchState *st)
{
	s32 y;
	s32 x;
	for (y = 0; y < GRID_HEIGHT; y++)
		for (x = 0; x < GRID_WIDTH; x++)
			if (y >= GRID_BORDER && y < GRID_HEIGHT - GRID_BORDER && x >= GRID_BORDER && x < GRID_WIDTH - GRID_BORDER && (x > SEED_ON_CELL_X || y <= SEED_ON_CELL_Y))
			{
				GoLGrid_set_cell_on (st->undef_cells, x, y);
				st->undef_shadow [x] [y] = 1;
			}
}

static __not_inline s32 try_subset_division (s64 *op_cnt_at_new_tag, s64 max_ops_in_subset, s32 *first_tag_in_subset)
{
	s32 subset_ix = 0;
	first_tag_in_subset [subset_ix] = 0;
	
	s32 tag_ix;
	for (tag_ix = 0; tag_ix <= TAG_CNT_AT_TAG_SIZE_9; tag_ix++)
	{
		s64 next_subset_op_cnt = op_cnt_at_new_tag [tag_ix] - op_cnt_at_new_tag [first_tag_in_subset [subset_ix]];
		if (next_subset_op_cnt > max_ops_in_subset)
		{
			subset_ix++;
			first_tag_in_subset [subset_ix] = tag_ix - 1;
		}
	}
	
	subset_ix++;
	first_tag_in_subset [subset_ix] = TAG_CNT_AT_TAG_SIZE_9;
	
	return subset_ix;
}

static __not_inline void print_search_subset_division_table (s64 *op_cnt_at_new_tag)
{
	s32 first_tag_in_subset [4 * WANTED_SEARCH_SUBSETS];
	s64 max_ops_in_subset = MAX_OPS_IN_SUBSET_LOW_ESTIMATE;
	
	s32 subset_cnt;
	while (TRUE)
	{
		subset_cnt = try_subset_division (op_cnt_at_new_tag, max_ops_in_subset, first_tag_in_subset);
		if (subset_cnt <= WANTED_SEARCH_SUBSETS)
			break;
		
		max_ops_in_subset += (MAX_OPS_IN_SUBSET_LOW_ESTIMATE / 10000);
	}
	
	printf ("\n");
	s32 subset_ix;
	for (subset_ix = 0; subset_ix <= 100; subset_ix++)
		printf ("%d, ", first_tag_in_subset [subset_ix]);
	
	printf ("\n\n");
	printf ("subsets = %d, max ops in subset = %" PRIu64 ", total ops = %" PRIu64 ", ops in last subset = %" PRIu64 "\n", subset_cnt, max_ops_in_subset, op_cnt_at_new_tag [first_tag_in_subset [subset_cnt]],
			op_cnt_at_new_tag [first_tag_in_subset [subset_cnt]] - op_cnt_at_new_tag [first_tag_in_subset [subset_cnt - 1]]);
}

static __not_inline int do_search (s32 min_wanted_bit_cnt, s32 max_wanted_bit_cnt, s32 search_subset, int write_files, int report_complex_pseudo_still_lifes, int build_subset_division_table, GridVisualization *grid_visualization)
{
	// This is used to prepare tables for dividing the search space into equal subsets when this mode is selected
	s64 op_cnt_at_new_tag [TAG_CNT_AT_TAG_SIZE_9 + 1];
	
	SearchState st;
	
	make_stability_tables (&st);
	
	st.write_files = write_files;
	
	st.min_wanted_bit_cnt = min_wanted_bit_cnt;
	st.max_wanted_bit_cnt = max_wanted_bit_cnt;
	
	st.search_subset = search_subset;
	
	if (st.search_subset < 0)
	{
		st.wanted_tag_on = 0;
		st.wanted_tag_off = -1;
	}
	else
	{
		st.wanted_tag_on = tag_size_9_in_subsets [st.search_subset];
		st.wanted_tag_off = tag_size_9_in_subsets [st.search_subset + 1];
	}
	
	st.undef_cells = alloc_grid ();
	st.on_cells = alloc_grid ();
	st.on_cnt = 0;
	
	st.taken_define_cnt = 0;
	
	st.new_on_cells_defined = TRUE;
	st.new_tag_cells_defined = TRUE;
	st.cur_tag_ix = -1;
	st.op_cnt = 0;
	
	st.not_stable_cnt = 0;
	st.not_canonical_cnt = 0;
	st.not_connected_cnt = 0;
	
	s32 on_cnt;
	for (on_cnt = 0; on_cnt <= MAX_BIT_CNT; on_cnt++)
	{
		st.strict_sol_cnt [on_cnt] = 0;
 		st.pseudo_sol_cnt [on_cnt] = 0;
	}
	
	st.gv = grid_visualization;
	
	if (st.write_files)
		if (!open_files (&st))
		{
			close_files (&st);
			fprintf (stderr, "Failed to open output files\n");
			return FALSE;
		}
	
	add_open_cells (&st);
	try_define_cell (&st, SEED_ON_CELL_X, SEED_ON_CELL_Y, 1);
	
	fprintf (stderr, "\nCells open to define and the (%d, %d) cell that should always be on:\n\n", SEED_ON_CELL_X, SEED_ON_CELL_Y);
	GoLGrid_print_life_history_full (stderr, NULL, st.on_cells, st.undef_cells, NULL, NULL, TRUE, 68);
	fprintf (stderr, "\n");
	
	s32 vis_cnt = 0;
	s32 upd_cnt = 0;
	while (TRUE)
	{
		if (vis_cnt == 0)
		{
			vis_cnt = 500000;
			visualize_cur (&st);
			GridVisualization_update (st.gv);
		}
		
		if (upd_cnt == 0)
		{
			upd_cnt = 50000000;
			fprintf (stderr, "%" PRIu64 " strict and %" PRIu64 " pseudo %d bit still lifes so far\n", st.strict_sol_cnt [st.max_wanted_bit_cnt], st.pseudo_sol_cnt [st.max_wanted_bit_cnt], st.max_wanted_bit_cnt);
		}
		
		vis_cnt--;
		upd_cnt--;
		
		st.op_cnt++;
		
		if (st.new_on_cells_defined)
		{
			if (st.on_cnt >= st.min_wanted_bit_cnt && st.on_cnt <= st.max_wanted_bit_cnt)
				if (verify_possible_solution (&st))
				{
					if (is_pseudo_still (&st, TRUE))
					{
						if (report_complex_pseudo_still_lifes)
							if (!is_pseudo_still (&st, FALSE))
							{
								printf ("Pseudo still life not partitionable in two parts:\n");
								GoLGrid_print (st.on_cells);
							}
						
						st.pseudo_sol_cnt [st.on_cnt]++;
						if (st.write_files)
							write_result (&st, TRUE);
					}
					else
					{
						st.strict_sol_cnt [st.on_cnt]++;
						if (st.write_files)
							write_result (&st, FALSE);
					}
				}
			
			if (st.new_tag_cells_defined && st.on_cnt >= TAG_SIZE)
			{
				st.cur_tag_ix++;
				if (build_subset_division_table)
					op_cnt_at_new_tag [st.cur_tag_ix] = st.op_cnt;
				
				st.new_tag_cells_defined = FALSE;
			}
			
			st.new_on_cells_defined = FALSE;
		}
		
		if (st.wanted_tag_off != -1 && st.cur_tag_ix >= st.wanted_tag_off)
			break;
		
		if (st.on_cnt < TAG_SIZE || st.cur_tag_ix >= st.wanted_tag_on)
			if (st.on_cnt < st.max_wanted_bit_cnt)
			{
				s32 cell_to_define_x;
				s32 cell_to_define_y;
				
				int found = find_cell_to_define (&st, &cell_to_define_x, &cell_to_define_y);
				if (found)
				{
					if (try_define_cell (&st, cell_to_define_x, cell_to_define_y, 1))
						continue;
					
					if (try_define_cell (&st, cell_to_define_x, cell_to_define_y, 0))
						continue;
				}
			}
		
		while (TRUE)
		{
			const TakenDefine *undone_td = undo_taken_define (&st);
			
			if (st.taken_define_cnt == 0)
				break;
			
			if (!(undone_td->is_forced) && undone_td->state == 1)
				if (try_define_cell (&st, undone_td->x, undone_td->y, 0))
					break;
		}
		
		if (st.taken_define_cnt == 0)
			break;
	}
	
	if (st.write_files)
		close_files (&st);
	
	if (build_subset_division_table)
	{
		op_cnt_at_new_tag [TAG_CNT_AT_TAG_SIZE_9] = st.op_cnt;
		print_search_subset_division_table (op_cnt_at_new_tag);
	}
	
	printf ("Not stable = %" PRIu64 ", not canonical = %" PRIu64 ", not connected = %" PRIu64 "\n", st.not_stable_cnt, st.not_canonical_cnt, st.not_connected_cnt);
	
	for (on_cnt = st.min_wanted_bit_cnt; on_cnt <= st.max_wanted_bit_cnt; on_cnt++)
	{
		printf ("\nNumber of on-cells: %10d\n", on_cnt);
		if (st.search_subset < 0)
			printf ("Result for full search space:\n");
		else
			printf ("Result for subset %d in (0..%d) of search space:\n", st.search_subset, SELECTED_SEARCH_SUBSETS - 1);
		
		printf ("Strict still lifes: %10" PRIu64 "\n", st.strict_sol_cnt [on_cnt]);
		printf ("Pseudo still lifes: %10" PRIu64 "\n", st.pseudo_sol_cnt [on_cnt]);
	}
	
	return TRUE;
}

static __not_inline int main_do (int argc, const char *const *argv)
{
	PerfTimer_init ();
	
	int cl_write_files = FALSE;
	u32 cl_min_wanted_bit_cnt;
	u32 cl_max_wanted_bit_cnt;
	u32 cl_selected_subset;
	s32 selected_subset = -1;
	
	int usage_fail = FALSE;
	if (argc < 4 || argc > 5)
		usage_fail = TRUE;
	
	if (!usage_fail && (strcmp (argv [1], "w") != 0) && (strcmp (argv [1], "c") != 0))
		usage_fail = TRUE;
	
	if (!usage_fail && argc == 5)
	{
		if (!str_to_u32 (argv [4], &cl_selected_subset))
			usage_fail = TRUE;
		else
			selected_subset = cl_selected_subset;
	}
	
	if (!usage_fail && (!str_to_u32 (argv [2], &cl_min_wanted_bit_cnt) || !str_to_u32 (argv [3], &cl_max_wanted_bit_cnt)))
		usage_fail = TRUE;
	
	if (usage_fail)
	{
		fprintf (stderr, "Usage: StillCount <command> <min on cells> <max on cells> [<selected subset>]\n");
		fprintf (stderr, "       where <command> is \"w\" to write files, or \"c\" to only count\n");
		return FALSE;
	}
	
	if (strcmp (argv [1], "w") == 0)
		cl_write_files = TRUE;
	
	if (cl_max_wanted_bit_cnt > MAX_BIT_CNT)
	{
		fprintf (stderr, "<max on cells> may not be higher than %d\n", MAX_BIT_CNT);
		return FALSE;
	}
	if (cl_min_wanted_bit_cnt > cl_max_wanted_bit_cnt)
	{
		fprintf (stderr, "<min on cells> may not be higher than <max on cells>\n");
		return FALSE;
	}
	if (selected_subset >= SELECTED_SEARCH_SUBSETS)
	{
		fprintf (stderr, "<selected_subset> must be between 0 and %d\n", SELECTED_SEARCH_SUBSETS - 1);
		return FALSE;
	}
	if (selected_subset >= 0 && cl_min_wanted_bit_cnt < TAG_SIZE + 10)
	{
		fprintf (stderr, "Searching for a subset is not supported if <min on cells> is lower than %d\n", TAG_SIZE + 10);
		return FALSE;
	}
	
	s32 vizualization_side = cl_max_wanted_bit_cnt + 6;
	Rect visualization_area;
	Rect_make (&visualization_area, 0, SEED_ON_CELL_Y - vizualization_side, SEED_ON_CELL_X + vizualization_side, 2 * vizualization_side);
	
	GridVisualization gv;
	GridVisualization_create (&gv, "Still life search", &visualization_area, 8, 2);
	
	int success = do_search (cl_min_wanted_bit_cnt, cl_max_wanted_bit_cnt, selected_subset, cl_write_files, TRUE, FALSE, &gv);
	
	GridVisualization_close (&gv);
	
	PerfTimer_report ();
	return success;
}

int main (int argc, const char *const *argv)
{
	if (!verify_cpu_type ())
		return EXIT_FAILURE;
	
	if (!main_do (argc, argv))
		return EXIT_FAILURE;
	
	return EXIT_SUCCESS;
}
