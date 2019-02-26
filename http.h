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
#include <sys/select.h>
#include <unistd.h>
#include <time.h>
#include <fstream>

#define DEVELOPMENT 1
#define LOG "/var/log/erss/proxy.log" // Name and path of the log

#define BUFFSIZE 1024
#define TEMPSIZE 256

std::mutex mymutex;
std::vector<char> myrecv(int myfd);
std::string readAge(std::string control);
std::string computeExpire(std::string checkDate, std::string age_tmp);
std::string getNow();
void return404(int client_fd);

bool isExpire(std::string now, std::string date, std::string seconds);
bool isExpire(std::string now, std::string date);

//BEGIN_REF - https://www.youtube.com/watch?v=ojOUIg13g3I&t=543s
class MyLock
{
  private:
    std::mutex *mtx;

  public:
    explicit MyLock(std::mutex *temp)
    {
        temp->lock();
        mtx = temp;
    }

    ~MyLock()
    {
        mtx->unlock();
    }
};
//END_REF

class HTTP
{
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

    std::string readHeader(std::string lines)
    {
        size_t pos;
        std::string line;
        while (lines.find("\r\n") != std::string::npos && lines.substr(0, lines.find("\r\n")) != "")
        {
            pos = lines.find("\r\n");
            line = lines.substr(0, pos);

            size_t colon = line.find(": ");
            if (colon == std::string::npos)
            {
                colon = line.find(":");
                if (colon == std::string::npos)
                {
                    std::cout << "Error in format of header" << std::endl;
                    return "";
                }
                else
                {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 1);
                    header[key] = value;

                    if (DEVELOPMENT > 1)
                        std::cout << "Key: " << key << " & Value: " << value << std::endl;
                }
            }
            else
            {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 2);
                header[key] = value;

                if (DEVELOPMENT > 1)
                    std::cout << "Key: " << key << " & Value: " << value << std::endl;
            }

            lines.erase(0, pos + 2);
        }

        if (DEVELOPMENT > 1)
            std::cout << "After read header, total " << header.size() << " headers" << std::endl;
        lines.erase(0, 2);
        if (DEVELOPMENT > 1)
            std::cout << "Remain body: " << lines << std::endl;
        return lines;
    }

    std::unordered_map<std::string, std::string> &accessHeader()
    {
        return header;
    }

    std::unordered_map<std::string, std::string> getheader()
    {
        return header;
    }

    std::vector<char> getBuffer()
    {
        return buffer;
    }

    std::string getBody()
    {
        return body;
    }

    std::string getStartLine()
    {
        return startline;
    }

    void reParse()
    {
        std::vector<char> temp;

        // parse startline
        for (size_t i = 0; i < startline.size(); i++)
        {
            temp.push_back(startline[i]);
        }
        temp.push_back('\r');
        temp.push_back('\n');

        // parse header
        for (std::unordered_map<std::string, std::string>::const_iterator it = header.begin(); it != header.end(); ++it)
        {
            std::string key = it->first;
            std::string value = it->second;

            // ""
            for (size_t k = 0; k < key.size(); k++)
            {
                temp.push_back(key[k]);
            }

            // "Host"
            temp.push_back(':');
            temp.push_back(' ');

            // "Host: "
            for (size_t v = 0; v < value.size(); v++)
            {
                temp.push_back(value[v]);
            }

            // "Host: www.example.com"
            temp.push_back('\r');
            temp.push_back('\n');
        }

        temp.push_back('\r');
        temp.push_back('\n');

        for (size_t i = 0; i < body.size(); i++)
        {
            temp.push_back(body[i]);
        }

        temp.push_back('\0');

        buffer = temp;
    }
};

class HTTPResponse : public HTTP
{
  private:
    std::string code;
    std::string reason;

  public:
    HTTPResponse() {}

    HTTPResponse(std::vector<char> temp)
    {
        buffer = temp;
        parseBuffer();
    }

    HTTPResponse(const HTTPResponse &rhs)
    {
        buffer = rhs.buffer;
        parseBuffer();
    }

    HTTPResponse &operator=(const HTTPResponse &rhs)
    {
        if (this != &rhs)
        {
            buffer = rhs.buffer;
            parseBuffer();
        }
        return *this;
    }

