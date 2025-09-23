# PA1: Simple Shell Interface

A simple Unix-like shell written in C++ that demonstrates process creation and execution.  
The program provides a minimal command-line interface where user commands are run in child processes.

## Features
- **Prompt Loop:** Displays `osh>` and waits for user input
- **Process Creation:** Uses `fork()` to create child processes
- **Command Execution:** Executes parsed commands with `execvp()`
- **Foreground/Background Execution:** Supports `&` to run processes concurrently
- **Built-in Exit:** Recognizes `exit` to terminate the shell

## Implementation Details
- **System Calls:** `fork()`, `execvp()`, `waitpid()`
- **Parsing:** Tokenizes user input into arguments, strips newlines, and handles `&`
- **Error Handling:** Relies on `execvp` for invalid command errors

## Project Roadmap
- [x] Read and parse user input
- [x] Implement child process creation with `fork()`
- [x] Add command execution with `execvp()`
- [x] Support background execution (`&`)
- [x] Add built-in `exit` command

## Usage
Compile with:
```
g++ sshell.cpp -o sshell
```
Run with:
```
./sshell
```
Type commands at the `osh>` prompt. Append `&` to run in background. Use `exit` to quit.

## Grading Notes
- Program name: `sshell.c/sshell.cpp`
- Functionality, error handling, and adherence to assignment description are implemented
- Documentation and style included
