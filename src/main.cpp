/*
 * main.cpp - Demo in C++ of a simple shell
 *
 *   Copyright 2010-2013 Jesús Torres <jmtorres@ull.es>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <string>

#include <cli/callbacks.hpp>
#include <cli/prettyprint.hpp>
#include <cli/shell.hpp>
#include <cli/utility.hpp>

#include <errno.h>      // errno
#include <fcntl.h>      // open()
#include <stdlib.h>     // exit(), setenv(), ...
#include <string.h>     // strerror()
#include <sys/types.h>  // waitpid()
#include <sys/wait.h>   // waitpid(), open()
#include <sys/stat.h>   // open()
#include <unistd.h>     // exec(), fork(), close(), dup2(), pipe(), ...
#include <signal.h>
#include <sys/stat.h>

const char INTRO_TEXT[] = "\x1b[2J\x1b[H"
                          "Simple Shell - C++ Demo\n"
                          "Copyright 2010-2013 Jesús Torres <jmtorres@ull.es>\n";

const char PROMPT_TEXT[] = "$ ";

// Variable externa usada para el pipe
int aux = 0;

//
// Function to be invoked by the interpreter when the user inputs the
// 'exit' command.
//
// If this function returns true, the interpreter ends.
//
// cli::ShellArguments is an alias of cli::parser::shellparser::Arguments.
// See include/cli/shell.hpp for its definition.
//

bool onExit(const std::string& command, cli::ShellArguments const& arguments)
{
    std::cout << "command:   " << command << std::endl;
    std::cout << "arguments: " << arguments << std::endl;
    std::cout << std::endl;
    return true;
}

bool onEcho(const std::string& command, cli::ShellArguments const& arguments)
{
        int pipeFileDes[2] = {-1, -1};
    
    if (arguments.terminator == cli::ShellArguments::PIPED)
      pipe(pipeFileDes);
    
    pid_t childPid = fork();
    if (childPid == 0) {            // Proceso hijo
      if (aux > 0) {
			dup2(aux,0);	// duplicamos para poder cerrarlo después
			close(aux);
      }	
      if (arguments.terminator == cli::ShellArguments::PIPED) {
			close (pipeFileDes[0]);
			dup2(pipeFileDes[1], 1);
			close (pipeFileDes[1]);
      }
      for (unsigned i = 0; i < arguments.redirections.size(); ++i){
            int fd = -1;
            int mode = S_IRUSR | S_IWUSR |      // u+rw
                       S_IRGRP | S_IWGRP |      // g+rw
                       S_IROTH | S_IWOTH;       // o+rw
  		if (arguments.redirections[i].type ==
                	cli::StdioRedirection::TRUNCATED_OUTPUT) {
                	fd = open(arguments.redirections[i].argument.c_str(),
                   	 O_CREAT | O_TRUNC | O_WRONLY, mode);
                	if (fd >= 0) {
                   		dup2(fd, 1);    // El fd 1 se cierra antes de duplicar
                    		close(fd);
                	}
            	}
		if (arguments.redirections[i].type ==
                	cli::StdioRedirection::APPENDED_OUTPUT) {
                	fd = open(arguments.redirections[i].argument.c_str(),
                   	 O_CREAT | O_APPEND | O_WRONLY, mode);
                	if (fd >= 0) {
                   		dup2(fd, 1);    // El fd 1 se cierra antes de duplicar
                    		close(fd);
                	}
            	}
		if (arguments.redirections[i].type ==
                	cli::StdioRedirection::INPUT) {
                	fd = open(arguments.redirections[i].argument.c_str(),
                   	 O_RDONLY, mode);
                	if (fd >= 0) {
                   		dup2(fd, 0);    // El fd 1 se cierra antes de duplicar
                    		close(fd);
                	}
            	}
		if (fd < 0) {
                std::cerr << program_invocation_short_name
                          << ": open: "
                          << strerror(errno)
                          << std::endl;
                exit(1);
            	}
      }
      if (! arguments.arguments.empty()) {
            // Ejecutar el programa indicado en la línea de comandos.
            // Usamos execvp() para que el programa sea buscado en el PATH
           if (arguments.arguments.size() < 2) {
	      std::cout << std::endl;
	      return false;
	    }

	  std::cout << arguments.arguments[1];
	  for (unsigned i = 2; i < arguments.arguments.size(); ++i) {
	    std::cout << ' ';
	    std::cout << arguments.arguments[i];
	  }
	  std::cout << std::endl;
      }
      exit(0);
    }
    else {			    // Proceso padre
  	// Si no se quiso lanzar en background (terminador '&') esperar a que el
    	// proceso hijo termine
	if (childPid > 0 ) {
		if (arguments.terminator == cli::ShellArguments::PIPED) {
			close(pipeFileDes[1]);
			aux = pipeFileDes[0];
		}
		if (arguments.terminator == cli::ShellArguments::NORMAL) {
			waitpid(childPid, NULL, 0);
		}
    	}
    	else {
       	 std::cerr << program_invocation_short_name
                  << ": fork: "
                  << strerror(errno)
                  << std::endl;
    	}
    	// Espera no bloqueante para recuperar la información acerca de los hijos
    	// evitando la aparición de procesos zombi.
    	while(waitpid(-1, NULL, WNOHANG) > 0);
    	return false;
    }
    return false;
}

bool onCd(const std::string& command, cli::ShellArguments const& arguments)
{
    char** path =  cli::utility::stdVectorStringToArgV(arguments.arguments);
    chdir(path[1]);
    return false;
}

bool onKill(const std::string& command, cli::ShellArguments const& arguments)
{
    using namespace cli::prettyprint;
    
    if (arguments.arguments.size() < 2) {
	      std::cout << std::endl;
	      std::cout << "----------------------LISTADO DE SEÑALES----------------------" << std::endl;
	      std::cout << std::endl;
	      std::cout << "1) SIGHUP        2) SIGINT       3) SIGQUIT      4) SIGILL" << std::endl;
	      std::cout << "5) SIGTRAP       6) SIGABRT      7) SIGBUS       8) SIGFPE" << std::endl;
	      std::cout << "9) SIGKILL      10) SIGUSR1     11) SIGSEGV     12) SIGUSR2" << std::endl;
	      std::cout << "13) SIGPIPE     14) SIGALRM     15) SIGTERM     17) SIGCHLD" << std::endl;
	      std::cout << "18) SIGCONT     19) SIGSTOP     20) SIGTSTP     21) SIGTTIN" << std::endl;
	      std::cout << "22) SIGTTOU     23) SIGURG      24) SIGXCPU     25) SIGXFSZ" << std::endl;
	      std::cout << "26) SIGVTALRM   27) SIGPROF     28) SIGWINCH    29) SIGIO" << std::endl;
	      std::cout << "30) SIGPWR      31) SIGSYS      34) SIGRTMIN    35) SIGRTMIN+1" << std::endl;
	      std::cout << "36) SIGRTMIN+2  37) SIGRTMIN+3  38) SIGRTMIN+4  39) SIGRTMIN+5" << std::endl;
	      std::cout << "40) SIGRTMIN+6  41) SIGRTMIN+7  42) SIGRTMIN+8  43) SIGRTMIN+9" << std::endl;
	      std::cout << "44) SIGRTMIN+10 45) SIGRTMIN+11 46) SIGRTMIN+12 47) SIGRTMIN+13" << std::endl;
	      std::cout << "48) SIGRTMIN+14 49) SIGRTMIN+15 50) SIGRTMAX-14 51) SIGRTMAX-13" << std::endl;
	      std::cout << "52) SIGRTMAX-12 53) SIGRTMAX-11 54) SIGRTMAX-10 55) SIGRTMAX-9" << std::endl;
	      std::cout << "56) SIGRTMAX-8  57) SIGRTMAX-7  58) SIGRTMAX-6  59) SIGRTMAX-5" << std::endl;
	      std::cout << "60) SIGRTMAX-4  61) SIGRTMAX-3  62) SIGRTMAX-2  63) SIGRTMAX-1" << std::endl;
	      std::cout << "64) SIGRTMAX" << std::endl;   
	      std::cout << std::endl;
	      return false;
    }
    else if (arguments.arguments.size() == 2) {
      int pid = atoi(arguments.arguments[1].c_str());
      std::cout << "Matamos el proceso con pid: " << pid << std::endl;
      kill(pid, SIGTERM);
    }
    
    else if (arguments.arguments.size() == 4) {
      if (strcmp(arguments.arguments[1].c_str(),"-s") == 0){
	int senal = atoi(arguments.arguments[2].c_str());
	int pid = atoi(arguments.arguments[3].c_str());
	kill(pid, senal);
      }
    }
    return false;
}

bool onTest(const std::string& command, cli::ShellArguments const& arguments)
{
    using namespace cli::prettyprint;
    
    if (arguments.arguments.size() < 2) {
      std::cout << "-----------------------------------Comando Test--------------------------------------" << std::endl;
      std::cout << "Uso: test expresion..." << std::endl;
      std::cout << std::endl;
      std::cout << "Expresiones:" << std::endl;
      std::cout << "-b fichero verdad si fichero existe y es un fichero especial de bloques." << std::endl;
      std::cout << "-c fichero verdad si fichero existe y es un fichero especial de caracteres." << std::endl;
      std::cout << "-d fichero verdad si fichero existe y es un directorio." << std::endl;
      std::cout << "-e fichero verdad si fichero existe." << std::endl;
      std::cout << "-f fichero verdad si fichero existe y es un fichero regular." << std::endl;
      std::cout << "-g fichero verdad si fichero existe y tiene el bit SGID." << std::endl;
      std::cout << "-h fichero verdad si fichero existe y es un enlace simbólico o blando." << std::endl;
      std::cout << "-p fichero verdad si fichero existe y es una tubería con nombre (FIFO)." << std::endl;
      std::cout << "-r fichero verdad si fichero existe y se puede leer." << std::endl;
      std::cout << "-s fichero verdad si fichero existe y tiene un tamaño mayor que cero." << std::endl;
      std::cout << "-t fd verdad si el descriptor de fichero fd está abierto y se refiere a una terminal." << std::endl;
      std::cout << "-u fichero verdad si fichero existe y tiene el bit SUID." << std::endl;
      std::cout << "-w fichero verdad si fichero existe y se puede modificar." << std::endl;
      std::cout << "-x fichero verdad si fichero existe y es ejecutable." << std::endl;
      std::cout << "-L fichero verdad si fichero existe y es un enlace simbólico o blando." << std::endl;
      std::cout << "-z cadena verdad si la longitud de cadena es cero." << std::endl;
      std::cout << "-n cadena verdad si la longitud de cadena no es cero." << std::endl;
      std::cout << "cad1 = cad2 verdad si las cadenas son iguales." << std::endl;
      std::cout << "cad1 != cad2 verdad si las cadenas no son iguales." << std::endl;
      std::cout << "INTEGER1 -eq INTEGER2	INTEGER1 es igual a INTEGER2" << std::endl;
      std::cout << "INTEGER1 -ge INTEGER2	INTEGER1 es mayor o igual a INTEGER2" << std::endl;
      std::cout << "INTEGER1 -gt INTEGER2	INTEGER1 es mayor que INTEGER2" << std::endl;
      std::cout << "INTEGER1 -le INTEGER2	INTEGER1 es menor o igual a INTEGER2" << std::endl;
      std::cout << "INTEGER1 -lt INTEGER2	INTEGER1 es menor que INTEGER2" << std::endl;
      std::cout << "INTEGER1 -ne INTEGER2	INTEGER1 es no igual a INTEGER2" << std::endl;
      std::cout << "FILE1 -ef FILE2	FILE1 y FILE2 tienen el mismo device y numero de inode" << std::endl;
      std::cout << "FILE1 -nt FILE2	FILE1 es mas nuevo que FILE2" << std::endl;
      std::cout << "FILE1 -ot FILE2	FILE1 es mas antiguo que FILE2" << std::endl;
      std::cout << std::endl;
    }
    
    if (arguments.arguments.size() == 3) {
      std::cout << arguments.arguments[1].c_str() << std::endl;
      
       if (strcmp(arguments.arguments[1].c_str(),"-n") != 0){
	 if (strlen(arguments.arguments[2].c_str()) != 0)
	    std::cout << "TRUE" << std::endl;
	 else
	    std::cout << "FALSE" << std::endl;
      }
       else if (strcmp(arguments.arguments[1].c_str(),"-z") == 0){
	 if (strlen(arguments.arguments[1].c_str()) == 0)
	    std::cout << "TRUE" << std::endl;
	 else
	    std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-b") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && S_ISBLK(buf.st_mode)) 
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-c") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && S_ISCHR(buf.st_mode))
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-d") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && S_ISDIR(buf.st_mode))
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-e") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1)
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-f") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && S_ISREG(buf.st_mode))
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-g") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && (buf.st_mode & S_ISGID))
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-h") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && (S_ISLNK(buf.st_mode))) 
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-L") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && S_ISLNK(buf.st_mode)) 
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-p") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && S_ISFIFO(buf.st_mode)) 
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-r") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && (buf.st_mode & S_IRUSR))
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-s") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && buf.st_size > 0)
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-S") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && S_ISSOCK(buf.st_mode)) 
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-k") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && (buf.st_mode & S_ISVTX))
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl; 
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-u") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && (buf.st_mode & S_ISUID))
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-w") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && (buf.st_mode & S_IWUSR))
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
       else if (strcmp(arguments.arguments[1].c_str(),"-x") == 0){
	 struct stat buf;
	 if (stat(arguments.arguments[2].c_str(),&buf) != -1 && (buf.st_mode & S_IXUSR))
	   std::cout << "TRUE" << std::endl;
	 else
	   std::cout << "FALSE" << std::endl;
       }
    }
    
    if (arguments.arguments.size() == 4) {
      if (strcmp(arguments.arguments[2].c_str(),"=") == 0){
	if (strcmp(arguments.arguments[1].c_str(),arguments.arguments[3].c_str()) == 0)
		      std::cout << "TRUE" << std::endl;
		    else
		      std::cout << "FALSE" << std::endl;
      }
      else if (strcmp(arguments.arguments[2].c_str(),"!=") == 0){
	if (strcmp(arguments.arguments[1].c_str(),arguments.arguments[3].c_str()) != 0)
		      std::cout << "TRUE" << std::endl;
		    else
		      std::cout << "FALSE" << std::endl;
      }
      else if (strcmp(arguments.arguments[2].c_str(),"-eq") == 0){
	int num1 = atoi(arguments.arguments[1].c_str());
	int num2 = atoi(arguments.arguments[3].c_str());
	if (num1 == num2)
	  std::cout << "TRUE" << std::endl;
	else
	  std::cout << "FALSE" << std::endl;
      }
      else if (strcmp(arguments.arguments[2].c_str(),"-ge") == 0){
	int num1 = atoi(arguments.arguments[1].c_str());
	int num2 = atoi(arguments.arguments[3].c_str());
	if (num1 >= num2)
	  std::cout << "TRUE" << std::endl;
	else
	  std::cout << "FALSE" << std::endl;
      }
      else if (strcmp(arguments.arguments[2].c_str(),"-gt") == 0){
	int num1 = atoi(arguments.arguments[1].c_str());
	int num2 = atoi(arguments.arguments[3].c_str());
	if (num1 > num2)
	  std::cout << "TRUE" << std::endl;
	else
	  std::cout << "FALSE" << std::endl;
      }
      else if (strcmp(arguments.arguments[2].c_str(),"-le") == 0){
	int num1 = atoi(arguments.arguments[1].c_str());
	int num2 = atoi(arguments.arguments[3].c_str());
	if (num1 <= num2)
	  std::cout << "TRUE" << std::endl;
	else
	  std::cout << "FALSE" << std::endl;
      }
      else if (strcmp(arguments.arguments[2].c_str(),"-lt") == 0){
	int num1 = atoi(arguments.arguments[1].c_str());
	int num2 = atoi(arguments.arguments[3].c_str());
	if (num1 < num2)
	  std::cout << "TRUE" << std::endl;
	else
	  std::cout << "FALSE" << std::endl;
      }
      else if (strcmp(arguments.arguments[2].c_str(),"-ne") == 0){
	int num1 = atoi(arguments.arguments[1].c_str());
	int num2 = atoi(arguments.arguments[3].c_str());
	if (num1 != num2)
	  std::cout << "TRUE" << std::endl;
	else
	  std::cout << "FALSE" << std::endl;
      }
      else if (strcmp(arguments.arguments[2].c_str(),"-ef") == 0){
	 struct stat buf,buf2;
	 if (stat(arguments.arguments[1].c_str(),&buf) != -1 && stat(arguments.arguments[3].c_str(),&buf2) != -1){
	   if (buf.st_ino == buf2.st_ino && buf.st_dev == buf2.st_dev){
	      std::cout << "TRUE" << std::endl;
	   }
	   else{
	      std::cout << "FALSE" << std::endl;
	   }
	 }
      }
      else if (strcmp(arguments.arguments[2].c_str(),"-nt") == 0){
	struct stat buf,buf2;
	 if (stat(arguments.arguments[1].c_str(),&buf) != -1 && stat(arguments.arguments[3].c_str(),&buf2) != -1){
	   if (buf.st_mtime >= buf2.st_mtime)
	      std::cout << "TRUE" << std::endl;
	   else
	      std::cout << "FALSE" << std::endl;
	 }
      }
      else if (strcmp(arguments.arguments[2].c_str(),"-ot") == 0){
	struct stat buf,buf2;
	 if (stat(arguments.arguments[1].c_str(),&buf) != -1 && stat(arguments.arguments[3].c_str(),&buf2) != -1){
	   if (buf.st_mtime <= buf2.st_mtime)
	      std::cout << "TRUE" << std::endl;
	   else
	      std::cout << "FALSE" << std::endl;
	}
      }
    }
    return false;
}

/*
// Function to be invoked by the interpreter when the user inputs any
// other command.
//
// If this function returns true, the interpreter ends.
//
// cli::ShellArguments is an alias of cli::parser::shellparser::Arguments.
// See include/cli/shell.hpp for its definition.
//
//    struct ShellArguments
//    {
//        enum TypeOfTerminator
//        {
//            NORMAL,             // command ;
//            BACKGROUNDED,       // command &
//            PIPED               // command1 | command2
//        };
//
//        std::vector<VariableAssignment> variables;
//        std::vector<std::string> arguments;
//        std::vector<StdioRedirection> redirections;
//        TypeOfTerminator terminator;
//        ...
//    };
//
//    struct VariableAssignment
//    {
//        std::string name;
//        std::string value;
//    };
//
//    struct StdioRedirection
//    {
//        enum TypeOfRedirection
//        {
//            INPUT,              // command < filename
//            TRUNCATED_OUTPUT,   // command > filename
//            APPENDED_OUTPUT     // command >> filename
//        };
//
//        TypeOfRedirection type;
//        std::string argument;
//    };
*/