    virtual void parseBuffer()
    {

        std::string temp = buffer.data();

        // Parse start line
        size_t pos = temp.find("\r\n");
        if (pos == std::string::npos)
        {
            std::cout << "Error parsing first line" << std::endl;
            return;
        }
        startline = temp.substr(0, pos);

        if (DEVELOPMENT > 1)
        {
            std::cout << "Startline is: " << startline << std::endl;
        }

        readStartLine(startline);

        if (DEVELOPMENT > 1)
        {
            std::cout << "After read startline, HTTP version is: " << HTTPversion << std::endl;
            std::cout << "And code is: " << code << std::endl;
            std::cout << "And reason is: " << reason << std::endl
                      << std::endl;
        }

        temp.erase(0, pos + 2);

        if (DEVELOPMENT > 1)
        {
            std::cout << "Erase startline, the rest is: " << std::endl
                      << temp << std::endl;
        }

        // Parse header & get body
        body = readHeader(temp);
    }

    virtual void readStartLine(std::string line)
    {
        // Read HTTP version
        size_t pos = line.find(" ");
        if (pos == std::string::npos)
        {
            std::cout << "Error in reading method" << std::endl;
            return;
        }

        HTTPversion = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read code
        pos = line.find(" ");
        if (pos == std::string::npos)
        {
            std::cout << "Error in reading url" << std::endl;
            return;
        }

        code = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read reason
        reason = line;
    }

    std::string getCode()
    {
        return code;
    }
};

std::unordered_map<std::string, HTTPResponse> cache;
int checkResponse(HTTPResponse response);
int checkExpire(HTTPResponse response);

class HTTPRequest : public HTTP
{
  private:
    std::string method;
    std::string url;
    std::string port;
    std::string hostaddr;
    std::string IP;
    int ID;
    std::ofstream log;

  public:
    static int amount;

    HTTPRequest(std::vector<char> temp)
    {
        buffer = temp;
        parseBuffer();
        ID = amount;
        amount++;
        log.open(LOG, std::ios::app);
        printInitialMsg();
    }

    HTTPRequest(const HTTPRequest &rhs)
    {
        buffer = rhs.buffer;
        parseBuffer();
        ID = amount;
        amount++;
        log.open(LOG, std::ios::app);
        printInitialMsg();
    }

    HTTPRequest &operator=(const HTTPRequest &rhs)
    {
        if (this != &rhs)
        {
            buffer = rhs.buffer;
            parseBuffer();
            ID = amount;
            amount++;
            log.open(LOG, std::ios::app);
            printInitialMsg();
        }
        return *this;
    }

    ~HTTPRequest()
    {
        log.close();
    }

    void printInitialMsg()
    {
        time_t now;
        struct tm *nowdate;
        time(&now);
        nowdate = gmtime(&now);
        log << ID << ": "
            << "\"" << startline << "\""
            << " from " << IP << " @ " << asctime(nowdate);
    }

    void printReceiving(std::string line) {
        log << ID << ": Responding "<< "\"" << line << "\"" << std::endl;
    }

    void printReceived(std::string line) {
        log << ID << ": Received "<< "\"" << line << "\""<< " from " << hostaddr << std::endl;
    }

    virtual void parseBuffer()
    {
        std::string temp = buffer.data();

        // Parse start line
        size_t pos = temp.find("\r\n");
        if (pos == std::string::npos)
        {
            std::cout << "Error parsing first line" << std::endl;
            return;
        }
        startline = temp.substr(0, pos);

        if (DEVELOPMENT)
        {
            std::cout << "Startline is: " << startline << std::endl;
        }

        readStartLine(startline);

        if (DEVELOPMENT > 1)
        {
            std::cout << "After read startline, method is: " << method << std::endl;
            std::cout << "And url is: " << url << std::endl;
            std::cout << "And HTTP version is: " << HTTPversion << std::endl
                      << std::endl;
        }

        temp.erase(0, pos + 2);

        if (DEVELOPMENT > 1)
        {
            std::cout << "Erase startline, the rest is: " << std::endl
                      << temp << std::endl;
        }

        // Parse header & get body
        body = readHeader(temp);

        // Get port
        std::string host = header["Host"];

        // Keep track of last ':' appears
        int count = 0;
        int last = -1;
        for (size_t i = 0; i < host.size(); i++)
        {
            if (host[i] == ':')
            {
                count++;
                last = i;
            }
        }

        // Update port
        // Kind 1: "abc.com"
        if (host.find("://") == std::string::npos && count == 0)
        {
            port = "80";
            hostaddr = host;
        }
        else if (host.find("://") == std::string::npos && count > 0)
        { // Kind 2: "abc.com:8000"
            port = host.substr(last + 1);
            hostaddr = host.substr(0, last);
        }
        else if (host.find("://") != std::string::npos && count == 1)
        { // Kind 3: "http://abc.com"
            port = "80";
            hostaddr = host;
        }
        else if (host.find("://") != std::string::npos && count > 1)
        { // Kind 4: "http://abc.com:8000"
            port = host.substr(last + 1);
            hostaddr = host.substr(0, last);
        }

        // Update IP
        struct hostent *hent;
        hent = gethostbyname(hostaddr.c_str());
        if (NULL == hent)
        {
            std::cout << "Cannot get IP" << std::endl;
            return;
        }

        char *clientIP;
        // get from h_addr
        clientIP = inet_ntoa(*(struct in_addr *)hent->h_addr);

        IP = clientIP;
    }

