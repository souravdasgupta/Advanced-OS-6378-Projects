#include "server.h"

char *get_client_IP(int socket) {
        struct sockaddr_in peer;
        int peer_len;
        
        peer_len = sizeof(peer);
        if (getpeername(socket, &peer, &peer_len) == -1) {
                perror("getpeername() failed");
                return NULL;
        }
        return inet_ntoa(peer.sin_addr);
}


int main(){
        int i, sock_self_server, num_clients_left = NUM_CLIENTS, newfd = -1, s, yes=1, fdmax = 0, j, lastline_len[] = {0, 0, 0};
        struct sockaddr_in my_addr, remoteaddr;
        fd_set master, read_fds; 
        FILE *ipfile = NULL, *file;
        struct ifaddrs *ifaddr, *ifa;
        char ipaddr[16];
        char host[NI_MAXHOST];
        socklen_t addrlen;
        
        FD_ZERO(&read_fds);
        FD_ZERO(&master);
        
        addrlen = sizeof(struct sockaddr_in);
        
          /** Seed random number generator */
        srand(time(NULL));
        
         /** Get own IP adress **/
         if (getifaddrs(&ifaddr) == -1) {
                perror("getifaddrs");
                exit(EXIT_FAILURE);
        }
        
        for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if((ifa->ifa_addr->sa_family == AF_INET) && strncmp(ifa->ifa_name, "lo", 2)) {
                        /** Not loopback/assuming only one AF_INET interface other than lo **/
                        s = getnameinfo(ifa->ifa_addr,  sizeof(struct sockaddr_in) , host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                        printf("IP address of this server is :%s\n", host);
                        break;
                }
        }
        freeifaddrs(ifaddr);
        
         /**  Write IP address for clients to read **/
        ipfile = fopen ( SERV_IP_FILE_PATH, "a+" );
        if (!ipfile) {
                perror( SERV_IP_FILE_PATH);
                exit(EXIT_FAILURE);
        }
         if(fprintf(ipfile, "%s\n", host) < 0){
                perror("fprintf()::");
                exit(EXIT_FAILURE);
        }
        fclose(ipfile);
        printf("Done creating file\n");
        
        /** Delete pre-exisiting Critical Resource File */
        for(i = 0; i < NUM_FILES; i++) {
                char filename[25];
                
                snprintf(filename, 25, "crit_resource_file%d.txt", i);
                if( access( filename, F_OK ) != -1 ) {
                        /* file exists */
                        if (unlink(filename) < 0) {
                                perror("unlink() error");
                        }
                }        
        }
        
        if((sock_self_server = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
                perror("socket()::");
                exit(EXIT_FAILURE);
        }

        if (setsockopt(sock_self_server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
                perror("setsockopt()::");
                exit(EXIT_FAILURE);
        }
        my_addr.sin_family = AF_INET;
        my_addr.sin_port = htons(serv_port_num);
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
        
          while (1) {
                read_fds = master;
                if( select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
                        perror("select()::");
                        close(sock_self_server); 
                        exit(EXIT_FAILURE);
                }
                
                for(i = 0; i <= fdmax; i++) {
                        if(FD_ISSET(i, &read_fds)){
                                if( i == sock_self_server && num_clients_left) {
                                        if ((newfd = accept(sock_self_server, (struct sockaddr *)&remoteaddr, &addrlen)) == -1) {
                                                perror("accept()::");
                                        } else {
                                                FD_SET(newfd, &master); 
                                                fdmax = max(newfd, fdmax);
                                                num_clients_left--;
                                                if(!num_clients_left){
                                                        close(sock_self_server);
                                                        FD_CLR(sock_self_server, &master);
                                                }
                                        }
                                        printf("Server : new connection from %s on socket %d\n", inet_ntoa(remoteaddr.sin_addr), newfd);
                                } else {
                                        int num_bytes = 0, request_type;
                                        srv_msg_t msg;
                                        char filename[25];
                                        
                                        memset(filename, 0, 25);
                                        if( recv(i , &msg, sizeof(srv_msg_t), MSG_WAITALL) < 0){
                                                perror("recv() receiving request from server");
                                                exit(EXIT_FAILURE);
                                        }
                                        
                                        if(msg.file_num < 0 || msg.file_num >=NUM_FILES)
                                                printf("ERROR::File number incorrect\n");
                                        
                                        snprintf(filename, 25, "crit_resource_file%d.txt", msg.file_num);
                                        request_type = msg.msg_type;
                                        printf("Received a request from client %s for %s \n", get_client_IP(i), request_type? "write" : "read");

                                        file = fopen ( filename, "a+" );
                                        if (!file) {
                                                perror( filename);
                                                exit(EXIT_FAILURE);
                                        }
                                        
                                        if(request_type == REQ_TYPE_READ) {
                                                char message[STRING_MAX_LEN + 1];
                                                size_t len;

                                                memset(message, 0, STRING_MAX_LEN);
                                                fseek(file, -lastline_len[msg.file_num], SEEK_END);
                                                len = fread( message, sizeof(char), lastline_len[msg.file_num], file);                  
                                                if(!len ){
                                                        if (feof(file)){
                                                                len = sprintf(message, "End-Of-File");
                                                        } else {
                                                                len = sprintf(message, "Read Error");
                                                        }
                                                }
                                                printf("Read %ld bytes of message.\n",len);
                                               
                                                if( send (i, message, STRING_MAX_LEN, 0)  < 0) {
                                                        perror("send() sending reply to client");
                                                        exit(EXIT_FAILURE);
                                                }
                                                printf("Sent message %s\n",message);
                                        } else if (request_type == REQ_TYPE_WRITE) {
                                                char *message;
                                                size_t len;
                                                int ret = SERVER_DONE_MSG;
                                                
                                                message = get_client_IP(i);
                                                fseek(file, 0, SEEK_END);
                                                len = fprintf(file, "Got a message from peer %s\n", message);
                                                lastline_len[msg.file_num] = len;
                                                if(len < 0) {
                                                        ret = SERVER_ERROR;
                                                        perror("ERROR::fprintf() to FILE");
                                                }
                                                printf("Written %ld bytes, returning the value: %d\n", len, ret);
                                                if( send (i, &ret, sizeof(int), 0)  < 0) {
                                                        perror("send() sending reply to client");
                                                        exit(EXIT_FAILURE);
                                                }
                                        }
                                        fclose(file);
                                }
                        }
                }
        }
        return 0;
}