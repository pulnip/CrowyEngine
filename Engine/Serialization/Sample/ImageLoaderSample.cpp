#include <format>
#include <iostream>
#include <print>
#include <string>
#include "ImageLoader.hpp"

int main(void){
    std::print("Image File to Load: ");

    std::string str;
    std::getline(std::cin, str);

    auto image = Crowy::loadImage(str);
    const auto width = image.GetWidth();
    const auto height = image.GetHeight();
    const auto channels = image.GetChannels();
    const auto size = image.GetBufferSize();

    std::println(
        "Load Result: width={}, height={}, channels={}\n"
        "size={}, same with w*h*c={}",
        width, height, channels,
        size, size == width * height * channels
    );
}
