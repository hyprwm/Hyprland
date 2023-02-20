all:
	g++ -shared -fPIC --no-gnu-unique main.cpp customLayout.cpp -o examplePlugin.so -g -I "/usr/include/pixman-1" -I "/usr/include/libdrm" -I "../../" -std=c++23
clean:
	rm ./examplePlugin.so
