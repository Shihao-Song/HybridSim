#include "System/mmu.hh"

namespace System
{
void MFUPageToNearRows::train(std::vector<const char*> &traces)
{
    int core_id = 0;
    for (auto &training_trace : traces)
    {
        TXTTrace trace(training_trace);

        Instruction instr;
        bool more_insts = trace.getInstruction(instr);
        while (more_insts)
        {
            if (instr.opr == Simulator::Instruction::Operation::LOAD ||
                instr.opr == Simulator::Instruction::Operation::STORE)
            {
                Addr addr = mappers[core_id].va2pa(instr.target_addr);
                
                // Get page ID
                Addr page_id = addr >> Mapper::va_page_shift;
                // Is the page has already been touched?
                if (auto iter = pages.find(page_id);
                         iter != pages.end())
                {
                    ++(iter->second).num_refs;
                }
                else
                {
                    // pages.insert({page_id, {page_id, 0}});

                    // Is this page in the near row region?
                    std::vector<int> dec_addr;
                    dec_addr.resize(mem_addr_decoding_bits.size());

                    Decoder::decode(addr, mem_addr_decoding_bits, dec_addr);
                    int part_id = dec_addr[int(Config::Decoding::Partition)];
                    int row_id = dec_addr[int(Config::Decoding::Row)];
                    unsigned row_id_plus = part_id * num_of_rows_per_partition + row_id;
                }
            }
            more_insts = trace.getInstruction(instr);
        }

        ++core_id;
    }


    Addr addr = 140485259487848;
    Addr page = addr >> Mapper::va_page_shift;
    std::cout << page << "\n";

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
