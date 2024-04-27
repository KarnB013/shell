# Shell
A shell implementation with following constraints and features:

Constraints: argc >= 1 and <= 5 (special characters are immune)

Ex : shell24$ ls -1 -l -t ~/ (argc = 5)

Special characters (no multiple special characters in single command):

'#' : Text file concatenation (upto 5 permitted)

Ex : shell24$ file1.txt # file2.txt

'|' : Piping (upto 6 operations permitted)

Ex : shell24$ ls | grep *.txt | wc -w

'>,<,>>' : Redirection

Ex : shell24$ echo Hello >> output.txt

'&&, ||' : Conditional execution (upto 5 permitted)

Ex : shell24$ cmd1 && cmd2 && cmd3

'&' : Background processing

Ex : shell24$ cmd1 & (running shell24$fg should bring process back to foreground)

';' : Sequential execution (upto 5 commands)

Ex: shell24$ ls ; date ; ls -l
