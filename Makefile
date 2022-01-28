a.out: microshell.c
	clang -Wall -Wextra -Werror -g3 $^ -o $@