bool onOtherCommand(const std::string& command,
    cli::ShellArguments const& arguments)
{
    using namespace cli::prettyprint;

    /*std::cout << prettyprint;
    std::cout << "command:   " << command << std::endl;
    std::cout << "arguments: " << arguments << std::endl;
    std::cout << "------------------------" << std::endl;
    std::cout << noprettyprint << std::endl;*/

    int pipeFileDes[2] = {-1, -1};
    
    for (unsigned i = 0; i < arguments.variables.size(); ++i){
      std::cout << "Este comando será ejecutado con la variable de entorno " << arguments.variables[i].name.c_str() << " a " <<  arguments.variables[i].value.c_str() << std::endl;
      setenv(arguments.variables[i].name.c_str(),arguments.variables[i].value.c_str(),1);
    }
      
    if (arguments.terminator == cli::ShellArguments::PIPED)
      pipe(pipeFileDes);
    
    pid_t childPid = fork();
    if (childPid == 0) {            // Proceso hijo
      if (aux > 0) {
			dup2(aux,0);	// duplicamos para poder cerrarlo después
			close(aux);
      }	
      if (arguments.terminator == cli::ShellArguments::PIPED) {
			close (pipeFileDes[0]);
			dup2(pipeFileDes[1], 1);
			close (pipeFileDes[1]);
      }
      for (unsigned i = 0; i < arguments.redirections.size(); ++i){
            int fd = -1;
            int mode = S_IRUSR | S_IWUSR |      // u+rw
                       S_IRGRP | S_IWGRP |      // g+rw
                       S_IROTH | S_IWOTH;       // o+rw
  		if (arguments.redirections[i].type ==
                	cli::StdioRedirection::TRUNCATED_OUTPUT) {
                	fd = open(arguments.redirections[i].argument.c_str(),
                   	 O_CREAT | O_TRUNC | O_WRONLY, mode);
                	if (fd >= 0) {
                   		dup2(fd, 1);    // El fd 1 se cierra antes de duplicar
                    		close(fd);
                	}
            	}
		if (arguments.redirections[i].type ==
                	cli::StdioRedirection::APPENDED_OUTPUT) {
                	fd = open(arguments.redirections[i].argument.c_str(),
                   	 O_CREAT | O_APPEND | O_WRONLY, mode);
                	if (fd >= 0) {
                   		dup2(fd, 1);    // El fd 1 se cierra antes de duplicar
                    		close(fd);
                	}
            	}
		if (arguments.redirections[i].type ==
                	cli::StdioRedirection::INPUT) {
                	fd = open(arguments.redirections[i].argument.c_str(),
                   	 O_RDONLY, mode);
                	if (fd >= 0) {
                   		dup2(fd, 0);    // El fd 1 se cierra antes de duplicar
                    		close(fd);
                	}
            	}
		if (fd < 0) {
                std::cerr << program_invocation_short_name
                          << ": open: "
                          << strerror(errno)
                          << std::endl;
                exit(1);
            	}
      }
      if (! arguments.arguments.empty()) {
            // Ejecutar el programa indicado en la línea de comandos.
            // Usamos execvp() para que el programa sea buscado en el PATH
            char** argv = cli::utility::stdVectorStringToArgV(arguments.arguments);
            execvp(argv[0], argv);
            // Si execvp() retorna, es porque ha habido algún error.
            // Mostramos el error y matamos el proceso para no tener múltiples
            // shells ejectuándose a la vez.
            std::cerr << program_invocation_short_name
                      << ": execvp: "
                      << strerror(errno)
                      << std::endl;
            exit(127);
      }
      exit(0);
    }
    else {			    // Proceso padre
  	// Si no se quiso lanzar en background (terminador '&') esperar a que el
    	// proceso hijo termine
	if (childPid > 0 ) {
		if (arguments.terminator == cli::ShellArguments::PIPED) {
			close(pipeFileDes[1]);
			aux = pipeFileDes[0];
		}
		if (arguments.terminator == cli::ShellArguments::NORMAL) {
			waitpid(childPid, NULL, 0);
		}
    	}
    	else {
       	 std::cerr << program_invocation_short_name
                  << ": fork: "
                  << strerror(errno)
                  << std::endl;
    	}
    	// Espera no bloqueante para recuperar la información acerca de los hijos
    	// evitando la aparición de procesos zombi.
    	while(waitpid(-1, NULL, WNOHANG) > 0);
    	return false;
    }
    return false;
}

