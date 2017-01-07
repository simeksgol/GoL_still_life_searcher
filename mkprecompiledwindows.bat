gcc stillcount.c -lm -lgdi32 -o sc128.exe -std=c99 -O3 -Wall -Werror -fmax-errors=2 -Wno-unused-function -Werror -fno-tree-loop-distribute-patterns -march=core2 -D __NO_AVX2
gcc stillcount.c -lm -lgdi32 -o sc256.exe -std=c99 -O3 -Wall -Werror -fmax-errors=2 -Wno-unused-function -Werror -fno-tree-loop-distribute-patterns -march=haswell
