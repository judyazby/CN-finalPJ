all: ./src/client.c ./src/server.c
	gcc ./src/client.c -lpthread -o ./src/client
	gcc ./src/server.c -o ./src/server
	mkdir ./src/history
demo:
	mkdir ./src/history ./src/client01 ./src/client02
	gcc ./src/client.c -lpthread -o ./src/client01/client
	gcc ./src/client.c -lpthread -o ./src/client02/client
	gcc ./src/server.c -o ./src/server
	
cleanDemo: 
	rm -f ./src/server ./src/user.dat
	rm -rf ./src/history ./src/client01 ./src/client02

clean:
	rm -f ./src/server ./src/client ./src/user.dat
	rm -rf ./src/history