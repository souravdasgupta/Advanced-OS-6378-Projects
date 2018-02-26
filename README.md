# Advanced-OS-6378-Projects
Project to implement Lamport Mutual Exclusion algorithm between clients which act like peers. The servers perfrom read and write to files. These files are considered as critical resources


## Steps to Build the project:
Inside the src/ folder, there are two folders: server/ and client/, which corresponds to the server and the client respectively. To build the server the following commands are to be given

$cd server/
$make server

Similarly for client:
$cd client/
$make client

This will generate the executables in the respective folders.


## Steps to Run the program:
After the exectables are built. The servers and the client exectables are to be copied to a total of 8 different machines (5 clients and 3 servers):

### The command to run the client:
$./client <client_num>

client_num is a value that denotes the client number. It <b> must </b> be a number from 1 to 5.

### The command to run the server
Before running the server, 3 folders, one corresponding to each server must be created in the file system. For the sake of example, let them be srv1/, srv2/ and srv3/. All these 3 folders must be in the same location as the client. One server executable generated in the previous step must be copied into each folder.
For running the server:

$cd srv<1, 2 or 3>
$./server

## Important points to remember

1. Each execution of the servers and the clients will generate two txt files in the root folder (the folder where client is present): ipaddrs.txt and serv_ipaddrs.txt. Prior to next execution, these two files must be manually deleted.
2. Servers must be executed first, i.e, before the execution of clients. And the clients must be executed in order. This means, first ./client 1 must run, then ./client 2 (on a different machine ofcourse), then ./client 3 and so on.
