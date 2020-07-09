## Client-Server file transfer application

A simple client-server network application using the sockets API.

ftserver and ftclient can be run on separate hosts and are used to view and transfer files using two connections, a control connection and a data connection.

### ftserver.c

Compile with make

#### Usage:
	./ftserver <server port>

### ftclient.py

#### Usage (order of arguments matters):
	./ftclient.py <fserver host name> <server port> <command> <filename> <data port>

#### Commands:
	`-l` ftserver sends its directory contents to ftclient over the data connection
	`-g <FILENAME>` ftserver validates FILENAME and sends contents of the file over the data connection or error message
