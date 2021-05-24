CXX=clang++
CFLAGS += -Weverything
CFLAGS += -Wno-c++98-compat
CFLAGS += -Wno-poison-system-directories
CFLAGS += -Wno-padded
CFLAGS += -Wno-c++98-c++11-c++14-c++17-compat-pedantic
CFLAGS += -std=c++20

CORE_LIBS += IOKit
CORE_LIBS += Foundation
FRAMEWORKS:=$(addprefix -framework , $(CORE_LIBS))

hid_explorer: hid_explorer.cpp hid_explorer.h
	$(CXX) $(CFLAGS)  hid_explorer.cpp -o hid_explorer $(FRAMEWORKS) -I externals/

clean:
	rm -rf hid_explorer