bool onLswc(const std::string& command,
    cli::ShellArguments const& arguments)
{
    using namespace cli::prettyprint;

    std::cout << prettyprint;
    std::cout << "command:   " << command << std::endl;
    std::cout << "arguments: " << arguments << std::endl;
    std::cout << "------------------------" << std::endl;
    std::cout << noprettyprint << std::endl;

    int pipeFileDes[2] = {-1, -1};
    int result = pipe(pipeFileDes);
        if (result != 0) {
            std::cerr << program_invocation_short_name
                      << ": pipe: "
                      << strerror(errno)
                      << std::endl;
        }
    pid_t childPid = fork();
    if (childPid == 0) {            // Proceso hijo
	   dup2(pipeFileDes[1], 1);
           close(pipeFileDes[0]);
           close(pipeFileDes[1]);
           char **argv;
	   argv = (char**)malloc(2*sizeof(char*));
	   argv[0] = (char*)malloc(3*sizeof(char));
	   argv[1] = NULL;
	   argv[0] = "ls";
	   execvp(argv[0], argv);
            // Si execvp() retorna, es porque ha habido algún error.
            // Mostramos el error y matamos el proceso para no tener múltiples
            // shells ejectuándose a la vez.
            std::cerr << program_invocation_short_name
                      << ": execvp: "
                      << strerror(errno)
                      << std::endl;
            exit(127);
    }
    else {			    // Proceso padre
  	// Si no se quiso lanzar en background (terminador '&') esperar a que el
    	// proceso hijo termine
   	if (childPid > 0 ) {
      	  if (arguments.terminator == cli::ShellArguments::NORMAL) {
            waitpid(childPid, NULL, 0);
       	  }
    	}
    	else {
       	 std::cerr << program_invocation_short_name
                  << ": fork: "
                  << strerror(errno)
                  << std::endl;
    	}
    }
    childPid = fork();
    if (childPid == 0) {            // Proceso hijo
	   dup2(pipeFileDes[0], 0);
           close(pipeFileDes[0]);
           close(pipeFileDes[1]);
           char **argv;
	   argv = (char**)malloc(2*sizeof(char*));
	   argv[0] = (char*)malloc(3*sizeof(char));
	   argv[1] = NULL;
	   argv[0] = "wc";        
	   execvp(argv[0], argv);

            // Si execvp() retorna, es porque ha habido algún error.
            // Mostramos el error y matamos el proceso para no tener múltiples
            // shells ejectuándose a la vez.
            std::cerr << program_invocation_short_name
                      << ": execvp: "
                      << strerror(errno)
                      << std::endl;
            exit(127);
    }
    else {			    // Proceso padre
	 close(pipeFileDes[0]);
          close(pipeFileDes[1]);
  	// Si no se quiso lanzar en background (terminador '&') esperar a que el
    	// proceso hijo termine
   	if (childPid > 0 ) {
      	  if (arguments.terminator == cli::ShellArguments::NORMAL) {
            waitpid(childPid, NULL, 0);
       	  }
    	}
    	else {
       	 std::cerr << program_invocation_short_name
                  << ": fork: "
                  << strerror(errno)
                  << std::endl;
    	}
    }
    return false;
}

