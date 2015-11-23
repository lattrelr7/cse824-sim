# Setup some aliases to these can be easily altered in the future.
include ../../tos_conf
GCC:=g++
CFLAGS:=-Wall -fpermissive
TOS_TOOLS:= $(TINYOS_ROOT_DIR)/tools/tinyos/c
LFLAGS:= -L$(TOS_TOOLS)/sf
LDFLAGS:= -lsqlite3 -lmote
INCLUDE:=-I $(TOS_TOOLS) -I $(TOS_TOOLS)/sf -I ../
APP_NAME:=tc
SRCS:=$(wildcard *.cpp) $(wildcard *.c) $(wildcard ../*.cpp) $(wildcard ../*.c)
SRCS:=$(SRCS)

OBJS:=$(subst .cpp,.o,$(SRCS))

# Link the object files together into the final executable.
all: ${APP_NAME} copy_files


${APP_NAME}: ${OBJS}
	$(GCC) $(INCLUDE) $(LFLAGS) -o $(APP_NAME) $(OBJS) $(LDFLAGS)
	
copy_files:
	cp ${APP_NAME} ../../

# Compile the individual code files into object files.

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(GCC) $(CFLAGS) -MM $^>>./.depend;	
	
.cpp.o:
	$(GCC) $(INCLUDE) $(CFLAGS) -c -o $@ $<

# Cleanup all of the auto-generated files

clean:
	rm -f ./$(APP_NAME) ../../$(APP_NAME) *.o *~.

dist-clean: clean
	rm -f *~ .depend

include .depend
