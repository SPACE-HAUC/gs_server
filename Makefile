CXX = g++
COBJS = src/main.o src/gss.o network/network.o network/sha_digest.o
CXXFLAGS = -I ./include/ -I ./network/ -Wall -pthread -DGSNID=\"server\"
EDLDFLAGS = -lpthread -lm -lssl -lcrypto
TARGET = server.out

all: $(COBJS)
	$(CXX) $(CXXFLAGS) $(COBJS) -o $(TARGET) $(EDLDFLAGS)
	./$(TARGET)

%.o: %.c
	$(CXX) $(CXXFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	$(RM) *.out
	$(RM) *.o
	$(RM) src/*.o
	$(RM) network/*.o