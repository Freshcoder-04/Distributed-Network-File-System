main:
	gcc -g -pthread storageServer.c -o SS
	gcc -g -pthread storageServer.c -o ./Test1/SS
	gcc -g -pthread storageServer.c -o ./Test2/SS
	gcc -g -pthread storageServer.c -o ./Test3/SS