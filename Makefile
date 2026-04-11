all:
	gcc -fPIC -c handler.c db.c
	gcc -shared -o handler.so handler.o db.o

	gcc server.c -o server -ldl
	gcc db_query.c -o query -lgdbm_compat -lgdbm

clean:
	rm -f *.o server handler.so query data.db*