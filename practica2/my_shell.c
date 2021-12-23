/*
nivelA.c - Adelaida Delgado (adaptación de nivel3.c)
Cada nivel incluye la funcionalidad de la anterior.
nivel A: Muestra el prompt, captura la línea de comandos, 
la trocea en tokens y chequea si se trata de comandos internos 
(cd, export, source, jobs, fg, bg o exit). Si son externos los ejecuta con execvp()
*/

#define _POSIX_C_SOURCE 200112L

#define DEBUGN1 1 //parse_args()
#define DEBUGN3 1 //execute_line()

#define PROMPT_PERSONAL 1 // si no vale 1 el prompt será solo el carácter de PROMPT

#define RESET_FORMATO "\033[0m"
#define NEGRO_T "\x1b[30m"
#define NEGRO_F "\x1b[40m"
#define GRIS "\x1b[94m"
#define ROJO_T "\x1b[31m"
#define VERDE_T "\x1b[32m"
#define AMARILLO_T "\x1b[33m"
#define AZUL_T "\x1b[34m"
#define MAGENTA_T "\x1b[35m"
#define CYAN_T "\x1b[36m"
#define BLANCO_T "\x1b[97m"
#define NEGRITA "\x1b[1m"

#define COMMAND_LINE_SIZE 1024
#define ARGS_SIZE 64
#define N_JOBS 24 // cantidad de trabajos permitidos

char const PROMPT = '$';

#include <errno.h>  //errno
#include <stdio.h>  //printf(), fflush(), fgets(), stdout, stdin, stderr, fprintf()
#include <stdlib.h> //setenv(), getenv()
#include <string.h> //strcmp(), strtok(), strerror()
#include <unistd.h> //NULL, getcwd(), chdir()
#include <sys/types.h> //pid_t
#include <sys/wait.h>  //wait()
#include <signal.h> //signal(), SIG_DFL, SIG_IGN, SIGINT, SIGCHLD
#include <fcntl.h> //O_WRONLY, O_CREAT, O_TRUNC
#include <sys/stat.h> //S_IRUSR, S_IWUSR

int check_internal(char **args);
int internal_cd(char **args);
int internal_export(char **args);
int internal_source(char **args);
int internal_jobs();
int internal_bg(char **args);
int internal_fg(char **args);

char *read_line(char *line);
int parse_args(char **args, char *line);
int execute_line(char *line);
int jobs_list_find(pid_t pid);
int  jobs_list_remove(int pos);
int jobs_list_add(pid_t pid,char status,char *cmd);
int is_background(char **args);
void ctrlc(int signum);

int is_output_redirection (char **args);
int execute_line(char *line); 


static char mi_shell[COMMAND_LINE_SIZE]; //variable global para guardar el nombre del minishell

//static pid_t foreground_pid = 0;
struct info_process {
	pid_t pid;
	char status;
	char cmd[COMMAND_LINE_SIZE];
};
static struct info_process jobs_list[N_JOBS]; //Tabla de procesos. La posición 0 será para el foreground
static int n_pids=1;
void imprimir_prompt();

int check_internal(char **args) {
    if (!strcmp(args[0], "cd"))
        return internal_cd(args);
    if (!strcmp(args[0], "export"))
        return internal_export(args);
    if (!strcmp(args[0], "source"))
        return internal_source(args);
    if (!strcmp(args[0], "jobs"))
        return internal_jobs(args);
    if (!strcmp(args[0], "bg"))
        return internal_bg(args);
    if (!strcmp(args[0], "fg"))
        return internal_fg(args);
    if (!strcmp(args[0], "exit"))
        exit(0);
    return 0; // no es un comando interno
}

int internal_cd(char **args) {
    printf("[internal_cd()→ comando interno no implementado]\n");
    return 1;
} 

int internal_export(char **args) {
    printf("[internal_export()→ comando interno no implementado]\n");
    return 1;
}

int internal_source(char **args) {
    printf("[internal_source()→ comando interno no implementado]\n");
    return 1;
}

