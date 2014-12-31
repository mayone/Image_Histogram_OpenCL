CC = clang++
CFLAGS =
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
	LDFLAGS = -framework OpenCL
else
	LDFLAGS = -lOpenCL
endif
SOURCES = histogram.cpp
OBJECTS = $(SOURCES:.cpp=.o)
EXECUTE = $(SOURCES:.cpp=)

all: $(OBJECTS) $(EXECUTE)

$(EXECUTE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
$(OBJECTS): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -c

run:
	./$(EXECUTE) input
clean:
	rm -rf *~ *.o $(EXECUTE)
