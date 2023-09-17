#OPT = -O3 
OPT = -O3 -g3 -fPIC  -std=c++0x
CXXFLAGS += $(OPT) 
EXE = .exe
OBJ = .o
RM = rm
CC = g++


#BIN=nox/bin/
#NOX=./nox/ 

#OBJs=\
$(BIN)app$(OBJ)\
$(BIN)flit$(OBJ)\
$(BIN)flit_move$(OBJ)\
$(BIN)link$(OBJ)\
$(BIN)main$(OBJ)\
$(BIN)multimedia$(OBJ)\
$(BIN)nic$(OBJ)\
$(BIN)node_init$(OBJ)\
$(BIN)node_marking$(OBJ)\
$(BIN)route_adaptive$(OBJ)\
$(BIN)route_deterministic$(OBJ)\
$(BIN)route_proximity_aware$(OBJ)\
$(BIN)router$(OBJ)\
$(BIN)router_common$(OBJ)\
$(BIN)route_simple_ft$(OBJ)\
$(BIN)route_west_first$(OBJ)\
$(BIN)sim_init$(OBJ)\
$(BIN)sim_result$(OBJ)\
$(BIN)batch$(OBJ)\
$(BIN)rank$(OBJ)\


OBJs +=\
    bin/cpu_engine$(OBJ)\
    bin/cache$(OBJ)\
    bin/processor$(OBJ)\
    bin/pepsi_init$(OBJ)\
    bin/pepsi_stats$(OBJ)\
    bin/transaction$(OBJ)
    #bin/compress$(OBJ)\


HDRs =\
    cpu_engine.h\
    cache.h\
    defines.h\
    globals.h\
    sharedglobals.h\
    processor.h\
    pepsi_stats.h\
    pepsi_init.h\
    transaction.h


bin/%.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@ -DTR_INTEG

nox/bin/%.o:  nox/%.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@ -DTR_INTEG

all:	bin/casper$(EXE)
#all:	bin/casper$(EXE)

bin/casper$(EXE):  $(OBJs)
		   #$(CC) $(CXXFLAGS) -o $@ $^ -L. -lstdc++ -lm -lz 
#bin/casper$(EXE):  $(OBJs)
#$(CC) $(CXXFLAGS) -o $@ $^ -L. -lstdc++ -lm -lz 

clean:
	$(RM) $(OBJs) 

clobber:	clean
	$(RM) bin/casper$(EXE)

#bin/sim_engine$(OBJ): sim_engine.cpp $(HDRs)

#bin/cache$(OBJ): cache.cpp $(HDRs)

#bin/compress$(OBJ): compress.cpp $(HDRs)

#bin/transaction$(OBJ): transaction.cpp $(HDRs)

#bin/processor$(OBJ): processor.cpp $(HDRs)

#globals.h: cache.h

#transaction.h: defines.h cache.h
