Usage: chinit [options] [command]

chinit is a linux utility that allows to kill a process tree or, in another words, kill all child processes.

More exactly, chinit creates new pid namespace with 'init' process linked with caller.
If one of auxiliary processes terminated, than 'init' process terminates, so whole process tree terminates by kernel.
