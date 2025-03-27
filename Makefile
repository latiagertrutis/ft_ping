.PHONY: all clean distclean

TARGET = ping

SRC = $(addprefix src/,ping.c)

OBJ = $(SRC:.c=.o)

CC = gcc

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	@rm -f $(OBJ)

distclean: clean
	@rm -f $(TARGET)
