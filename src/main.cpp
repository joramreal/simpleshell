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
    interpreter.onRunCommand("mi_ls", &onMi_ls);
    interpreter.onRunCommand("lswc", &onLswc);

    // Set the callback function that will be invoked when the user inputs
    // any other command
    interpreter.onRunCommand(&onOtherCommand);

    // Run the interpreter
    interpreter.loop();

    return 0;
}
