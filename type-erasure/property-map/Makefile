all: build
	./property-map

build:
	g++ -std=c++17 -O2 main.cpp -o property-map

debug:
	g++ -std=c++17 -g -O0 main.cpp -o property-map-dbg
	gdb property-map-dbg

clean:
	rm -f property-map property-map-dbg property-map.exe property-map-dbg.exe