int internal_jobs(char **args) {
    #if DEBUGN1 
        printf("[internal_jobs()→ Esta función mostrará el PID de los procesos que no estén en foreground]\n");
    #endif
    
    for(int i=1; i<n_pids;i++) {
        printf("[%d] %d \n%c \n%s \n",i,jobs_list[i].pid, jobs_list[i].status, jobs_list[i].cmd);
    }
    return 1;
}
//comando fg
int internal_fg(char **args) {
    if (args[1] == NULL) {	// Si el numero de trabajo no esta especificado, error y salir
		fprintf(stderr,"No job number specified\n");
		return -1;
	}else{
        // Obtenemos el índice del trabajo 
        // Restamos 48 que es el valor del caracter '0' en ASCII
        int pos = (int) *(args[1]) - 48;

        if((pos >= n_pids) || (pos == 0)){                                   // No existe un trabajo en la posicion especificada
            fprintf(stderr, "no existe ese trabajo\n");
            return -1;
        
        }else{
            //si esta parado
            if (jobs_list[pos].status=='D')
            {
                //enviamos señal
              printf("[internal_fg() -> Enviando la señal SIGCONT al proceso %d\n",jobs_list[pos].pid);
		      kill(jobs_list[pos].pid, SIGCONT);
            }
            // Pasamos los parametros del proceso en background a foreground
            jobs_list[0].status = 'E';
	        jobs_list[0].pid = jobs_list[pos].pid;	
	        strcpy(jobs_list[0].cmd, jobs_list[pos].cmd);
            strcpy(jobs_list[0].cmd, strtok(jobs_list[0].cmd, "&"));
            //eliminamos el trabajo 
            jobs_list_remove(pos);
            printf(" %s\n", jobs_list[0].cmd);
            while (jobs_list[0].pid != 0) {
		          pause();
	        }

        }
    }
    return 1;
}
//comando bg
int internal_bg(char **args) {

    if(args[1]==NULL){
        fprintf(stderr,"Error");
        return EXIT_FAILURE;
    }else{
        int pos = (int) *(args[1]) - 48;
        if((pos>=n_pids)||(pos==0)){
            fprintf(stderr,"No existe ese trabajo \n");
            return EXIT_FAILURE;
        }else{
            if(jobs_list[pos].status=='E'){
                fprintf(stderr,"El trabajo ya está en segundo plano\n");
                return EXIT_FAILURE;
            }else{
                //actualizamos el estado el trabajo
                jobs_list[pos].status='E';
                //añadimos el & y el acabar linea
                strcat(jobs_list[pos].cmd,"&\0");
                //enviamos señal
                kill(jobs_list[pos].pid,SIGCONT);
                fprintf(stderr, "[internal_bg() -> Señal %d enviada a %d (%s)\n", SIGCONT, jobs_list[pos].pid, jobs_list[pos].cmd);  
                

                fprintf(stderr,"[%d] %d \t%c \t%s \n", pos, jobs_list[pos].pid, jobs_list[pos].status, jobs_list[pos].cmd);
                //Mostramos por pantalla el numero de trabajo, el PID, el estado y el cmd
                return EXIT_SUCCESS;
                
            }
        }
    }
    return 1;
}

void imprimir_prompt() {
#if PROMPT_PERSONAL == 1
    printf(NEGRITA ROJO_T "%s" BLANCO_T ":", getenv("USER"));
    printf(AZUL_T "MINISHELL" BLANCO_T "%c " RESET_FORMATO, PROMPT);
#else
    printf("%c ", PROMPT);

#endif
    fflush(stdout);
    return;
}


char *read_line(char *line) {
  
    imprimir_prompt();
    char *ptr=fgets(line, COMMAND_LINE_SIZE, stdin); // leer linea
    if (ptr) {
        // ELiminamos el salto de línea (ASCII 10) sustituyéndolo por el \0
        char *pos = strchr(line, 10);
        if (pos != NULL){
            *pos = '\0';
        } 
	}  else {   //ptr==NULL por error o eof
        printf("\r");
        if (feof(stdin)) { //se ha pulsado Ctrl+D
            fprintf(stderr,"Bye bye\n");
            exit(0);
        }   
    }
    return ptr;
}

int parse_args(char **args, char *line) {
    int i = 0;

    args[i] = strtok(line, " \t\n\r");
    #if DEBUGN1 
        fprintf(stderr, GRIS "[parse_args()→ token %i: %s]\n" RESET_FORMATO, i, args[i]);
    #endif
    while (args[i] && args[i][0] != '#') { // args[i]!= NULL && *args[i]!='#'
        i++;
        args[i] = strtok(NULL, " \t\n\r");
        #if DEBUGN1 
            fprintf(stderr, GRIS "[parse_args()→ token %i: %s]\n" RESET_FORMATO, i, args[i]);
        #endif
    }
    if (args[i]) {
        args[i] = NULL; // por si el último token es el símbolo comentario
        #if DEBUGN1 
            fprintf(stderr, GRIS "[parse_args()→ token %i corregido: %s]\n" RESET_FORMATO, i, args[i]);
        #endif
    }
    return i;
}

