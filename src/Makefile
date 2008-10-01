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
	@g++ $(FLAGS) $(LDFLAGS) $(OBJECTS) -o $(BINDIR)$(BINARY)

%.o: %.cpp $(INCLUDES)
	@echo "COMPILE $<"
	@g++ $(FLAGS) -I../include/ -c $< -o $@

