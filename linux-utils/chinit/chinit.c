#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sched.h>
#include <unistd.h>


int print_help(const char* exec_name)
{
    fprintf(stderr, "Usage: %s cmd [cmd_args]\n", exec_name);
    fprintf(
        stderr,
        "Run cmd in new temporary pid namespace.\n"
        "Closing of new namespace (which kills all descendants)\n"
        "is guaranteed after normal exit of cmd\n"
        "or in case of killing any of auxiliary processes\n"
        "including initially launched process.\n\n"
    );

    fprintf(
        stderr,
        "It works only in case of applicability of PR_SET_PDEATHSIG.\n"
        "The most common cases of unacceptable are:\n"
        "  1. running suid binaries\n"
        "  2. run from thread which dies before process finish\n"
        "This cases can be considered as features\n\n"
    );

    fprintf(
        stderr,
        "Implementation scheme:\n"
        "  root process\n"
        "    unshare(CLONE_NEWPID)\n"
        "    fork\n"
        "    waitpid for child process 1\n"
        "  child process ('init' for new pid namespace):\n"
        "    drop privileges\n"
        "    set prctl(PR_SET_PDEATHSIG, SIGKILL)\n"
        "    execvp\n"
    );

    return EXIT_FAILURE;
}

#define PERROR_EXIT_FAILURE(msg) {perror("chinit: " msg); return EXIT_FAILURE;}

int main (int argc, char* const argv[])
{

    pid_t pid;
    int status = 0;
    uid_t userid = 0;
    gid_t groupid = 0;

    if (argc == 1)
        return print_help(argv[0]);

    /* after unshare(CLONE_NEWPID) fork of current process */
    /* well be an analogue of the init for new pid namespace */

    if (unshare(CLONE_NEWPID)<0)
        PERROR_EXIT_FAILURE("unshare(CLONE_NEWPID) failed");

    /* drop privileges */
    groupid = getgid();
    if (setgid(groupid) != 0)
        PERROR_EXIT_FAILURE("dropping privileges: setgid failed");

    userid = getuid();
    if (setuid(userid) != 0)
        PERROR_EXIT_FAILURE("dropping privileges: setuid failed");

    /* expect SIGKILL in child if parent process fail */

    if ((pid = fork()) < (pid_t)0)
        PERROR_EXIT_FAILURE("fork failed");

    if (pid != (pid_t)0)
    {
        /* parent process waiting for the completion of a child */
        if (waitpid(pid, &status, 0) < 0)
            PERROR_EXIT_FAILURE("waitpid failed");

        if (!WIFEXITED(status))
            return status;

        return WEXITSTATUS(status);
    }

    /* child process */

    if (prctl(PR_SET_PDEATHSIG, SIGKILL))
        PERROR_EXIT_FAILURE("prctl failed");

    ++argv;
    execvp(*argv, argv);

    /* this code should never be reached if execvp successful */

    PERROR_EXIT_FAILURE("execvp failed");
}
