namespace eDRAMSimulator
{
    class Constants
    {
      public:
        /*
	  64 MB cache size by default.
        */
        static const bool WRITE_ONLY = true; // only cache write requests
        static const unsigned long long SIZE = 64 * 1024 * 1024; 
        static const unsigned BLOCK_SIZE = 64; // 64 bytes cache block
        static const unsigned NUM_MSHRS = 32;
        static const unsigned NUM_WB_ENTRIES = 32;
        static const unsigned TAG_LOOKUP_LATENCY = 6; // Assume 6 clock cycles
        static const unsigned TICKS_PER_NS = 3; // Assume 3GHz
    };
}
