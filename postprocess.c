#include <stdlib.h>
#include <inttypes.h>
#include <memory.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "lib/lib.c"

#define MAX_FILENAME_SIZE 256
#define MAX_LINE_LENGTH 1024


static __not_inline int make_filename (const char *template, s32 cur_file_number, s32 template_entry, s32 template_size, char *filename)
{
	strcpy (filename, template);
	
	if (cur_file_number >= 0 && template_entry >= 0)
	{
		char num_str [16];
		sprintf (num_str, "%d", cur_file_number);
		
		if (strlen (num_str) > template_size)
			return FALSE;
		
		strcpy (filename + template_entry + template_size - strlen (num_str), num_str);
		strcpy (filename + template_entry + template_size, template + template_entry + template_size);
	}
	
	return TRUE;
}

static __not_inline int read_line (FILE *f, char *buf)
{
	if (!fgets (buf, MAX_LINE_LENGTH, f))
	{
		fclose (f);
		return 0;
	}
	
	if (strlen (buf) >= MAX_LINE_LENGTH - 4)
	{
		fprintf (stderr, "Line buffer overflow\n");
		fclose (f);
		return -1;
	}
	
	return 1;
}

static __not_inline int post_process (const char *in_template, s32 in_template_entry, s32 in_template_size, s32 in_first_number, s32 in_last_number,
		const char *out_template, s32 out_template_entry, s32 out_template_size, s64 lines_per_out_file)
{
	FILE *in_file = NULL;
	s32 cur_in_file_number = in_first_number;
	char in_filename [MAX_FILENAME_SIZE + 1];
	
	FILE *out_file = NULL;
	s32 cur_out_file_number = 0;
	char out_filename [MAX_FILENAME_SIZE + 1];
	
	s64 pattern_cnt = 0;
	s64 lines_in_cur_out_file = 0;
	
	while (TRUE)
	{
		char line_buf [MAX_LINE_LENGTH + 1];
		
		if (in_file)
		{
			int result = read_line (in_file, line_buf);
			if (result == -1)
			{
				if (out_file)
					fclose (out_file);
				
				return FALSE;
			}
			if (result == 0)
			{
				in_file = NULL;
				cur_in_file_number++;
			}
			if (result == 1)
				pattern_cnt++;
		}
		
		if (!in_file)
		{
			if (cur_in_file_number > in_last_number)
			{
				if (out_file)
					fclose (out_file);
				
				fprintf (stderr, "Done: %" PRIi64 " patterns found\n", pattern_cnt);
				return TRUE;
			}
			
			make_filename (in_template, cur_in_file_number, in_template_entry, in_template_size, in_filename);
			in_file = fopen (in_filename, "r");
			if (!in_file)
			{
				if (out_file)
					fclose (out_file);
				
				fprintf (stderr, "Failed to open in file %s\n", in_filename);
				return FALSE;
			}
			
			continue;
		}
		
		if (!out_file || (lines_per_out_file >= 0 && lines_in_cur_out_file >= lines_per_out_file))
		{
			if (out_file && (lines_per_out_file >= 0 && lines_in_cur_out_file >= lines_per_out_file))
			{
				fclose (out_file);
				cur_out_file_number++;
				lines_in_cur_out_file = 0;
			}
			
			if (!make_filename (out_template, cur_out_file_number, out_template_entry, out_template_size, out_filename))
			{
				fclose (in_file);
				fprintf (stderr, "Overflow in out file template\n");
				return FALSE;
			}
			
			out_file = fopen (out_filename, "w");
			if (!out_file)
			{
				fclose (in_file);
				fprintf (stderr, "Failed to open out file %s\n", out_filename);
				return FALSE;
			}
		}
		
		s32 line_size = strlen (line_buf);
		s32 written_size = fwrite (line_buf, sizeof (char), line_size, out_file);
		if (written_size < line_size)
		{
			fclose (in_file);
			fclose (out_file);
			fprintf (stderr, "Write error on out file %s\n", out_filename);
			return FALSE;
		}
		
		lines_in_cur_out_file++;
	}
}

static __not_inline int verify_template (const char *template, int must_be_template, char *filename, s32 *template_entry, s32 *template_size)
{
	strcpy (filename, template);
	*template_entry = 0;
	*template_size = 0;
	
	s32 template_ix = 0;
	while (template [template_ix] != '\0')
	{
		if (template [template_ix] == '#')
			filename [template_ix] = '0';
		template_ix++;
	}
	
	template_ix = 0;
	s32 hash_on = -1;
	s32 hash_off = -1;
	
	while (TRUE)
	{
		char c = template [template_ix];
		
		if (c != '#')
			if (hash_on >= 0 && hash_off < 0)
				hash_off = template_ix;
		
		if (c == '\0')
		{
			if (must_be_template && hash_off < 0)
				return FALSE;
			
			*template_entry = hash_on;
			*template_size = hash_off - hash_on;
			return TRUE;
		}
		
		if (c == '#')
		{
			if (hash_off >= 0)
				return FALSE;
			if (hash_on < 0)
				hash_on = template_ix;
		}
		
		template_ix++;
	}
}

static __not_inline int main_do (int argc, const char *const *argv)
{
	int usage_fail = FALSE;
	
	char in_template [MAX_FILENAME_SIZE + 1];
	s32 in_template_entry;
	s32 in_template_size;
	u32 cl_in_first_number;
	u32 cl_in_last_number;
	
	char out_template [MAX_FILENAME_SIZE + 1];
	s32 out_template_entry;
	s32 out_template_size;
	u64 cl_lines_per_out_file;
	s64 lines_per_out_file = -1;
	
	if (argc < 5 || argc > 6 || strlen (argv [1]) >= MAX_FILENAME_SIZE || strlen (argv [4]) >= MAX_FILENAME_SIZE)
		usage_fail = TRUE;
	
	if (!usage_fail && !verify_template (argv [1], TRUE, in_template, &in_template_entry, &in_template_size))
		usage_fail = TRUE;
	
	if (!usage_fail && (!str_to_u32 (argv [2], &cl_in_first_number) || !str_to_u32 (argv [3], &cl_in_last_number)))
		usage_fail = TRUE;
	
	if (!usage_fail && (digits_in_u32 (cl_in_last_number) > in_template_size || cl_in_first_number > cl_in_last_number))
		usage_fail = TRUE;
	
	if (!usage_fail && !verify_template (argv [4], FALSE, out_template, &out_template_entry, &out_template_size))
		usage_fail = TRUE;
	
	if (!usage_fail && argc > 5)
	{
		if (!str_to_u64 (argv [5], &cl_lines_per_out_file) || cl_lines_per_out_file == 0)
			usage_fail = TRUE;
		else
			lines_per_out_file = cl_lines_per_out_file;
	}
	
	if (!usage_fail && ((out_template_entry >= 0) != (lines_per_out_file >= 0)))
		usage_fail = TRUE;
	
	if (usage_fail)
	{
		fprintf (stderr, "USAGE: pp <in template> <first number> <last number>\n");
		fprintf (stderr, "          <out template> [<lines per out file>]\n");
		fprintf (stderr, "where a template could be \"28_bits_strict_subset_####_of_1024.txt\"\n");
		return FALSE;
	}
	
	if (!post_process (in_template, in_template_entry, in_template_size, (s32) cl_in_first_number, (s32) cl_in_last_number, out_template, out_template_entry, out_template_size, lines_per_out_file))
		return FALSE;
	
	return TRUE;
}

int main (int argc, const char *const *argv)
{
	if (!main_do (argc, argv))
		return EXIT_FAILURE;
	
	return EXIT_SUCCESS;
}
