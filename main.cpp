#include "src/core/editor.h"
#include <iostream>
 
int main(const int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>\n";
        return 1;
    }
    Editor editor(argv[1]);
    editor.run();

    return 0;
}
