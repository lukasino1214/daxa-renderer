#include "utils.hpp"

#include <daxa/daxa.hpp>
using namespace daxa::types;
#include <fstream>

auto file_to_string(const std::string& file_path) -> std::string {
    std::string content;
    std::ifstream in(file_path, std::ios::in | std::ios::binary);
    if (in) {
        in.seekg(0, std::ios::end);
        u32 size = static_cast<u32>(in.tellg());
        if (size != 0) {
            content.resize(size);
            in.seekg(0, std::ios::beg);
            in.read(&content[0], size);
        } else {
            throw std::runtime_error("file is empty");
        }
    } else {
        throw std::runtime_error("couldnt read a file");
    }
    return content;
}