#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>
#include <algorithm> 
#include <cctype>
#include <locale>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_SIZE_ARGUMENTS 30
#define MAX_ENV_PATH 128
#define MAX_PATH_LENGTH 1024
#define MAX_BACKGROUND_COMMAND 256

using namespace std;

int parseInputLine(string inputLine, string *command, string *filename, string *arguments);
int split(string input, string *left, string *right, char delimiter);
int checkIORedirection(string inputLine);
void trim(string *input, int numb_of_elements);
void headTrim(string &s);
void tailTrim(string &s);
void changePath(string *path);
int parseEnvironmentPath(string *directories);
string validateCommand(int numb_of_directories, string *directories, string command);
void cleanArgument(int numb, string *argument);
void reArrangeArgument(int &numb, string *arguments);
void reArrangeBackgroundList(int &numb, string *list, int pos);
void reArrangePIDList(int numb, pid_t *list, int pos);

void reArrangeArgument(int &numb, string *arguments) {
	int i;
	if (numb == 1) {
		arguments[0] = "";
	}
	else {
		for (i = 0; i < numb - 1; i++) {
			arguments[i] = arguments[i + 1];
		}
		arguments[i] = "";
	}
	numb -= 1;
}

void reArrangeBackgroundList(int &numb, string *list, int pos) {
	if (numb == 1) {
		list[0] = "";
	}
	else {
		for (pos; pos < numb - 1; pos++) {
			list[pos] = list[pos + 1];
		}
		list[pos] = "";
	}
	numb -= 1;
}

void reArrangePIDList(int numb, pid_t *list, int pos) {
	if (numb == 1) {
		list[0] = -1;
	}
	else {
		for (pos; pos < numb - 1; pos++) {
			list[pos] = list[pos + 1];
		}
		list[pos] = -1;
	}
}

int main(int argc, char * argv[]) {
	string inputLine;		// string variable to save string input from keyboard
	string command;			// string variable save Linux shell command
	string filename;		// string variable to save filename in case of IO redirection
	string arguments[MAX_SIZE_ARGUMENTS];	// string array that stores arguments comes along with Linux shell command
	string environment_directories[MAX_ENV_PATH];	// string array that stores Environment Path
	string bg_command[MAX_BACKGROUND_COMMAND];

	int numb_of_arguments = 0;		
	int numb_of_directories = 0;
	int numb_of_background_command = 0;

	int io_redirection_opt = 0;		// IO redirection option; 1- "<" read from file -> output to screen, 2- ">" redirect input to file
	int fd;			// file descriptor	
	int status;

	bool interactive = true;	// boolean variable to check whether command running in background
	bool error = false;			// boolean variable to check background command is valid

	pid_t pid;	
	pid_t bg_pid[128];
	
	numb_of_directories = parseEnvironmentPath(environment_directories);

	do {
		cout << "MyShell: ";
		getline(cin, inputLine);

		if (inputLine == "exit") { exit(0); }
		else {
			io_redirection_opt = parseInputLine(inputLine, &command, &filename, arguments);
			for (int i = 0; i < MAX_SIZE_ARGUMENTS; i++) {
				if (arguments[i] != "") { numb_of_arguments++; }
			}
			if (command == "cd") {
				if (numb_of_arguments == 0) {
					cout << "Error! cd command is invalid \n";
				}
				else {
					changePath(&arguments[0]);
					int ret_val = chdir(arguments[0].c_str());
					if (ret_val == -1)
						cout << "Error! No such file or directory" << endl;
					char *ptrPath;
					char currentPath[1024];
					ptrPath = getcwd(currentPath, 1024);
					cout << "Current Directory: " << string(currentPath) << endl;
				}
			}
			else {
				if (command == "bg") {
					if (numb_of_arguments == 0) {
						cout << "Error! " << command << " is invalid." << endl;
						error = true;
					}
					else {
						interactive = false;
						command = arguments[0];
						reArrangeArgument(numb_of_arguments, arguments);
						bg_command[numb_of_background_command] = command + " ";
						for (int i = 0; i < numb_of_arguments; i++) {
							if (i < numb_of_arguments - 1) {
								bg_command[numb_of_background_command] += arguments[i] + " ";
							}	
							else {
								bg_command[numb_of_background_command] += arguments[i];
							}
						}
						numb_of_background_command++;
					}
				}
				else if (inputLine == "processes") {
					for (int i = 0; i < numb_of_background_command; i++) {
						if (bg_pid[i] > 0) {
							pid = waitpid(bg_pid[i], &status, WNOHANG);
							if (pid == 0)
								cout << bg_pid[i] << " " << bg_command[i] << endl;
							else {
								reArrangePIDList(numb_of_background_command, bg_pid, i);
								reArrangeBackgroundList(numb_of_background_command, bg_command, i);
							}
						}
					}
					error = true;
				}
				if (!error){
					string path = validateCommand(numb_of_directories, environment_directories, command);
					char *argv[numb_of_arguments + 2];
					argv[0] = const_cast<char*>(path.c_str());
					for (int i = 1; i < numb_of_arguments + 1; i++) {
						argv[i] = const_cast<char*>(arguments[i - 1].c_str());
					}
					argv[numb_of_arguments + 1] = NULL;

					pid = fork();
					if (pid == 0) {
						int err;
						if (path != "Invalid command") {
							if (io_redirection_opt != 0) {
								if (io_redirection_opt == 1) {
									fd = open(filename.c_str(), O_RDONLY);
									close(0);
									dup(fd);
									close(fd);
								}
								else {
									fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0644);
									close(1);
									dup(fd);
									close(fd);
								}
							}
							if (numb_of_arguments != 0)
								err = execv(path.c_str(), argv);
							else
								err = execl(path.c_str(), path.c_str(), NULL);
							if (err == -1)
								cout << "cannot execute" << endl;
							_exit(1);
						}
						else
							cout << "Command " << command << " not found" << endl;
						_exit(1);
					}
					else if (pid > 0) {
						if (interactive)
							while (wait(0) != pid);
						else {
							bg_pid[numb_of_background_command - 1] = pid;
						}		
					}
					else {
						cout << "Fork failed" << endl;
						_exit(1);
					}
				}	
			}
			error = false;
			interactive = true;
			cleanArgument(numb_of_arguments, arguments);
			filename = "";
			io_redirection_opt = 0;
			numb_of_arguments = 0;

		}
	} while (1);
	return 0;
}

