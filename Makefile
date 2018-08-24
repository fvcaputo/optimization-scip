PUZZLES = branches agents
OPT = -m64 -mtune=native -fomit-frame-pointer -O3 -Wall -g
all : $(PUZZLES)

clear :
	rm $(PUZZLES)

% : %.mip.cc easyscip/easyscip.h
	g++ -std=c++11 $< -o $@ $(OPT) -lm -lscip
