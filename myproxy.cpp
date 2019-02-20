#include <sys/select.h>
#include <unistd.h>
#include <netdb.h> // gethostbyname
#include <netinet/in.h>
#include <mutex>
#include "http.h"
#include <cstring>

#define PORT 7654

#define DEVELOPMENT 1

std::mutex mymutex;

int main(int argc, char ** argv) {
    // read command line parameters


    // Create socket
    int sockfd;
    struct sockaddr_in proxyaddr;
    socklen_t len = sizeof(proxyaddr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (DEVELOPMENT) std::cout<<"Sockfd: "<<sockfd<<std::endl;


    // Get IP
    char hostname[128];
    if (gethostname(hostname, sizeof(hostname)) == -1) {
        std::cout<<"Hostname access fail"<<std::endl;
        exit(1);
    }
    struct hostent *hent;
    hent = gethostbyname(hostname);
    if (hent == NULL) {
        std::cout<<"Host info access fail"<<std::endl;
        exit(1);
    }
    if (DEVELOPMENT) std::cout<<"Hostname: "<<hostname<<std::endl;

    char *serIP;
    // get from h_addr
    serIP = inet_ntoa(*(struct in_addr *)hent->h_addr);
    if (DEVELOPMENT) std::cout<<"Proxy IP: "<<serIP<<std::endl;

    // Initial memory address
    memset(&proxyaddr, '\0', sizeof(proxyaddr));
    proxyaddr.sin_family = AF_INET;
    proxyaddr.sin_port = htons(PORT);
    proxyaddr.sin_addr.s_addr = inet_addr(serIP);

    // Bind socket to address:port
    int ret = bind(sockfd, (struct sockaddr *)&proxyaddr, sizeof(proxyaddr));
    if (ret < 0) {
        std::cout<<"Error in binding"<<std::endl;
        exit(1);
    }

    // Listening
    if (listen(sockfd, 10) != 0) {
        std::cout<<"Error in listening"<<std::endl;
        exit(1);
    }

    while (1) {
        // Get address info
        struct sockaddr_in reqaddr;

        // Accept request
        int reqfd = accept(sockfd, (struct sockaddr *)&reqaddr, &len);
        if (reqfd < 0) {
            std::cout<<"Error in accept"<<std::endl;
            exit(1);
        }

        if (DEVELOPMENT) {
            std::cout<<"Request from "<<inet_ntoa(reqaddr.sin_addr)<<std::endl;
        }

        // Create thread to handle request
        MyLock lk(&mymutex);

        handlehttp(reqfd);

        if (DEVELOPMENT) {
            std::cout<<"Finish service, close connection"<<std::endl;
            close(reqfd);
            exit(1);
        }

        close(reqfd);
    }


    return 0;
}