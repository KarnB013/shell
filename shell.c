#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

// ip stores the input by user
char ip[1000];
// char args[1000][1000];
int argcnt, seqcnt;
int k = 0;
// flags to detect which special characters are entered
int bg, redir, pipes, condition, seq, concat, newt, fg;
// stores background process id to bring it to foreground
int bg_id = 0;

// to run a new shell copy
void run_newt()
{
	int pid = fork();
	if (pid == -1)
	{
		printf("Fork failed!\n");
		return;
	}
	if (pid == 0)
	{
		// execlp replace the process with shell's path to run a copy
		int x = execlp("./shell", "./shell", NULL);
		if (x == -1)
		{
			printf("Error in executing the required command, check your input!");
		}
	}
	else
	{
		waitpid(-1, NULL, 0);
	}
}

// this program takes in a command and uses execvp to run it
int execute(char cmd[1000])
{
	// tokenising the command to separate the arguments
	char *cmdd[1000] = {'\0'};
	char *t = strtok(cmd, " ");
	char *first = t;
	if (t != NULL)
	{
		int i = 0;
		while (t != NULL)
		{
			cmdd[i++] = t;
			t = strtok(NULL, " ");
		}
		cmdd[i++] = NULL;
		// forking to use execvp
		int pid = fork();
		if (pid == -1)
		{
			printf("Fork failed!\n");
			return -2;
		}
		if (pid == 0)
		{
			// using execvp to execute command
			int x = execvp(first, cmdd);
			if (x == -1)
			{
				return -3;
			}
		}
		else
		{
			waitpid(-1, NULL, 0);
		}
	}
	else
	{
		return -1;
	}
	return 0;
}

// this function will bring the last background process to foreground
void bring_to_fg()
{
	int fg_id = bg_id, fg_wait;
	int fg_wait_res = waitpid(fg_id, &fg_wait_res, 0);
	int fg_res = kill(fg_wait_res, SIGCONT);
	if (fg_res < 0 || fg_wait_res < 0)
		printf("Can't bring the process to foreground\n");
	return;
}

// this is to run a command in background
void bg_call()
{
	char *args[100];
	char *token;
	int i = 0;
	// we duplicate ip to not lose it's original value when using strtok
	char ip1[1000];
	strcpy(ip1, ip);
	token = strtok(ip1, " ");
	while (token != NULL)
	{
		args[i++] = token;
		token = strtok(NULL, " ");
	}
	// replacing & by NULL
	args[i - 1] = NULL;

	pid_t pid = fork();

	if (pid == 0)
	{
		if (execvp(args[0], args) == -1)
		{
			perror("execvp");
		}
		exit(EXIT_FAILURE);
	}
	else if (pid < 0)
	{
		perror("fork");
	}
	else
	{
		// parent stores pid of background process to bring it to foreground when fg is typed in shell and doesn't wait so that process can run in background
		bg_id = pid;
	}
}

// this function simply returns last word from input which is useful to get name of file when redirecting input or output
char *getLastWord(char *str)
{
	int length = strlen(str);
	char *lastWord = (char *)malloc(length + 1);
	if (!lastWord)
	{
		fprintf(stderr, "Memory allocation failed\n");
		return NULL;
	}

	int i = length - 1;
	while (i >= 0 && isspace((unsigned char)str[i]))
	{
		i--;
	}

	int end = i;

	while (i >= 0 && !isspace((unsigned char)str[i]))
	{
		i--;
	}

	int start = i + 1;

	strncpy(lastWord, &str[start], end - start + 1);
	lastWord[end - start + 1] = '\0';

	return lastWord;
}

