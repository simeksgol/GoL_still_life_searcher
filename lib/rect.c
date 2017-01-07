// Note that Rects with no area are supported and still retain a position - for example two 0-by-0 Rects can be different

typedef struct
{
	s32 left_x;
	s32 top_y;
	s32 width;
	s32 height;
} Rect;

static __force_inline void Rect_make (Rect *r, s32 left_x, s32 top_y, s32 width, s32 height)
{
	if (!r)
		return (void) ffsc (__func__);
	
	if (width < 0 || height < 0)
	{
		width = 0;
		height = 0;
		ffsc (__func__);
	}
	
	r->left_x = left_x;
	r->top_y = top_y;
	r->width = width;
	r->height = height;
}

static __force_inline int Rect_is_equal (const Rect *rect_1, const Rect *rect_2)
{
	if (!rect_1 || !rect_2)
		return ffsc (__func__);
	
	return (rect_1->left_x == rect_2->left_x && rect_1->top_y == rect_2->top_y && rect_1->width == rect_2->width && rect_1->height == rect_2->height);
}

static __force_inline int Rect_within (const Rect *r, s32 x, s32 y)
{
	return (x >= r->left_x && x < (r->left_x + r->width) && y >= r->top_y && y < (r->top_y + r->height));
}

static __force_inline int Rect_is_subset (const Rect *obj, const Rect *ref)
{
	if (!obj || !ref)
		return ffsc (__func__);
	
	return (obj->left_x >= ref->left_x && obj->left_x + obj->width <= ref->left_x + ref->width && obj->top_y >= ref->top_y && obj->top_y + obj->height <= ref->top_y + ref->height);
}

static __force_inline void Rect_copy (const Rect *src, Rect *dst)
{
	if (!src || !dst)
		return (void) ffsc (__func__);
	
	dst->left_x = src->left_x;
	dst->top_y = src->top_y;
	dst->width = src->width;
	dst->height = src->height;
}

// dst is allowed to alias src. If src have zero area, its position will be ignored and dst will be a 1-by-1 Rect
static __force_inline void Rect_include (const Rect *src, Rect *dst, s32 x, s32 y)
{
	if (!src || !dst)
		return (void) ffsc (__func__);
	
	if (src->width <= 0 || src->height <= 0)
	{
		dst->left_x = x;
		dst->top_y = y;
		dst->width = 1;
		dst->height = 1;
	}
	else
	{
		s32 left_x = lower_of_s32 (src->left_x, x);
		s32 x_off = higher_of_s32 (src->left_x + src->width, x + 1);
		s32 top_y = lower_of_s32 (src->top_y, y);
		s32 y_off = higher_of_s32 (src->top_y + src->height, y + 1);
		
		dst->left_x = left_x;
		dst->top_y = top_y;
		dst->width = x_off - left_x;
		dst->height = y_off - top_y;
	}
}

// dst is allowed to alias either src
static __force_inline void Rect_union (const Rect *src_1, const Rect *src_2, Rect *dst)
{
	if (!src_1 || !src_2 || !dst)
		return (void) ffsc (__func__);
	
	s32 left_x = lower_of_s32 (src_1->left_x, src_2->left_x);
	s32 x_off = higher_of_s32 (src_1->left_x + src_1->width, src_2->left_x + src_2->width);
	s32 top_y = lower_of_s32 (src_1->top_y, src_2->top_y);
	s32 y_off = higher_of_s32 (src_1->top_y + src_1->height, src_2->top_y + src_2->height);
	
	dst->left_x = left_x;
	dst->top_y = top_y;
	dst->width = x_off - left_x;
	dst->height = y_off - top_y;
}

// dst is allowed to alias either src. Returns TRUE of the result is non-empty
static __force_inline int Rect_intersection (const Rect *src_1, const Rect *src_2, Rect *dst)
{
	if (!src_1 || !src_2 || !dst)
		return ffsc (__func__);
	
	s32 left_x = higher_of_s32 (src_1->left_x, src_2->left_x);
	s32 x_off = lower_of_s32 (src_1->left_x + src_1->width, src_2->left_x + src_2->width);
	s32 top_y = higher_of_s32 (src_1->top_y, src_2->top_y);
	s32 y_off = lower_of_s32 (src_1->top_y + src_1->height, src_2->top_y + src_2->height);
	
	dst->left_x = left_x;
	dst->top_y = top_y;
	
	if (x_off < left_x || y_off < top_y)
	{
		dst->width = 0;
		dst->height = 0;
		return FALSE;
	}
	else
	{
		dst->width = x_off - left_x;
		dst->height = y_off - top_y;
		return (x_off > left_x && y_off > top_y);
	}
}

static __force_inline void Rect_add_borders (Rect *r, s32 border_size)
{
	if (!r || border_size < 0)
		return (void) ffsc (__func__);
	
	r->left_x -= border_size;
	r->top_y -= border_size;
	r->width += (2 * border_size);
	r->height += (2 * border_size);
}
