#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PIPE_READ_SIDE 0
#define PIPE_WRITE_SIDE 1

/* TODO update comments */

/**
 * Define a pipe file descriptor type for neater code.
 */
typedef int pipe_fd_t[2];

void check_error(int, char*);
void close_pipes(int, pipe_fd_t*);

int main(int argc, char** argv) {

    pid_t childpid;
    /* Array of three pipe file descriptors. */
    pipe_fd_t pipe_fds[3];
    int num_pipes;
    int return_value;
    int status;
    int i;
    /* The index of the pipe_fd that follows the current filter. */
    int cur_pipe = 0;
    char* pager;

    if (argc > 1) {
        num_pipes = 3;
    } else {
        num_pipes = 2;
    }

    /* Create pipes. */
    for (i = 0; i < num_pipes; ++i) {
        return_value = pipe(pipe_fds[i]);
        check_error(return_value, "Could not initialize pipe.\n");
    }

    /* Create child process that will eventually execute `printenv`. */
    childpid = fork();

    if (0 == childpid) {
        /* Replace stdout med duplicated write side of the pipe. */
        return_value = dup2(pipe_fds[cur_pipe][PIPE_WRITE_SIDE], STDOUT_FILENO);
        check_error(return_value,  "printenv&: Could not duplicate write side of first pipe.\n");
    
        /* printenv should not read from pipe -- close the read side. */
        return_value = close(pipe_fds[cur_pipe][PIPE_READ_SIDE]);
        check_error(return_value,  "Could not close read side of pipe.\n");

        /* stdout now points to write side of pipe -- close duplicated file desc. */
        return_value = close(pipe_fds[cur_pipe][PIPE_WRITE_SIDE]);
        check_error(return_value,  "Could not close write side of pipe.\n");

        /* Execute printenv and give it "printenv" as first parameter. */
        (void) execlp("printenv", "printenv", (char *) 0);

        fprintf(stderr, "Could not execute printenv.\n");
        exit(1);
    }

    check_error(childpid,  "Could not fork printenv.\n");

    /* Forking `printenv` went fine. */
    /* Done with one filter, move cur_pipe one step ahead. */
    ++cur_pipe; /* == 1 */

    /* If arguments where given, use grep with those. */
    if (argc > 1) {
        /* Create child process that will eventually execute `grep`. */
        childpid = fork();

        if (0 == childpid) {
            return_value = dup2(pipe_fds[cur_pipe-1][PIPE_READ_SIDE], STDIN_FILENO);
            check_error(return_value, "grep&: Could not duplicate read side of pipe.\n");

            return_value = close(pipe_fds[cur_pipe-1][PIPE_WRITE_SIDE]);
            check_error(return_value, "grep&: Could not close write side of pipe.");

            return_value = dup2(pipe_fds[cur_pipe][PIPE_WRITE_SIDE], STDOUT_FILENO);
            check_error(return_value, "grep&: Could not duplicate write side of pipe.\n");

            return_value = close(pipe_fds[cur_pipe][PIPE_READ_SIDE]);
            check_error(return_value, "grep&: Could not close read side of second pipe.");
            
            /* digenv is longer than "grep", so no buffer overflow */
            argv[0] = "grep";
            (void) execvp("grep", argv);

            fprintf(stderr, "Could not execute grep.\n");
            exit(1);
        }

        check_error(childpid,  "Could not fork grep.\n");
 
        /* Another filter done -- increment cur_pipe and close pipes. */
        ++cur_pipe;
        close_pipes(cur_pipe, pipe_fds);
    }

    /* Create child process that will eventually execute `sort`. */
    childpid = fork();

    if (0 == childpid) {
        /* Replace stdin with duplicated read side of first pipe. */
        return_value = dup2(pipe_fds[cur_pipe-1][PIPE_READ_SIDE], STDIN_FILENO);
        check_error(return_value, "sort&: Could not duplicate read side of first pipe.\n");

        /* sort should not write to first pipe -- close the write side. */
        return_value = close(pipe_fds[cur_pipe-1][PIPE_WRITE_SIDE]);
        check_error(return_value, "sort&: Could not close write side of first pipe.");

        /* Replace stdout with duplicated write side of second pipe. */
        return_value = dup2(pipe_fds[cur_pipe][PIPE_WRITE_SIDE], STDOUT_FILENO);
        check_error(return_value, "sort&: Could not duplicate write side of second pipe.\n");

        /* sort should not read from second pipe, but stdin -- close the read side. */
        return_value = close(pipe_fds[cur_pipe][PIPE_READ_SIDE]);
        check_error(return_value, "sort&: Could not close read side of second pipe.");

        /* Execute sort and give it "sort" as first parameter. */
        (void) execlp("sort", "sort", (char *) 0);

        fprintf(stderr, "Could not execute sort.\n");
        exit(1);
    }

    check_error(childpid,  "Could not fork sort.\n");

    /* Forking `sort` went fine. */
    /* Another filter forked -- move cur_pipe forward and close pipes if necessary. */
    ++cur_pipe;
    close_pipes(cur_pipe, pipe_fds);

    /* Create child process that will eventually execute `less`. */
    childpid = fork();

    if (0 == childpid) {
        /* Replace stdin with duplicated read side of second pipe. */
        return_value = dup2(pipe_fds[cur_pipe-1][PIPE_READ_SIDE], STDIN_FILENO);
        check_error(return_value, "less&: Could not duplicate read side of pipe.\n");

        /* less should not write to write side of second pipe -- close it. */
        return_value = close(pipe_fds[cur_pipe-1][PIPE_WRITE_SIDE]);
        check_error(return_value, "less&: Could not close write side of pipe.\n");

        /* less uses STDIN_FILENO to read from pipe, so we can close this. */
        return_value = close(pipe_fds[cur_pipe-1][PIPE_READ_SIDE]);
        check_error(return_value, "less&: Could not close write side of pipe.\n");

        pager = getenv("PAGER");
        /* Try first with $PAGER, then "less" and then with "more". */
        (void) execlp(pager, pager, (char *) 0);
        (void) execlp("less", "less", (char *) 0);
        (void) execlp("more", "more", (char *) 0);

        fprintf(stderr, "Could not execute pager.\n");
        exit(1);
    }

    check_error(childpid, "Could not fork.\n");

    /* Forking `less` went fine. */
    /* Another filter forked -- increment cur_pipe and close pipes if necessary. */
    ++cur_pipe;
    close_pipes(cur_pipe, pipe_fds);

    /* Wait for children to exit. */
    for (i = 0; i < num_pipes + 1; ++i) {
        childpid = wait(&status);
        check_error(childpid, "wait() failed unexpectedly.\n");
    }

    exit(0);
}


void check_error(int return_value, char* error_message) {
    if (-1 == return_value) {
        fputs(error_message, stderr);
        exit(1);
    }
}

void close_pipes(int cur_pipe, pipe_fd_t* pipe_fds) {
    int return_value;
    if (cur_pipe >= 2) {
        return_value = close(pipe_fds[cur_pipe-2][PIPE_WRITE_SIDE]);
        check_error(return_value, "Could not close write side of pipe.\n");
        return_value = close(pipe_fds[cur_pipe-2][PIPE_READ_SIDE]);
        check_error(return_value, "Could not close read side of pipe.\n");
    }
}
