CC = clang

crestgen: src/crestgen.c
	$(CC) src/crestgen.c -o bin/crestgen

test_server: crestgen test/server.c
	./bin/crestgen test/routes > test/routes.c
	$(CC) -Isrc src/crest.c test/server.c test/routes.c -o bin/test_server
	rm -f test/routes.c
