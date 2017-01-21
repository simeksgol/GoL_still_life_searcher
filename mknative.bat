gcc stillcount.c -lm -lgdi32 -o sc128.exe -std=c99 -O3 -Wall -Wextra -Werror -fmax-errors=2 -Wno-unused-function -fno-tree-loop-distribute-patterns -march=native -D __NO_AVX2
gcc stillcount.c -lm -lgdi32 -o sc256.exe -std=c99 -O3 -Wall -Wextra -Werror -fmax-errors=2 -Wno-unused-function -fno-tree-loop-distribute-patterns -march=native
