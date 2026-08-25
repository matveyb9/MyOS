#include <myos.h>

int myos_main(uint64_t argc, const char *arguments) {
    myos_write_text("[sdk-hello] Hello from MyOS SDK!\n");

    if (argc == 1U && arguments[0] != '\0') {
        myos_write_text("Arguments: ");
        myos_write_text(arguments);
        myos_write_text("\n");
    }

    return 0;
}
