# Target library
lib := libfs.a

CC := gcc
CFLAGS := -Wall -Wextra -Werror -MMD 
CFLAGS += -g

objs := fs.o disk.o

ifneq ($(V),1)
Q = @
endif

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

$(lib): $(objs)
	@echo "AR $@"
	$(Q)ar rcs $@ $^

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "clean"
	$(Q)rm -f $(objs) $(deps) $(lib)