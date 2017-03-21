Usage: ./chinit cmd [cmd_args]

Run cmd in new temporary pid namespace.
Closing of new namespace (which kills all descendants)
is guaranteed after normal exit of cmd
or in case of killing any of auxiliary processes
including initially launched process.

It works only in case of applicability of PR_SET_PDEATHSIG.
The most common cases of unacceptable are:
  1. running suid binaries
  2. run from thread which dies before process finish
This cases can be considered as features

Implementation scheme:
  root process
    unshare(CLONE_NEWPID)
    fork
    waitpid for child process 1
  child process ('init' for new pid namespace):
    drop privileges
    set prctl(PR_SET_PDEATHSIG, SIGKILL)
    execvp
