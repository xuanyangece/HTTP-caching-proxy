#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <iostream>

#define DEVELOPMENT 1


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

                    if (DEVELOPMENT) std::cout<<"Key: "<<key<<" & Value: "<<value<<std::endl;
                }
            }
            else {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 2);
                header[key] = value;

                if (DEVELOPMENT) std::cout<<"Key: "<<key<<" & Value: "<<value<<std::endl;
            }

            lines.erase(0, pos + 2);
        }
        
        if (DEVELOPMENT) std::cout<<"After read header, total "<<header.size()<<" headers"<<std::endl;
        lines.erase(0, 2);
        if (DEVELOPMENT) std::cout<<"Remain body: "<<lines<<std::endl;
        return lines;
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

    std::string getBuffer() {
        return buffer;
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

        if (DEVELOPMENT) {
            std::cout<<"After read startline, method is: "<<method<<std::endl;
            std::cout<<"And url is: "<<url<<std::endl;
            std::cout<<"And HTTP version is: "<<HTTPversion<<std::endl<<std::endl;
        }

        temp.erase(0, pos + 2);

        if (DEVELOPMENT) {
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



};

class HTTPResponse: public HTTP {
    private:
    std::string code;
    std::string reason;
    public:
    HTTPResponse() {}

    HTTPResponse(std::string temp) {
        buffer = temp;
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

    std::string getBuffer() {
        return buffer;
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
    
        if (DEVELOPMENT) {
            std::cout<<"After read startline, HTTP version is: "<<HTTPversion<<std::endl;
            std::cout<<"And code is: "<<code<<std::endl;
            std::cout<<"And reason is: "<<reason<<std::endl<<std::endl;
        }

        temp.erase(0, pos + 2);

        if (DEVELOPMENT) {
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
    HTTPResponse ans;
    return ans;
}

void handlehttp(int reqfd) {
    // Get request
    char buffer[40960];
    int size = recv(reqfd, buffer, 40960, 0);
    if (DEVELOPMENT) {
        std::cout<<"Buffer received in handlehttp: "<<std::endl<<buffer;
    }

    std::string temp(buffer);
    HTTPRequest newreq(temp);

    HTTPResponse responsefound;

    // Check cache
    if (cache.find(newreq.getBuffer()) != cache.end()) {
        responsefound = cache[newreq.getBuffer()];
    }
    else {
        responsefound = getResponse(newreq);
    }

    // Send request

    // Send response
}

