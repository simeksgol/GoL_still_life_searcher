#ifdef USE_GRID_VISUALIZATION

#include <windows.h>

typedef struct
{
	Rect shown_area;
	s32 cell_size;
	s32 graph_bug_redraw_cnt;
	
	HBRUSH background_brush;
	HWND window;
	
	s8 *cell_state;
	s8 *changed_cell;
	
	int is_new_click;
} GridVisualization;

typedef struct
{
	s32 red;
	s32 green;
	s32 blue;
} GridVisualization_colour;

static GridVisualization_colour GridVisualization_state_colour [6] = {{0x50, 0x50, 0x50}, {0xe0, 0xe0, 0xe0}, {0x20, 0x20, 0xa0}, {0x00, 0xc0, 0xf0}, {0xe0, 0x40, 0x40}, {0x50, 0xe0, 0x50}};


static __not_inline void GridVisualization_preinit (GridVisualization *gv)
{
	Rect_make (&gv->shown_area, 0, 0, 0, 0);
	gv->cell_size = 0;
	gv->graph_bug_redraw_cnt = 0;
	gv->background_brush = NULL;
	gv->window = NULL;
	gv->cell_state = NULL;
	gv->changed_cell = NULL;
}

static __not_inline void GridVisualization_close (GridVisualization *gv)
{
	if (!gv)
		return (void) ffsc (__func__);
	
	if (gv->window)
		DestroyWindow (gv->window);
	
	if (gv->background_brush)
		DeleteObject (gv->background_brush);
	
	GridVisualization_preinit (gv);
}

static __not_inline void GridVisualization_redraw (GridVisualization *gv, HDC hdc, int update_all)
{
	if (!gv || !gv->cell_state || !gv->changed_cell || !hdc)
		return (void) ffsc (__func__);
	
	s32 y;
	s32 x;
	for (y = gv->shown_area.top_y; y < gv->shown_area.top_y + gv->shown_area.height; y++)
		for (x = gv->shown_area.left_x; x < gv->shown_area.left_x + gv->shown_area.width; x++)
			if (update_all || (gv->changed_cell [(gv->shown_area.width * (y - gv->shown_area.top_y)) + (x - gv->shown_area.left_x)] > 0))
			{
				int state = gv->cell_state [(gv->shown_area.width * (y - gv->shown_area.top_y)) + (x - gv->shown_area.left_x)];
				
				HGDIOBJ original_brush = SelectObject (hdc, GetStockObject(DC_BRUSH));
				
				SetDCBrushColor (hdc, RGB (GridVisualization_state_colour [state].red, GridVisualization_state_colour [state].green, GridVisualization_state_colour [state].blue));
				
				HGDIOBJ original_pen = SelectObject (hdc, GetStockObject(DC_PEN));
				SetDCPenColor (hdc, RGB (GridVisualization_state_colour [state].red, GridVisualization_state_colour [state].green, GridVisualization_state_colour [state].blue));
				
				s32 cell_left_x = gv->cell_size * (x - gv->shown_area.left_x);
				s32 cell_top_y = gv->cell_size * (y - gv->shown_area.top_y);
				Rectangle (hdc, cell_left_x, cell_top_y, cell_left_x + gv->cell_size - 1, cell_top_y + gv->cell_size - 1);
				
				SelectObject (hdc, original_pen);
				SelectObject (hdc, original_brush);
				
				if (update_all)
					gv->changed_cell [(gv->shown_area.width * (y - gv->shown_area.top_y)) + (x - gv->shown_area.left_x)] = gv->graph_bug_redraw_cnt;
				
				gv->changed_cell [(gv->shown_area.width * (y - gv->shown_area.top_y)) + (x - gv->shown_area.left_x)]--;
			}
}

static LRESULT CALLBACK GridVisualization_window_procedure (HWND window, UINT message, WPARAM wp, LPARAM lp)
{
	GridVisualization *gv = NULL;
	
	if (message == WM_CREATE)
	{
        gv = (GridVisualization *) ((const CREATESTRUCT *) lp)->lpCreateParams;
        SetWindowLongPtr (window, GWLP_USERDATA, (LONG_PTR) gv);
	}
	else
	    gv = (GridVisualization *) GetWindowLongPtr (window, GWLP_USERDATA);
	
	if (message == WM_LBUTTONDOWN)
		gv->is_new_click = TRUE;
	else if (message == WM_PAINT)
	{
		PAINTSTRUCT ps;
		
		HDC hdc = BeginPaint (window, &ps);
		GridVisualization_redraw (gv, hdc, TRUE);
		EndPaint(window, &ps);
	}
	else
		return DefWindowProc(window, message, wp, lp);
	
	return 0;
}

