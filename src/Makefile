FLAGS = -pipe -ggdb -Wall -D_FORTIFY_SOURCE -D_FILE_OFFSET_BITS=64
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

