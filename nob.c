

#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv)
{
    GO_REBUILD_URSELF(argc, argv);

    char *program = shift(argv, argc);

    Cmd cmd = {0};

    cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "main", "main.c");
    if (!cmd_run(&cmd)) return 1;
    
    if(0 < argc) {
        char *command = shift(argv, argc);
        if(strcmp(command, "run") == 0) {
            cmd_append(&cmd, "./main");
            if (!cmd_run(&cmd)) return 1;
        }
    }

    return 0;
}

