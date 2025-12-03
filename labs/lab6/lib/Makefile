SRC= printk.c rand.c string.c
OBJ=$(SRC:.c=.o)


%.o:%.c
	$(GCC) $(CFLAG) -c $< -o $@

all: $(OBJ)

clean:
	rm -f $(OBJ)

.PHONY: all clean

# all:printk.o

# printk.o:
# 	$(GCC) $(CFLAG) -c printk.c -o printk.o

# clean: 
# 	rm -f printk.o

# .PHONY: all clean