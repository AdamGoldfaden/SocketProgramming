#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <math.h>

#define UDP_PORT "23164" // this server's UDP port
#define UDP_PORT_AWS "24164" //UDP port of the AWS

using namespace std;

//The following 2 functions are taken from Beej
void sigchld_handler(int s)
{
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//Most of this is copied from the section in Beej about datagram sockets
void establish_port (int& sockfd) {
    struct addrinfo hints, *servinfo, *p;
    socklen_t sin_size;
    struct sigaction sa;
    int rv;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    //hints.ai_flags = AI_PASSIVE;
    
    if ((rv = getaddrinfo("localhost", UDP_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        //return 1;
    }
    
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue; 
        }
        
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue; 
        }
        break; 
    }
    
   
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    
    freeaddrinfo(servinfo); 
    
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1); 
    }
}

//Most of this is copied from the section in Beej about datagram sockets
void send_results(float delay_t, float delay_p) {
    struct addrinfo hints, *servinfo, *p;
    int rv,sockfd;
    char s[INET6_ADDRSTRLEN];
  
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    
    if ((rv = getaddrinfo("localhost", UDP_PORT_AWS, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        //return 1;
    }
    
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue; 
        }
        break; 
    }
    
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        //return 2;
    }
        
    
    sendto(sockfd, (char*)& delay_t, sizeof delay_t, 0, p->ai_addr, p->ai_addrlen);
    sendto(sockfd, (char*)& delay_p, sizeof delay_p, 0, p->ai_addr, p->ai_addrlen);
    
    freeaddrinfo(servinfo);
    close(sockfd);
}

int main(void)
{
    int sockfd;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    struct addrinfo *p;
    char s[INET6_ADDRSTRLEN];
    
    cout << "The Server C is up and running using UDP on port <" << UDP_PORT << ">." << endl;
    
    while(1) {
        
        //create receiving port
        establish_port(sockfd);
        
        //Declare storage variables
        int link_id; int size;
        float power;
        float bw, l, v, np, np_w, sp_w, c, delay_p, delay_t;
        addr_len = sizeof their_addr;
        
        //receive link information from the aws
        recvfrom(sockfd, (char*)& link_id, sizeof link_id, 0,(struct sockaddr *)&their_addr, &addr_len);
        inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
        recvfrom(sockfd, (char*)& size, sizeof size, 0,(struct sockaddr *)&their_addr, &addr_len);
        recvfrom(sockfd, (char*)& power, sizeof power, 0,(struct sockaddr *)&their_addr, &addr_len);
        recvfrom(sockfd, (char*)& bw, sizeof bw, 0,(struct sockaddr *)&their_addr, &addr_len);
        recvfrom(sockfd, (char*)& l, sizeof l, 0,(struct sockaddr *)&their_addr, &addr_len);
        recvfrom(sockfd, (char*)& v, sizeof v, 0,(struct sockaddr *)&their_addr, &addr_len);
        recvfrom(sockfd, (char*)& np, sizeof np, 0,(struct sockaddr *)&their_addr, &addr_len);
        
        cout << "The Server C received link information of link <" << link_id << ">, file size <" << 
                size << ">, and signal power <" << power << ">" << endl;
        
        //Calculate propagation delay first
        delay_p = (l*pow(10,3))/(v*pow(10,7));
        
        //Convert power to Watts
        np_w = pow(10,np/10.0)/1000.0;
        sp_w = pow(10,power/10.0)/1000.0;
                
        //Find channel capacity
        c = (bw*pow(10,6))*log2(1.0 + sp_w/np_w);
        
        //Calculate transmission delay
        delay_t = (float)size/c;
        
        cout << "The server C finished the calculation for link <" << link_id << ">" << endl;
        
        //Send all link information back to AWS
        send_results(delay_t, delay_p);    
        cout << "The Server C finished sending the output to AWS" << endl;

        close(sockfd);
    }
    
    return 0; 
}