// this function will redirect the input or output depending of user choice
void redir_call()
{
	int pid = fork();
	if (pid == -1)
	{
		printf("Fork failed\n");
	}
	if (pid == 0)
	{
		const char *fname = getLastWord(ip);
		int fd = open(fname, O_WRONLY, 0777);
		if (fd < 0)
		{
			fd = open(fname, O_CREAT | O_WRONLY, 0777);
		}
		// in case user wants to take input from file
		if (strstr(ip, "<") != NULL)
		{
			// replace input with file descriptor which is taken from entered file name by user
			if (dup2(fd, 0) < 0)
			{
				printf("dup2 failed\n");
			}
		}
		// in case user wants to send output to file
		if (strstr(ip, ">") != NULL && strstr(ip, ">>") == NULL)
		{
			// replace output with file descriptor which is taken from entered file name by user
			if (dup2(fd, 1) < 0)
			{
				printf("dup2 failed\n");
			}
		}
		// in case user wants to append output to file
		if (strstr(ip, ">>") != NULL)
		{
			close(fd);
			fd = open(fname, O_WRONLY | O_APPEND, 0777);
			// replace output with file descriptor which is taken from entered file name by user
			if (dup2(fd, 1) < 0)
			{
				printf("dup2 failed\n");
			}
		}
		close(fd);
		// using delimiters > and < to trim and run command before < or > is encountered using execvp
		char dl[] = "<>";
		int x = strcspn(ip, dl);
		// keeping input copy
		char ip1[1000];
		strncpy(ip1, ip, x);
		ip1[x] = '\0';
		char *cmd[100];
		int n = 0;
		// tokenising and running command using execvp
		cmd[n] = strtok(ip1, " ");
		while (cmd[n] != NULL)
		{
			cmd[++n] = strtok(NULL, " ");
		}
		execvp(cmd[0], cmd);
	}
	else
	{
		wait(NULL);
	}
}

// this program will simple remove whitespaces from string which will help in concatenation to get just the filenames in case there's a whitespace between # and a filename
void removewhitespaces(char *str)
{
	int itr = 0;
	for (int i = 0; i < strlen(str); i++)
	{
		if (str[i] != ' ')
		{
			str[itr++] = str[i];
		}
	}
	str[itr] = '\0';
}

// this function will take different files as input separated by #, and show the merged output in order that files are entered
int concat_call()
{
	char *files[1000] = {'\0'};
	char *files1[1000] = {'\0'};
	int j = 0;
	// cat will be the very first part of command which will merge all files
	files1[j++] = "cat ";
	int f = 0;
	// dividing the input based on # to get all files
	char *t = strtok(ip, "#");
	// storing filenames in files
	while (t != NULL)
	{
		files[f++] = t;
		t = strtok(NULL, "#");
	}
	if (f > 5)
	{
		return -1;
	}
	for (int i = 0; i < f; i++)
	{
		// removing whitespaces from filenames
		removewhitespaces(files[i]);
		FILE *fp = fopen(files[i], "r");
		if (fp == NULL)
		{
			return -2;
		}
		files1[j++] = files[i];
	}
	files1[j++] = NULL;
	// showing ouput in the order the file names were entered using execvp
	int pid = fork();
	if (pid == -1)
	{
		return -3;
	}
	if (pid == 0)
	{
		int x = execvp("cat", files1);
	}
	else
	{
		waitpid(-1, NULL, 0);
	}
	return 0;
}

// this program will return -1 in case execvp fails to run which will help in condition case
int execute_command(char *argv[])
{
	pid_t pid = fork();
	if (pid == -1)
	{
		perror("fork");
		return -1;
	}
	else if (pid == 0)
	{
		execvp(argv[0], argv);
		perror("execvp");
		exit(EXIT_FAILURE);
	}
	else
	{
		// to return -1 in case execvp fails, 0 otherwise
		int status;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		{
			return 0;
		}
		else
		{
			return -1;
		}
	}
}

// this program will use &&, || and use these logical operators to run the commands
void condition_call()
{
	char ip1[1000];
	char vals[1000][1000];
	// cleaning the vals array which'll store commands and special characters
	for (int i = 0; i < 1000; i++)
	{
		vals[i][0] = '\0';
	}
	// keeping a copy of input
	strcpy(ip1, ip);
	// tokenising based on whitespace
	char *t = strtok(ip1, " ");
	int argcnt = 0, x = 0;
	// in case we see the special operators, we are checking that they don't exceed 5 and also adding the commands and special characters
	while (t != NULL)
	{
		if (strcmp(t, "&&") == 0 || strcmp(t, "||") == 0)
		{
			x++;
			strcat(vals[x], t);
			x++;
			argcnt++;
		}
		else if (strcmp(t, " ") != 0)
		{
			strcat(vals[x], t);
			strcat(vals[x], " ");
		}
		t = strtok(NULL, " ");
	}
	if (argcnt > 5)
	{
		printf("Upto 5 conditional operators permitted!\n");
		return;
	}
	// checking each command doesn't exceed 5 arguments
	for (int i = 0; i <= x; i++)
	{
		char check[1000];
		strcpy(check, vals[i]);
		char *tt = strtok(check, " ");
		int argcnt1 = 0;
		while (tt != NULL)
		{
			argcnt1++;
			tt = strtok(NULL, " ");
		}
		if (argcnt1 > 5)
		{
			printf("Upto 5 arguments permitted!\n");
			return;
		}
	}
	// for each special character, if the special character is || and we have a successful command ran before, we stop
	// and if special character is && and we keeping executing until we have a failed command
	int or_flag = 0;
	for (int i = 0; i <= x; i++)
	{
		if (strcmp(vals[i], "&&") == 0)
		{
			if (execute(vals[i - 1]) == -1)
			{
				return;
			}
			else
			{
				execute(vals[i + 1]);
			}
		}
		if (strcmp(vals[i], "||") == 0)
		{
			if (or_flag)
				return;

			if (execute(vals[i - 1]) != -1)
			{
				or_flag = 1;
			}
		}
	}
}

