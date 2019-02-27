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
#include <list>
#include <utility>

#define TEMPSIZE 33331  // Temporary buffer size for recv
#define CONNECTSIZE 51200   // Buffer size for transmission CONNECT
#define CACHESIZE 128   // Cache size

/*
    Print debug message in specific methods:
    0 - NO PRINT
    1 - parseBuffer & readHeader
    2 - doGET
    3 - doCONNECT
    4 - getResponse
    5 - MyLock
*/
#define HTTPDEVELOPMENT 0

#define LOG "/var/log/erss/proxy.log" // Name and path of the log


std::mutex mymutex;
std::vector<char> myrecv(int myfd);
std::string readAge(std::string control);
std::string computeExpire(std::string checkDate, std::string age_tmp);
std::string getNow();
void return404(int client_fd);
void return400(int client_fd);
void return502(int client_fd);

bool isExpire(std::string now, std::string date, std::string seconds);
bool isExpire(std::string now, std::string date);


/*
    Lock using RAII
*/
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
        if (HTTPDEVELOPMENT == 5) std::cout<<"\n\nLOCKED !!!!!!!!\n\n";
    }

    ~MyLock()
    {
        mtx->unlock();
        if (HTTPDEVELOPMENT == 5) std::cout<<"\n\nUNLOCKED !!!!!!!!\n\n";
    }
};
//END_REF


/*
    HTTP used for HTTPRequest and HTTPResponse
*/
class HTTP
{
  protected:
    std::vector<char> buffer;  // Buffer used for send & recv
    std::string startline;  // Startline
    std::unordered_map<std::string, std::string> header; // Header
    std::string body;   // Body
    std::string HTTPversion;    // HTTPversion

  public:
    virtual void parseBuffer() = 0; // Parse received buffer to generate other fields
    virtual void readStartLine(std::string line) = 0;   // Parse startline
    virtual ~HTTP() {} 

    /*
        Read header into an unordered_map, return body as string
    */
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
                    throw "400";
                }
                else
                {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 1);
                    header[key] = value;

                    if (HTTPDEVELOPMENT == 1)
                        std::cout << "Key: " << key << " & Value: " << value << std::endl;
                }
            }
            else
            {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 2);
                header[key] = value;

                if (HTTPDEVELOPMENT == 1)
                    std::cout << "Key: " << key << " & Value: " << value << std::endl;
            }

            lines.erase(0, pos + 2);
        }

        if (HTTPDEVELOPMENT == 1) std::cout << "After read header, total " << header.size() << " headers" << std::endl;
        
        lines.erase(0, 2);
        
        if (HTTPDEVELOPMENT == 1) std::cout << "Remain body: " << lines << std::endl;

        return lines;
    }

    /*
        Return reference of header
    */
    std::unordered_map<std::string, std::string> &accessHeader()
    {
        return header;
    }

    /*
        Return copy of header
    */
    std::unordered_map<std::string, std::string> getheader()
    {
        return header;
    }

    /*
        Return copy of buffer
    */
    std::vector<char> getBuffer()
    {
        return buffer;
    }

    /*
        Return copy of body
    */
    std::string getBody()
    {
        return body;
    }

    /*
        Return copy of startline
    */
    std::string getStartLine()
    {
        return startline;
    }

    /*
        When content changes, use this to generate new buffer
    */
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

/*
    HTTPResponse class to store info of a response
*/
class HTTPResponse : public HTTP
{
  private:
    std::string code;   // Response code
    std::string reason; // Response reason

  public:
    bool valid; // Flag to determine 502
    
    HTTPResponse() {}

    HTTPResponse(std::vector<char> temp)
    {
        buffer = temp;
        valid = false;
        parseBuffer();
    }

    HTTPResponse(const HTTPResponse &rhs)
    {
        buffer = rhs.buffer;
        valid = false;
        parseBuffer();
    }

    HTTPResponse &operator=(const HTTPResponse &rhs)
    {
        if (this != &rhs)
        {
            buffer = rhs.buffer;
            valid = false;
            parseBuffer();
        }
        return *this;
    }

