CCP = g++
FLAGS = -pipe -g -O3 -Wall
LDFLAGS = 
FILES := $(wildcard *.cpp)
OBJECTS := $(patsubst %.cpp,%.o,$(FILES))
INCLUDES := $(wildcard ../include/*.h)
BINDIR = ../
BINARY = fastdup

all: build

build: $(OBJECTS)
	@echo "LINK    $(BINARY)"
	@$(CCP) $(FLAGS) $(LDFLAGS) $(OBJECTS) -o $(BINDIR)$(BINARY)

%.o: %.cpp $(INCLUDES)
	@echo "COMPILE $<"
	@$(CCP) $(FLAGS) -I../include/ -c $< -o $@

