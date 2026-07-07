all: build
	./plugin_system

build:
	g++ -std=c++17 -O2 -pthread main.cpp -o plugin_system

debug:
	g++ -std=c++17 -g -O0 -pthread main.cpp -o plugin_system-dbg
	gdb plugin_system-dbg

clean:
	rm -f plugin_system plugin_system-dbg plugin_system.exe plugin_system-dbg.exe
