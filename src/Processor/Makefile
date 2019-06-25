SOURCE  := Main.cpp ../PCMSim/Memory_System/pcm_sim_memory_system.cc ../PCMSim/Controller/pcm_sim_controller.cc ../PCMSim/Configs/pcm_sim_config.cc ../PCMSim/Array_Architecture/pcm_sim_array.cc ../PCMSim/Trace/pcm_sim_trace.cc ../eDRAMSim/eDRAM_Cache_Side/eDRAM_cache.cc ../eDRAMSim/eDRAM_Cache_Side/tags/eDRAM_cache_fa_tags.cc ../eDRAMSim/eDRAM_Cache_Side/replacement_policies/eDRAM_cache_fa_lru.cc Core/processor.cc

CC      := g++
FLAGS   := -O3 -std=c++11 -w
TARGET  := PCMSim

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(FLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm $(TARGET)                                                                                   
