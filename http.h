#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <iostream>
#include <netdb.h>
#include <cstring>
#include <string>
#include <vector>

#define DEVELOPMENT 1

#define BUFFSIZE 1024
#define TEMPSIZE 256

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

        if (segsize < TEMPSIZE) {  // Finish receive
            break;
        } else if (segsize == TEMPSIZE && tempbuf[tempbuf.size() - 1] == '\0') {  // Corner case: size = N * 256
            break;
        }
    }

    return tempbuf;
}


class HTTP {
    protected:
    std::vector<char> buffer;
    std::string startline;
    std::unordered_map<std::string, std::string> header;
    std::string body;
    std::string HTTPversion;

    public:
    virtual void parseBuffer() = 0;
    virtual void readStartLine(std::string line) = 0;
    virtual ~HTTP() {}

    std::string readHeader(std::string lines) {
        size_t pos;
        std::string line;
        while (lines.find("\r\n") != std::string::npos && lines.substr(0, lines.find("\r\n")) != "") {
            pos = lines.find("\r\n");
            line = lines.substr(0, pos);

            size_t colon = line.find(": ");
            if (colon == std::string::npos) {
                colon = line.find(":");
                if (colon == std::string::npos) {
                    std::cout<<"Error in format of header"<<std::endl;
                    return "";
                }
                else {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 1);
                    header[key] = value;

                    if (DEVELOPMENT > 1) std::cout<<"Key: "<<key<<" & Value: "<<value<<std::endl;
                }
            }
            else {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 2);
                header[key] = value;

                if (DEVELOPMENT > 1) std::cout<<"Key: "<<key<<" & Value: "<<value<<std::endl;
            }

            lines.erase(0, pos + 2);
        }
        
        if (DEVELOPMENT > 1) std::cout<<"After read header, total "<<header.size()<<" headers"<<std::endl;
        lines.erase(0, 2);
        if (DEVELOPMENT > 1) std::cout<<"Remain body: "<<lines<<std::endl;
        return lines;
    }

    std::unordered_map<std::string, std::string> getheader(){
        return header;
    }

    std::vector<char> getBuffer() {
        return buffer;
    }
};

class HTTPResponse: public HTTP {
    private:
    std::string code;
    std::string reason;
    public:
    HTTPResponse() {}

    HTTPResponse(std::vector<char> temp) {
        buffer = temp;
        parseBuffer();
    }

    HTTPResponse(const HTTPResponse & rhs) {
        buffer = rhs.buffer;
        parseBuffer();
    }

    HTTPResponse & operator=(const HTTPResponse & rhs) {
        if (this != &rhs) {
            buffer = rhs.buffer;
            parseBuffer();
        }
        return *this;
    }

    virtual void parseBuffer() {

        std::string temp = buffer.data();

        // Parse start line
        size_t pos = temp.find("\r\n");
        if (pos == std::string::npos) {
            std::cout<<"Error parsing first line"<<std::endl;
            return;
        }
        startline = temp.substr(0, pos);

        if (DEVELOPMENT > 1) {
            std::cout<<"Startline is: "<<startline<<std::endl;
        }

        readStartLine(startline);
    
        if (DEVELOPMENT > 1) {
            std::cout<<"After read startline, HTTP version is: "<<HTTPversion<<std::endl;
            std::cout<<"And code is: "<<code<<std::endl;
            std::cout<<"And reason is: "<<reason<<std::endl<<std::endl;
        }

        temp.erase(0, pos + 2);

        if (DEVELOPMENT > 1) {
            std::cout<<"Erase startline, the rest is: "<<std::endl<<temp<<std::endl;
        }

        // Parse header & get body
        body = readHeader(temp); 
    }

    virtual void readStartLine(std::string line) {
        // Read HTTP version
        size_t pos = line.find(" ");
        if (pos == std::string::npos) {
            std::cout<<"Error in reading method"<<std::endl;
            return;
        }

        HTTPversion = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read code
        pos = line.find(" ");
        if (pos == std::string::npos) {
            std::cout<<"Error in reading url"<<std::endl;
            return;
        }

        code = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read reason
        reason = line;
    }
};

std::unordered_map<std::string, HTTPResponse> cache;


class HTTPRequest: public HTTP {
    private:
    std::string method;
    std::string url;

    public:
    HTTPRequest() {}

    HTTPRequest(std::vector<char> temp) {
        buffer = temp;
        parseBuffer();
    }

    HTTPRequest(const HTTPRequest & rhs) {
        buffer = rhs.buffer;
        parseBuffer();
    }

    HTTPRequest & operator=(const HTTPRequest & rhs) {
        if (this != &rhs) {
            buffer = rhs.buffer;
            parseBuffer();
        }
        return *this;
    }



