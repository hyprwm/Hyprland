# compile with HYPRLAND_HEADERS=<path_to_hl> make all
# make sure that the path above is to the root hl repo directory, NOT src/
# and that you have ran `make protocols` in the hl dir.

all:
	$(CXX) -shared -fPIC --no-gnu-unique main.cpp customLayout.cpp customDecoration.cpp -o examplePlugin.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b
clean:
	rm ./examplePlugin.so
