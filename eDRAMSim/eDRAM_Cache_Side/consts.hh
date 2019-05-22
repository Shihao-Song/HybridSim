namespace eDRAMSimulator
{
    class Constants
    {
      public:
        /*
	  64 MB cache size by default but please perform your experiments with
          different size of cache so that you can get a chance to feel how
          size of cache affects your policy
        */
        static const bool WRITE_ONLY = false;
        static const unsigned long long SIZE = 64 * 1024 * 1024; 
        static const unsigned BLOCK_SIZE = 128; // 128 bytes cache block
        static const unsigned NUM_MSHRS = 32;
        static const unsigned NUM_WB_ENTRIES = 32;
        static const unsigned TAG_LOOKUP_LATENCY = 6; // Assume 6 clock cycles
        static const unsigned TICKS_PER_CLK = 1; // Assume 1GHz
    };
}