    virtual void parseBuffer() {
        std::string temp = buffer.data();

        // Parse start line
        size_t pos = temp.find("\r\n");
        if (pos == std::string::npos) {
            std::cout<<"Error parsing first line"<<std::endl;
            return;
        }
        startline = temp.substr(0, pos);

        if (DEVELOPMENT) {
            std::cout<<"Startline is: "<<startline<<std::endl;
        }

        readStartLine(startline);

        if (DEVELOPMENT > 1) {
            std::cout<<"After read startline, method is: "<<method<<std::endl;
            std::cout<<"And url is: "<<url<<std::endl;
            std::cout<<"And HTTP version is: "<<HTTPversion<<std::endl<<std::endl;
        }

        temp.erase(0, pos + 2);

        if (DEVELOPMENT > 1) {
            std::cout<<"Erase startline, the rest is: "<<std::endl<<temp<<std::endl;
        }

        // Parse header & get body
        body = readHeader(temp);        
    }

    virtual void readStartLine(std::string line) {
        // Read method
        size_t pos = line.find(" ");
        if (pos == std::string::npos) {
            std::cout<<"Error in reading method"<<std::endl;
            return;
        }

        method = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read url
        pos = line.find(" ");
        if (pos == std::string::npos) {
            std::cout<<"Error in reading url"<<std::endl;
            return;
        }

        url = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read HTTP version
        HTTPversion = line;
        if (HTTPversion != "HTTP/1.1") {
            std::cout<<"HTTP version not normal"<<std::endl;
            return;
        }
    }

    void handlereq(int client_fd) {
        if (method == "GET") {
            doGET(client_fd);
        }
        else if (method == "POST") {
            doPOST(client_fd);
        }
        else if (method == "CONNECT") {
            doCONNECT(client_fd);
        }
    }

    void doGET(int client_fd) {
        HTTPResponse responsefound;

        // Check cache & send request
        std::string bufstr = getBuffer().data();
        if (cache.find(bufstr) != cache.end()) {
            responsefound = cache[bufstr];
        }
        else {
            responsefound = getResponse();
            cache[bufstr] = responsefound;
        }

        if (DEVELOPMENT > 1) std::cout<<"Content sent back is: "<<std::endl<<responsefound.getBuffer().data()<<std::endl;

        // Send back
        int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
    }

    void doPOST(int client_fd) {
        HTTPResponse responsefound;

        // Check cache & send request
        std::string bufstr = getBuffer().data();
        if (cache.find(bufstr) != cache.end()) {
            responsefound = cache[bufstr];
        }
        else {
            responsefound = getResponse();
            cache[bufstr] = responsefound;
        }

        if (DEVELOPMENT) std::cout<<"Content sent back is: "<<std::endl<<responsefound.getBuffer().data()<<std::endl;

        // Send back
        int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
    }

    void doCONNECT(int client_fd) {
        // New socket

        // Then back and force
    }

   HTTPResponse getResponse() {
    // Get hostname
    std::unordered_map<std::string, std::string> reqheader = getheader();
    std::string host = reqheader["Host"];

    if (DEVELOPMENT) std::cout<<"Host is: "<<host<<std::endl;

    int status;
    int web_fd;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    const char *hostname =host.c_str();
    const char *port = "80";

    memset(&host_info, 0 , sizeof(host_info));
    host_info.ai_family   = AF_INET;
    host_info.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(hostname, port, &host_info, &host_info_list);
    if (status != 0) {
        std::cerr << "Error: cannot get address info for host" << std::endl;
    }

    web_fd = socket(host_info_list->ai_family,
                        host_info_list->ai_socktype,
                        host_info_list->ai_protocol);

    if (web_fd == -1) {
        std::cerr << "Error: cannot create socket to the web" << std::endl;
    }

    if (DEVELOPMENT > 2) std::cout << "Connecting to " << hostname << " on port " << port << "..." << std::endl;

    status = connect(web_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        std::cerr << "Error: cannot connect to socket to the web" << std::endl;
    }

    if (DEVELOPMENT > 2) std::cout << "Connected to web" << std::endl;

    int sizesend = send(web_fd, &getBuffer().data()[0], getBuffer().size(), 0);

    if (DEVELOPMENT > 2) std::cout<<"Request send to web: "<<std::endl; 

    std::vector<char> tempbuf = myrecv(web_fd);

    if (DEVELOPMENT) std::cout<<"Buffer received from real server: "<<std::endl<<tempbuf.data()<<std::endl;

    // Construct response
    HTTPResponse ans(tempbuf);

    // Close & clear
    freeaddrinfo(host_info_list);
    close(web_fd);

    return ans;
   }
};




//BEGIN_REF - https://www.youtube.com/watch?v=ojOUIg13g3I&t=543s
class MyLock {
    private:
    std::mutex * mtx;
    public:

    explicit MyLock(std::mutex * temp) {
        temp->lock();
        mtx = temp;
    }

    ~MyLock() {
        mtx->unlock();
    }
};
//END_REF




void handlehttp(int reqfd) {
    // Get request
    std::vector<char> tempbuf = myrecv(reqfd);

    if (DEVELOPMENT) std::cout<<"Temporary buffer got "<<tempbuf.size()<<" bytes."<<std::endl<<"The content is: "<<std::endl<<tempbuf.data()<<std::endl;

    HTTPRequest newreq(tempbuf);

    // Handle request
    newreq.handlereq(reqfd);
}