    /*
        Parse received buffer to generate other fields
    */
    virtual void parseBuffer()
    {
        std::string temp = buffer.data();

        // Parse start line
        size_t pos = temp.find("\r\n");
        if (pos == std::string::npos)
        {
            if (HTTPDEVELOPMENT == 1) std::cout << "Error parsing first line in response" << std::endl;
            return; // this return will make sure valid == false, so we can achieve 502 Bad Response
        }
        startline = temp.substr(0, pos);

        if (HTTPDEVELOPMENT == 1)
        {
            std::cout << "Startline is: " << startline << std::endl;
        }

        readStartLine(startline);

        if (HTTPDEVELOPMENT == 1)
        {
            std::cout << "After read startline, HTTP version is: " << HTTPversion << std::endl;
            std::cout << "And code is: " << code << std::endl;
            std::cout << "And reason is: " << reason << std::endl
                      << std::endl;
        }

        temp.erase(0, pos + 2);

        if (HTTPDEVELOPMENT == 1)
        {
            std::cout << "Erase startline, the rest is: " << std::endl
                      << temp << std::endl;
        }

        // Parse header & get body
        body = readHeader(temp);

        // Set valid
        valid = true;
    }

    /*
        Parse startline
    */
    virtual void readStartLine(std::string line)
    {
        // Read HTTP version
        size_t pos = line.find(" ");
        if (pos == std::string::npos)
        {
            std::cout << "Error in reading method" << std::endl;
            throw "502";
        }

        HTTPversion = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read code
        pos = line.find(" ");
        if (pos == std::string::npos)
        {
            std::cout << "Error in reading url" << std::endl;
            throw "502";
        }

        code = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read reason
        reason = line;
    }

    /*
        Return copy of response code
    */
    std::string getCode()
    {
        return code;
    }
};

/*
    Cache class implementing LRU.
    Size can be specified in CACHESIZE.
    Part of it referred from Leetcode
*/
//BEGIN_REF - https://leetcode.com/problems/lru-cache/discuss/45976/C%2B%2B11-code-74ms-Hash-table-%2B-List
class MyCache {
    private:
    typedef std::list<std::string> LIST;    // List of response
    typedef std::pair<HTTPResponse, LIST::iterator> PAIR;   // Data entry for the map, first is response and second is it's position in the used list
    typedef std::unordered_map<std::string, PAIR> MAP;  // Map to map startline to (reponse + its position in the list)

    LIST used;
    MAP cache;
    size_t _capacity;

    // Renew order for LRU
    void touch(MAP::iterator it) {
        std::string key = it->first;

        // change used
        used.erase(it->second.second);
        used.push_front(key);

        it->second.second = used.begin();
    }

    public:
    MyCache(size_t capacity) : _capacity(capacity) {}

    // Check whether response exist
    bool checkExist(std::string startline) {
        auto it = cache.find(startline);
        if (it == cache.end()) return false;
        else return true;
    }

    // Get response
    HTTPResponse get(std::string startline) {
        auto it = cache.find(startline);
        touch(it);
        return it->second.first;
    }

    // Put new response
    void put(std::string startline, HTTPResponse res) {
        auto it = cache.find(startline);
        if (it != cache.end()) touch(it);
        else {
            if (used.size() == _capacity) {
                cache.erase(used.back());
                used.pop_back();
            }
            used.push_front(startline);
        }
        cache[startline] = {res, used.begin()};
    }
};
//END_REF


MyCache cache(CACHESIZE);
int checkResponse(HTTPResponse response);
int checkExpire(HTTPResponse response);

/*
    HTTPRequest class to store info of a request
*/
class HTTPRequest : public HTTP
{
  private:
    int ID; // ID of request
    std::string method; // Method - GET POST CONNECT
    std::string url;    // URL
    std::string port;   // PORT
    std::string hostaddr;   // Target host address
    std::string IP; // Target host IP address
    std::ofstream log;  // Log helper

