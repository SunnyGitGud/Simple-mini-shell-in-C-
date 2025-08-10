# Simple Shell in C++

A Unix shell implementation written in C++ that supports command execution, pipes, I/O redirection, background processes, and built-in commands.
source https://medium.com/@WinnieNgina/guide-to-code-a-simple-shell-in-c-bd4a3a4c41cd

## Features

- **Interactive Command Execution**: Execute system commands with proper process management
- **Pipe Support**: Chain multiple commands using pipes (`|`)
- **I/O Redirection**: Input (`<`), output (`>`), and append (`>>`) redirection
- **Background Processes**: Run commands in background using `&`
- **Built-in Commands**: `cd`, `mkdir`, and `exit`
- **Signal Handling**: Proper cleanup of child processes
- **Terminal Detection**: Different behavior for interactive vs non-interactive mode

## System Calls Used

### Process Management
- **`fork()`**: Creates a new process by duplicating the calling process. Returns 0 in child, child PID in parent, -1 on error.
- **`execvp()`**: Replaces the current process image with a new program. Searches PATH for the executable and handles argument vectors.
- **`wait()`** / **`waitpid()`**: Parent process waits for child process to terminate. Prevents zombie processes and retrieves exit status.
- **`getpid()`** / **`getppid()`**: Get process ID and parent process ID respectively.

### File Operations
- **`open()`**: Opens a file and returns a file descriptor. Used for I/O redirection with flags like O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC, O_APPEND.
- **`close()`**: Closes a file descriptor, releasing system resources.
- **`dup2()`**: Duplicates a file descriptor to a specific descriptor number. Critical for I/O redirection and pipes.
- **`pipe()`**: Creates a unidirectional data channel (pipe) that can be used for inter-process communication.

### Directory Operations
- **`chdir()`**: Changes the current working directory of the calling process.
- **`getcwd()`**: Gets the current working directory path into a buffer.
- **`mkdir()`**: Creates a new directory with specified permissions (mode 0755 = rwxr-xr-x).

### System Information
- **`isatty()`**: Tests whether a file descriptor refers to a terminal device. Used to detect interactive vs batch mode.
- **`getuid()`**: Returns the real user ID of the calling process.
- **`getpwuid()`**: Retrieves user account information from the user database using user ID.
- **`gethostname()`**: Gets the system hostname into a buffer.
- **`getenv()`**: Retrieves the value of an environment variable (used for HOME directory).

### Signal Handling
- **`signal()`**: Sets up signal handlers for specific signals like SIGCHLD.
- **`SIGCHLD`**: Signal sent to parent when child process terminates. Handler prevents zombie processes.

## Architecture

### Main Loop
The shell operates in a read-eval-print loop:
1. Display prompt with user@hostname:cwd format
2. Read user input using `std::getline()`
3. Parse input into tokens using `std::istringstream`
4. Determine command type (built-in, pipe, or external)
5. Execute appropriate handler
6. Repeat until exit command

### Process Creation Pattern
```cpp
pid_t pid = fork();
if (pid == 0) {
    // Child process: setup I/O, exec new program
    execvp(program, args);
} else if (pid > 0) {
    // Parent process: wait for child or continue
    waitpid(pid, &status, 0);
} else {
    // Fork failed
    perror("fork");
}
```

### Pipe Implementation
Pipes use multiple processes connected via file descriptors:
1. Create pipe file descriptors using `pipe()`
2. Fork processes for each command in pipeline
3. Connect stdout of process N to stdin of process N+1 using `dup2()`
4. Close all pipe file descriptors in each process
5. Execute commands with `execvp()`
6. Parent waits for all children to complete

### I/O Redirection
Redirection works by:
1. Parsing command line for `<`, `>`, `>>` operators
2. Opening appropriate files with correct flags
3. Using `dup2()` to redirect STDIN/STDOUT to file descriptors
4. Executing command with modified file descriptors

## Built-in Commands

### `cd [directory]`
- Changes current working directory using `chdir()`
- No argument: changes to HOME directory
- Uses `getenv("HOME")` to find user's home directory

### `mkdir <directories...>`
- Creates one or more directories using `mkdir()`
- Sets permissions to 0755 (rwxr-xr-x)
- Continues creating remaining directories even if one fails

### `exit`
- Terminates the shell gracefully
- Breaks out of main command loop

## Error Handling

- **System Call Failures**: All system calls check return values and use `perror()` for error reporting
- **Process Failures**: Failed `fork()` or `exec()` calls are handled gracefully
- **File Operations**: File open/close operations check for errors and report meaningful messages
- **Signal Safety**: `sigchld_handler` uses `WNOHANG` to avoid blocking

## Signal Management

The shell handles `SIGCHLD` signals to prevent zombie processes:
- Signal handler uses `waitpid(-1, nullptr, WNOHANG)` in a loop
- Reaps all available terminated children without blocking
- Maintains clean process table

## Memory Management

- Uses RAII with C++ containers (`std::vector`, `std::string`)
- Automatic cleanup of dynamically allocated resources
- No manual memory management required for string/vector operations
- File descriptors properly closed after use

## Limitations

- No job control (fg/bg commands)
- No command history or line editing
- No globbing or shell expansion
- No environment variable expansion
- No command substitution
- No conditional execution (`&&`, `||`)
- Basic error recovery

## Compilation

```bash
g++ -o shell shell.cpp -std=c++11
```

## Usage

```bash
./shell
```

The shell will detect if input is from a terminal and provide appropriate interactive features. Non-interactive mode reads commands from stdin until EOF.
