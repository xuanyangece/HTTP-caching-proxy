#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unordered_map>

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
};

class HTTPRequest: public HTTP {
    private:
    std::string method;
    std::string url;

    public:
    HTTPRequest(std::string temp) {
        buffer = temp;
    }

    virtual void parseBuffer() {
        // Read start line
        size_t pos = buffer.find("\r\n");
        if (pos == std::string::npos) {
            std::cout<<"Error parsing first line"<<std::endl;
            return;
        }
        startline = buffer.substr(0, pos);

        if (DEVELOPMENT) {
            std::cout<<"Startline is: "<<startline<<std::endl;
        }

        readStartLine(startline);

        if (DEVELOPMENT) {
            std::cout<<"After read startline, method is: "<<method<<std::endl;
            std::cout<<"And url is: "<<url<<std::endl;
            std::cout<<"And HTTP version is: "<<HTTPversion<<std::endl;
        }

        pos += 2;

        // Read header

        // Read body
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

class HTTPResponse {
    private:
    int code;
    std::string readon;
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
    char buffer[40960];
    int size = recv(reqfd, buffer, 40960, 0);
    if (DEVELOPMENT) {
        std::cout<<"Buffer received in handlehttp: "<<std::endl<<buffer;
    }

    std::string temp(buffer);
    HTTPRequest newreq(temp);
    newreq.parseBuffer();

    // Send request

    // Send response
}