myproxy: myproxy.cpp
	g++ -std=c++11 -o myproxy myproxy.cpp

clean:
	rm -f myproxy