bool onMi_ls(const std::string& command,
    cli::ShellArguments const& arguments)
{
    using namespace cli::prettyprint;

    std::cout << prettyprint;
    std::cout << "command:   " << command << std::endl;
    std::cout << "arguments: " << arguments << std::endl;
    std::cout << "------------------------" << std::endl;
    std::cout << noprettyprint << std::endl;

    pid_t childPid = fork();
    if (childPid == 0) {            // Proceso hijo
	for (unsigned i = 0; i < arguments.redirections.size(); ++i){
            int fd = -1;
            int mode = S_IRUSR | S_IWUSR |      // u+rw
                       S_IRGRP | S_IWGRP |      // g+rw
                       S_IROTH | S_IWOTH;       // o+rw
  		if (arguments.redirections[i].type ==
                	cli::StdioRedirection::TRUNCATED_OUTPUT) {
                	fd = open(arguments.redirections[i].argument.c_str(),
                   	 O_CREAT | O_TRUNC | O_WRONLY, mode);
                	if (fd >= 0) {
                   		 dup2(fd, 1);    // El fd 1 se cierra antes de duplicar
                    		close(fd);
                	}
            	}
		if (fd < 0) {
                std::cerr << program_invocation_short_name
                          << ": open: "
                          << strerror(errno)
                          << std::endl;
                exit(1);
            	}
    	}
           char **argv;
	   argv = (char**)malloc(2*sizeof(char*));
	   argv[0] = (char*)malloc(3*sizeof(char));
	   argv[1] = (char*)malloc(3*sizeof(char));
	   argv[0] = "ls"; 
	   argv[1] = "-l";
	   std::cout << argv[0] << endl;
	   std::cout << argv[1] << endl; 
	   execvp(argv[0], argv);

            // Si execvp() retorna, es porque ha habido algún error.
            // Mostramos el error y matamos el proceso para no tener múltiples
            // shells ejectuándose a la vez.
            std::cerr << program_invocation_short_name
                      << ": execvp: "
                      << strerror(errno)
                      << std::endl;
            exit(127);
    }
    else {			    // Proceso padre
  	// Si no se quiso lanzar en background (terminador '&') esperar a que el
    	// proceso hijo termine
   	if (childPid > 0 ) {
      	  if (arguments.terminator == cli::ShellArguments::NORMAL) {
            waitpid(childPid, NULL, 0);
       	  }
    	}
    	else {
       	 std::cerr << program_invocation_short_name
                  << ": fork: "
                  << strerror(errno)
                  << std::endl;
    	}
    	// Espera no bloqueante para recuperar la información acerca de los hijos
    	// evitando la aparición de procesos zombi.
    	while(waitpid(-1, NULL, WNOHANG) > 0);
    	return false;
    }
    return false;
}
//
// Main function
//

int main(int argc, char** argv)
{
    // Create the shell-like interpreter
    cli::ShellInterpreter interpreter;

    // Set the intro and prompt texts
    interpreter.introText(INTRO_TEXT);
    interpreter.promptText(PROMPT_TEXT);

    // Set the callback function that will be invoked when the user inputs
    // the 'exit' command
    interpreter.onRunCommand("exit", &onExit);
    interpreter.onRunCommand("echo", &onEcho);
    interpreter.onRunCommand("cd", &onCd);
    interpreter.onRunCommand("kill", &onKill);
    interpreter.onRunCommand("test", &onTest);
    interpreter.onRunCommand("mi_ls", &onMi_ls);
    interpreter.onRunCommand("lswc", &onLswc);

    // Set the callback function that will be invoked when the user inputs
    // any other command
    interpreter.onRunCommand(&onOtherCommand);

    // Run the interpreter
    interpreter.loop();

    return 0;
}