    virtual void readStartLine(std::string line)
    {
        // Read method
        size_t pos = line.find(" ");
        if (pos == std::string::npos)
        {
            std::cout << "Error in reading method" << std::endl;
            return;
        }

        method = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read url
        pos = line.find(" ");
        if (pos == std::string::npos)
        {
            std::cout << "Error in reading url" << std::endl;
            return;
        }

        url = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read HTTP version
        HTTPversion = line;
        if (HTTPversion != "HTTP/1.1")
        {
            std::cout << "HTTP version not normal" << std::endl;
            return;
        }
    }

    void handlereq(int client_fd)
    {
        try {
            if (method == "GET")
            {
                doGET(client_fd);
            }
            else if (method == "POST")
            {
                doPOST(client_fd);
            }
            else if (method == "CONNECT")
            {
                doCONNECT(client_fd);
            }
        } catch (const char* msg) {
            return404(client_fd);
            close(client_fd);
        }

    }

    void doGET(int client_fd)
    {
        MyLock lk(&mymutex);
        HTTPResponse responsefound;

        if (cache.find(startline) != cache.end())  // cache has response
        { 
            std::cout << "find in cache! " << std::endl;

            responsefound = cache[startline];

            int checkE = checkExpire(responsefound);

            std::cout << "check Expire: " << checkExpire << std::endl;

            
            std::unordered_map<std::string, std::string> response_header = responsefound.getheader();
            
            //if has expired or needed to revalidate, modify the previous request buffer
            if (checkE == 0 || checkE == 1 || checkE == 3)
            {
                /*
                    LOG VALIDATION - if req contains no-cache
                */
                if (checkContent("no-cache") || checkE == 0) {
                    log<<ID<<": in cache, requires validation"<<std::endl;
                }

                /*
                    LOG EXPIRED - res has "Date" and "max-age"
                */
                else if (checkE == 1) {
                    std::string expireDate = response_header["Date"];
                    log<<ID<<": in cache, but expired at "<<expireDate<<std::endl;
                }

                /*
                    LOG EXPIRED - res has "Expires"
                */
                else if (checkE == 3) {
                    std::string expireDate = response_header["Expires"];
                    log<<ID<<": in cache, but expired at "<<expireDate<<std::endl;
                }

                // Validation process
                if (response_header.find("Etag") == response_header.end() || response_header.find("Last-Modified") == response_header.end())
                {
                    // Get new response from server because validation fail
                    responsefound = getResponse();

                    if (responsefound.getCode() != "200") {
                        printReceiving(responsefound.getStartLine());
                        int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
                        printReceived(responsefound.getStartLine());
                        return;
                    }

                    /*
                        LOG NOT CACHEABLE: max-age/no-store in request header
                    */
                    if (checkContent("max-age=0")) {
                        log<<ID<<": not cacheable because max-age=0 in request header"<<std::endl;
                    }
                    else if (checkContent("no-store")) {
                        log<<ID<<": not cacheable because no-store in request header"<<std::endl;
                    }
                    else {
                        int mycheck = checkResponse(responsefound);
                        int format = checkExpire(responsefound);
                        std::unordered_map<std::string, std::string> temphd = responsefound.getheader();

                        /*
                            LOG NOT CACHEABLE: max-age/no-store in response header
                        */
                        if (mycheck == 1) {

                            log<<ID<<": not cacheable because no-store in response header"<<std::endl;
                        }
                        else if (mycheck == 2) {
                            log<<ID<<": not cacheable because private in response header"<<std::endl;
                        }

                        /*
                            LOG CACHED, BUT RE_VALIDATION - by checking response header
                        */
                        else if (mycheck == 3 || format == 0) {
                            log<<ID<<": cached, but requires re-validation"<<std::endl;
                            cache[startline] = responsefound;
                        }

                        /*
                            LOG CACHED, EXPIRES AT
                        */
                        else if (format == 1 || format == 2) { // has "max-age" and "Date"
                            std::string seconds = readAge(temphd["Cache-Control"]);
                            std::string date = temphd["Date"];
                            std::string expires = computeExpire(date, seconds);

                            log<<ID<<": cached, expires at "<<expires<<std::endl;
                            cache[startline] = responsefound;
                        }
                        else if (format == 3 || 4) { // has "Expires"
                            log<<ID<<": cached, expires at "<<temphd["Expires"]<<std::endl;
                            cache[startline] = responsefound;
                        }

                    }


                    // final send back
                    printReceiving(responsefound.getStartLine());
                    int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
                    printReceived(responsefound.getStartLine());

                    return;
                }

                header["If-None-Match"] = response_header["Etag"];
                header["If-Modified-Since"] = response_header["Last-Modified"];
                reParse();
                getBuffer();

                //send the modified buffer to get a new response and check if validate
                responsefound = getResponse();
                std::string newCode = responsefound.getCode();

                //if is validate, reture the response in cache
                if (newCode.compare("304") == 0)
                {
                    responsefound = cache[startline];

                    printReceiving(responsefound.getStartLine());
                    int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
                    printReceived(responsefound.getStartLine());
                    return;
                }

                //if not validate, return the new 200 ok response and update the cache
                if (newCode.compare("200") == 0)
                {
                    cache[startline] = responsefound;

                    printReceiving(responsefound.getStartLine());
                    int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
                    printReceived(responsefound.getStartLine());
                    return;
                }
            }
            //if is not expired, return it
            else
            {
                /*
                    LOG VALID
                */
                log << ID << ": in cache, valid" << std::endl;
                responsefound = cache[startline];

                printReceiving(responsefound.getStartLine());
                int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
                printReceived(responsefound.getStartLine());
                return;
            }
        }
        else
        { //need to check if could store in cache
            /*
                LOG NOT IN CACHE
            */
            log<<ID<<": not in cache"<<std::endl;

            responsefound = getResponse();

            // If code != 200, go away
            if (responsefound.getCode() != "200") {
                printReceiving(responsefound.getStartLine());
                int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
                printReceived(responsefound.getStartLine());
                return;
            }

            /*
                LOG NOT CACHEABLE: max-age/no-store in request header
            */
            if (checkContent("max-age=0")) {
                log<<ID<<": not cacheable because max-age=0 in request header"<<std::endl;
            }
            else if (checkContent("no-store")) {
                log<<ID<<": not cacheable because no-store in request header"<<std::endl;
            }
            else {
                int mycheck = checkResponse(responsefound);
                int format = checkExpire(responsefound);
                std::unordered_map<std::string, std::string> temphd = responsefound.getheader();

                /*
                    LOG NOT CACHEABLE: max-age/no-store in response header
                */
                if (mycheck == 1) {

                    log<<ID<<": not cacheable because no-store in response header"<<std::endl;
                }
                else if (mycheck == 2) {
                    log<<ID<<": not cacheable because private in response header"<<std::endl;
                }

                /*
                    LOG CACHED, BUT RE_VALIDATION - by checking response header
                */
                else if (mycheck == 3 || format == 0) {
                    log<<ID<<": cached, but requires re-validation"<<std::endl;
                    cache[startline] = responsefound;
                }

                /*
                    LOG CACHED, EXPIRES AT
                */
                else if (format == 1 || format == 2) { // has "max-age" and "Date"
                    std::string seconds = readAge(temphd["Cache-Control"]);
                    std::string date = temphd["Date"];
                    std::string expires = computeExpire(date, seconds);

                    log<<ID<<": cached, expires at "<<expires<<std::endl;
                    cache[startline] = responsefound;
                }
                else if (format == 3 || 4) { // has "Expires"
                    log<<ID<<": cached, expires at "<<temphd["Expires"]<<std::endl;
                    cache[startline] = responsefound;
                }

            }


            // final send back
            printReceiving(responsefound.getStartLine());
            int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
            printReceived(responsefound.getStartLine());
        }
    }

