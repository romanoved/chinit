Usage: chinit [-e | --exec] [command]

chinit exec command in new temporary pid namespace.
Closing of new namespace (which kills all descendants)
is guaranteed after normal exit of cmd
or in case of killing any of auxiliary processes
including initially launched process.
