#include "app.hpp"

int main() {
    App app = {};
    while (true) {
        if (app.update())
            break;
    }

    return 0;
}