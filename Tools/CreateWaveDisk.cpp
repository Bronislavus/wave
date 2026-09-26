#include "Firmware/DosFloppyImage.h"

#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: CreateWaveDisk output.img setup.set\n";
        return 2;
    }
    const auto result = wave::firmware::DosFloppyImage::createWithWaveSetup(
        juce::File(juce::String::fromUTF8(argv[1])),
        juce::File(juce::String::fromUTF8(argv[2])));
    if (result.failed())
    {
        std::cerr << result.getErrorMessage() << '\n';
        return 1;
    }
    return 0;
}
