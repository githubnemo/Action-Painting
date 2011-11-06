
test: test.cpp
	g++ -o $@ $< $(shell pkg-config --cflags opencv --libs)

mblur: mblur.cpp
	g++ -o $@ $< $(shell pkg-config --cflags opencv --libs) -g

all: test mblur

run: mblur
	./mblur

debug: mblur
	gdb mblur
