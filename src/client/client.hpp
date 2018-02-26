#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sched.h>
 #include <sys/select.h>
 #include <ifaddrs.h>
 #include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if_link.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/prctl.h>
#include <queue>
#include <vector>

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

#define NUM_CLIENTS 5
#define NUM_SERVERS 3
#define MAX_Q_LEN NUM_CLIENTS*3

#define REQ_TYPE_READ 0
#define REQ_TYPE_WRITE 1

#define STACK_SIZE (1024 * 1024)    /* Stack size for cloned child */
#define IP_FILE_PATH "ipaddrs.txt" 

#define SERV_IP_FILE_PATH "serv_ipaddrs.txt" 

#define NUM_FILES 3

#define MSG_TYPE_REQUEST 0x1000
#define MSG_TYPE_REPLY 0x1001
#define MSG_TYPE_RELEASE 0x1010

#define SERVER_DONE_MSG 0x100
#define SERVER_ERROR 0x101

#define STRING_MAX_LEN 50

using namespace std;

typedef struct payload {
        unsigned long int ts;
        unsigned int cl;
}payload_t;

/** This structure is for both requsts and replies */
typedef struct message_req{
        payload_t pl;
        short int file_num;
        unsigned int req_id;
        int msg_type;
}msg_t;

class Compare
{
        public:
                bool operator() (msg_t& left, msg_t& right) {
                        if(left.pl.ts != right.pl.ts){
                                return (left.pl.ts > right.pl.ts);
                        }
                        return (left.pl.cl > right.pl.cl);
                }
};

typedef struct server_msg {
        int file_num;
        int msg_type;
} srv_msg_t;


static unsigned int req_id;
static const short int port_num = 1145 ;
static const short int serv_port_num = 1345 ;

static int request_type = 0;

static unsigned long int timestamp;

static volatile sig_atomic_t got_SIGCHLD = 0;

/** Priority Queue to store incoming requests to access critical section **/
/*static  msg_t minQ[NUM_FILES][MAX_Q_LEN];
static  int minQ_len[NUM_FILES];*/

priority_queue<msg_t, vector<msg_t>, Compare> minQ[NUM_FILES];

/*
extern void insert(msg_t);
extern msg_t peek(short int);
extern msg_t heap_remove(short int);
*/


 
