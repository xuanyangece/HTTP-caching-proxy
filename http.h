#include <arpa/inet.h>
#include <iostream>
#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <unordered_map>

#define DEVELOPMENT 1

class HTTP {
protected:
  std::string buffer;
  std::unordered_map<std::string, std::string> header;
  std::string body;

public:
  virtual void parseBuffer() = 0;
  virtual ~HTTP() {}
};

class HTTPRequest : public HTTP {
private:
  std::string method;
  std::string url;

public:
  HTTPRequest(std::string temp) { buffer = temp; }

  virtual void parseBuffer() {
    // Read first line
    size_t pos = buffer.find("\r\n");
    if (pos != std::string::npos) {
    }

    // Read header

    // Read body
  }
};

class HTTPResponse {
private:
  int code;
  std::string readon;
};

// BEGIN_REF - https://www.youtube.com/watch?v=ojOUIg13g3I&t=543s
class MyLock {
private:
  std::mutex *mtx;

public:
  explicit MyLock(std::mutex *temp) {
    temp->lock();
    mtx = temp;
  }

  ~MyLock() { mtx->unlock(); }
};
// END_REF

void handlehttp(int reqfd) {
  // Get request
  char buffer[40960];
  int size = recv(reqfd, buffer, 40960, 0);
  if (DEVELOPMENT) {
    std::cout << "Buffer received in handlehttp: " << std::endl << buffer;
  }
  std::string temp(buffer);
  HTTPRequest newreq(temp);

  // Send request

  // Send response
}