// this function will take ; separated commands and run them one by one
int seq_call()
{
	int arg = 0, k = 0;
	char seq[100][1000] = {'\0'};
	char seq1[100][1000] = {'\0'};
	// tokenising based on ; and checking there's no more 5 ; characters
	char *t = strtok(ip, ";");
	while (t != NULL)
	{
		strcpy(seq[argcnt++], t);
		t = strtok(NULL, ";");
	}
	if (argcnt > 6)
	{
		return -2;
	}
	for (int i = 0; i <= argcnt; i++)
		strcpy(seq1[i], seq[i]);
	// tokenising further commands to get arguments and run using execvp
	for (int i = 0; i <= argcnt; i++)
	{
		char *tt = strtok(seq1[i], " ");
		seqcnt = 0;
		while (tt != NULL)
		{
			seqcnt++;
			tt = strtok(NULL, " ");
		}
		if (seqcnt > 6)
			return -1;
	}
	for (int i = 0; i < argcnt; i++)
	{
		int e = execute(seq[i]);
		if (e == -1)
			return -3;
		if (e == -2)
			return -7;
		if (e == -3)
			return -6;
	}
	return 0;
}

// this function simulates pipes but redirecting input from one command and passing it to other
void pipe_execute(int args, char seq[100][1000])
{
	// args - 1 are number of pipes and we need that many pipes shown by fd
	int fd[args - 1][2];
	for (int i = 0; i < args - 1; i++)
	{
		if (pipe(fd[i]) == -1)
		{
			printf("Failed to create pipe\n");
		}
	}
	for (int i = 0; i < args; i++)
	{
		int pid = fork();
		if (pid == -1)
		{
			printf("Fork failed\n");
		}
		if (pid == 0)
		{
			if (i > 0)
			{
				// replacing the stdin
				if (dup2(fd[i - 1][0], 0) == -1)
				{
					printf("dup error\n");
				}
			}
			if (i < args - 1)
			{
				// replacing the stdout
				if (dup2(fd[i][1], 1) == -1)
				{
					printf("dup error\n");
				}
			}
			// for first command, we just run it using execvp
			for (int j = 0; j < args - 1; j++)
			{
				close(fd[j][0]);
				close(fd[j][1]);
			}
			char *cmd[100];
			int n = 0;
			cmd[n] = strtok(seq[i], " ");
			while (cmd[n] != NULL)
			{
				cmd[++n] = strtok(NULL, " ");
			}
			execvp(cmd[0], cmd);
		}
	}
	// in main process, we close the pipes' read and write ends
	for (int i = 0; i < args - 1; i++)
	{
		close(fd[i][0]);
		close(fd[i][1]);
	}
	// parent waits for all children(pipes) to complete the task
	for (int i = 0; i < args; i++)
	{
		wait(NULL);
	}
}

// this function will check if the number of pipes and number of command arguments are valid or not
int pipes_call()
{
	int arg = 0, k = 0;
	char seq[100][1000] = {'\0'};
	char seq1[100][1000] = {'\0'};
	// tokenising based on pipe
	char *t = strtok(ip, "|");
	while (t != NULL)
	{
		strcpy(seq[argcnt++], t);
		t = strtok(NULL, "|");
	}
	if (argcnt > 7)
	{
		return -2;
	}
	for (int i = 0; i <= argcnt; i++)
		strcpy(seq1[i], seq[i]);
	// tokenising individual commands to use pipe_execute
	for (int i = 0; i <= argcnt; i++)
	{
		char *tt = strtok(seq1[i], " ");
		seqcnt = 0;
		while (tt != NULL)
		{
			seqcnt++;
			tt = strtok(NULL, " ");
		}
		if (seqcnt > 6)
			return -1;
	}
	pipe_execute(argcnt, seq);
	int e = 0;
	if (e == -1)
		return -3;
	if (e == -2)
		return -7;
	if (e == -3)
		return -6;
	return 0;
}

