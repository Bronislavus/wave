#include "Firmware/M68000.h"

#include <array>
#include <barrier>
#include <cstdint>
#include <iostream>
#include <thread>

namespace
{
struct Bus final : wave::firmware::M68000Bus
{
    std::array<uint8_t, 65536> memory{};
    uint8_t read8(uint32_t address) noexcept override { return memory[address % memory.size()]; }
    void write8(uint32_t address, uint8_t value) noexcept override { memory[address % memory.size()] = value; }
    void word(uint32_t address, uint16_t value)
    {
        write8(address, static_cast<uint8_t>(value >> 8));
        write8(address + 1, static_cast<uint8_t>(value));
    }
    explicit Bus(int seed)
    {
        word(2, 0x8000); // Reset SP.
        word(6, 0x0100); // Reset PC.
        word(0x100, static_cast<uint16_t>(0x7000 | seed)); // MOVEQ #seed,D0
        word(0x102, 0x5280); // ADDQ.L #1,D0
        word(0x104, 0x60fc); // BRA.S $102
    }
};
}

int main()
{
    constexpr int workers = 4;
    constexpr int rounds = 2000;
    std::barrier rendezvous(workers);
    std::array<std::array<uint32_t, rounds>, workers> registers{};
    std::array<std::array<uint32_t, rounds>, workers> counters{};
    std::array<bool, workers> exceptionsPassed{};
    std::array<std::thread, workers> threads;
    for (int i = 0; i < workers; ++i)
        threads[i] = std::thread([&, i]
        {
            Bus bus(10 + i);
            wave::firmware::M68000 cpu;
            // Exercise first-time initialisation on multiple host threads.
            rendezvous.arrive_and_wait();
            cpu.reset(bus);
            for (int round = 0; round < rounds; ++round)
            {
                rendezvous.arrive_and_wait();
                cpu.execute(bus, 512);
                rendezvous.arrive_and_wait();
                // Initialising another instance must not alter saved registers.
                Bus scratchBus(80 + i);
                wave::firmware::M68000 scratch;
                scratch.reset(scratchBus);
                registers[i][round] = cpu.dataRegister(0);
                counters[i][round] = cpu.programCounter();
            }
            Bus exceptionBus(10 + i);
            exceptionBus.word(14, 0x0300); // Address-error vector.
            exceptionBus.word(0x100, 0x3039); // MOVE.W ($201).L,D0: odd address.
            exceptionBus.word(0x102, 0);
            exceptionBus.word(0x104, 0x0201);
            exceptionBus.word(0x300, static_cast<uint16_t>(0x7200 | (40 + i)));
            exceptionBus.word(0x302, 0x4e72); // STOP #$2700
            exceptionBus.word(0x304, 0x2700);
            bool valid = true;
            for (int round = 0; round < rounds; ++round)
            {
                cpu.reset(exceptionBus);
                rendezvous.arrive_and_wait();
                cpu.execute(exceptionBus, 256);
                valid = valid && cpu.dataRegister(1) == static_cast<uint32_t>(40 + i);
            }
            exceptionsPassed[i] = valid;
        });
    for (auto& thread : threads)
        thread.join();
    for (int i = 0; i < workers; ++i)
    {
        if (!exceptionsPassed[i])
        {
            std::cerr << "Concurrent 68000 address-error handlers crossed instances\n";
            return 1;
        }
        Bus bus(10 + i);
        wave::firmware::M68000 reference;
        reference.reset(bus);
        for (int round = 0; round < rounds; ++round)
        {
            reference.execute(bus, 512);
            if (registers[i][round] != reference.dataRegister(0)
                || counters[i][round] != reference.programCounter())
            {
                std::cerr << "Concurrent 68000 instances corrupted register state\n";
                return 1;
            }
        }
    }
    // A host may move a single instance between rendering threads. Register
    // reads on its former thread must use its saved context, not stale TLS.
    Bus migratedBus(33);
    wave::firmware::M68000 migrated;
    migrated.reset(migratedBus);
    migrated.execute(migratedBus, 512);
    uint32_t migratedResult = 0;
    std::thread nextHostThread([&]
    {
        migrated.execute(migratedBus, 512);
        migratedResult = migrated.dataRegister(0);
    });
    nextHostThread.join();
    if (migrated.dataRegister(0) != migratedResult)
    {
        std::cerr << "68000 register snapshot became stale after thread migration\n";
        return 1;
    }
    std::cout << "Concurrent 68000 instances match serial execution\n";
}