static __not_inline int GridVisualization_create (GridVisualization *gv, const char *window_name, const Rect *shown_area, s32 cell_size, s32 graph_bug_redraw_cnt)
{
	if (!gv)
		return ffsc (__func__);
	
	GridVisualization_preinit (gv);
	
	if (!window_name || !shown_area || cell_size < 1)
		return ffsc (__func__);
	
	s32 screen_height = GetSystemMetrics (SM_CYSCREEN);
	s32 screen_width = GetSystemMetrics (SM_CXSCREEN);
	
	Rect_copy (shown_area, &gv->shown_area);
	gv->cell_size = cell_size;
	gv->graph_bug_redraw_cnt = graph_bug_redraw_cnt;
	
	s32 max_height = screen_height - 128;
	if (gv->shown_area.height > max_height)
	{
		gv->shown_area.top_y += ((gv->shown_area.height - max_height) / 2);
		gv->shown_area.height = max_height;
	}
	
	s32 max_width = screen_width - 128;
	if (gv->shown_area.width > max_width)
	{
		gv->shown_area.left_x += ((gv->shown_area.width - max_width) / 2);
		gv->shown_area.width = max_width;
	}
	
	while (gv->cell_size * gv->shown_area.height > max_height || gv->cell_size * gv->shown_area.width > max_width)
		gv->cell_size--;
	
	RECT window_rect;
	window_rect.left = screen_width - (gv->cell_size * gv->shown_area.width) - 32;
	window_rect.top = 64;
	window_rect.right = screen_width - 32;
	window_rect.bottom = 64 + (gv->cell_size * gv->shown_area.height);
	
	HINSTANCE instance_handle = GetModuleHandle (NULL);
	HANDLE gv_cursor = LoadImage (NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
	gv->background_brush = CreateSolidBrush (RGB (0x30, 0x30, 0x30));
	const char *gv_class_name = "GridVisualization_window_class";
	
	gv->cell_state = malloc (gv->shown_area.height * gv->shown_area.width);
	gv->changed_cell = malloc (gv->shown_area.height * gv->shown_area.width);
	
	if (!AdjustWindowRect (&window_rect, WS_OVERLAPPEDWINDOW, FALSE) || !instance_handle || !gv_cursor || !gv->background_brush || !gv->cell_state || !gv->changed_cell)
	{
		GridVisualization_close (gv);
		fprintf (stderr, "Failed to open visualization window\n");
		return FALSE;
	}
	
	memset (gv->cell_state, 0, gv->shown_area.height * gv->shown_area.width);
	memset (gv->changed_cell, gv->graph_bug_redraw_cnt, gv->shown_area.height * gv->shown_area.width);
	
	WNDCLASSEX gv_class = {sizeof (WNDCLASSEX), CS_NOCLOSE, &GridVisualization_window_procedure, 0, 0, instance_handle, NULL, gv_cursor, gv->background_brush, NULL, gv_class_name, NULL};
	if (!RegisterClassEx (&gv_class))
	{
		GridVisualization_close (gv);
		fprintf (stderr, "Failed to open visualization window\n");
		return FALSE;
	}
	
	gv->window = CreateWindowEx (0, gv_class_name, window_name, WS_OVERLAPPEDWINDOW, window_rect.left, window_rect.top, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top,
			NULL, NULL, instance_handle, gv);
	if (!gv->window)
	{
		GridVisualization_close (gv);
		fprintf (stderr, "Failed to open visualization window\n");
		return FALSE;
	}
	
	ShowWindow (gv->window, SW_SHOWNA);
	UpdateWindow (gv->window);
	
	return TRUE;
}

static __not_inline void GridVisualization_update (GridVisualization *gv)
{
	if (!gv || !gv->window || !gv->cell_state || !gv->changed_cell)
		return (void) ffsc (__func__);
	
	MSG message;
	while (PeekMessage (&message, gv->window, 0, 0, PM_REMOVE))
	{
		TranslateMessage (&message);
		DispatchMessage (&message);
	}
	
	HDC hdc = GetDC (gv->window);
	GridVisualization_redraw (gv, hdc, FALSE);
	ReleaseDC (gv->window, hdc);
}

static __not_inline void GridVisualization_set_cell (GridVisualization *gv, s32 x, s32 y, int state)
{
	if (!gv || !gv->window || !gv->cell_state || !gv->changed_cell)
		return (void) ffsc (__func__);
	
	if (!Rect_within (&gv->shown_area, x, y))
		return;
	
	int old_state = gv->cell_state [(gv->shown_area.width * (y - gv->shown_area.top_y)) + (x - gv->shown_area.left_x)];
	
	if (state != old_state)
	{
		gv->cell_state [(gv->shown_area.width * (y - gv->shown_area.top_y)) + (x - gv->shown_area.left_x)] = state;
		gv->changed_cell [(gv->shown_area.width * (y - gv->shown_area.top_y)) + (x - gv->shown_area.left_x)] = gv->graph_bug_redraw_cnt;
	}
}

static __not_inline void GridVisualization_wait_for_click (GridVisualization *gv)
{
	if (!gv || !gv->window || !gv->cell_state || !gv->changed_cell)
		return (void) ffsc (__func__);
	
	gv->is_new_click = FALSE;
	while (TRUE)
	{
		GridVisualization_update (gv);
		if (gv->is_new_click)
			break;
		
		WaitMessage ();
	}
}

#else

typedef struct
{
} GridVisualization;

static __force_inline void GridVisualization_close (GridVisualization *gv)
{
	(void) gv;
}

static __force_inline int GridVisualization_create (GridVisualization *gv, const char *window_name, const Rect *shown_area, s32 cell_size, s32 graph_bug_redraw_cnt)
{
	(void) gv;
	(void) window_name;
	(void) shown_area;
	(void) cell_size;
	(void) graph_bug_redraw_cnt;
	
	return TRUE;
}

static __force_inline void GridVisualization_update (GridVisualization *gv)
{
	(void) gv;
}

static __force_inline void GridVisualization_set_cell (GridVisualization *gv, s32 x, s32 y, int state)
{
	(void) gv;
	(void) x;
	(void) y;
	(void) state;
}

static __force_inline void GridVisualization_wait_for_click (GridVisualization *gv)
{
	(void) gv;
}

#endif
