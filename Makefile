CPPC=g++
CPPFLAGS=-pedantic -Wall
BIN=ims_proj

all: main.cc
	$(CPPC) $(CPPFLAGS) main.cc -o $(BIN) -lsimlib -D I_REALLY_KNOW_HOW_TO_USE_WAITUNTIL

run:
	./$(BIN)

clean:
	rm -f $(BIN)