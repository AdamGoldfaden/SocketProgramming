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

#define TCP_PORT_CLIENT "25164"  // the TCP port the client will be connecting to
#define TCP_PORT_MONITOR "26164"  // the TCP port the aws will use to connect to the monitor
#define UDP_PORT "24164" // the main UDP port of the aws

//The following are the UDP ports associated with the three backend servers
#define UDP_PORT_SERVERA "21164"
#define UDP_PORT_SERVERB "22164"
#define UDP_PORT_SERVERC "23164"

#define BACKLOG 10   // how many pending connections queue will hold

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

//Most of this is copied from the section in Beej about stream servers
void establish_tcp_client (int& sockfd, int& new_fd, socklen_t& sin_size, sockaddr_storage& their_addr) {
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    int yes = 1;
    int rv;
    
    memset(&hints, 0, sizeof hints);
    
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if ((rv = getaddrinfo("localhost", TCP_PORT_CLIENT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        //return 1;
    }
    
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
    
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1); 
    }
}

//Most of this is copied from the section in Beej about stream clients
void establish_tcp_monitor(int& sockfd) {
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
  
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((rv = getaddrinfo("localhost", TCP_PORT_MONITOR, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        //return 1;
    }
    
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
        //return 2;
    }
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    
    freeaddrinfo(servinfo);
        
}

//Taken from beej section about datagram sockets
void send_data_search(char server,int link_id, int& m_in, float& bw_in, 
        float& l_in, float& v_in, float& np_in) {
    int sockfd, sockfd_s;
    char * servname;
   
    struct addrinfo hints, *servinfo, *p;
    socklen_t sin_size;
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
    
    if(server == 'A') {
        servname = (char *) UDP_PORT_SERVERA;
    }
    else if(server == 'B') {
        servname = (char *) UDP_PORT_SERVERB;
    }
    
    if ((rv = getaddrinfo("localhost", servname, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        //return 1;
    }
    
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd_s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue; 
        }
        break; 
    }
    
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        //return 2;
    }
    
    sendto(sockfd, (char*)& link_id, sizeof link_id, 0, p->ai_addr, p->ai_addrlen);
    
    freeaddrinfo(servinfo);
    
    cout << "The AWS sent link ID=<" << link_id << "> to Backend-Server <" << server << 
        "> using UDP over port <" << UDP_PORT << ">" << endl;
    
    int m; float bw; float l; float v; float np;
    recvfrom(sockfd, (char*)& m, sizeof m, 0, NULL, NULL);
    recvfrom(sockfd, (char*)& bw, sizeof bw, 0, NULL, NULL);
    recvfrom(sockfd, (char*)& l, sizeof l, 0, NULL, NULL);
    recvfrom(sockfd, (char*)& v, sizeof v, 0, NULL, NULL);
    recvfrom(sockfd, (char*)& np, sizeof np, 0, NULL, NULL);
    
    m_in = m; bw_in = bw; l_in = l; v_in = v; np_in = np;
    
    cout << "The AWS received <" << m << "> matches from Backend-Server <" << server << 
        "> using UDP over port <" << UDP_PORT << ">" << endl;
    
    close(sockfd);
    close(sockfd_s);
}

//Taken from beej section about datagram sockets
void send_data_calculate(int link_id, int size, float power, float bw, 
        float l, float v, float np, float& delay_t_in, float& delay_p_in) {
    int sockfd, sockfd_s;
   
    struct addrinfo hints, *servinfo, *p;
    socklen_t sin_size;
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
    
    
    if ((rv = getaddrinfo("localhost", UDP_PORT_SERVERC, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        //return 1;
    }
    
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd_s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue; 
        }
        break; 
    }
    
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        //return 2;
    }
    
    sendto(sockfd, (char*)& link_id, sizeof link_id, 0, p->ai_addr, p->ai_addrlen);
    sendto(sockfd, (char*)& size, sizeof size, 0, p->ai_addr, p->ai_addrlen);
    sendto(sockfd, (char*)& power, sizeof power, 0, p->ai_addr, p->ai_addrlen);
    sendto(sockfd, (char*)& bw, sizeof bw, 0, p->ai_addr, p->ai_addrlen);
    sendto(sockfd, (char*)& l, sizeof l, 0, p->ai_addr, p->ai_addrlen);
    sendto(sockfd, (char*)& v, sizeof v, 0, p->ai_addr, p->ai_addrlen);
    sendto(sockfd, (char*)& np, sizeof np, 0, p->ai_addr, p->ai_addrlen);
    
    freeaddrinfo(servinfo);
    
    cout << "The AWS sent link ID=<" << link_id << ">, size=<" << size << ">, power=<" <<
            power << ">, and link information to Backend-Server C using UDP over port <" <<
            UDP_PORT << ">" << endl;
    
    float delay_t, delay_p;
    recvfrom(sockfd, (char*)& delay_t, sizeof delay_t, 0, NULL, NULL);
    recvfrom(sockfd, (char*)& delay_p, sizeof delay_p, 0, NULL, NULL);
    
    delay_t_in = delay_t*1000.0; delay_p_in = delay_p*1000.0;
    
    cout << "The AWS received outputs from Backend-Server C using UDP over port <" 
            << UDP_PORT << ">" << endl;
    
    close(sockfd);
    close(sockfd_s);
}

