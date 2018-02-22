#include "client.hpp"

int childFunc(void *);
static void child_sig_handler(int ) ;
int start_child_process();
int connect_to_server(char *, int);
char *get_peer_IP(int);
int execute_critical_section(int[], int);
int enter_and_exec_cs(int , int , int , int[], msg_t , int[]);
void print_queue(int );

int main(int argc, const char *argv[]) {
                
        int i, j, sock_srv[NUM_SERVERS], sock_self_server = -1, yes=1, fdmax = 0, client_num, num_clients_left = NUM_CLIENTS-1, newfd, s, ret, replycount = 0, is_waiting_for_reply = 0, request_type = 0, peer_sockets[NUM_CLIENTS-1], child_pid = -1, num_serv_available = 0;
        fd_set master; 
        fd_set read_fds;
        struct sockaddr_in my_addr, peer_addr;
        struct sockaddr_in remoteaddr; 
        sigset_t sigmask, empty_mask;
        struct sigaction sa1, sa2;
        socklen_t addrlen;
        FILE *file = NULL;
        msg_t sent_request;
        struct ifaddrs *ifaddr, *ifa;
        char ipaddr[16];
        char host[NI_MAXHOST];
        priority_queue<msg_t, vector<msg_t>, Compare> release_queue[NUM_FILES];
        
        addrlen = sizeof(struct sockaddr_in);
        
        if(argc < 2) {
                printf("Usage: client <client_num>\n client_num is a value from [1-5]\n");
                exit(EXIT_FAILURE);
        }
        
        //memset(is_waiting_for_reply, 0 , NUM_FILES * sizeof(int));

          /** Identify which client this is **/
        client_num = atoi(argv[1]);
        printf("Executing client %d\n", client_num);
        
        if(client_num < 1 || client_num > NUM_CLIENTS) {
                printf("Client number should be a value between 1 and 5\n");
                 exit(EXIT_FAILURE);
        }
        
        /** Connect with servers **/
         file = fopen ( SERV_IP_FILE_PATH, "a+" );
         if (!file) {
                perror( SERV_IP_FILE_PATH);
                exit(EXIT_FAILURE);
        }
        i=0;
        while(fgets ( ipaddr, sizeof (ipaddr), file ) || i < NUM_SERVERS){
                if(ipaddr[strlen(ipaddr) - 1] == '\n') {
                        /* Remove the trailing newline character */
                        ipaddr[strlen(ipaddr) - 1] = '\0';
                }
                if((sock_srv[i++] = connect_to_server(ipaddr, serv_port_num)) < 0) {
                        printf("Error in connection with server %d, exiting...", i);
                        exit(EXIT_FAILURE);
                }
        }
        fclose(file);
        file = NULL;
        
        if(i < NUM_SERVERS){
                printf("Error:: One or the other servers might not be running\n");
        }
        num_serv_available = i;
        
       /** Block SIGCHLD as of now */
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGCHLD);
        sigaddset(&sigmask, SIGHUP);
        if (sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
                perror("sigprocmask()::");
                exit(EXIT_FAILURE);
        }
        
         /** Set up Signal Handler From Child **/
        sa1.sa_flags = 0;
        sa1.sa_handler = child_sig_handler;
        sigemptyset(&sa1.sa_mask);
        if (sigaction(SIGCHLD, &sa1, NULL) == -1) {
                perror("sigaction()::");
                exit(EXIT_FAILURE);
        }
        sigemptyset(&empty_mask);
        
        FD_ZERO(&read_fds);
        FD_ZERO(&master);
        
        /** Get own IP adress **/
         if (getifaddrs(&ifaddr) == -1) {
                perror("getifaddrs");
                exit(EXIT_FAILURE);
        }
        
        for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if((ifa->ifa_addr->sa_family == AF_INET) && strncmp(ifa->ifa_name, "lo", 2)) {
                        /** Not loopback/assuming only one AF_INET interface other than lo **/
                        s = getnameinfo(ifa->ifa_addr,  sizeof(struct sockaddr_in) , host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                        printf("IP address of this client is :%s\n", host);
                        break;
                }
        }
        freeifaddrs(ifaddr);
        
        /**  Connect to other clients **/
        file = fopen ( IP_FILE_PATH, "a+" );
        if (!file) {
                perror( IP_FILE_PATH);
                exit(EXIT_FAILURE);
        }
        while(fgets ( ipaddr, sizeof (ipaddr), file )){
                /* We have read one ip address to which to connect as client */
                
                if(ipaddr[strlen(ipaddr) - 1] == '\n') {
                        /* Remove the trailing newline character */
                        ipaddr[strlen(ipaddr) - 1] = '\0';
                }
                printf("Connecting to %s\n",ipaddr);
                if (( ret = connect_to_server(ipaddr, port_num)) < 0 ) {
                       printf("Connection to node : %s failed\n", ipaddr);
                       exit(EXIT_FAILURE);
               }
               FD_SET(ret, &master);
               fdmax = max(fdmax, ret);
               peer_sockets[NUM_CLIENTS-1-num_clients_left] = ret;
               printf("Added socket:%d at position %d of peer_sockets[]\n", ret, NUM_CLIENTS-1-num_clients_left);
               num_clients_left--;
        }
        if(fprintf(file, "%s\n", host) < 0){
                perror("fprintf()::");
                exit(EXIT_FAILURE);
        }
        fclose(file);
        
        /** Seed random number generator */
        srand(time(NULL));
        
        /** Create socket for listening to incoming connection; typical socket server code **/
        if(num_clients_left) {
                if((sock_self_server = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
                        perror("socket()::");
                        exit(EXIT_FAILURE);
                }

                if (setsockopt(sock_self_server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
                        perror("setsockopt()::");
                        exit(EXIT_FAILURE);
                }
                my_addr.sin_family = AF_INET;
                my_addr.sin_port = htons(port_num);
                my_addr.sin_addr.s_addr = inet_addr(host);
                memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

                if( bind(sock_self_server, (struct sockaddr *)&my_addr, sizeof my_addr) < 0) {
                        perror("bind()::");
                        close(sock_self_server);
                        exit(EXIT_FAILURE);
                }

                if (listen(sock_self_server, 10) == -1) {
                        perror("listen()");
                        close(sock_self_server);
                        exit(EXIT_FAILURE);
                }
        
                FD_SET(sock_self_server, &master);
                fdmax = max(fdmax, sock_self_server);
        } else {
                printf("Connected to all peers...\n");
                child_pid = start_child_process();
                
        }/* end of if(!num_clients_left) */
        
       printf("Done all initializations.. Waiting for connections/messages\n");
        while (1) {
                                
                printf("Will sleep again..\n");
                read_fds = master;               
                /** Polling instead of starting a new thread, pselect() causes SIGCHLD to unblock, and block again once pselect() returns **/
                if( pselect(fdmax+1, &read_fds, NULL, NULL, NULL, &empty_mask) == -1) {
                        if(errno != EINTR){
                                perror("select()::");
                                close(sock_self_server); 
                                exit(EXIT_FAILURE);
                        }
                }

               
                if (got_SIGCHLD) {
                        
                        got_SIGCHLD = 0;
                        printf("Got REQUEST message from child\n");
                       
                        sent_request.req_id = req_id++;
                        sent_request.file_num = ( rand() % NUM_FILES );
                        replycount = (1<< (NUM_CLIENTS)) - 1;
                        replycount &= ~(1<<(client_num - 1));
                        timestamp += client_num;
                        sent_request.pl.ts = timestamp;
                        sent_request.pl.cl = client_num;
                        sent_request.msg_type = MSG_TYPE_REQUEST;
                        
                        is_waiting_for_reply  = 1;
                        
                        /* Add request to client's own queue */
                        /*insert(sent_request)*/minQ[sent_request.file_num].push(sent_request);
                        /* Send request to all other clients */
                        for(i = 0; i < NUM_CLIENTS-1; i++) {
                                printf("Sending REQUEST to enter CS to IP %s\n", get_peer_IP(peer_sockets[i]));
                                if(send(peer_sockets[i], &sent_request, sizeof(msg_t), 0) < 0){
                                        perror("send() called to send request to peers::");
                                         exit(EXIT_FAILURE);
                                }
                        }
                        continue;
               }
               
               /** Check all the file descriptors available for read **/
               for(i = 0; i <= fdmax; i++) {
                       if(FD_ISSET(i, &read_fds)) {
                               if( i == sock_self_server && num_clients_left) {
                                       /** We have an incoming connection from another client **/
                                       
                                        if ((newfd = accept(sock_self_server, (struct sockaddr *)&remoteaddr, &addrlen)) == -1) {
                                                perror("accept()::");
                                        } else {
                                                FD_SET(newfd, &master); 
                                                fdmax = max(newfd, fdmax);
                                                peer_sockets[NUM_CLIENTS - 1 - num_clients_left] = newfd;
                                                num_clients_left--;
                                                 if(!num_clients_left){
                                                        /** Start child process **/
                                                        printf("All the other peer clients have connected\n");
                                                        child_pid = start_child_process();
                                                        close(sock_self_server);
                                                        FD_CLR(sock_self_server, &master);
                                                }
                                        }
                                        printf("Client %d: new connection from %s on socket %d\n", client_num, inet_ntoa(remoteaddr.sin_addr), newfd);
                               } else {
                                       int num_bytes = 0;
                                       msg_t rcv_msg;
                                       
                                       num_bytes = recv(i, &rcv_msg, sizeof(msg_t), MSG_WAITALL) ;
                                       
                                       switch (num_bytes) {
                                                case -1:
                                                        perror("recv() from client:: ");
                                                        exit(EXIT_FAILURE);
                                                case 0:
                                                        printf("One of the peer has disconnected\n");
                                                        print_queue(0); print_queue(1); print_queue(2); 
                                                        /* May be close all sockets and go home (TODO)*/
                                                        return -1;
                                                case sizeof(msg_t):
                                                        break;
                                                default:
                                                        printf("ERROR::Incomplete message received. This should not happen\n");
                                                        exit(EXIT_FAILURE);
                                       }
                                       
                                       timestamp = max(timestamp + client_num , rcv_msg.pl.ts);
                                       
                                       if(is_waiting_for_reply &&  (sent_request.file_num == rcv_msg.file_num) && ((sent_request.pl.ts < rcv_msg.pl.ts) || ( (sent_request.pl.ts == rcv_msg.pl.ts) && (client_num < rcv_msg.pl.cl)  )  )  ){
                                                //replycount++;
                                                replycount &= ~(1<<(rcv_msg.pl.cl - 1));
                                                printf("Received a reply from client %d for file %d :: replycount = %d\n", rcv_msg.pl.cl, rcv_msg.file_num, replycount);
                                                if(!replycount /*== NUM_CLIENTS - 1*/) {
                                                        printf("Got the necessary replies\n");
                                                }
                                                if((!replycount /*== NUM_CLIENTS - 1*/) && /*(peek(sent_request.file_num).pl.cl != sent_request.pl.cl)*/
                                                                                                                        minQ[sent_request.file_num].top().pl.cl != sent_request.pl.cl ) {
                                                        
                                                        printf("\nSent Client= %d, file_num = %d || " , sent_request.pl.cl, sent_request.file_num);
                                                        printf("Queue Top Client= %d, file_num = %d\n\n" , /*peek(sent_request.file_num).pl.cl*/minQ[sent_request.file_num].top().pl.cl, /*peek(sent_request.file_num).file_num*/minQ[sent_request.file_num].top().file_num);
                                                }
                                                if((replycount == 0 /*>= NUM_CLIENTS - 1*/) && (/*peek(sent_request.file_num).pl.cl == sent_request.pl.cl*/                                      minQ[sent_request.file_num].top().pl.cl == sent_request.pl.cl)) {
                                                        
                                                        is_waiting_for_reply = 0;
                                                        replycount = 0;
                                                        enter_and_exec_cs(rcv_msg.file_num, client_num, timestamp, peer_sockets, sent_request, sock_srv);
                                                        
                                                        /** Wake up child **/
                                                       /* if(child_pid >= 0)
                                                                kill(child_pid, SIGCONT);*/
                                                        printf("Released Other peers, Going to sleep again\n");
                                                }
                                        }
                                        if(rcv_msg.msg_type == MSG_TYPE_REQUEST) {
                                                printf("Sending REPLY back to  client %d\n", rcv_msg.pl.cl);
                                                /*insert(rcv_msg)*/minQ[rcv_msg.file_num].push(rcv_msg);
                                                rcv_msg.msg_type = MSG_TYPE_REPLY;
                                                timestamp += client_num;
                                                rcv_msg.pl.ts = timestamp;
                                                rcv_msg.pl.cl = client_num;
                                                if(send(i, &rcv_msg, sizeof(msg_t), 0) < 0) {
                                                        perror("send() called to send reply to peers::");
                                                        exit(EXIT_FAILURE);
                                                }
                                       }
                                       if(rcv_msg.msg_type == MSG_TYPE_RELEASE) {
                                                /* Received message should be at the top of the priority queue **/
                                                printf("Got RELEASE message for request %d from peer %d on file: %d \n", rcv_msg.req_id, rcv_msg.pl.cl, rcv_msg.file_num);
                                                release_queue[rcv_msg.file_num].push(rcv_msg);
                                                if(minQ[rcv_msg.file_num].top().pl.cl == release_queue[rcv_msg.file_num].top().pl.cl) {
                                                        printf("Rmoved request from client %d for ID %d on file %d\n", release_queue[rcv_msg.file_num].top().pl.cl, release_queue[rcv_msg.file_num].top().req_id, release_queue[rcv_msg.file_num].top().file_num);
                                                        
                                                        minQ[rcv_msg.file_num].pop();
                                                        release_queue[rcv_msg.file_num].pop();
                                                } else {
                                                        printf("This should not happen!! The msg in topmost queue is from %d and the RELEASE is from %d\n", minQ[rcv_msg.file_num].top().pl.cl, rcv_msg.pl.cl);
                                                }
                                       }
                                } 
                        }
                }
        }
        return 0;
}

void print_queue(int filenum) {
        msg_t top;
        if(minQ[filenum].empty()){
                printf("Queue is empty\n\n");
                return;
        }
        printf("Printing the Queue for file:%d\n\n", filenum);
        while(!minQ[filenum].empty()) {
                top = minQ[filenum].top();
                printf("Client: %d, MessageID: %d, Timestamp: %ld\n", top.pl.cl, top.req_id, top.pl.ts);
                minQ[filenum].pop();
        }
}

int childFunc(void *arg) {
            
        int ppid, randomTime;
        int r ;
        
        /* Linux specific, to automatically terminate child upon parent process' death */
        if ( prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) { 
                perror("prctl() :: child"); 
                exit(1); 
        }
        ppid = getppid();
        srand(time(NULL));
        
        while(1){
                randomTime = (rand() % 3) +2;
                //printf("Sleeping for %d seconds\n",randomTime);
                /** Sleep for random time **/
                sleep(randomTime);
                kill(ppid, SIGCHLD);
                //kill(getpid(), SIGSTOP); 
                printf("*** CHILD IS AWAKE***\n");
        }
}

int enter_and_exec_cs(int filenum, int clientnum, int timestamp, int peer_sockets[], msg_t sent_request, int sock_srv[]){
                msg_t ret;
                int i; 
                /* Enter critical section i.e, send request to server*/
                execute_critical_section(sock_srv, filenum);
                                                        
                /* Remove own message from queue*/
                ret = minQ[filenum].top();
                /*ret = heap_remove(filenum)*/minQ[filenum].pop();
                printf("Removed from Queue, client: %d, file: %d\n", ret.pl.cl, ret.file_num);
                                                        
                /* Once done with critical section, send MSG_TYPE_RELEASE to peers */
                sent_request.msg_type = MSG_TYPE_RELEASE;
                timestamp += clientnum;
                sent_request.pl.ts = timestamp;

                for(i = 0; i < NUM_CLIENTS-1; i++) {
                                printf("Sending RELEASE to IP %s\n", get_peer_IP(peer_sockets[i]));
                                if(send(peer_sockets[i], &sent_request, sizeof(msg_t), 0) < 0){
                                        perror("send() called to send request to peers::");
                                                        exit(EXIT_FAILURE);
                                        }                               
                }
}

static void child_sig_handler(int sig) {
           got_SIGCHLD = 1;
}

int start_child_process(){
        unsigned char *stack;                    /* Start of stack buffer */
        unsigned char *stackTop;                 /* End of stack buffer */
        pid_t pid;

        /* Allocate stack for child */

        stack = (unsigned char *)malloc(STACK_SIZE);
        if (stack == NULL) {
                perror("malloc()::");
        }
        stackTop = stack + STACK_SIZE;  /* Assume stack grows downward */
        
        /** Create child that has its own UTS namespace; child commences execution in childFunc() **/
        pid = clone(childFunc, stackTop, 0, NULL);
        if (pid == -1) {
               perror("clone()");
               return -1;
        }
        printf("clone() returned %ld\n", (long) pid);
        return pid;
}

int connect_to_server(char *ip, int port_num) {
        int sockfd;
        struct sockaddr_in dest_addr;
        
        sockfd = socket(PF_INET, SOCK_STREAM, 0); // do some error checking!
        dest_addr.sin_family = AF_INET;

        dest_addr.sin_port = htons(port_num);

        dest_addr.sin_addr.s_addr = inet_addr(ip);
        memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);
        if (connect(sockfd, (struct sockaddr *)&dest_addr, sizeof dest_addr) < 0){
                perror("ERROR connect()");
                return -1;
        }
        
        return sockfd;
}

