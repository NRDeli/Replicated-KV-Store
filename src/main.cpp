#include "node.h"
#include <iostream>

int main()
{
    Node node("wal.log");

    node.recover();

    node.put("x", "100");
    node.put("y", "200");

    std::string value;
    if (node.get("x", value))
    {
        std::cout << "x = " << value << "\n";
    }

    return 0;
}