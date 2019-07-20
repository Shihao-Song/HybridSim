#include "System/mmu.hh"

namespace System
{
void MFUPageToNearRows::train(std::vector<const char*> traces)
{
    Addr addr = 140485259487848;

    std::vector<int> dec_addr;
    dec_addr.resize(mem_addr_decoding_bits.size());

    Decoder::decode(addr, mem_addr_decoding_bits, dec_addr);
    int part_id = dec_addr[int(Config::Decoding::Partition)];
    int row_id = dec_addr[int(Config::Decoding::Row)];
    unsigned row_id_plus = part_id * num_of_rows_per_partition + row_id;
    std::cout << "Row ID PLUS: " << row_id_plus << "\n";

    std::cout << "Rank: " << dec_addr[int(Config::Decoding::Rank)] << "\n";
    std::cout << "Partition: "
              << dec_addr[int(Config::Decoding::Partition)] << "\n";
    std::cout << "Row: " << dec_addr[int(Config::Decoding::Row)] << "\n";
    std::cout << "Col: " << dec_addr[int(Config::Decoding::Col)] << "\n";
    std::cout << "Bank: " << dec_addr[int(Config::Decoding::Bank)] << "\n";
    std::cout << "Channel: " << dec_addr[int(Config::Decoding::Channel)] << "\n\n";
}
}
