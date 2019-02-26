#include "newhttp.h"
#include <cstdlib>
#include <iomanip>

// 1 - myrecv
#define DEVELOPMENT 1


/*
    Receive buffer from client
*/
std::vector<char> myrecv(int myfd)
{
    std::cout<<"Enter myrecv\n";
    std::vector<char> tempbuf;
    std::vector<char> tempseg(TEMPSIZE);
    while (1)
    {
        int segsize = recv(myfd, &tempseg.data()[0], TEMPSIZE, 0);

        if (DEVELOPMENT == 1) std::cout<<"RECV SIZE = "<<segsize<<std::endl;

        if (segsize < 0)
        {
            std::cout << "Error in receive request" << std::endl;
            close(myfd);
            exit(1);
        }

        for (int i = 0; i < segsize; i++)
        {
            tempbuf.push_back(tempseg[i]);
        }

        if (DEVELOPMENT == 1) std::cout<<"Loop middle\n";
        
        std::fill(tempseg.begin(), tempseg.end(), '\0');

        if (DEVELOPMENT == 1) std::cout<<"after fill\n";

        // Has to be done first
        if (segsize == 0)
        {
            std::cout<<"\nTHIS IS A EMPTY REQUEST\n\n";
            tempbuf.push_back('\0');
            break;
        }

        if (segsize < TEMPSIZE)
        { 
            if (DEVELOPMENT == 1) std::cout<<"Enter if\n";
            
            // Finish receive
            tempbuf.push_back('\0');
            HTTPResponse tempres(tempbuf);
            std::unordered_map<std::string, std::string> tempheader = tempres.getheader();

            if (tempheader.find("Content-Length") == tempheader.end())
            {
                break;
            }

            std::string tempbody = tempres.getBody();
            if (DEVELOPMENT == 1) std::cout << "Body size in buffer: " << tempbody.size() << std::endl;
            if (DEVELOPMENT == 1) std::cout << "Content-Length: " << tempheader["Content-Length"] << std::endl;

            if (tempbody.size() >= std::stoi(tempheader["Content-Length"]) - 1)
            {
                break;
            }
            else
            {
                tempbuf.pop_back();
            }
        }
    }

    if (DEVELOPMENT == 1) std::cout << "Final tempbuf size: " << tempbuf.size() << std::endl;

    return tempbuf;
}

void handlehttp(int reqfd)
{
    MyLock lk(&mymutex);
    std::ofstream log(LOG, std::ios::app);

    // Get request
    std::vector<char> tempbuf = myrecv(reqfd);

    std::cout<<"\nTemp buffer: \n"<<"\""<<tempbuf.data()<<"\""<<std::endl<<std::endl;
}

void return404(int client_fd) {
    std::cout<<"Cannot found"<<std::endl;
  std::string header("HTTP/1.1 404 Not Found\r\nContent-Length: 36\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html><body>Not Found</body></html>\n");
  int len = send(client_fd, header.c_str(),
                 header.length(), MSG_NOSIGNAL);
}