int execute_line(char *line) {
    
    char *args[ARGS_SIZE];
    pid_t pid, status;
    char command_line[COMMAND_LINE_SIZE];

    //copiamos la línea de comandos sin '\n' para guardarlo en el array de structs de los procesos
    memset(command_line, '\0', sizeof(command_line)); 
    strcpy(command_line, line); //antes de llamar a parse_args() que modifica line

    if (parse_args(args, line) > 0) {
        if (check_internal(args)) {
            return 1;
        }
        else{
            int status;
            int background ;
            background = is_background(args);
            pid_t pid=fork();
            //proceso hijo
            if (pid==0) {
                //asociamos la accion por defecto a sigchld
                signal(SIGCHLD,SIG_DFL);
                //ignoraremos sigint
                signal(SIGINT,SIG_IGN);
                signal(SIGTSTP,SIG_IGN);
                is_output_redirection(args);
                execvp(args[0],args);
                
                fprintf(stderr,"Error comando externo: %s\n",args[0]); 
                fflush(stderr);
                exit(EXIT_FAILURE);
            }
            //asociamos el manejador ctrlc a la señal sigint            //preso padre
            else if (pid>0) {
                if(background==0){
                        //signal(SIGINT,ctrlc);
                     strcpy(jobs_list[0].cmd,line);
                     jobs_list[0].pid=pid;
                     jobs_list[0].status='E';
                     
                         printf("[execute_line() → PID padre: %d] %s\n",getpid(),jobs_list[1].cmd);
                         printf("[execute_line() → PID hijo: %d] %s\n",pid,jobs_list[0].cmd);
                    
                    while(jobs_list[0].pid>0){
                          pause();
                    }

                }else{
                    #if DEBUG5
                        printf("proceso en background");
                    #endif
                    jobs_list_add(pid,'E',command_line);
                }
                    
            }
            else{
                perror("fork error");
                exit(EXIT_FAILURE);
            }
        }
        fprintf(stderr, GRIS "[execute_line()→ PID padre: %d]\n" RESET_FORMATO, getpid());
    }
    return EXIT_SUCCESS;
}
//añadimos nuevo elemento al array
int jobs_list_add(pid_t pid,char status,char *cmd){
    //si no estamos en el final
    if(n_pids<N_JOBS){
        //asignamos los atributos del parametro
        jobs_list[n_pids].status = status;
        jobs_list[n_pids].pid= pid;
        strcpy(jobs_list[n_pids].cmd,cmd);
        //aumentamos los n_pids
        n_pids++;
        return 1;
    }
    else{
        fprintf(stderr,"no caben mas procesos");
        return 0;
    }
}
//Busca en el array de trabajos el PID que recibe como argumento y retorna la posición en él.
int jobs_list_find(pid_t pid){ 
    for(int i=0;i<N_JOBS;i++){
        if(jobs_list[i].pid == pid){
            return i;
        }
    }
    return -1;
}
//borra posicion
int  jobs_list_remove(int pos){
  
   if(pos<N_JOBS && pos>0){
       //anula los atributos de ese trabajo
       jobs_list[pos].pid='\0';
       jobs_list[pos].status='\0';
       memset(jobs_list[0].cmd, '\0',COMMAND_LINE_SIZE);
       n_pids--;
       //asigna el proceso anterior
       if(n_pids != 0){
           strcpy(jobs_list[pos].cmd,jobs_list[n_pids].cmd);
           jobs_list[pos].pid=jobs_list[n_pids].pid;
           jobs_list[pos].status=jobs_list[n_pids].status;
       }
       return EXIT_SUCCESS;
   } else {
       fprintf(stderr,"posicion incorrect\n");
       return EXIT_FAILURE;
   }
}

