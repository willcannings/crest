CC = clang

crestgen: src/crestgen.c
	$(CC) src/crestgen.c -o bin/crestgen
