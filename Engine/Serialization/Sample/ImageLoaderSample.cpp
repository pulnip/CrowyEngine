#include <format>
#include <iostream>
#include <print>
#include <string>
#include "ImageLoader.hpp"

int main(void){
    using namespace Crowy;

    std::print("Image File to Load: ");

    std::string str;
    std::getline(std::cin, str);

    auto image = LoadImage(str);
    const auto width = image.width;
    const auto height = image.height;
    const auto channels = GetBytesPerBlock(image.format);

    std::println(
        "Load Result: width={}, height={}, channels={}",
        width, height, channels
    );
}
