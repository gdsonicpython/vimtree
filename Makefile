CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wno-unused-result
LDFLAGS  = -lncurses

vimtree: vimtree.cpp
        $(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
        rm -f vimtree
