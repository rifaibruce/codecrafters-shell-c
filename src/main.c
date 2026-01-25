#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

extern char *xmalloc PARAMS((size_t));

char *builtin_commands[] = {"echo", "exit", "type", "cd", "pwd", NULL};

typedef struct Command {
  char **argv;
  char *output_file_name;
  int output_type_flag;
  int append_flag;
} Command;

char *dupstr(char *s) {
  char *r;

  r = xmalloc(strlen(s) + 1);
  strcpy(r, s);
  return (r);
}

void parse_command(char *input, Command *cmd) {
  int argv_idx = 0;
  int input_len = strlen(input);
  char token[1024];
  int token_idx = 0;
  int quote_mode_flag = 0;
  int expect_output_flag = 0;

  for (int i = 0; i < input_len; i++) {
    if (quote_mode_flag == 1) {
      if (input[i] == '\'') {
        quote_mode_flag = 0;
      } else {
        token[token_idx++] = input[i];
      }
    } else if (quote_mode_flag == 2) {
      if (input[i] == '\"') {
        quote_mode_flag = 0;
      } else if (input[i] == '\\') {
        if (input[i + 1]) {
          char next_c = input[i + 1];
          if (next_c == '\"' || next_c == '\\' || next_c == '$' ||
              next_c == '`' || next_c == '\n') {
            token[token_idx++] = next_c;
            i++;
          } else {
            token[token_idx++] = '\\';
          }
        }
      } else {
        token[token_idx++] = input[i];
      }
    } else {
      if (input[i] == '\'') {
        quote_mode_flag = 1;
      } else if (input[i] == '\"') {
        quote_mode_flag = 2;
      } else if (input[i] == ' ') {
        if (token_idx > 0) {
          token[token_idx] = '\0';
          if (strcmp(token, ">") == 0 || strcmp(token, "1>") == 0 ||
              strcmp(token, "2>") == 0) {
            if (strcmp(token, "2>") == 0) {
              cmd->output_type_flag = 2;
            } else {
              cmd->output_type_flag = 1;
            }
            expect_output_flag = 1;
          } else if (strcmp(token, ">>") == 0 || strcmp(token, "1>>") == 0 ||
                     strcmp(token, "2>>") == 0) {
            if (strcmp(token, "2>>") == 0) {
              cmd->output_type_flag = 2;
            } else {
              cmd->output_type_flag = 1;
            }
            cmd->append_flag = 1;
            expect_output_flag = 1;
          } else if (expect_output_flag == 1) {
            cmd->output_file_name = strdup(token);
            expect_output_flag = 0;
          } else {
            cmd->argv[argv_idx++] = strdup(token);
          }
          token_idx = 0;
        }
      } else if (input[i] == '\\') {
        if (input[i + 1]) {
          char next_c = input[i + 1];
          token[token_idx++] = next_c;
          i++;
        }
      } else {
        token[token_idx++] = input[i];
      }
    }
  }

  if (token_idx > 0) {
    token[token_idx] = '\0';
    if (strcmp(token, ">") == 0 || strcmp(token, "1>") == 0 ||
        strcmp(token, "2>") == 0) {
      if (strcmp(token, "2>") == 0) {
        cmd->output_type_flag = 2;
      } else {
        cmd->output_type_flag = 1;
      }
      expect_output_flag = 1;
    } else if (strcmp(token, ">>") == 0 || strcmp(token, "1>>") == 0 ||
               strcmp(token, "2>>") == 0) {
      if (strcmp(token, "2>>") == 0) {
        cmd->output_type_flag = 2;
      } else {
        cmd->output_type_flag = 1;
      }

      cmd->append_flag = 1;
      expect_output_flag = 1;

    } else if (expect_output_flag == 1) {
      cmd->output_file_name = strdup(token);
      expect_output_flag = 0;
    } else {
      cmd->argv[argv_idx++] = strdup(token);
    }
  }
  cmd->argv[argv_idx] = NULL;
}

void handle_echo(char **argv) {
  int argv_idx = 1;
  while (argv[argv_idx] != NULL) {
    printf("%s", argv[argv_idx]);
    if (argv[argv_idx + 1] != NULL) {
      printf(" ");
    }
    argv_idx++;
  }
  printf("\n");
}

int find_path(char *command, char *buffer) {
  const char *path_var = getenv("PATH");
  if (path_var == NULL)
    return -1;
  char path_var_dup[PATH_MAX];

  strcpy(path_var_dup, path_var);

  char *path_var_saveptr;
  char *path_token = strtok_r(path_var_dup, ":", &path_var_saveptr);

  while (path_token != NULL) {
    char full_path[PATH_MAX];
    if (path_token[strlen(path_token) - 1] == '/') {
      strcpy(full_path, path_token);
      strcat(full_path, command);
      int access_status = access(full_path, X_OK);
      if (access_status == 0) {
        strcpy(buffer, full_path);
        return 1;
      }
    } else {
      strcpy(full_path, path_token);
      strcat(full_path, "/");
      strcat(full_path, command);
      int access_status = access(full_path, X_OK);
      if (access_status == 0) {
        strcpy(buffer, full_path);
        return 1;
      }
    }
    path_token = strtok_r(NULL, ":", &path_var_saveptr);
  }

  return -1;
}

void handle_type(char **argv) {
  if (argv[1] == NULL) {
    return;
  }
  char path[PATH_MAX];
  if (strcmp(argv[1], "echo") == 0 || strcmp(argv[1], "exit") == 0 ||
      strcmp(argv[1], "type") == 0 || strcmp(argv[1], "pwd") == 0) {
    printf("%s is a shell builtin\n", argv[1]);
  } else if (find_path(argv[1], path) == 1) {
    printf("%s is %s\n", argv[1], path);
  } else {
    printf("%s: not found\n", argv[1]);
  }
}

