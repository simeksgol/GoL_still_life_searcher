#ifdef USE_PERF_TIMER

#include <windows.h>

#define PERF_TIMER_TIMER_CNT 16
#define PERF_TIMER_MAX_NAME_SIZE 64

typedef struct
{
	s64 ticks;
	s64 cur_on;
	s64 ops;
	char name [PERF_TIMER_MAX_NAME_SIZE + 1];
} PerfTimer_item;

typedef struct
{
	s64 init_on;
	PerfTimer_item item [PERF_TIMER_TIMER_CNT];
} PerfTimer;


static PerfTimer perf_timer;

static __not_inline void PerfTimer_init (void)
{
	LARGE_INTEGER pc_ticks;
	QueryPerformanceCounter (&pc_ticks);
	s64 ticks = (s64) pc_ticks.QuadPart;
	
	perf_timer.init_on = ticks;
	
	int item_ix;
	for (item_ix = 0; item_ix < PERF_TIMER_TIMER_CNT; item_ix++)
	{
		PerfTimer_item *item = &perf_timer.item [item_ix];
		item->ticks = -1;
		item->cur_on = -1;
		item->ops = 0;
		item->name [0] = '\0';
	}
}

static __not_inline void PerfTimer_set_name (int timer_ix, const char *name)
{
	if (strlen (name) > PERF_TIMER_MAX_NAME_SIZE)
		return (void) ffsc (__func__);
	
	strcpy (perf_timer.item [timer_ix].name, name);
}

static __not_inline void PerfTimer_start (int timer_ix)
{
	PerfTimer_item *item = &perf_timer.item [timer_ix];
	
	if (item->ticks == -1)
		item->ticks = 0;
	
	LARGE_INTEGER pc_ticks;
	QueryPerformanceCounter (&pc_ticks);
	s64 ticks = (s64) pc_ticks.QuadPart;
	
	item->cur_on = ticks;
}

static __not_inline void PerfTimer_stop (int timer_ix)
{
	PerfTimer_item *item = &perf_timer.item [timer_ix];
	
	if (item->cur_on == -1)
		return;
	
	LARGE_INTEGER pc_ticks;
	QueryPerformanceCounter (&pc_ticks);
	s64 ticks = (s64) pc_ticks.QuadPart;
	
	item->ticks += (ticks - item->cur_on);
	item->cur_on = -1;
	item->ops++;
}

static __not_inline void PerfTimer_was_ops (int timer_ix, s64 ops)
{
	perf_timer.item [timer_ix].ops += (ops - 1);
}

static __not_inline void PerfTimer_report (void)
{
	LARGE_INTEGER pc_ticks;
	QueryPerformanceCounter (&pc_ticks);
	s64 ticks = (s64) pc_ticks.QuadPart;
	
	LARGE_INTEGER pc_freq;
	QueryPerformanceFrequency (&pc_freq);
	s64 frequency = (s64) pc_freq.QuadPart;
	
	double ms_per_tick = 1000.0 / (double) frequency;
	s64 elapsed = ticks - perf_timer.init_on;
	
	fprintf (stderr, "\nPerformence timer report:\n");
	fprintf (stderr, "Timer ticks/s: %" PRIi64 "\n", frequency);
	fprintf (stderr, "Elapsed time: %11.3f ms\n", ((double) elapsed) * ms_per_tick);
	
	int item_ix;
	for (item_ix = 0; item_ix < PERF_TIMER_TIMER_CNT; item_ix++)
	{
		PerfTimer_item *item = &perf_timer.item [item_ix];
		
		if (item->ticks == -1)
			continue;
		
		char name_string [PERF_TIMER_MAX_NAME_SIZE + 8];
		name_string [0] = '\0';
		
		if (strlen (item->name) > 0)
			sprintf (name_string, " (%s)", item->name);
		
		if (item->cur_on != -1)
			fprintf (stderr, "   Timer %d%s was not stopped\n", item_ix, name_string);
		else
		{
			fprintf (stderr, "   Timer %d: %11.3f ms%s\n", item_ix, ((double) item->ticks) * ms_per_tick, name_string);
			if (item->ops > 1)
				fprintf (stderr, "%11" PRIu64 " ops, %11.6f ms each\n", item->ops, ((double) item->ticks) * ms_per_tick / (double) item->ops);
		}
	}
}

#else

static __force_inline void PerfTimer_init (void)
{
}

static __force_inline void PerfTimer_set_name (int timer_ix, const char *name)
{
	(void) timer_ix;
	(void) name;
}

static __force_inline void PerfTimer_start (int timer_ix)
{
	(void) timer_ix;
}

static __force_inline void PerfTimer_stop (int timer_ix)
{
	(void) timer_ix;
}

static __force_inline void PerfTimer_was_ops (int timer_ix, s64 ops)
{
	(void) timer_ix;
	(void) ops;
}

static __force_inline void PerfTimer_report (void)
{
}

#endif
