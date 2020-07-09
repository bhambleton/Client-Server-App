#!/usr/bin/python3

"""
	 Author: Brian Hambleton (hambletb)
	   Date: 12 August 2019
 	Program: ftclient (ftclient.py)
    Description: Connects to a listen socket on the supplied host and port combo and can receive
    		 a text file over the connection

	Usage: ./ftclient.py <SERVER_HOST> <SERVER_PORT> <COMMAND> <FILENAME> <DATA_PORT>

	socket logic obtained from https://docs.python.org/2/library/socket.html#socket-objects
"""

import sys
import socket
import os

"""
	   Description:	Checks command-line arguments
	    Parameters:	receives a list args
	       Returns:	none
	Pre-Conditions:	args is not null
       Post-Conditions:	program exits if correct arguments are not supplied
"""
def argCheck(args):
	if len(args) < 5 or len(args) > 6:
		print("Insufficient Number of Arguments\nCorrect Usage: ./ftclient.py <SERVER_HOST> <SERVER_PORT> <COMMAND> <FILENAME> <DATA_PORT>", file=sys.stderr)
		sys.exit(2)
	elif args[3] == "-g" and len(args) != 6:
		print("Insufficient Number of Arguments\nCorrect Usage: ./ftclient.py <SERVER_HOST> <SERVER_PORT> <COMMAND> <FILENAME> <DATA_PORT>", file=sys.stderr)
		sys.exit(2)
	elif int(args[2]) >= 65536:
		print("Connection port number too high", file=sys.stderr)
		sys.exit(2)

"""
	   Description:	Class to handle 2 socket connections and receive file through socket
	    Parameters:	receives command-line arguments in the list args
	       Returns:	none
	Pre-Conditions:	args is not null and contains hostname and port of an
			open socket connection
       Post-Conditions:	file is received and saved in current directory or
       			connection's directory contents are printed to the screen
"""
class ftClient:
	# Initializer, receives validated command-line arguments
	def __init__(self, args):
		if args[1] == "flip1" or args[1] == "flip2" or args[1] == "flip3":
			self.controlHost = args[1] + ".engr.oregonstate.edu"
		else:
			self.controlHost = args[1]
		
		self.controlPort = int(args[2])
		self.command = args[3]
		
		if len(args) == 6 and args[3] == "-g":
			self.dataPort = int(args[5])
			self.filename = args[4]
		elif len(args) == 5 and args[3] == "-l":
			self.dataPort = int(sys.argv[4])
			self.filename = None
	def setupConnection(self):
		self.controlSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.controlSocket.connect((self.controlHost, self.controlPort))
		if self.controlSocket is None:
			print("Client could not connect", file=sys.stderr)
			sys.exit(1)
	def sendCommand(self):
		if self.filename is not None:
			WRITE_DATA = str(self.dataPort) + ' ' + self.command + ' ' + self.filename
		else:
			WRITE_DATA = str(self.dataPort) + ' ' + self.command

		self.controlSocket.sendall(WRITE_DATA.encode('utf-8'))
		results = self.controlSocket.recv(32)
		return results.decode('utf-8')
	def setupDataSocket(self):
		DATA_HOST = ''
		self.dataListenSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self.dataListenSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		self.dataListenSocket.bind((DATA_HOST, self.dataPort))
		self.dataListenSocket.listen(1)
	def executeCommand(self):
		dataSocket, connection_address = self.dataListenSocket.accept()
		print('Obtained data connection from ', connection_address)
		# receive data
		if self.command == "-l":
			rec_data = dataSocket.recv(1023)
			print((rec_data.decode('utf-8')).replace(" ", "\n"))
		elif self.command == "-g":
			rec_control = self.controlSocket.recv(32)
			if rec_control:
				print((rec_control.decode('utf-8')))
			else:
				self.receiveFile(dataSocket)
		dataSocket.close()
	def receiveFile(self, dataSocket):
		while os.path.isfile(self.filename) is True:
			print("ERROR file already exists")
			self.filename = input("Enter new file name with file extension: ")
		
		newFile = open(self.filename, 'a+')
		
		while True:
			data_chunk = dataSocket.recv(1024)
			if data_chunk and data_chunk != 0:
				newFile.write(data_chunk.decode('utf-8')) #write contents of 
				data_chunk = None
			else:
				break
		print("File transfer complete.")
	def run(self):
		self.setupConnection()
		response = self.sendCommand()

		if response != "0":
			print(response)
		else:
			self.setupDataSocket()
			self.executeCommand()
			self.dataListenSocket.close()

#
# End ftClient Class declarations
#

argCheck(sys.argv)
ftClient = ftClient(sys.argv)
ftClient.run()

