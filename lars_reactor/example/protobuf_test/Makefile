##
# main
#
# @file
# @version 0.0.4
CXX=g++
CFLAGS=-g -O2 -Wall -Wno-deprecated

LIB=-lprotobuf
OBJS = $(addsuffix .o, $(basename $(wildcard *.cpp)))

all:
	$(CXX) -o read $(CFLAGS) msg.pb.cc read.cpp $(LIB)
	$(CXX) -o write $(CFLAGS) msg.pb.cc write.cpp $(LIB)

clean:
	-rm -f read write log

# end
