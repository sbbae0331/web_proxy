all: web_proxy

web_proxy: web_proxy.cpp
	g++ -o $@ $< -lpthread

clean:
	rm -f web_proxy

