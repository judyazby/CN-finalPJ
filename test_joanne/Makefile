all: client.c server.c
	gcc client.c -lpthread -o client
	gcc server.c -o server
	mkdir history
demo:
	mkdir history client01 client02
	gcc client.c -lpthread -o ./client01/client
	gcc client.c -lpthread -o ./client02/client
	gcc server.c -o server
	
cleanDemo: 
	rm -f server
	rm -rf history client01 client02

clean:
	rm -f server client
	rm -rf history