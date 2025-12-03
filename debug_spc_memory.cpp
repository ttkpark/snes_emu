// Temporary debug file to dump SPC memory
// Add this to APU::executeSPC700Instruction() after loading SPC program

if (m_spcLoadState == SPC_LOAD_COMPLETE && savedPC == 0x0300) {
    static bool once = true;
    if (once) {
        once = false;
        std::cout << "=== SPC Memory Dump at 0x0300 ===" << std::endl;
        for (int i = 0; i < 32; i++) {
            std::cout << "0x" << std::hex << std::setfill('0') << std::setw(4) << (0x0300 + i) 
                      << ": 0x" << std::setw(2) << (int)readARAM(0x0300 + i) << std::dec << std::endl;
        }
    }
}


