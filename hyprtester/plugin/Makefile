CXXFLAGS = -shared -fPIC --no-gnu-unique -g -std=c++2b -Wno-c++11-narrowing
INCLUDES = `pkg-config --cflags pixman-1 libdrm pangocairo libinput libudev wayland-server xkbcommon`
LIBS = `pkg-config --libs pangocairo`

SRC = src/main.cpp
TARGET = hyprtestplugin.so

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -I../.. -I../../protocols $(INCLUDES) $^ $> -o $@ $(LIBS) -O2

clean:
	rm -f ./$(TARGET)

.PHONY: all clean
