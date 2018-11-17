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
#include <math.h>
#include <iomanip>

#define TCP_PORT "25164"

using namespace std;

//The majority of the following is taken from Beej's guide from the Stream Client section
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
  
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((rv = getaddrinfo("localhost", TCP_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue; 
        }
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue; 
        }
        break; 
    }
    
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    
    freeaddrinfo(servinfo);
    //End Beej's stuff
    
    cout << "The client is up and running." << endl;
    
    //Store three variable integer values from the command line
    int link_id = atoi(argv[1]);
    int size = atoi(argv[2]);
    float power = atof(argv[3]);
    
    //sending variable data to AWS
    send(sockfd,(char*)& link_id,sizeof link_id ,0);
    send(sockfd,(char*)& size,sizeof size,0);
    send(sockfd,(char*)& power,sizeof power,0);
    
    cout << "The client sent link ID=<" << link_id <<
    ">, size=<" << size << ">, and power=<" << power << 
    "> to AWS" << endl;
    
    //receiving link data
    float delay_p, delay_t, delay_total;
    recv(sockfd, (char*)& delay_t, sizeof delay_t,0);
    recv(sockfd, (char*)& delay_p, sizeof delay_p,0);

    if(delay_p == -1 && delay_t == -1) {
        cout << "Found no matches for link <" << link_id << ">" << endl;
        close(sockfd);
        return 0;
    }
    
    //calculate total delay and round to 2 decimal places (taken from StackOverflow)
    delay_total = delay_t + delay_p;
    delay_total = floorf(delay_total * 100.0) / 100.0;  
    delay_total = roundf(delay_total * 100.0) / 100.0; 
    delay_total = ceilf(delay_total * 100.0) / 100.0;   
    
    //Print result for total delay
    cout << fixed << setprecision(2) << "The delay for link <" << link_id << "> is <" 
            << delay_total << ">ms" << endl;
    
    close(sockfd);
    
    return 0; 
}
