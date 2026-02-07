all: admiral

admiral:
	gcc -Wall -Wextra -pedantic -std=gnu99 -g src/main.c src/libstmp.c src/stmp.c -o build/admiral

clean:
	rm admiral