  public:
    static int amount;  // Total amount of request to generate ID


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

    /*
        Print initial message when get request
    */
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

    /*
        Print log when responding
    */
    void printReceiving(std::string line) {
        log << ID << ": Responding "<< "\"" << line << "\"" << std::endl;
    }

    /*
        Print log when received
    */
    void printReceived(std::string line) {
        log << ID << ": Received "<< "\"" << line << "\""<< " from " << hostaddr << std::endl;
    }

    /*
        Parse buffer to get info about header, body and address info
    */
    virtual void parseBuffer()
    {
        std::string temp = buffer.data();

        // Parse start line
        size_t pos = temp.find("\r\n");
        if (pos == std::string::npos)
        {
            if (HTTPDEVELOPMENT == 1) std::cout << "Error parsing first line in request" << std::endl;
            // THROW INFO
            throw "400";
        }
        startline = temp.substr(0, pos);

        if (HTTPDEVELOPMENT == 1) std::cout << "Startline is: " << startline << std::endl;

        readStartLine(startline);

        temp.erase(0, pos + 2);

        if (HTTPDEVELOPMENT == 1) std::cout << "\nErase startline, the rest is: " << std::endl << temp << std::endl<< std::endl;

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
        if (hent == NULL)
        {
            std::cout << "Cannot get IP" << std::endl;
            throw "404 Not Found";
        }

        char *clientIP;
        // get from h_addr
        clientIP = inet_ntoa(*(struct in_addr *)hent->h_addr);

        IP = clientIP;
    }

    /*
        Read start line
    */
    virtual void readStartLine(std::string line)
    {
        // Read method
        size_t pos = line.find(" ");
        if (pos == std::string::npos)
        {
            std::cout << "Error in reading method" << std::endl;
            throw "400";
        }

        method = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read url
        pos = line.find(" ");
        if (pos == std::string::npos)
        {
            std::cout << "Error in reading url" << std::endl;
            throw "400";
        }

        url = line.substr(0, pos);
        line.erase(0, pos + 1);

        // Read HTTP version
        HTTPversion = line;
        if (HTTPversion != "HTTP/1.1")
        {
            std::cout << "Bad protocol format" << std::endl;
            throw "400";
        }
    }