int handle_exec(char **argv) {
  char path[PATH_MAX];
  if (find_path(argv[0], path) == 1) {
    pid_t parent_pid = getpid();
    pid_t child_pid = fork();

    if (child_pid == -1) {
      perror("fork failed");
      return -1;
    } else if (child_pid > 0) {
      int status;
      waitpid(child_pid, &status, 0);
    } else {
      execv(path, argv);
      exit(EXIT_FAILURE);
    }
    return 1;
  }
  return -1;
}

void handle_cd(char **argv) {
  const char *home_env = getenv("HOME");
  char cwd[PATH_MAX];
  char expanded_path[PATH_MAX];
  char *target_dir;

  if (argv[1] == NULL || strcmp(argv[1], "") == 0 ||
      strcmp(argv[1], "~") == 0) {
    if (chdir(home_env) == 0) {
      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        setenv("PWD", cwd, 1);
      }
      return;
    } else {
      perror("CHDIR failed");
      return;
    }
  }

  if (argv[1][0] == '~') {
    snprintf(expanded_path, sizeof(expanded_path), "%s%s", home_env,
             argv[1] + 1);
    target_dir = expanded_path;
  } else {
    target_dir = argv[1];
  }
  if (chdir(target_dir) == 0) {
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      setenv("PWD", cwd, 1);
      return;
    }
  }
  printf("cd: %s: No such file or directory\n", argv[1]);
}

char *command_generator(const char *text, int state) {
  static int list_index, len;
  static char *path_cpy = (char *)NULL, *current_path_ptr = (char *)NULL;
  static DIR *current_dir = (DIR *)NULL;
  char *name;
  static char *path_token = (char *)NULL;

  if (!state) {
    list_index = 0;
    len = strlen(text);
    if (path_cpy) {
      free(path_cpy);
      path_cpy = (char *)NULL;
    }
    if (current_dir) {
      closedir(current_dir);
      current_dir = (DIR *)NULL;
    }
    path_token = (char *)NULL;
  }

  while (name = builtin_commands[list_index]) {
    list_index++;
    if (strncmp(name, text, len) == 0) {
      return (dupstr(name));
    }
  }

  if (!path_cpy) {
    path_cpy = strdup(getenv("PATH"));
    path_token = strtok_r(path_cpy, ":", &current_path_ptr);
  }

  while (1) {
    if (!current_dir) {
      if (!path_token) {
        path_token = strtok_r(NULL, ":", &current_path_ptr);
      }

      if (!path_token)
        return (char *)NULL;

      current_dir = opendir(path_token);
      path_token = (char *)NULL;
      if (!current_dir)
        continue;
    }

    struct dirent *file = readdir(current_dir);

    if (!file) {
      closedir(current_dir);
      current_dir = (DIR *)NULL;
      continue;
    }

    if (strncmp(file->d_name, text, len) == 0)
      return ((dupstr(file->d_name)));
  }

  return ((char *)NULL);
}

char **command_completion(const char *text, int start, int end) {
  char **matches;
  matches = (char **)NULL;
  if (start == 0) {
    matches = rl_completion_matches(text, command_generator);
  }
  return (matches);
}

void init_readline() { rl_attempted_completion_function = command_completion; }

char *rl_gets(char *line_read) {
  if (line_read) {
    free(line_read);
    line_read = (char *)NULL;
  }

  line_read = readline("$ ");
  if (line_read && *line_read) {
    add_history(line_read);
  }
  return line_read;
}

int main(int argc, char *argv[]) {
  // Flush after every printf
  setbuf(stdout, NULL);
  init_readline();
  char *command = (char *)NULL;
  do {
    // Get command
    command = rl_gets(command);
    if (strcmp(command, "\n") == 0)
      continue;
    command[strcspn(command, "\n")] = '\0';

    Command cmd;
    cmd.argv = malloc(64 * sizeof(char *));
    cmd.output_file_name = NULL;
    parse_command(command, &cmd);
    if (cmd.argv[0] == NULL) {
      continue;
    }

    int save_fd = -1;
    int output_fd = -1;
    int input_fd = -1;

    if (cmd.output_file_name) {
      if (cmd.output_type_flag == 2) {
        input_fd = STDERR_FILENO;
      } else if (cmd.output_type_flag == 1) {
        input_fd = STDOUT_FILENO;
      }
      save_fd = dup(input_fd);

      if (cmd.append_flag == 0) {
        output_fd =
            open(cmd.output_file_name, O_CREAT | O_WRONLY | O_TRUNC, 0644);

      } else {
        output_fd =
            open(cmd.output_file_name, O_CREAT | O_WRONLY | O_APPEND, 0644);
      }

      if (dup2(output_fd, input_fd) == -1) {
        perror("dup2");
        return EXIT_FAILURE;
      }
    }

    // Command is exit without arguments
    if (strcmp(cmd.argv[0], "exit") == 0) {
      exit(EXIT_SUCCESS);
    } else if (strcmp(cmd.argv[0], "echo") == 0) {
      handle_echo(cmd.argv);
    } else if (strcmp(cmd.argv[0], "type") == 0) {
      handle_type(cmd.argv);
    } else if (handle_exec(cmd.argv) == 1) {
    } else if (strcmp(cmd.argv[0], "pwd") == 0) {
      const char *pwd_path = getenv("PWD");
      printf("%s\n", pwd_path);
    } else if (strcmp(cmd.argv[0], "cd") == 0) {
      handle_cd(cmd.argv);
    } else {
      printf("%s: command not found\n", command);
    }

    dup2(save_fd, input_fd);
    close(output_fd);
  } while (1);

  return 0;
}
