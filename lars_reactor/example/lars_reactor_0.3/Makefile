##
# main
#
# @file
# @version 0.1
CXX=g++
CFLAGS=-g -O2 -Wall -fPIC -Wno-deprecated

INC=-I../../include
LIB=-L../../lib -llreactor -lpthread
OBJS = $(addsuffix .o, $(basename $(wildcard *.cpp)))

all:
	$(CXX) -o server $(CFLAGS) main.cpp $(INC) $(LIB)

clean:
	-rm -f *.o server

# end