int main(void)
{
    int sockfd_c, new_fd_c, sockfd_m;
    socklen_t sin_size;
    struct sockaddr_storage their_addr;
    char s[INET6_ADDRSTRLEN];
    
    //establish listening socket
    establish_tcp_client(sockfd_c,new_fd_c,sin_size,their_addr);
    
    cout << "The AWS is up and running." << endl;
    
    //main accept loop of the AWS server 
    while(1) {
        //this stuff right here is taken from beej as well
        sin_size = sizeof their_addr;
        new_fd_c = accept(sockfd_c, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd_c == -1) {
            perror("accept");
            continue; 
        }
        
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
        //End of beej's stuff
        
        //establish socket connection with monitor
        establish_tcp_monitor(sockfd_m);
        
        //declaring three integer variables
        int link_id, size;
        float power;
        
        //declaring data storage variables
        int m_a,m_b;
        float bw_a,l_a,v_a,np_a;
        float bw_b,l_b,v_b,np_b;
        float delay_p = -1; float delay_t = -1; float delay_total;
        
        //receiving variable data from the client
        recv(new_fd_c,(char*)& link_id, sizeof link_id,0);
        recv(new_fd_c,(char*)& size, sizeof size,0);
        recv(new_fd_c,(char*)& power, sizeof power,0);
        
        //sending variable data to the monitor
        send(sockfd_m,(char*)& link_id, sizeof link_id ,0);
        send(sockfd_m,(char*)& size, sizeof size,0);
        send(sockfd_m,(char*)& power, sizeof power,0);
        
        //Printing confirmation of data reception
        cout << "The AWS received link ID=<" << link_id <<
        ">, size=<" << size << ">, and power=<" << power << 
        "> from the client using TCP over port <" << TCP_PORT_CLIENT << ">" << endl;
       
        //Printing confirmation of data sending
        cout << "The AWS sent link ID=<" << link_id <<
        ">, size=<" << size << ">, and power=<" << power << 
        "> to the monitor using TCP over port <" << TCP_PORT_MONITOR << ">" << endl;
        
        //Querying backend servers
        send_data_search('A',link_id, m_a, bw_a, l_a, v_a, np_a);
        send_data_search('B',link_id, m_b, bw_b, l_b, v_b, np_b);
        
        //Send link information to client and monitor
        if (m_a == 0 && m_b == 0) { 
            send(new_fd_c,(char*)& delay_t, sizeof delay_t ,0);
            send(new_fd_c,(char*)& delay_p, sizeof delay_p ,0);
            send(sockfd_m,(char*)& delay_t, sizeof delay_t ,0);
            send(sockfd_m,(char*)& delay_p, sizeof delay_p ,0);
            cout << "The AWS sent No Match to the monitor and the client using TCP over ports <" <<
                    TCP_PORT_CLIENT << "> and <" << TCP_PORT_MONITOR << ">, respectively" << endl;
        }
        else if(m_a == 1) { 
            send_data_calculate(link_id, size, power, bw_a, l_a, v_a, np_a, delay_t, delay_p);
            
            //calculate total delay and round to 2 decimal places (taken from Geeks for Geeks)
            delay_total = delay_t + delay_p;
            delay_total = (int)(delay_total * 100.0 + 0.5);
            delay_total = (float)delay_total/100.0;
            
            
            send(new_fd_c,(char*)& delay_t, sizeof delay_t ,0);
            send(new_fd_c,(char*)& delay_p, sizeof delay_p ,0);
            cout << "The AWS sent delay=<" << delay_total << ">ms to the client using TCP over port <"
                    << TCP_PORT_CLIENT << ">" << endl;
            
            send(sockfd_m,(char*)& delay_t, sizeof delay_t ,0);
            send(sockfd_m,(char*)& delay_p, sizeof delay_p ,0);
            cout << "The AWS sent detailed results to the monitor using TCP over port <"
                    << TCP_PORT_MONITOR << ">" << endl;
        }
        else if(m_b == 1) {
            send_data_calculate(link_id, size, power, bw_b, l_b, v_b, np_b, delay_t, delay_p);
            
            //calculate total delay and round to 2 decimal places (taken from StackOverflow)
            delay_total = delay_t + delay_p;
            delay_total = floorf(delay_total * 100.0) / 100.0;  
            delay_total = roundf(delay_total * 100.0) / 100.0; 
            delay_total = ceilf(delay_total * 100.0) / 100.0;   
        
            
            send(new_fd_c,(char*)& delay_t, sizeof delay_t ,0);
            send(new_fd_c,(char*)& delay_p, sizeof delay_p ,0);
            cout << fixed << setprecision(2) << "The AWS sent delay=<" << delay_total << ">ms to the client using TCP over port <"
                    << TCP_PORT_CLIENT << ">" << endl;
            
            send(sockfd_m,(char*)& delay_t, sizeof delay_t ,0);
            send(sockfd_m,(char*)& delay_p, sizeof delay_p ,0);
            cout << "The AWS sent detailed results to the monitor using TCP over port <"
                    << TCP_PORT_MONITOR << ">" << endl;
        }
        
        close(new_fd_c);
        close(sockfd_m);
    }
    
    return 0; 
}