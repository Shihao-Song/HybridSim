SOURCE  := Main.cpp ../PCMSim/Memory_System/pcm_sim_memory_system.cc ../PCMSim/Controller/pcm_sim_controller.cc ../Configs/config.cc ../PCMSim/Array_Architecture/pcm_sim_array.cc ../PCMSim/Trace/pcm_sim_trace.cc src/tags/fa_tags.cc src/replacement_policies/fa_lru.cc

CC      := g++
FLAGS   := -O3 -std=c++11 -w
TARGET  := PCMSim

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(FLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm $(TARGET)                                                                                   
