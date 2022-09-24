all: lets-talk.c
	gcc -pthread -g -o lets-talk lets-talk.c list.c

Valgrind: lets-talk.c
	valgrind --leak-check=full --show-leak-kinds=all  ./lets-talk 3000 localhost 3001

clean:
	$(RM) lets-talk