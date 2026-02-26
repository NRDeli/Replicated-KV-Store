#pragma once
#include <string>
#include <cstdint>

struct Operation
{
    int64_t index;
    int64_t term;
    std::string key;
    std::string value;
};