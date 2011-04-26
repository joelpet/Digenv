
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main (int argc, char **argv, char **envp) {

    pid_t childpid;
    int i;

    envp[0] = "parent";

    for (i = 0; envp[i] != NULL; ++i) {
        printf("%2d: %s\n", i , envp[i]);
    }

    childpid = fork();

    if (0 == childpid) {
        envp[0] = "child";
        exit(0);
    } 


    printf("%s\n", envp[0]);

}