int execute_critical_section(int sock_srv[], int file_num) {
        int j, ret = 0;
        char line[STRING_MAX_LEN];
         srv_msg_t msg;
        
        request_type = (request_type + 1) %2 ;
        msg.msg_type = request_type;
        msg.file_num = file_num;
        
        if (request_type == REQ_TYPE_READ) {
                /** Send a message randomly to one of the server */
                
                int index = rand() % NUM_SERVERS;
                memset(line, 0, STRING_MAX_LEN);
                if( send (sock_srv[index], &msg, sizeof(srv_msg_t), 0)  < 0) {
                        perror("send() sending request to server");
                        exit(EXIT_FAILURE);
                }
                printf("** Sending READ request **\n");
                if( recv(sock_srv[index] , line, STRING_MAX_LEN, MSG_WAITALL) < 0){
                        perror("recv() receiving request from server");
                        exit(EXIT_FAILURE);
                }
                printf("#####Message received is :%s\n", line);
                ret = 1;
                
        } else if (request_type = REQ_TYPE_WRITE) {
                /** Send a message to all three servers */
                
                for(j = 0; j <NUM_SERVERS;  j++) {
                        /** The server will simply write the current timestamp  */
                
                        if( send (sock_srv[j], &msg, sizeof(srv_msg_t), 0)  < 0) {
                                perror("send() sending request to server");
                                exit(EXIT_FAILURE);
                        }
                        if( recv(sock_srv[j] , &ret, sizeof(int), MSG_WAITALL) < 0){
                                perror("recv() receiving request from server");
                                exit(EXIT_FAILURE);
                        }
                        printf("Received %d from server\n", ret);
                }
        }
        return ret;
}

void debug_print_peer_sockets(int peer_sockets[]) {
        int i; 
        
        for(i = 0; i < NUM_CLIENTS; i++){
                printf("Connected to peer %d\n", peer_sockets[i]);
        }
}

char *get_peer_IP(int socket) {
        struct sockaddr_in peer;
        socklen_t peer_len;
        
        peer_len = sizeof(peer);
        if (getpeername(socket, (sockaddr *)&peer, &peer_len) == -1) {
                perror("getpeername() failed");
                return NULL;
        }
        return inet_ntoa(peer.sin_addr);
}