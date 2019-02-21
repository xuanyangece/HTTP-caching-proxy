#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <iostream>
#include <netdb.h>
#include <cstring>

#define DEVELOPMENT 1

#define BUFFSIZE 40960


class HTTP {
    protected:
    std::string buffer;
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
        
        if (DEVELOPMENT) std::cout<<"After read header, total "<<header.size()<<" headers"<<std::endl;
        lines.erase(0, 2);
        if (DEVELOPMENT > 1) std::cout<<"Remain body: "<<lines<<std::endl;
        return lines;
    }

    std::unordered_map<std::string, std::string> getheader(){
        return header;
    }

    std::string getBuffer() {
        return buffer;
    }
};

class HTTPRequest: public HTTP {
    private:
    std::string method;
    std::string url;

    public:
    HTTPRequest() {}

    HTTPRequest(std::string temp) {
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
        std::string temp = buffer;
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
            doGET();
        }
        else if (method == "POST") {
            doPOST();
        }
        else if (method == "CONNECT") {
            doCONNECT(client_fd);
        }
    }

    void doGET() {
        // Check cache

        // Send back
    }

    void doPOST() {
        // Check cache

        // Send back
    }

    void doCONNECT(int client_fd) {
        // New socket

        // Then back and force
    }

};

class HTTPResponse: public HTTP {
    private:
    std::string code;
    std::string reason;
    public:
    HTTPResponse() {}

    HTTPResponse(std::string temp) {
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

        std::string temp = buffer;
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


std::unordered_map<std::string, HTTPResponse> cache;

HTTPResponse getResponse(HTTPRequest request) {
    // Get hostname
    std::unordered_map<std::string, std::string> reqheader = request.getheader();
    std::string host = reqheader["Host"];

    // https
    size_t s;
    if (host.find(":443") != std::string::npos) {
        s = host.find(":443");
        host = host.substr(0, s);
    }

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

    int sizesend = send(web_fd, request.getBuffer().c_str(), BUFFSIZE, 0);

    if (DEVELOPMENT > 2) std::cout<<"Request send to web: "<<std::endl; 

    char resbuffer[BUFFSIZE];

    int sizerecv = recv(web_fd, resbuffer, BUFFSIZE, 0);

    if (DEVELOPMENT) std::cout<<"Buffer received from real server: "<<std::endl<<resbuffer<<std::endl;

    // Construct response
    HTTPResponse ans(resbuffer);

    return ans;
}

void handlehttp(int reqfd) {
    // Get request
    char buffer[BUFFSIZE];
    int size = recv(reqfd, buffer, BUFFSIZE, 0);
    if (DEVELOPMENT > 1) {
        std::cout<<"Buffer received in handlehttp: "<<std::endl<<buffer;
    }

    std::string temp(buffer);
    HTTPRequest newreq(temp);

    newreq.handlereq(reqfd);
}

