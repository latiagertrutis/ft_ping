.PHONY: all clean distclean

TARGET = ft_ping

SRC = $(addprefix src/,ping.c ping_utils.c)
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

CC = gcc

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM -MT '$(@:.d=.o)' $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

include $(DEP)

clean:
	@rm -f $(OBJ) $(DEP)

distclean: clean
	@rm -f $(TARGET)
