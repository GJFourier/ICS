/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and ID here>
 * 郭俊甫 521021910522
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/

// 本次作业中以对所有的系统调用函数进行包装，进行额外的类型检查
int SIGFILLSET(sigset_t *mask){
    int value = sigfillset(mask);
    if(value == 0)
        return 0;
    else {
        printf("sigfillset failed.\n");
        exit(-1);
    }
}
int SIGEMPTYSET(sigset_t *mask){
    int value = sigemptyset(mask);
    if(value == 0)
        return 0;
    else {
        printf("sigemptyset failed.\n");
        exit(-1);
    }
}
int SIGADDSET(sigset_t *mask, int signal){
    int value = sigaddset(mask, signal);
    if(value == 0)
        return 0;
    else {
        printf("sigaddset failed.\n");
        exit(-1);
    }
}
int SIGPROCMASK(int how, sigset_t *mask, sigset_t *prev){
    int value = sigprocmask(how, mask, prev);
    if(value == 0)
        return 0;
    else {
        printf("sigprocmask failed.\n");
        exit(-1);
    }
}
int SETPGID(pid_t pid, pid_t pgid){
    int value = setpgid(pid, pgid);
    if(value == 0)
        return 0;
    else {
        printf("setpgid failed.\n");
        exit(-1);
    }
}
int EXECVE(char* command, char *argv[], char *environ[]){
    int value = execve(command, argv, environ);
    if(value == 0)
        return 0;
    else {
        printf("%s: Command not found\n", command);
        exit(-1);
    }
}
int KILL(pid_t pid, int signal){
    int value = kill(pid, signal);
    if(value == 0)
        return 0;
    else {
        printf("Send signal failed\n");
        exit(-1);
    }
}
int SIGSUSPEND(sigset_t *mask){
    int value = sigsuspend(mask);
    return value;
}
pid_t FORK(){
    pid_t pid;
    if((pid = fork()) < 0)
        printf("Fork failed.\n");
    return pid;
}
void eval(char *cmdline) 
{
    char* argv[MAXARGS];
    char buf[MAXLINE];
    int bg;
    pid_t pid;
    sigset_t mask_all, mask_child, prev;

    // 解析命令行
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);

    // 空白行
    if(argv[0] == NULL)
        return;

    // 是内置命令则在builtin_cmd中执行，否则按照非内置命令执行
    if(!builtin_cmd(argv)){
        // 阻塞SIG_CHLD，防止在addjob之前子进程结束
        SIGFILLSET(&mask_all);
        SIGEMPTYSET(&mask_child);
        SIGADDSET(&mask_child, SIGCHLD);
        SIGPROCMASK(SIG_BLOCK, &mask_child, &prev);
        if((pid = FORK()) == 0){
            // 在子进程中运行程序
            SIGPROCMASK(SIG_SETMASK, &prev, NULL);
            SETPGID(0, 0);
            EXECVE(argv[0], argv, environ);
        }
        SIGPROCMASK(SIG_SETMASK, &mask_all, NULL);
        addjob(jobs, pid, bg?BG:FG, cmdline);
        SIGPROCMASK(SIG_SETMASK, &prev, NULL);

        // 防止printf前后台已结束,因此此时仍然需要阻塞SIG_CHLD
        SIGPROCMASK(SIG_BLOCK, &mask_child, NULL);
        if(!bg){
            waitfg(pid);
        }
        else{
            SIGPROCMASK(SIG_SETMASK, &mask_all, NULL);
            printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
        }
        SIGPROCMASK(SIG_SETMASK, &prev, NULL);
    }
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    int flag = 0;
    // 判断是否为4种内置命令，是则执行
    if(strcmp(argv[0], "quit") == 0){
        flag = 1;
        exit(0);
    }
    if(strcmp(argv[0], "jobs") == 0){
        // jobs为全局变量，需要阻塞信号
        sigset_t mask_all, prev;
        SIGFILLSET(&mask_all);
        SIGPROCMASK(SIG_SETMASK, &mask_all, &prev);
        listjobs(jobs);
        SIGPROCMASK(SIG_SETMASK, &prev, NULL);
        flag = 1;
    }
    if(strcmp(argv[0], "bg") == 0 || strcmp(argv[0], "fg") == 0){
        do_bgfg(argv);
        flag = 1;
    }
    return flag;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    if(argv[1] == NULL){
        printf("%s command requires PID or %%jobid argument\n", argv[0]);
        return;
    }
    int id = 0;
    char percent = '\0';
    struct job_t* job = NULL;

    // jobs为全局变量，需要阻塞信号
    sigset_t mask_all, prev;
    SIGFILLSET(&mask_all);
    SIGPROCMASK(SIG_SETMASK, &mask_all, &prev);

    // jid
    if(sscanf(argv[1], "%c%d", &percent, &id) > 0 && percent == '%'){
//        printf("abc");
//        printf("%d", id);
        job = getjobjid(jobs, id);
        if(job == NULL) {
            printf("%%%d: No such job\n", id);
            return;
        }
    }

    // pid
    else if(sscanf(argv[1], "%d", &id) > 0){
        job = getjobpid(jobs, id);
        if(job == NULL){
            printf("(%d): No such process\n", id);
            return;
        }
    }
    // 参数错误
    else{
        printf("%s: argument must be a PID or %%jobid\n", argv[0]);
        return;
    }
    // 后台运行
    if(strcmp(argv[0], "bg") == 0){
        job->state = BG;
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
        SIGPROCMASK(SIG_SETMASK, &prev, NULL);
        KILL(-(job->pid), SIGCONT);
    }
    // 前台运行
    if(strcmp(argv[0], "fg") == 0){
        job->state = FG;
//        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
        SIGPROCMASK(SIG_SETMASK, &prev, NULL);
        KILL(-(job->pid), SIGCONT);
        waitfg(job->pid);
    }
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    sigset_t mask_empty;
    SIGEMPTYSET(&mask_empty);
    // 存在前台进程
    while (fgpid(jobs) > 0)
        SIGSUSPEND(&mask_empty);
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    int olderrno = errno;
    pid_t pid;
    int status;
    sigset_t mask_all, prev;
    SIGFILLSET(&mask_all);
    while ((pid=waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
        SIGPROCMASK(SIG_SETMASK, &mask_all, &prev);
        // 正常退出
        if(WIFEXITED(status)){
            deletejob(jobs, pid);
        }
        // 未被捕获信号退出
        else if(WIFSIGNALED(status)){
            struct job_t* job = getjobpid(jobs, pid);
//            WRITE(STDOUT_FILENO, "Job [", 5);
//            WRITE(STDOUT_FILENO, itoa(job->jid), 4);
//            WRITE(STDOUT_FILENO, "] (", 3);
//            WRITE(STDOUT_FILENO, itoa(job->pid), 4);
//            WRITE(STDOUT_FILENO, ") terminated by signal ", 23);
//            WRITE(STDOUT_FILENO, itoa(signal), 4);
//            WRITE(STDOUT_FILENO, "\n", 1);
            printf("Job [%d] (%d) terminated by signal %d\n", job->jid, job->pid, WTERMSIG(status));
            deletejob(jobs, pid);
        }
        // 停止
        else if(WIFSTOPPED(status)){
            struct job_t* job = getjobpid(jobs, pid);
            job->state = ST;
//            WRITE(STDOUT_FILENO, "Job [", 5);
//            WRITE(STDOUT_FILENO, itoa(job->jid), 4);
//            WRITE(STDOUT_FILENO, "] (", 3);
//            WRITE(STDOUT_FILENO, itoa(job->pid), 4);
//            WRITE(STDOUT_FILENO, ") stopped by signal ", 20);
//            WRITE(STDOUT_FILENO, itoa(signal), 4);
//            WRITE(STDOUT_FILENO, "\n", 1);
            printf("Job [%d] (%d) stopped by signal %d\n", job->jid , job->pid, WSTOPSIG(status));
        }
        SIGPROCMASK(SIG_SETMASK, &prev, NULL);
    }
    errno = olderrno;
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    int olderrno = errno;
    pid_t pid = fgpid(jobs);
    // 获取前台进程组pgid并发送信号
    if(pid != 0)
        KILL(-pid, SIGINT);
    errno = olderrno;
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    int olderrno = errno;
    pid_t pid = fgpid(jobs);
    // 获取前台进程组pgid并发送信号
    if (pid != 0)
        KILL(-pid, SIGTSTP);
    errno = olderrno;
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    SIGEMPTYSET(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



