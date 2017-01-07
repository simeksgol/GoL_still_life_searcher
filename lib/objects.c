// A description of a glider
// (dir) is 0 for a NW, 1 for a NE, 2 for a SE and 3 for SW-bound glider
// (lane) is the x-coordinate for the center cell of the glider if it is moved backwards or forwards in time, so that its center cell has y-coordinate 0
// and it is in the phase with three cells in a horizontal line
// (timing) is the generation if the glider is moved backwards or forwards in time, so that its center cell has x-coordinate 0 (instead of y-coordinate
// as for (lane)) and with the same phase as above

typedef struct
{
	s32 dir;
	s32 lane;
	s32 timing;
} Glider;

typedef struct
{
	const ObjCellList_Cell cells [5];
	s32 x_offs;
	s32 y_offs;
	s32 lane_y_dir;
	s32 timing_x_dir;
	s32 timing_y_dir;
} Objects_GliderData;

static const Objects_GliderData Objects_glider_data [4] [4] =
{{{{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 2}}, -1, -1, -1,  1,  1}, {{{1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 2}}, -1, -1, -1,  1,  1},
  {{{0, 0}, {1, 0}, {0, 1}, {2, 1}, {0, 2}},  0, -1, -1,  1,  1}, {{{1, 0}, {0, 1}, {1, 1}, {0, 2}, {2, 2}},  0, -1, -1,  1,  1}},
 {{{{0, 0}, {1, 0}, {2, 0}, {2, 1}, {1, 2}}, -1, -1,  1, -1,  1}, {{{0, 0}, {1, 0}, {1, 1}, {2, 1}, {0, 2}}, -1, -1,  1, -1,  1},
  {{{1, 0}, {2, 0}, {0, 1}, {2, 1}, {2, 2}}, -2, -1,  1, -1,  1}, {{{1, 0}, {1, 1}, {2, 1}, {0, 2}, {2, 2}}, -2, -1,  1, -1,  1}},
 {{{{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}}, -1, -1, -1, -1, -1}, {{{0, 0}, {1, 1}, {2, 1}, {0, 2}, {1, 2}}, -1, -1, -1, -1, -1},
  {{{2, 0}, {0, 1}, {2, 1}, {1, 2}, {2, 2}}, -2, -1, -1, -1, -1}, {{{0, 0}, {2, 0}, {1, 1}, {2, 1}, {1, 2}}, -2, -1, -1, -1, -1}},
 {{{{1, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2}}, -1, -1,  1,  1, -1}, {{{2, 0}, {0, 1}, {1, 1}, {1, 2}, {2, 2}}, -1, -1,  1,  1, -1},
  {{{0, 0}, {0, 1}, {2, 1}, {0, 2}, {1, 2}},  0, -1,  1,  1, -1}, {{{0, 0}, {2, 0}, {0, 1}, {1, 1}, {1, 2}},  0, -1,  1,  1, -1}}};


static __not_inline void Objects_get_glider_timing_range (s32 glider_dir, const Rect *allowed_rect, s32 *timing_on, s32 *timing_off)
{
	if (timing_on)
		*timing_on = 0;
	if (timing_off)
		*timing_off = 0;
	
	if (glider_dir < 0 || glider_dir >= 4 || !allowed_rect || !timing_on || !timing_off)
		return (void) ffsc (__func__);
	
	if (glider_dir == 0 || glider_dir == 3)
	{
		*timing_on = (4 * allowed_rect->left_x) - 6;
		*timing_off = (4 * (allowed_rect->left_x + allowed_rect->width)) + 2;
	}
	else
	{
		*timing_on = -(4 * (allowed_rect->left_x + allowed_rect->width)) - 2;
		*timing_off = -(4 * allowed_rect->left_x) + 6;
	}
}

static __force_inline void Objects_set_glider_progression (Glider *gl, s32 progression)
{
	if (!gl || gl->dir < 0 || gl->dir >= 4)
		return (void) ffsc (__func__);
	
	gl->timing = -progression + (Objects_glider_data [gl->dir] [0].timing_x_dir * (2 * gl->lane));
}

static __force_inline void Objects_shift_glider (Glider *gl, s32 offs_x, s32 offs_y)
{
	if (!gl || gl->dir < 0 || gl->dir >= 4)
		return (void) ffsc (__func__);
	
	gl->lane += (offs_x + (Objects_glider_data [gl->dir] [0].lane_y_dir * offs_y));
	gl->timing += 4 * (Objects_glider_data [gl->dir] [0].timing_x_dir * offs_x);
}

static __force_inline void Objects_mirror_glider (Glider *gl)
{
	if (!gl || gl->dir < 0 || gl->dir >= 4)
		return (void) ffsc (__func__);
	
	s32 lane_offs = (-gl->lane - 1) - gl->lane;
	gl->lane += lane_offs;
	gl->timing += 2 * (Objects_glider_data [gl->dir] [0].timing_x_dir * lane_offs);
}

// The cell list is fully initialized by this call and ocl->cell is set to point to a static const object, so the cell list may not be modified
static __force_inline void Objects_make_glider_obj_cell_list (ObjCellList *ocl, const Glider *gl)
{
	if (!ocl || !gl || gl->dir < 0 || gl->dir >= 4)
	{
		if (ocl)
			ObjCellList_make_zero_size (ocl);
		
		return (void) ffsc (__func__);
	}
	
	s32 timing_phase = gl->timing & 3;
	s32 timing_step = (gl->timing - timing_phase) / 4;
	
	const Objects_GliderData *gl_data = &Objects_glider_data [gl->dir] [timing_phase];
	
	s32 left_x = gl_data->x_offs + (gl_data->timing_x_dir * timing_step);
	s32 top_y = gl_data->y_offs +  (gl_data->lane_y_dir * gl->lane) + (gl_data->timing_y_dir * timing_step);
	
	Rect_make (&ocl->obj_rect, left_x, top_y, 3, 3);
	ocl->cell_cnt = 5;
	ocl->max_cells = 5;
	
	// To initialize a standard ObjCellList with static const cells we must cast away the constness
	ocl->cell = (ObjCellList_Cell *) gl_data->cells;
}