int parseInputLine(string inputLine, string *command, string *filename, string *arguments) {
	int check = checkIORedirection(inputLine);
	string temp;
	int numb_of_filename = 0; int numb_of_arguments = 0;
	if (check != 0) {
		switch (check) {
		case 1: {
			numb_of_filename = split(inputLine, &temp, filename, '<');
			numb_of_arguments = split(temp, command, arguments, ' ');
			trim(filename, numb_of_filename); trim(arguments, numb_of_arguments);
			return 1;
		}
		case 2: {
			numb_of_filename = split(inputLine, &temp, filename, '>');
			numb_of_arguments = split(temp, command, arguments, ' ');
			trim(filename, numb_of_filename); trim(arguments, numb_of_arguments);
			return 2;
		}
		}
	}
	split(inputLine, command, arguments, ' ');
	trim(arguments, numb_of_arguments);
	return 0;
}

int split(string input, string *left, string *right, char delimiter) {
	stringstream ss(input);
	if (delimiter != ' ') {
		getline(ss, *left, delimiter);
		getline(ss, *right);
		return 1;
	}
	else {
		int pos = 0;
		getline(ss, *left, delimiter);
		while (getline(ss, right[pos++], delimiter));
		return pos;
	}
}

int checkIORedirection(string inputLine) {
	size_t found_io_read = inputLine.find("<");
	size_t found_io_write = inputLine.find(">");

	if (found_io_read != string::npos)
		return 1;
	else if (found_io_write != string::npos)
		return 2;
	return 0;
}

void trim(string *input, int numb_of_elements) {
	for (int i = 0; i < numb_of_elements; i++) {
		headTrim(input[i]);
		tailTrim(input[i]);
	}
}

void headTrim(string &s) {
	s.erase(s.begin(), find_if(s.begin(), s.end(), [](int ch) {
		return !isspace(ch);
	}));
}

void tailTrim(string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), s.end());
}

void changePath(string *path) {
	char *ptrPath;
	char currentPath[MAX_PATH_LENGTH];

	if ((*path).at(0) != '/')
	{
		ptrPath = getcwd(currentPath, MAX_PATH_LENGTH);
		if (ptrPath == NULL) {
			cout << "Error! Cannot retrieve current working directory" << endl;
		}
		else {
			string newPath = string(currentPath) + "/" + *path;
			*path = newPath;
		}
	}
}

int parseEnvironmentPath(string *directories) {
	int pos = 0; int counter = 0;
	char *envPath;
	envPath = getenv("PATH");
	string strPath(envPath);
	stringstream ss(strPath);
	while (getline(ss, directories[pos++], ':'));
	for (int i = 0; i < pos; i++) {
		if (directories[i] == "" || directories[i] == "\0")
			continue;
		counter++;
	}
	return counter;
}

string validateCommand(int numb_of_directories, string *directories, string command) {
	string pathname = "";
	string invalid = "Invalid command";
	for (int i = 0; i < numb_of_directories; i++) {
		pathname = directories[i] + "/" + command;
		if (!access(pathname.c_str(), X_OK))
			return pathname;
		pathname = "";
	}
	return invalid;
}

void cleanArgument(int numb, string *argument) {
	for (int i = 0; i < numb; i++) {
		argument[i] = "";
	}
}


