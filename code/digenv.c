#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PIPE_READ_SIDE 0
#define PIPE_WRITE_SIDE 1

/**
 * Define a pipe file descriptor type for neater code.
 */
typedef int pipe_fd_t[2];

void check_error(int, char*);

int main(int argc, char** argv) {

    pid_t childpid;
    /* Array of three pipe file descriptors. */
    pipe_fd_t pipe_fds[3];
    int return_value;
    int status;
    char* pager;

    /* TODO fixa lite här */
    if (argc == 0) {
    } else {
    }

    /* Create first pipe. */
    return_value = pipe(pipe_fds[0]);
    check_error(return_value,  "Could not initialize first pipe.\n");

    /* Create child process that will eventually execute `printenv`. */
    childpid = fork();

    if (0 == childpid) {
        /* Replace stdout med duplicated write side of the pipe. */
        return_value = dup2(pipe_fds[0][PIPE_WRITE_SIDE], STDOUT_FILENO);
        check_error(return_value,  "printenv&: Could not duplicate write side of first pipe.\n");
    
        /* printenv should not read from pipe -- close the read side. */
        return_value = close(pipe_fds[0][PIPE_READ_SIDE]);
        check_error(return_value,  "Could not close read side of pipe.\n");

        /* stdout now points to write side of pipe -- close duplicated file desc. */
        return_value = close(pipe_fds[0][PIPE_WRITE_SIDE]);
        check_error(return_value,  "Could not close write side of pipe.\n");

        /* Execute printenv and give it "printenv" as first parameter. */
        (void) execlp("printenv", "printenv", (char *) 0);

        fprintf(stderr, "Could not execute printenv.\n");
        exit(1);
    }

    check_error(childpid,  "Could not fork printenv.\n");

    /* Forking `printenv` went fine. */
    /* Create second pipe. */
    return_value = pipe(pipe_fds[1]);
    check_error(return_value, "Could not create second pipe.");

    /* Create child process that will eventually execute `sort`. */
    childpid = fork();

    if (0 == childpid) {
        /* Replace stdin with duplicated read side of first pipe. */
        return_value = dup2(pipe_fds[0][PIPE_READ_SIDE], STDIN_FILENO);
        check_error(return_value, "sort&: Could not duplicate read side of first pipe.\n");

        /* sort should not write to first pipe -- close the write side. */
        return_value = close(pipe_fds[0][PIPE_WRITE_SIDE]);
        check_error(return_value, "Could not close write side of first pipe.");

        /* Replace stdout with duplicated write side of second pipe. */
        return_value = dup2(pipe_fds[1][PIPE_WRITE_SIDE], STDOUT_FILENO);
        check_error(return_value, "sort&: Could not duplicate write side of second pipe.\n");

        /* sort should not read from second pipe, but stdin -- close the read side. */
        return_value = close(pipe_fds[1][PIPE_READ_SIDE]);
        check_error(return_value, "Could not close read side of second pipe.");

        /* Execute sort and give it "sort" as first parameter. */
        (void) execlp("sort", "sort", (char *) 0);

        fprintf(stderr, "Could not execute sort.\n");
        exit(1);
    }

    check_error(childpid,  "Could not fork sort.\n");

    /* Forking `sort` went fine. */
    /* Parent should use none of the first pipe ends -- close them. */
    return_value = close(pipe_fds[0][PIPE_WRITE_SIDE]);
    check_error(return_value, "Could not close write side of first pipe.\n");
    return_value = close(pipe_fds[0][PIPE_READ_SIDE]);
    check_error(return_value, "Could not close read side of first pipe.\n");
    
    /* Create child process that will eventually execute `less`. */
    childpid = fork();

    if (0 == childpid) {
        /* Replace stdin with duplicated read side of second pipe. */
        return_value = dup2(pipe_fds[1][PIPE_READ_SIDE], STDIN_FILENO);
        check_error(return_value, "less&: Could not duplicate read side of second pipe.\n");

        /* less should not write to write side of second pipe -- close it. */
        return_value = close(pipe_fds[1][PIPE_WRITE_SIDE]);
        check_error(return_value, "Could not close write side of second pipe.\n");

        /* less uses STDIN_FILENO to read from pipe, so we can close this. */
        return_value = close(pipe_fds[1][PIPE_READ_SIDE]);
        check_error(return_value, "Could not close write side of second pipe.\n");

        /* Execute less and give it "less" as first parameter. */
        (void) execlp("less", "less", (char *) 0);

        fprintf(stderr, "Could not execute less.\n");
        exit(1);
    }

    check_error(childpid, "Could not fork.\n");

    /* Forking `less` went fine. */
    /* Parent should use none of the second pipe ends either -- close them. */
    return_value = close(pipe_fds[1][PIPE_WRITE_SIDE]);
    check_error(return_value, "Could not close write side of second pipe.\n");
    return_value = close(pipe_fds[1][PIPE_READ_SIDE]);
    check_error(return_value, "Could not close read side of second pipe.\n");

    /* Wait for children to exit. */
    childpid = wait(&status);
    check_error(childpid, "wait() failed unexpectedly.\n");
    childpid = wait(&status);
    check_error(childpid, "wait() failed unexpectedly.\n");
    childpid = wait(&status);
    check_error(childpid, "wait() failed unexpectedly.\n");

    exit(0);
}


void check_error(int return_value, char* error_message) {
    if (-1 == return_value) {
        fputs(error_message, stderr);
        exit(1);
    }
}

