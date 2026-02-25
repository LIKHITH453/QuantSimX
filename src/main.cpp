#include <iostream>
#include "OrderBook.h"

int main(int argc, char** argv) {
    std::cout << "QuantSimX scaffold starting...\n";
    OrderBook ob;
    ob.print_snapshot();
    std::cout << "Scaffold ready. Next: implement WebSocket ingestion.\n";
    return 0;
}
