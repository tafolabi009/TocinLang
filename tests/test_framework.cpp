#include "test_framework.h"
#include <cstring>

int main(int argc, char** argv) {
    std::string filter;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            filter = argv[++i];
        }
    }
    
    return tocin::testing::TestRegistry::instance().run_all(filter);
}