    /*
        Just handle supported method: GET POST CONNECT
    */
    void handlereq(int client_fd)
    {
        try {
            // Method supported
            if (method == "GET")
            {
                MyLock lk(&mymutex);
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
            
            // Method not supported, bad request
            else {
                return400(client_fd);
                close(client_fd);
            }
        } catch (const char* msg) {
            // Catch 404 not found
            return404(client_fd);
            close(client_fd);
        }

    }


    /*
        Method to handle GET
    */
    void doGET(int client_fd)
    {
        HTTPResponse responsefound;

        if (cache.checkExist(startline))  // cache has response
        { 
            if (HTTPDEVELOPMENT == 2) std::cout << "find in cache! " << std::endl;

            responsefound = cache.get(startline);

            int checkE = checkExpire(responsefound);

            if (HTTPDEVELOPMENT == 2) std::cout << "check Expire: " << checkExpire << std::endl;

            
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
                            cache.put(startline, responsefound);
                        }

                        /*
                            LOG CACHED, EXPIRES AT
                        */
                        else if (format == 1 || format == 2) { // has "max-age" and "Date"
                            std::string seconds = readAge(temphd["Cache-Control"]);
                            std::string date = temphd["Date"];
                            std::string expires = computeExpire(date, seconds);

                            log<<ID<<": cached, expires at "<<expires<<std::endl;
                            cache.put(startline, responsefound);
                        }
                        else if (format == 3 || 4) { // has "Expires"
                            log<<ID<<": cached, expires at "<<temphd["Expires"]<<std::endl;
                            cache.put(startline, responsefound);
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
                    responsefound = cache.get(startline);

                    printReceiving(responsefound.getStartLine());
                    int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
                    printReceived(responsefound.getStartLine());
                    return;
                }

                //if not validate, return the new 200 ok response and update the cache
                if (newCode.compare("200") == 0)
                {
                    cache.put(startline, responsefound);

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
                responsefound = cache.get(startline);

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

            /*
                CHECK BAD RESPONSE
            */
            responsefound = getResponse();
            if (responsefound.valid == false) {
                std::cout<<" Response error"<<std::endl;
                return502(client_fd);
                close(client_fd);
                return;
            }
            

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
                    cache.put(startline, responsefound);
                }

                /*
                    LOG CACHED, EXPIRES AT
                */
                else if (format == 1 || format == 2) { // has "max-age" and "Date"
                    std::string seconds = readAge(temphd["Cache-Control"]);
                    std::string date = temphd["Date"];
                    std::string expires = computeExpire(date, seconds);

                    log<<ID<<": cached, expires at "<<expires<<std::endl;
                    cache.put(startline, responsefound);
                }
                else if (format == 3 || 4) { // has "Expires"
                    log<<ID<<": cached, expires at "<<temphd["Expires"]<<std::endl;
                    cache.put(startline, responsefound);
                }

            }


            // final send back
            printReceiving(responsefound.getStartLine());
            int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
            printReceived(responsefound.getStartLine());
        }
    }

    /*
        Check request has the specific content
    */
    bool checkContent(std::string content) {
        if (header.find("Cache-Control") != header.end()) {
            std::string control = header["Cache-Control"];
            if (control.find(content) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    /*
        Method to handle POST
    */
    void doPOST(int client_fd)
    {
        HTTPResponse responsefound = getResponse();
        // Send back
        int ret = send(client_fd, &responsefound.getBuffer().data()[0], responsefound.getBuffer().size(), 0);
    }


    /*
        Method to handle CONNECT
    */
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

        if (HTTPDEVELOPMENT == 3) std::cout << "Host is: " << host << std::endl;

        int status;
        int web_fd;
        struct addrinfo host_info;
        struct addrinfo *host_info_list;
        const char *hostname = host.c_str();
        const char *port = "443";


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
            std::cerr << "Error: cannot create socket to the server" << std::endl;
            throw "Error creating socket to the server";
        }

        if (HTTPDEVELOPMENT == 3) std::cout << "Connecting to " << hostname << "on port" << port << "..." << std::endl;
        
        //connect host
        status = connect(web_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1)
        {
            std::cerr << "Error: cannot connect to socket to the server" << std::endl;
            throw "Error connecting socket to the server";
        }

        if (HTTPDEVELOPMENT == 3) std::cout << "Connected to web successfully" << std::endl;

        //send 200 OK info back to browser
        std::string sendOK("HTTP/1.1 200 OK\r\n\r\n");
        int sendConnect = send(client_fd, sendOK.c_str(), strlen(sendOK.c_str()), 0);
        if (sendConnect < 0)
        {
            std::cerr << "Error: connection fail" << std::endl;
            throw "Error sending 200 OK to client";
        }

        if (HTTPDEVELOPMENT == 3) 
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

            if (HTTPDEVELOPMENT == 3) std::cout << "start to select" << std::endl;

            int checkselect = select(sizeof(fds) * 3, &fds, NULL, NULL, NULL);
            if (checkselect == -1)
            {
                std::cerr << "Fail select in connect" << std::endl;
                throw "Fail select in CONNECT";
            }

            if (HTTPDEVELOPMENT == 3) std::cout << "start select succesfully" << std::endl << "select:" << checkselect << std::endl;
            
            //if the web send a message, receive and send back to browser
            if (FD_ISSET(web_fd, &fds))
            {
                if (HTTPDEVELOPMENT == 3) std::cout << "receive from the web" << std::endl;


                std::vector<char> recv_data(CONNECTSIZE);
                std::fill(recv_data.begin(), recv_data.end(), '\0');
                int recvsize = recv(web_fd, &recv_data.data()[0], CONNECTSIZE, 0);

                if (recvsize < 0)
                {
                    std::cerr << "Error: fail to receive from server" << std::endl;
                    throw "Error receiving message from server in CONNECT";
                }

                if (recvsize == 0)
                {
                    if (HTTPDEVELOPMENT == 3) std::cout << "web close" << std::endl;
                    return;
                }

                recv_data.push_back('\0');

                if (HTTPDEVELOPMENT == 3) std::cout << "trying to sent to the client" << std::endl;
                int checksend = send(client_fd, recv_data.data(), recvsize, MSG_NOSIGNAL);
                if (HTTPDEVELOPMENT == 3) std::cout << "checksend: " << checksend << std::endl;

                if (checksend < 0)
                {
                    std::cerr << "cannot send message to server" << web_fd << std::endl;
                    throw "Error sending message back to server in CONNECT";
                }
            }
            //if the browser send a message, receive and send back to web
            else if (FD_ISSET(client_fd, &fds))
            {
                if (HTTPDEVELOPMENT == 3) std::cout << "receive from the browser" << std::endl;

                std::vector<char> recv_data(CONNECTSIZE);
                std::fill(recv_data.begin(), recv_data.end(), '\0');
                int recvsize = recv(client_fd, &recv_data.data()[0], CONNECTSIZE, 0);

                if (HTTPDEVELOPMENT == 3) std::cout << "check recv size:" << recvsize << std::endl;
                if (recvsize < 0)
                {
                    std::cerr << "Error: fail to receive from client" << std::endl;
                    throw "Error receiving message from client in CONNECT";
                }

                if (recvsize == 0)
                {
                    if (HTTPDEVELOPMENT == 3) std::cout << "browser close" << std::endl;
                    return;
                }

                int checksend = send(web_fd, recv_data.data(), recvsize, MSG_NOSIGNAL);
                if (HTTPDEVELOPMENT == 3) std::cout << "checksend" << checksend << std::endl;

                if (checksend < 0)
                {
                    //close(client_fd);
                    std::cerr << "cannot send message to web" << web_fd << std::endl;
                    throw "Error sending message back to client in CONNECT";
                }
            }
        }

    }

    /*
        Get a HTTP response from server
    */
    HTTPResponse getResponse()
    {
        // Log
        log << ID << ": Requesting "
            << "\"" << startline << "\""
            << " from " << hostaddr << std::endl;

        // Get hostname
        std::unordered_map<std::string, std::string> reqheader = getheader();

        if (HTTPDEVELOPMENT == 4) std::cout << "Host is: " << hostaddr << std::endl;

        // Socket stuff
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
            throw "404 Not Found - cannot get host info";
        }

        web_fd = socket(host_info_list->ai_family,
                        host_info_list->ai_socktype,
                        host_info_list->ai_protocol);

        if (web_fd == -1)
        {
            std::cerr << "Error: cannot create socket to the web" << std::endl;
            throw "404 Not found - cannot create socket to web";
        }

        if (HTTPDEVELOPMENT == 4) std::cout << "Connecting to " << hostname << " on port " << portnum << "..." << std::endl;

        status = connect(web_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if (status == -1)
        {
            std::cerr << "Error: cannot connect to socket to the web" << std::endl;
            throw "404 Not Found - cannot connect socket to web";
        }

        if (HTTPDEVELOPMENT == 4) std::cout << "Connected to web" << std::endl;

        int sizesend = send(web_fd, &getBuffer().data()[0], getBuffer().size(), 0);

        if (HTTPDEVELOPMENT == 4) std::cout << "Request send to web: " << std::endl;

        std::vector<char> tempbuf = myrecv(web_fd);

        if (HTTPDEVELOPMENT == 4) std::cout << "Buffer received from real server: " << std::endl << tempbuf.data() << std::endl;

        if (tempbuf.size() < 2) {
            throw "Empty response in getResponse";
        }

        // Construct response
        HTTPResponse ans(tempbuf);

        // Close & clear
        freeaddrinfo(host_info_list);
        close(web_fd);

        return ans;
    }
};




