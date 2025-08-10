#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sched.h>
#include <sstream>
#include <string>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <vector>
#include <sys/wait.h>
#include <sys/stat.h>  // For mkdir()
#include <fcntl.h>
#include <signal.h>
#include <cstring>

bool makeDir(const std::vector<std::string>& args) {
  if (args.size() < 2) {
    std::cerr << "makeDir errror";
    return false;
  }
  const char* dir = args[1].c_str();
  if(mkdir(dir, 0755) == 0) {
    return true;
  }else {
  perror("mkdir error");
    return false;
  }
}

bool changeDir(const std::vector<std::string>& args){
  const char* dir = nullptr;
  if (args.size() < 2) {
    dir = getenv("HOME");
    if (!dir) dir = "/";
  } else {
  dir = args[1].c_str();
  }

  if (chdir(dir) == 0){
    return true;
  } else {
  perror("chdir failed");
    return false;
  }
}

void executepipes(std::vector<std::vector<std::string>> &commands, bool background) {
  int numCommands = commands.size();
  if (numCommands == 0) return;
  
  int pipefds[2 * (numCommands -1)];

  for (int i = 0; i < numCommands - 1; i++) {
    if (pipe(pipefds + i*2) == -1) {
      perror("pipe");
      return;
    }
  }
  for (int i = 0; i < numCommands; i++) {
    pid_t pid = fork();
    if (pid == 0) {
      if(i != 0) {
        dup2(pipefds[(i-1)*2], STDIN_FILENO);
      } 
      if (i != numCommands -1) {
        dup2(pipefds[i*2 +1], STDOUT_FILENO);
      }

      for (int j =0; j < 2*(numCommands-1); j++) {
        close(pipefds[j]);
      }
      std::vector<char*> args;
      for (auto &arg : commands[i]) args.push_back(const_cast<char*>(arg.c_str()));
      args.push_back(nullptr);

      execvp(args[0], args.data());
      perror("execvp");
      exit(EXIT_FAILURE);
    } else if (pid < 0) {
      perror("fork");
      return;
    }
  }

  for (int i = 0; i < 2*(numCommands-1); i++) {
    close(pipefds[i]);
  }
  
  if (background) {
    std::cout << "[Background pipeline started]\n";
  } else {
    for (int i = 0; i < numCommands; i++) {
      wait(nullptr);
    }
  }
}

void sigchld_handler(int) {
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}
}


void executecommand(std::vector<std::string> args) {
  if (args.empty()) return;

  // Detect background job
  bool background = false;
  if (!args.empty() && args.back() == "&") {
    background = true;
    args.pop_back(); // Remove '&' from arguments
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork failed");
    return;
  }

  if (pid == 0) {  // Child
    std::vector<char*> c_args;
    int in_fd = -1, out_fd = -1;
    std::vector<std::string> filtered_args;

    // Parse args for redirection tokens
    for (size_t i = 0; i < args.size(); ++i) {
      if (args[i] == "<" && i + 1 < args.size()) {
        in_fd = open(args[i + 1].c_str(), O_RDONLY);
        if (in_fd < 0) { perror("open input failed"); _exit(EXIT_FAILURE); }
        ++i; // skip filename
      } else if (args[i] == ">") {
        if (i + 1 >= args.size()) { std::cerr << "No output file\n"; _exit(EXIT_FAILURE); }
        out_fd = open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) { perror("open output failed"); _exit(EXIT_FAILURE); }
        ++i;
      } else if (args[i] == ">>") {
        if (i + 1 >= args.size()) { std::cerr << "No output file\n"; _exit(EXIT_FAILURE); }
        out_fd = open(args[i + 1].c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (out_fd < 0) { perror("open output failed"); _exit(EXIT_FAILURE); }
        ++i;
      } else {
        filtered_args.push_back(args[i]);
      }
    }

    // Redirect input if needed
    if (in_fd != -1) {
      dup2(in_fd, STDIN_FILENO);
      close(in_fd);
    }
    // Redirect output if needed
    if (out_fd != -1) {
      dup2(out_fd, STDOUT_FILENO);
      close(out_fd);
    }

    // Prepare arguments for execvp
    for (auto& arg : filtered_args) {
      c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);

    execvp(c_args[0], c_args.data());
    perror("execvp failed");
    _exit(EXIT_FAILURE);
  } else { // Parent
    if (background) {
      std::cout << "[Background job started] PID: " << pid << "\n";
    } else {
      int status;
      waitpid(pid, &status, 0);
    }
  }
}

std::string readInput() {
  std::string line;
  std::cout << "shell> ";
  getline(std::cin, line);
  return line;
}

std::vector<std::string> parseInput(const std::string &input) {
  std::vector<std::string> tokens;
  std::istringstream stream(input);
  std::string token;

  while (stream >> token) {
    tokens.push_back(token); 
  }
  return tokens;
}

int main() {
  if (isatty(STDIN_FILENO)) {
    std::cout << "Input is from terminal\n";

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
      strcpy(cwd, "unknown");
    }

    struct passwd *pw = getpwuid(getuid());

    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
      strcpy(hostname, "unknown");
    }
    
    std::cout << "User: " << (pw ? pw->pw_name : "unknown") << "\n"
              << "Host: " << hostname << "\n"
              << "CWD: " << cwd << "\n";
    
    signal(SIGCHLD, sigchld_handler);
    while (true) {
      std::string input = readInput();

      if (input.empty()) continue;
      
      std::vector<std::string> tokens = parseInput(input);

      if(tokens.empty()) continue;
    
      if(tokens[0] == "exit") {
        std::cout << "Existind shell. \n";
        break;
      }

      if (tokens[0] == "cd") {
        changeDir(tokens);
        continue;
      }

      if (tokens[0] == "mkdir"){
        makeDir(tokens);
        continue;
      }
      if (input.find('|') != std::string::npos) {
        std::vector<std::vector<std::string>> commands;
        std::stringstream ss(input);
        std::string segment;
        bool background = false;

        while (std::getline(ss, segment, '|')) {
          size_t start = segment.find_first_not_of(" \t");
          size_t end = segment.find_last_not_of(" \t");
          if (start != std::string::npos && end != std::string::npos) {
            segment = segment.substr(start, end - start + 1);
          }
          std::vector<std::string> cmdTokens = parseInput(segment);
          if (!cmdTokens.empty()) {
            if (!cmdTokens.empty() && cmdTokens.back() == "&") {
              background = true;
              cmdTokens.pop_back();
            }
            if (!cmdTokens.empty()) {
              commands.push_back(cmdTokens);
            }
          }
        }
        if (!commands.empty()) {
          executepipes(commands, background);
        }
        continue;
      }
      executecommand(tokens);
      
    }
    } else {
        std::cout << "Input is not from terminal\n";
    }
    return 0;
}
