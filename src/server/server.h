#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <sched.h>
 #include <sys/select.h>
 #include <ifaddrs.h>
 #include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_link.h>
#include <errno.h>
#include <time.h>

#define SERVER_DONE_MSG 0x100
#define SERVER_ERROR 0x101
#define STRING_MAX_LEN 50

#define REQ_TYPE_READ 0
#define REQ_TYPE_WRITE 1

#define NUM_CLIENTS 3

#define SERV_IP_FILE_PATH "../serv_ipaddrs.txt" 
#define NUM_FILES 3

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

static const short int serv_port_num = 1345 ;
typedef struct server_msg {
        int file_num;
        int msg_type;
} srv_msg_t;