// this will send each command to it's correct fucntion based on which special character is entered by the user
int shell24()
{
	argcnt = 0, bg = 0, redir = 0, pipes = 0, condition = 0, seq = 0, concat = 0, newt = 0, fg = 0;
	printf("shell24$ ");
	fgets(ip, sizeof(ip), stdin);
	ip[strcspn(ip, "\r\n")] = 0;
	if (strlen(ip) == 0)
		return -1;
	if (strstr(ip, "&") != NULL && strstr(ip, "&&") == NULL)
		bg = 1;
	if (strstr(ip, "<") != NULL || strstr(ip, ">") != NULL || strstr(ip, ">>") != NULL)
		redir = 1;
	if (strstr(ip, "#") != NULL)
		concat = 1;
	if (strstr(ip, "&&") != NULL || strstr(ip, "||") != NULL)
		condition = 1;
	if (strstr(ip, ";") != NULL)
		seq = 1;
	if (strstr(ip, "|") != NULL && strstr(ip, "||") == NULL)
		pipes = 1;
	if (!strcmp(ip, "newt"))
		newt = 1;
	if (!strcmp(ip, "fg"))
		fg = 1;
	// background process
	if (bg)
	{
		if (redir || concat || condition || seq || pipes)
		{
			printf("Multiple special characters detected.\n");
		}
		else
		{
			bg_call();
		}
	}
	// pipe case
	else if (pipes)
	{
		if (redir || concat || condition || seq || bg)
		{
			printf("Multiple special characters detected.\n");
		}
		else
		{
			int x = pipes_call();
			if (x == -1)
			{
				return -1;
			}
			if (x == -2)
			{
				return -3;
			}
			return 0;
		}
	}
	// redirection
	else if (redir)
	{
		if (pipes || concat || condition || seq || bg)
		{
			printf("Multiple special characters detected.\n");
		}
		else
		{
			redir_call();
		}
	}
	// concatenation
	else if (concat)
	{
		if (redir || pipes || condition || seq || bg)
		{
			printf("Multiple special characters detected.\n");
		}
		else
		{
			int x = concat_call();
			if (x == -1)
				return -4;
			if (x == -2)
				return -8;
			if (x == -3)
				return -7;
		}
	}
	// condition
	else if (condition)
	{
		if (redir || concat || pipes || seq || bg)
		{
			printf("Multiple special characters detected.\n");
		}
		else
		{
			condition_call();
		}
	}
	// sequence
	else if (seq)
	{
		if (redir || concat || condition || pipes || bg)
		{
			printf("Multiple special characters detected.\n");
		}
		else
		{
			int x = seq_call();
			if (x == -1)
			{
				return -1;
			}
			if (x == -2)
			{
				return -4;
			}
			if (x == -3)
			{
				return -6;
			}
			return 0;
		}
	}
	// for new shell copy
	else if (newt)
	{
		run_newt();
	}
	// to bring background process to foreground
	else if (fg)
	{
		bring_to_fg();
	}
	// in case no special characters are used
	else
	{
		int arg_count = 0;
		char ip1[1000];
		strcpy(ip1, ip);
		char *token = strtok(ip1, " ");
		while (token != NULL)
		{
			arg_count++;
			token = strtok(NULL, " ");
		}
		if (arg_count < 1 || arg_count > 5)
		{
			return -1;
		}
		else
		{
			execute(ip);
		}
	}
	return 0;
}

int main()
{
	while (1)
	{
		// running shell24() which handles all cases
		int x = shell24();
		// error handling
		if (x == -1)
			printf("Invalid number of arguments. Please enter between 1 and 5 arguments.\n");
		if (x == -2)
			printf("First argument can't be the special character!\n");
		if (x == -3)
			printf("Upto 6 pipe operations are permitted!\n");
		if (x == -4)
			printf("Upto 5 operations are permitted!\n");
		if (x == -5)
			printf("You can have only one redirection or background processing operation!\n");
		if (x == -6)
			printf("Check your input!\n");
		if (x == -7)
			printf("Fork failed!\n");
		if (x == -8)
			printf("One or more file/s does not exist\n");
	}
}