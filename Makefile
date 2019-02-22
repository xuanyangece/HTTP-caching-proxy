myproxy: myproxy.cpp
	g++ -std=c++11 -o myproxy myproxy.cpp -pthread

clean:
	rm -f myproxy
