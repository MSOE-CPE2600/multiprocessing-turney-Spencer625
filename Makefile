CC=gcc
CFLAGS=-c -Wall -g
LDFLAGS=-ljpeg -lpthread -lm
SOURCES= jpegrw.c mandelmovie.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=mandelmovie

all: $(SOURCES) $(EXECUTABLE) 

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.c.o: 
	$(CC) $(CFLAGS) $< -o $@
	$(CC) -MM $< > $*.d

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE) *.d
