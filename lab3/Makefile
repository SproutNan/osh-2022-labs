CCFLAGS ?= -Wall -O2 -D_GNU_SOURCE -luring
all_targets = 5 4 3 2 1

.PHONY: liburing 5

all: $(all_targets)

clean:
	rm -f $(all_targets)

liburing:
	+$(MAKE) -C ./liburing

5:
	g++ 5.cpp -o ./5  ${CCFLAGS}
	
4:
	g++ 4.cpp -o ./4
	
3:
	g++ 3.cpp -o ./3
	
2:
	g++ 2.cpp -o ./2 -lpthread
	
1:
	g++ 1.cpp -o ./1 -lpthread