void reaper(int signum){
    signal(SIGCHLD,reaper);
    //la asociamos a esta misma función void reaper(int signum){
    pid_t ended;
    int status;
    
    //controlamos si el hijo que acaba se ejecuta en primer plano
    while ((ended=waitpid(-1, &status, WNOHANG)) > 0) {

       if(ended==jobs_list[0].pid) {
           //cambiamos el pid, el status y el cmd
           jobs_list[0].pid=0;
           jobs_list[0].status='F';
           memset(jobs_list[0].cmd, '\0', strlen(jobs_list[0].cmd));
        
             if (WIFEXITED(status))
             {

            printf("reaper:[Proceso hijo %d () finalizado con exit code %d]\n", ended, WEXITSTATUS(status));

             }
             else if (WIFSIGNALED(status))
             {

            printf("reaper:[Proceso hijo %d () finalizado con exit code %d]\n", ended, WTERMSIG(status));

             }
       }
       else{
           int pos=jobs_list_find(ended);
           
           if (WIFEXITED(status))
            {
                #if DEBUG5
                    printf("\n[reaper() -> Proceso hijo %d (ps f) en background (%s) finalizado con exit code %d]\n", ended, jobs_list[pos].cmd, WEXITSTATUS(status));
                #endif
            }
            else if (WIFSIGNALED(status))
            {
                #if DEBUG5
                    printf("\n[Proceso hijo %d (ps f) en background (%s) finalizado con exit code %d]\n", ended, jobs_list[pos].cmd, WTERMSIG(status));
                #endif
            }
            printf("Proceso finalizado con PID %d (%s) en jobs_list[%d] con status %d\n", ended, jobs_list[pos].cmd, pos, status);
            
            jobs_list_remove(pos);
       }
    }
}
//comprueba si esta en background
int is_background(char **args){
      int pos =0;
      while(args[pos]!=NULL){
          if(strcmp(args[pos],"&")==0){
              args[pos] =NULL;
              return 1;
          }
          pos++;
      }
      return 0;
}
//Manejador propio para la señal SIGINT (Ctrl+C)
void ctrlc(int signum) {
   
    signal(SIGINT,ctrlc);
    //si hay proceso en foregraund
    if(jobs_list[0].pid>0){
        
        if(strcmp(jobs_list[0].cmd,mi_shell)) {
            //enviar señal SIGTERM
            kill(jobs_list[0].pid, SIGTERM);
            printf("señal SIGTERM envaida");
        }
        else {
          
              printf("Señal SIGTERM no enviada debido a que el proceso en foreground es el shell");
           
        }

    }else{
        
        printf("[ctrlc()→ Soy el proceso con PID %d (%s), el proceso en foreground es %d ()]\n",getpid(),jobs_list[0].cmd,jobs_list[0].pid);

        printf("ctrlc()→Señal SIGTERM no enviada por %d (%s) debido a que no hay proceso en foreground",getpid(),jobs_list[0].cmd);
        
    }
    printf("\n");
    fflush(stdout);
    imprimir_prompt();
}
void ctrlz(int signum){
    //asociamos manejador
    signal(SIGTSTP,ctrlz);
    printf("\n ctrlz()-->proceso conPID %d,el de foreground es %d (%s)]\n",getpid(),jobs_list[0].pid,jobs_list[0].cmd);
   //si hay prceso en foreground
    if(jobs_list[0].pid>0){ 
       
        if(strcmp(jobs_list[0].cmd,mi_shell)){ 
            //enviar señal
            kill(jobs_list[0].pid,SIGTSTP);
            printf("\n ctrlz()-->señal %d (SIGSTOP) enviada a %d (%s)por %d (%s)]\n",signum,jobs_list[0].pid,jobs_list[0].cmd,getpid(),mi_shell);
            jobs_list[0].status='D';
            //añadimos
            jobs_list_add(jobs_list[0].pid,jobs_list[0].status,jobs_list[0].cmd);
            jobs_list[0].status='N';
            jobs_list[0].pid=0;
            
        }else{
            printf("ctrlz()->señal %d (SIGTSTP) no enviada , proces in foreground es el shell",signum);
        }
    }else{
       printf("ctrlz() -> Señal %d (SIGTSTP) no enviada debido a que no hay proceso en el foreground\n", signum);
        } 
    
    fflush(stdout);
}

int is_output_redirection (char **args){

    for(int i=0; args[i]!=NULL ;i++){
       if(strcmp(args[i],">")==0){
          if(args[i+1]!=NULL){
              int d=open(args[i+1], O_WRONLY | O_CREAT, S_IRWXU);
              dup2(d,STDOUT_FILENO);
              close(d);
              args[i]=NULL;
              args[i+1]=NULL;
              return 1;
          }else{
              fprintf(stderr,"error de sintaxis");
              
          }
       }
    }
    return 0;
}

int main(int argc, char *argv[]) {

    char line[COMMAND_LINE_SIZE];
   
    memset(line, 0, COMMAND_LINE_SIZE);
    //obtener el nombre del programa que actúa como minishell
    strcpy(mi_shell, argv[0]);
    
    jobs_list[0].pid=0;
    jobs_list[0].status='N';
    memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);
    //añadimos escucha de señales
    signal(SIGCHLD, reaper);
    signal(SIGINT, ctrlc);
    signal(SIGTSTP,ctrlz);
    while (1) {
        if (read_line(line)) { // !=NULL
            execute_line(line);
            
        }
    }
    return 0;
}






