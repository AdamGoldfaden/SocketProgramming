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
#include <iomanip>
#include <math.h>

#define TCP_PORT "26164"

#define BACKLOG 10

using namespace std;

//The following is mostly taken from Beej's but is slightly modified for this assignment 
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


int main(void)
{
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    memset(&hints, 0, sizeof hints);
    
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if ((rv = getaddrinfo("localhost", TCP_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue; 
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1); 
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue; 
        }
        break; 
    }
    
    freeaddrinfo(servinfo); 
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1); 
    }
    
    
    cout << "The monitor is up and running." << endl;

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue; 
        }
        
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
        //End beej's stuff
        
        //declaring three variable integers
        int link_id, size;
        float power;
        
        //receiving variable data from the AWS
        recv(new_fd,(char*)& link_id,sizeof link_id,0);
        recv(new_fd,(char*)& size,sizeof size,0);
        recv(new_fd,(char*)& power,sizeof power,0);
        
        //Printing confirmation of data reception
        cout << "The monitor received link ID=<" << link_id <<
        ">, size=<" << size << ">, and power=<" << power << 
        "> from the AWS" << endl;
        
        //receiving link data
        float delay_p, delay_t = 0, delay_total;
        recv(new_fd, (char*)& delay_t, sizeof delay_t,0);
        recv(new_fd, (char*)& delay_p, sizeof delay_p,0);
        
        //Found no matches for link_id
        if(delay_p == -1 && delay_t == -1) {
            cout << "Found no matches for link <" << link_id << ">" << endl;
            close(new_fd);
            continue;
        }
        
        //calculate total delay and round to 2 decimal places (taken from StackOverflow)
        delay_total = delay_t + delay_p;
        //cout << delay_t << " " << delay_p << " " << delay_total << endl;
        delay_total = floorf(delay_total * 100.0) / 100.0;  
        delay_total = roundf(delay_total * 100.0) / 100.0; 
        delay_total = ceilf(delay_total * 100.0) / 100.0;   
        
        //Round the other delays to two decimal places
        delay_t = floorf(delay_t* 100.0) / 100.0;  
        delay_t = roundf(delay_t * 100.0) / 100.0; 
        delay_t = ceilf(delay_t * 100.0) / 100.0;   
        
        delay_p = floorf(delay_p * 100.0) / 100.0;  
        delay_p = roundf(delay_p * 100.0) / 100.0; 
        delay_p = ceilf(delay_p * 100.0) / 100.0;   
        
        //Print results 
        cout << "The result for link <" << link_id << ">:" << endl;
        cout << fixed << setprecision(2) << "Tt = <" << delay_t << ">ms" << endl;
        cout << fixed << setprecision(2) << "Tp = <" << delay_p << ">ms" << endl;
        cout << fixed << setprecision(2) << "Delay = <" << delay_total << ">ms" << endl;
        
        close(new_fd);
    }
    
return 0; 
}
