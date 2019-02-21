#include "http.h"

std::vector<char> myrecv(int myfd) {
    // Get request
    std::vector<char> tempbuf;
    std::vector<char> tempseg(TEMPSIZE);

    while (1) {
        //std::cout<<"In myrecv loop"<<std::endl;
        int segsize = recv(myfd, &tempseg.data()[0], TEMPSIZE, 0);

        //std::cout<<"Segsize: "<<segsize<<std::endl;
        //std::cout<<"temp is: "<<tempseg.data()<<std::endl;

        if (segsize < 0) {
            std::cout<<"Error in receive request"<<std::endl;
            close(myfd);
            exit(1);
        }

        for (int i = 0; i < segsize; i++) {
            tempbuf.push_back(tempseg[i]);
        }

        //std::cout<<"Tempbuf size: "<<tempbuf.size()<<std::endl;

        tempseg.clear();

        if (segsize < TEMPSIZE || (segsize == TEMPSIZE && tempbuf[tempbuf.size() - 1] == '\0')) {  // Finish receive
            // Check content-length
            HTTPResponse tempres(tempbuf);
            std::unordered_map<std::string, std::string> tempheader = tempres.getheader();
            if (tempheader.find("Content-Length") == tempheader.end()) {
                break;
            }
            std::string tempbody = tempres.getBody();
            if (tempbody.size() >= std::stoi(tempheader["Content-Length"]) - 1) {
                break;
            }
        }
    }

    std::cout<<"Final tempbuf size: "<<tempbuf.size()<<std::endl;

    return tempbuf;
}

void handlehttp(int reqfd) {
    // Get request
    std::vector<char> tempbuf = myrecv(reqfd);

    if (DEVELOPMENT) std::cout<<"Temporary buffer got "<<tempbuf.size()<<" bytes."<<std::endl<<"The content is: "<<std::endl<<tempbuf.data()<<std::endl;

    HTTPRequest newreq(tempbuf);

    // Handle request
    newreq.handlereq(reqfd);
}