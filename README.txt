This program counts and creates databases for strict still lifes and pseudo still lifes in Conway's Game of Life

Should work well on Windows and Linux. Try the Windows version if you can, it has a nice visualization window!

Windows executables are included, run from a DOS window. Use sc256 if you have a Haswell or later CPU or sc128 if not.

For Linux, compile with "mknative".

Usage is:

> sc128 <command> <min on cells> <max on cells> [<selected subset>]
where <command> is "w" to write database files, or "c" to only count still lifes.

Pick a suitable <max on cells>. You can generate databases for lower bit counts in the same run in virtually no extra time, for example:

> sc128 w 4 22

will generate 38 different files in the current directory, one for each bit count, and one each for strict and pseudo still lifes.

The program will keep an eye open for any triple or quad pseudo still life, and report them to stdout (the console) if any are found. The smallest known of these types has 32 on-cells, but there could possibly be smaller ones.

To redirect stdout to a file, use for example:

> sc128 w 4 22 >out.txt

There is also the possibility to search a subset of the search space.

There is currently a fixed set of 100 subsets that require about an equal amount of time to complete. They are numbered from 0 to 99, for example:

> sc128 w 31 32 91 >out.txt

will generate database files such as "32_bits_strict_subset_0091_of_0100.txt". The running time for searching one subset to 32 bits should be around a day. You can of course start one search for each CPU core of your computer in different subsets at the same time.

Note that it is normal for subset number 2 to contain no strict still lifes, only pseudo still lifes, so the corresponding database file will be empty.

The memory requirements of the program are very low, it doesn't need to keep track of which still lifes it has encountered so far, so there's no reason why a search can't keep running for weeks on end.

More information is available in this forum thread: http://www.conwaylife.com/forums/viewtopic.php?f=9&t=44&start=50#p38810


Also included a post processing program that reads a series of numbered text files and outputs them to either a single file or to a series of numbered output files with a predefined number of text lines in each.

Use mkpp to compile this into pp on Linux

USAGE: pp <in template> <first number> <last number>
          <out template> [<lines per out file>]
where a template could be "28_bits_strict_subset_####_of_1024.txt"

This will also print the total number of processed lines to the console
