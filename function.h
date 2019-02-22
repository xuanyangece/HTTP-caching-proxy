#include "http.h"

std::vector<char> myrecv(int myfd)
{
    std::vector<char> tempbuf;
    std::vector<char> tempseg(TEMPSIZE);
    while (1)
    {
        //std::cout << "In myrecv loop" << std::endl;
        int segsize = recv(myfd, &tempseg.data()[0], TEMPSIZE, 0);

        //std::cout << "Segsize: " << segsize << std::endl;

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
        
        //std::cout << "Tempbuf size: " << tempbuf.size() << std::endl;
        tempseg.clear();

        if (segsize < TEMPSIZE || (segsize == TEMPSIZE && tempbuf[tempbuf.size() - 1] == '\0'))
        { // Finish receive
            tempbuf.push_back('\0');
            HTTPResponse tempres(tempbuf);
            std::unordered_map<std::string, std::string> tempheader = tempres.getheader();
            if (tempheader.find("Content-Length") == tempheader.end())
            {
                break;
            }
            std::string tempbody = tempres.getBody();
            //std::cout << "Body size in buffer: " << tempbody.size() << std::endl;
            //std::cout << "Content-Length: " << tempheader["Content-Length"] << std::endl;
            if (tempbody.size() >= std::stoi(tempheader["Content-Length"]) - 1)
            {
                break;
            }
            else
            {
                std::cout << "OK, print whole temp buffer: " << std::endl;
                std::cout << tempbuf.data() << std::endl;
            }
        }

        if (segsize == 0)
        {
            tempbuf.push_back('\0');
            break;
        }
    }

    std::cout << "Final tempbuf size: " << tempbuf.size() << std::endl;

    return tempbuf;
}

void handlehttp(int reqfd)
{
    // Get request
    std::vector<char> tempbuf = myrecv(reqfd);

    if (DEVELOPMENT)
        std::cout << "Temporary buffer got " << tempbuf.size() << " bytes." << std::endl
                  << "The content is: " << std::endl
                  << tempbuf.data() << std::endl;

    HTTPRequest newreq(tempbuf);

    // Handle request
    newreq.handlereq(reqfd);
}
