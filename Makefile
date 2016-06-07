all: server_chat.o client_chat.o
	gcc server_chat.o -o server_chat -lpthread
	gcc client_chat.o -o client_chat

server_chat.o: server_chat.c
		gcc -c server_chat.c -Wall

client_chat.o: client_chat.c
		gcc -c client_chat.c -Wall

server: server_chat.o
	gcc server_chat.o -o server_chat -lpthread

client: client_chat.o
	gcc client_chat.o -o client_chat

clean:
	rm -vf *.o
	rm -vf server_chat
	rm -vf client_chat

remake: clean all