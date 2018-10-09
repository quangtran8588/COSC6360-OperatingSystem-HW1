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

string getHomeDirectory();
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

void executeCommand(int numb_of_directories, int numb_of_arguments,int numb_of_background_command, int fd, int io_redirection_opt, string *environment_directories, string *arguments, string command, string filename, pid_t pid, pid_t *bg_pid, bool interactive) {
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

int main(int argc, char * argv[]) {
	string inputLine;		// string variable to save string input from keyboard
	string command;			// string variable save Linux shell command
	string filename;		// string variable to save filename in case of IO redirection
	string arguments[MAX_SIZE_ARGUMENTS];	// string array that stores arguments comes along with Linux shell command
	string environment_directories[MAX_ENV_PATH];	// string array that stores Environment Path
	string bg_command[MAX_BACKGROUND_COMMAND];
	string home_directory;	// string variable that store the home directory

	int numb_of_arguments = 0;		
	int numb_of_directories = 0;
	int numb_of_background_command = 0;

	int io_redirection_opt = 0;		// IO redirection option; 1- "<" read from file -> output to screen, 2- ">" redirect input to file
	int fd;			// file descriptor	
	int status;

	bool interactive = true;	// boolean variable to check whether command running in background
	bool error = false;			// boolean variable to check background command is valid

	pid_t pid;					// pid is an integer that uses in fork() to check parent id or child id
	pid_t bg_pid[128];			// an array that stores all child id running in background
	
	//	Get Environment paths and store it into environment_directories[]
	//	then get home directory of current shell 
	//  When shell is compiled and executed, directory that shell is running will be stored as home directory
	//	It will be used to implement the command "cd ~"
	numb_of_directories = parseEnvironmentPath(environment_directories);
	home_directory = getHomeDirectory();

	//	Create loop to check input commands
	do {
		cout << "MyShell: ";
		getline(cin, inputLine);

		//	Check if command is exit, then checking if there's any background processes running
		//	to kill them before exit(0)
		if (inputLine == "exit") {
			for (int i = 0; i < numb_of_background_command; i++) {
				if (bg_pid[i] > 0) {
					pid = waitpid(bg_pid[i], &status, WNOHANG);
					if (pid == 0) {
						command = "kill";
						arguments[0] = to_string(bg_pid[i]);
						numb_of_arguments = 1;
						executeCommand(numb_of_directories, numb_of_arguments, numb_of_background_command, fd, io_redirection_opt, environment_directories, arguments, command, filename, pid, bg_pid, interactive);
					}
				}
			}
				exit(0); 
		}
		//	If not "Exit" command, then check and execute each commands
		else {
			//	calling parse to split string command that entered by user
			//	function also return a value io_redirection_opt to detect if any IO Redirection would happen
			io_redirection_opt = parseInputLine(inputLine, &command, &filename, arguments);
			for (int i = 0; i < MAX_SIZE_ARGUMENTS; i++) {
				if (arguments[i] != "") { numb_of_arguments++; }
			}

			//	Implement four types of cd command which are "cd .", "cd ..", "cd new_directory", "cd ~"
			if (command == "cd") {
				if (numb_of_arguments == 0) {
					cout << "Error! cd command is invalid \n";
				}
				else if (numb_of_arguments == 1 && arguments[0] == "~") {
					int ret_val = chdir(home_directory.c_str());
					if (ret_val == -1)
						cout << "Error! No such file or directory" << endl;
				}
				else {
					changePath(&arguments[0]);
					int ret_val = chdir(arguments[0].c_str());
					if (ret_val == -1)
						cout << "Error! No such file or directory" << endl;
				}
			}

			//	If command is running in background, boolean interactive will be set to false
			//	Parent does not need to wait for child terminates
			else {
				if (command == "bg") {
					if (numb_of_arguments == 0) {
						cout << "Error! " << command << " is invalid." << endl;
						error = true;
					}
					else {
						//	parseInputLine will split the string (i.e. bg sleep 100) into 2 parts
						//	command = bg; arguments[0] = "sleep"; arguments[1] = "100"
						//	But, bg is not a real command that need to be executed. It just indicates followed command running in background
						//	so, they need to be re-arranged
						interactive = false;
						command = arguments[0];
						reArrangeArgument(numb_of_arguments, arguments);
						//	store background command into an array that will be used in the future
					    //	when user types "processes" to check any background processes to display
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

				//	"processes" will be implemented to replace shell builtin command "jobs -rp"
				//	which display all background processes (id, and command) to the screen
				//	When commmand starts by "bg", the following command will be stored into an array
				//	along with child id that run in background (child id is also stored into another array)
				//	Child_id in bg_pid[] will be corresponding with command that stored in bg_command[]
				//	call Linux function waitpid() with option WNOHANG to detect whether child process has terminated
				//	If it's NOT terminated, the function will return 0. Then, print out child_id corresponding with the command
				//	also implement re-arrangement of two arrays in case of detecting terminating child process
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

				//	After splitting string command, and checking whether command is located in environtment path
				//	then execute the command
				if (!error){
					executeCommand(numb_of_directories, numb_of_arguments,numb_of_background_command, fd, io_redirection_opt, environment_directories, arguments,command, filename, pid, bg_pid, interactive);
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

//	Function will return home directory
//	When program is compiled and run, it will get current folder directory as home directory
string getHomeDirectory() {
	char *ptrPath;
	char currentPath[MAX_PATH_LENGTH];
	ptrPath = getcwd(currentPath, MAX_PATH_LENGTH);
	return string(currentPath);
}

//	Parse function to split a line of string command into different parts
//	and also checking whether any IO Redirection
//	Trimming input data to erase empty white space
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

//	Function to split string base on delimiter
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

//	Function to detect "<", or ">" as IO Redirection in a line of string command
int checkIORedirection(string inputLine) {
	size_t found_io_read = inputLine.find("<");
	size_t found_io_write = inputLine.find(">");

	if (found_io_read != string::npos)
		return 1;
	else if (found_io_write != string::npos)
		return 2;
	return 0;
}

//	Trimming function to erase empty white space
//	this function will call another sub-trimming (head and tail trimming) function to remove 
//	white space on both head and tail
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

//	Function to change path that followed by cd command
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

//	Function to get Environment Path and store them into an array
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

//	Function will check whether entering command is located in Environment Path
//	If it is NOT there, return invalid command to indicate in main to output error on screen
//	Otherwise, return the command path
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

//	Function to clean arguments[] array to re-use
void cleanArgument(int numb, string *argument) {
	for (int i = 0; i < numb; i++) {
		argument[i] = "";
	}
}

/*	Below functions will be used for re-arrangement of elements in an array	*/
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


