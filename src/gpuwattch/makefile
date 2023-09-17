TAR = mcpat

.PHONY: dbg opt depend clean clean_dbg clean_opt

OPTFLAGS = -fPIC
CPP = g++
CC = $(shell ls *.cc)
OBJS = $(CC:%.cc=%.o)

%.o: %.cc
	$(CPP) $(OPTFLAGS) $(CXXFLAGS) $(POWER_FLAGS) -o ./obj_opt/$*.o -c $*.cc

all: opt

dbg: $(TAR).mk obj_dbg
	@$(MAKE) TAG=dbg -C . -f $(TAR).mk

opt: $(TAR).mk obj_opt
	@$(MAKE) TAG=opt -C . -f $(TAR).mk

obj_dbg:
	mkdir $@

obj_opt:
	mkdir $@

depend:
	@$(MAKE) TAG=opt -C . -f $(TAR).mk depend

clean: clean_dbg clean_opt clean_cacti

clean_dbg: obj_dbg
	@$(MAKE) TAG=dbg -C . -f $(TAR).mk clean
	rm -rf $<

clean_opt: obj_opt
	@$(MAKE) TAG=opt -C . -f $(TAR).mk clean
	rm -rf $<

clean_cacti: cacti
	rm -rf cacti/obj_opt


