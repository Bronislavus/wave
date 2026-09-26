#include "Dsp/PpgWaveRom.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: DecodePpgWavetables input-rom output-samples\n";
        return 1;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input)
        return 1;
    std::vector<char> image((std::istreambuf_iterator<char>(input)), {});
    using Decoder = wave::dsp::PpgWaveRom;
    std::vector<int8_t> samples(Decoder::decodedTableCount * Decoder::wavesPerTable
                                * Decoder::samplesPerWave);
    const auto result = Decoder::decode(image.data(), image.size(), samples);
    if (!result.success)
    {
        std::cerr << result.detail << '\n';
        return 1;
    }
    std::ofstream output(argv[2], std::ios::binary);
    output.write(reinterpret_cast<const char*>(samples.data()),
                 static_cast<std::streamsize>(samples.size()));
    if (!output)
        return 1;
    std::cout << "Decoded " << result.decodedTables << " tables ("
              << samples.size() << " signed eight-bit samples).\n";
}
