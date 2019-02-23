#include "http.h"

#define CHUNKSIZE 4096

std::vector<char> handleChunked(int myfd, std::vector<char> firstbuff);

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
        /*
            May need to change to std::fill
        */
        std::fill(tempseg.begin(), tempseg.end(), '\0');

        if (segsize < TEMPSIZE || (segsize == TEMPSIZE && tempbuf[tempbuf.size() - 1] == '\0'))
        { // Finish receive
            tempbuf.push_back('\0');
            HTTPResponse tempres(tempbuf);
            std::unordered_map<std::string, std::string> tempheader = tempres.getheader();
            if (tempheader.find("Content-Length") == tempheader.end())
            {
                // handle chunked
                if (tempheader.find("Transfer-Encoding") != tempheader.end() && tempheader["Transfer-Encoding"] == "chunked") {
                    if (DEVELOPMENT) std::cout<<"\nChunked here!\n";
                    tempbuf.pop_back();
                    return handleChunked(myfd, tempbuf);
                }
                else {
                    break;
                }
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

std::vector<char> handleChunked(int myfd, std::vector<char> firstbuff) {
    int length = 0;
    std::vector<char> tempseg(CHUNKSIZE);
    std::vector<char> tempchunk;

    firstbuff.push_back('\0');
    if (DEVELOPMENT) std::cout<<"Response buffer: "<<firstbuff.data()<<std::endl<<std::endl;
    firstbuff.pop_back();

    // firstbuff doesn't contain '\0'
    while (1) {
        int recvsize = recv(myfd, &tempseg.data()[0], CHUNKSIZE, 0);
        
        for (int i = 0; i < recvsize; i++) {
            tempchunk.push_back(tempseg[i]);
        }

        std::fill(tempseg.begin(), tempseg.end(), '\0');

        if (recvsize == CHUNKSIZE) continue;

        tempchunk.push_back('\0');

        std::cout<<"Temp chunk size: "<<tempchunk.size()<<std::endl;
        std::cout<<"Temp chunk content: "<<tempchunk.data()<<std::endl;

        // handle chunk
        std::string chunkstr = tempchunk.data();
        size_t linebreak = chunkstr.find("\r\n");
        std::cout<<"Linebreak: "<<linebreak<<std::endl;
        int chunk_length = std::stoi(chunkstr.substr(0, linebreak), nullptr, 16);
        
        length += chunk_length;

        // last chunk
        if (chunk_length == 0) {
            // before break, add '\0'
            firstbuff.push_back('\0');

            // Receive trailer
            while (1) {
                int trailersize = recv(myfd, &tempseg.data()[0], CHUNKSIZE, 0);
                std::fill(tempseg.begin(), tempseg.end(), '\0');
                if (trailersize == CHUNKSIZE) continue;
                break;
            }
            
            // Finish content read, assembly
            HTTPResponse tempres(firstbuff);
            std::unordered_map<std::string, std::string> realHeader = tempres.accessHeader();

            // Erase chunk header & add content-length header
            realHeader.erase("Transfer-Encoding");
            realHeader["Content-Length"] = std::to_string(length);

            // Update temporary response's buffer
            tempres.reParse();

            return tempres.getBuffer();
        }
        // else parse content to firstbuff
        else {
            chunkstr.erase(0, linebreak + 2);
            for (int i = 0; i < chunk_length; i++) {
                firstbuff.push_back(tempchunk[i]);
            }
            tempchunk.clear();
        }

    }
}

void handlehttp(int reqfd)
{
    // MyLock lk(&mymutex);

    // Get request
    std::vector<char> tempbuf = myrecv(reqfd);

    if (DEVELOPMENT)
        std::cout << "Temporary buffer got " << tempbuf.size() << " bytes." << std::endl
                  << "The content is: " << std::endl
                  << tempbuf.data() << std::endl;

    HTTPRequest newreq(tempbuf);

    // Handle request
    newreq.handlereq(reqfd);

    close(reqfd);
}
