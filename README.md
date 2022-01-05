# Shell-Implementation
## About
This academic project is an implementation of bash
 * Basic commands like **ls**, **wc**, **pwd**, and other shell commands are supported and they can be piped together
 * I implemented built-in commands as well such as **cd**, **history** and **exit**
 * This works on **Linux**, behavior on macOS and Windows is unknown
## Built-in Commands
### cd
Entering "**cd**" will result in the same behavior as the **cd** command in bash.  
Flags are not supported.
### history
Entering "**history**" will display up to the last 1000 commands entered.  
Any of the commands in history can be executed by typing "**history #**" where **#** is the number of the command you want to execute.  
Typing "**history -c**" will clear the history.
### exit
Entering "**exit**" will simply exit the shell after performing any necessary cleanup
## Tutorial
To run the project, follow these steps:
1. Download **shell.c** and place in a directory of your choosing
2. Open a terminal in the directory and use **gcc -o shell shell.c** to compile the program
3. Use "**./shell**" to run the program
4. Now you can perform shell commands using the program!
