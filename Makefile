SOURCE	:= Main.cc
SOURCE	+= $(shell find include/ -name "*.cc")
CC      := g++
FLAGS   := -O3 -std=c++17 -w -I include
LD	:= -lprotobuf -lboost_program_options
TARGET  := PCMSim

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(FLAGS) $(SOURCE) -o $(TARGET) $(LD)

clean:
	rm $(TARGET)                                                                                   
