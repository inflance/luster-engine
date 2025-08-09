// Luster Engine - Main entry point
#include "core/application.hpp"
#include <exception>
#include <cstdlib>

int main() {
    try {
        luster::Application app;
        app.run();
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        // Error handling is done inside Application class
        return EXIT_FAILURE;
    }
}