    // Check request has the specific content
    bool checkContent(std::string content) {
        if (header.find("Cache-Control") != header.end()) {
            std::string control = header["Cache-Control"];
            if (control.find(content) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    void doPOST(int client_fd)
    {
        HTTPResponse responsefound = getResponse();
        // Send back
        int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
    }

    void doCONNECT(int client_fd)
    {
        //get hostname
        std::unordered_map<std::string, std::string> reqheader = getheader();
        std::string host = reqheader["Host"];
        size_t s;

        if (s = host.find(":443") != std::string::npos)
        {
            s = host.find(":443");
            host = host.substr(0, s);
        }

        if (DEVELOPMENT == 0)
            std::cout << "Host is: " << host << std::endl;

        int status;
        int web_fd;
        struct addrinfo host_info;
        struct addrinfo *host_info_list;
        const char *hostname = host.c_str();
        const char *port = "443";
        if (DEVELOPMENT == 0)
            std::cout << "hostname is: " << hostname << std::endl;

        memset(&host_info, 0, sizeof(host_info));
        host_info.ai_family = AF_INET;
        host_info.ai_socktype = SOCK_STREAM;

        status = getaddrinfo(hostname, port, &host_info, &host_info_list);
        if (status != 0)
        {
            std::cerr << "Error: cannot get address info for host" << std::endl;
            throw "404 Not Found";
        }
        //build socket
        web_fd = socket(host_info_list->ai_family,
                        host_info_list->ai_socktype,
                        host_info_list->ai_protocol);
        if (web_fd == -1)
        {
            std::cerr << "Error: cannot create socket to the web" << std::endl;
        }

        if (DEVELOPMENT == 0)
            std::cout << "Connecting to " << hostname << "on port" << port << "..." << std::endl;
        //connect host
        status = connect(web_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1)
        {
            std::cerr << "Error: cannot connect to socket to the web" << std::endl;
        }

        if (DEVELOPMENT == 0)
            std::cout << "Connected to web successfully" << std::endl;
        if (DEVELOPMENT == 0)
            std::cout << "need to handle https" << std::endl;
        //send 200 OK info back to browser
        std::string sendOK("HTTP/1.1 200 OK\r\n\r\n");
        int sendConnect = send(client_fd, sendOK.c_str(), strlen(sendOK.c_str()), 0);
        if (sendConnect < 0)
        {
            std::cerr << "Error: connection fail" << std::endl;
        }
        //std::cout << "check send ok" << sendConnect << std::endl;
        if (DEVELOPMENT == 0)
        {
            std::cout << "start to handle https" << std::endl;
            std::cout << "web_fd&resfd: " << web_fd << std::endl;
            std::cout << "client_fd: " << client_fd << std::endl;
        }
        //start select
        fd_set fds;
        while (1)
        {
            FD_ZERO(&fds);
            FD_SET(web_fd, &fds);
            FD_SET(client_fd, &fds);
            if (DEVELOPMENT)
                std::cout << "start to select" << std::endl;
            int checkselect = select(sizeof(fds) * 3, &fds, NULL, NULL, NULL);
            if (checkselect == -1)
            {
                std::cerr << "fail select in connect" << std::endl;
                return;
            }

            if (DEVELOPMENT)
                std::cout << "start select succesfully" << std::endl
                          << "select:" << checkselect << std::endl;
            //if the web send a message, receive and send back to browser
            if (FD_ISSET(web_fd, &fds))
            {
                if (DEVELOPMENT > 1)
                    std::cout << "receive from the web" << std::endl;
                std::vector<char> recv_data(40960);
                std::fill(recv_data.begin(), recv_data.end(), '\0');
                int recvsize = recv(web_fd, &recv_data.data()[0], 40696, 0);
                //rec_mess.push_back('\0');
                //std::cout << "receive from the web successfully" << std::endl;
                //std::cout << "check recv size:" << recvsize << std::endl;
                //std::cout << "check recv size:" << recvsize << std::endl;
                if (recvsize < 0)
                {
                    std::cerr << "Error: fail to receive from web" << std::endl;
                }
                if (recvsize == 0)
                {
                    std::cout << "web close" << std::endl;
                    return;
                }
                recv_data.push_back('\0');
                //std::cout << "trying to sent to the client" << std::endl;
                int checksend = send(client_fd, recv_data.data(), recvsize, MSG_NOSIGNAL);
                //std::cout << "checksend: " << checksend << std::endl;
                if (checksend < 0)
                {
                    //close(client_fd);
                    std::cerr << "cannot send message to web" << web_fd << std::endl;
                }
            }
            //if the browser send a message, receive and send back to web
            else if (FD_ISSET(client_fd, &fds))
            {
                if (DEVELOPMENT)
                    std::cout << "receive from the browser" << std::endl;
                // char rec_mess[40696];
                //memset(&rec_mess,0,40696);
                std::vector<char> recv_data(40960);
                std::fill(recv_data.begin(), recv_data.end(), '\0');
                int recvsize = recv(client_fd, &recv_data.data()[0], 40696, 0);
                //rec_mess.push_back('\0');
                std::cout << "check recv size:" << recvsize << std::endl;
                if (recvsize < 0)
                {
                    std::cerr << "Error: fail to receive from client" << std::endl;
                }
                if (recvsize == 0)
                {
                    std::cout << "browser close" << std::endl;
                    return;
                }
                int checksend = send(web_fd, recv_data.data(), recvsize, MSG_NOSIGNAL);
                std::cout << "checksend" << checksend << std::endl;
                if (checksend < 0)
                {
                    //close(client_fd);
                    std::cerr << "cannot send message to web" << web_fd << std::endl;
                }
            }
        }

        // Then back and force
    }

    HTTPResponse getResponse()
    {
        // Log
        log << ID << ": Requesting "
            << "\"" << startline << "\""
            << " from " << hostaddr << std::endl;

        // Get hostname
        std::unordered_map<std::string, std::string> reqheader = getheader();

        if (DEVELOPMENT)
            std::cout << "Host is: " << hostaddr << std::endl;

        int status;
        int web_fd;
        struct addrinfo host_info;
        struct addrinfo *host_info_list;
        const char *hostname = hostaddr.c_str();
        const char *portnum = port.c_str();

        memset(&host_info, 0, sizeof(host_info));
        host_info.ai_family = AF_INET;
        host_info.ai_socktype = SOCK_STREAM;

        status = getaddrinfo(hostname, portnum, &host_info, &host_info_list);
        if (status != 0)
        {
            std::cerr << "Error: cannot get address info for host" << std::endl;
            throw "404 Not Found";
        }

        web_fd = socket(host_info_list->ai_family,
                        host_info_list->ai_socktype,
                        host_info_list->ai_protocol);

        if (web_fd == -1)
        {
            std::cerr << "Error: cannot create socket to the web" << std::endl;
        }

        if (DEVELOPMENT > 2)
            std::cout << "Connecting to " << hostname << " on port " << portnum << "..." << std::endl;

        status = connect(web_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1)
        {
            std::cerr << "Error: cannot connect to socket to the web" << std::endl;
        }

        if (DEVELOPMENT > 2)
            std::cout << "Connected to web" << std::endl;

        int sizesend = send(web_fd, &getBuffer().data()[0], getBuffer().size(), 0);

        if (DEVELOPMENT > 2)
            std::cout << "Request send to web: " << std::endl;

        std::vector<char> tempbuf = myrecv(web_fd);

        if (DEVELOPMENT > 1)
            std::cout << "Buffer received from real server: " << std::endl
                      << tempbuf.data() << std::endl;

        // Construct response
        HTTPResponse ans(tempbuf);

        // Close & clear
        freeaddrinfo(host_info_list);
        close(web_fd);

        // Log
        // log << ID << ": Responding "
        //     << "\"" << ans.getStartLine() << "\"" << std::endl;
        // log << ID << ": Received "
        //     << "\"" << ans.getStartLine() << "\""
        //     << " from " << hostaddr << std::endl;

        return ans;
    }
};


