all: build
	./thread-pool-demo

build:
	g++ -std=c++17 -O2 -pthread main.cpp -o thread-pool-demo

debug:
	g++ -std=c++17 -g -O0 -pthread main.cpp -o thread-pool-demo-dbg
	gdb thread-pool-demo-dbg

clean:
	rm -f thread-pool-demo thread-pool-demo-dbg thread-pool-demo.exe thread-pool-demo-dbg.exe
