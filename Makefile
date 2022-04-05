all: client server

client:
	gcc -o client client.c

server:
	gcc -o server server2.0.c thpool.c -pthread