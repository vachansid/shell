// Compile using the following command:
// g++ Assignment2_36_20CS10070_20CS10071_20CS10025_20CS10017.cpp -o shell -lreadline



#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>
#include <readline/readline.h>
#include <readline/history.h>
#include<bits/stdc++.h>
#include<fstream>
#include<sstream>

using namespace std;

#define MAX_HISTORY 1000

/*Global variables*/
vector<string> commandHistory;
string historyFile = "history.txt";
int historyIndex = -1;

set<int> fpids;
int bg=0;
string latest_text ="";
int inp=0;

/*Functions used*/
int execute_pipe(vector<char*>);
void delep(char*);
int execute(string);
void loadHistory();
void addToHistory(string);
int uparrow(int,int);
int downarrow(int,int);
string seperate(string,char);
int checkinp(vector<char*>,int);
int checkout(vector<char*>,int);
void remove(vector<char*>,int,int);
vector<string> match_file(string);
vector<char*> split(char*,char);
void delep(char*);
void sb(int,vector<char*>);


void ctrl_c_handler(int signum)
{
    historyIndex = commandHistory.size();
    signal(SIGINT,ctrl_c_handler);
    printf("\n"); 
    rl_on_new_line();
    rl_replace_line("", 0); 
    rl_redisplay();
}
void ctrl_z_handler(int signum)
{
    historyIndex = commandHistory.size();
    signal(SIGTSTP,ctrl_z_handler);
    if(inp==0){
    printf("\n"); 
    rl_on_new_line();
    rl_replace_line("", 0); 
    rl_redisplay();
    }
    
}
void sigchld_handler(int signum)
{
    signal(SIGCHLD,sigchld_handler);
    int flag=0;
    while(1)
    {  
       int s;
       int wpid=waitpid(-1,&s,WNOHANG | WUNTRACED);
       if(wpid<=0) break;
       if(fpids.count(wpid)!=0)
       {
         fpids.erase(wpid);
         if(WIFSTOPPED(s)) kill(wpid,SIGCONT);    
       }
       else flag=1;
    }
    if(flag==1)
    {
        rl_on_new_line();
        rl_replace_line("", 0); 
        rl_redisplay();
    }
}
void sigchldBlocker(int state)
{
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGCHLD);
    sigprocmask(state, &sigs, NULL);
}
void Blocker(int state)
{
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTSTP);
    sigprocmask(state, &sigs, NULL);
}
int main()
{
    loadHistory();
    rl_bind_keyseq("\001", rl_beg_of_line);
    rl_bind_keyseq("\005", rl_end_of_line);
    rl_bind_keyseq("\033[A", uparrow);
    rl_bind_keyseq("\033[B", downarrow);
    signal(SIGINT, ctrl_c_handler);
    signal(SIGTSTP, ctrl_z_handler);
    signal(SIGCHLD, sigchld_handler); 
    int status;
    do
    {   char *in=new char[1024];
        inp=0;
        in = readline("\nshell>");
        historyIndex = commandHistory.size();
        if(commandHistory.size()==0 || (commandHistory.size()!=0 &&strcmp(in,commandHistory[commandHistory.size()-1].c_str())!=0))
        {
            addToHistory(in);    
        }
        inp=1;
        string input;
        input = seperate(in, '<');
        input = seperate(input, '>');
        input = seperate(input,'&');
        bg = 0;
        int end=input.length()-1;
        while(input[end]==' ') end--;
        input[end+1]='\0';
        if(input[end]=='&') bg=1;
        status=execute(input); 
        free(in);
    }
    while(status==EXIT_SUCCESS);
    return 0;
}


int execute_command(char* command,int in_fd,int out_fd)
{
        int k=strlen(command);
        vector<char *> args=split(command,' ');
      
        if (strcmp(args[0], "pwd") == 0)    //handling pwd
        {
            char cwd[1024];
            getcwd(cwd, sizeof(cwd));
            cout << cwd << endl;
        }
        else if (strcmp(args[0], "cd") == 0)   //handling cd
        {
            if (args.size()==1) chdir("~");
            else if (chdir(args[1]) != 0)
            {
                printf("shell: cd: %s: No such file or directory\n",args[1]);
            }     
        }
        else if (strcmp(args[0], "exit") == 0)   //handling exit
        {
            return 1;
        }
        else if (strcmp(args[0],"delep")==0)     //handling delep
        {
            if(args.size()!=2) 
            {
                 cout << "\ndelep: usage:  delep /path/to/the/file" << endl;
                return EXIT_SUCCESS; 
            }
            delep(args[1]);
        }
        else if (strcmp(args[0],"sb")==0)   //handling sb
        {
            if(args.size()==3||args.size()==2) sb(args.size(),args);
            else 
            {
                cout << "\nsb: usage:  sb <PID> [-suggest]" << endl;
                return EXIT_SUCCESS; 
            }
            
        }
        else
        {
            int in_redirect = checkinp(args, args.size());
            int out_redirect = checkout(args, args.size());
            int status;
            if(strcmp(args[args.size()-1],"&")==0) args.pop_back(); 
            args.push_back(NULL);
            pid_t pid;
            sigchldBlocker(SIG_BLOCK);
            if(bg==1) Blocker(SIG_BLOCK);
            pid=fork();
            if(pid==0)
            {    
                sigchldBlocker(SIG_UNBLOCK);
                if(bg==0) Blocker(SIG_UNBLOCK);
                
                if (strcmp(args[0],"xargs")==0)
                {
                    int fd;
                    fd=open("/dev/null",O_WRONLY);
                    close(2);
                    dup(fd);
                }
                if(in_fd!=0)
                {
                    dup2(in_fd,0);
                    close(in_fd);
                }
                if(out_fd!=1)
                {
                    dup2(out_fd,1);
                    close(out_fd);
                }
                if(in_redirect>=0)
                {
                    int fd=open(args[in_redirect+1], O_RDONLY);
                    dup2(fd,STDIN_FILENO);
                    close(fd);
                    remove(args, args.size(), in_redirect);
                }
                if (out_redirect >=0)
                {
                    int fd = open(args[out_redirect + 1], O_WRONLY | O_CREAT, 0644);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                    remove(args,args.size(), out_redirect);
                }
                if(execvp(args[0],args.data())==-1)
                {
                    perror("Error in execvp");
                    exit(EXIT_FAILURE);
                }
                
            }
            Blocker(SIG_UNBLOCK);
            if(!bg)
            {
                fpids.insert(pid);   
            }
            sigchldBlocker(SIG_UNBLOCK);
        }        
        args.clear();
        return EXIT_SUCCESS;
}

int execute_pipe(vector<char*> commands,int in_fd,int out_fd)
{
    int pipefd[2];
    int f=0;
    int status;
    for(int i=0;i<commands.size();i++)
    {
        if(i==commands.size()-1)
        {
            status=execute_command(commands[i],in_fd,out_fd);
            break;
        }
        if(pipe(pipefd) == -1)
        {
            f=1;
            perror("Pipe Error");
            break;
        }
        else
        {
            status=execute_command(commands[i],in_fd,pipefd[1]);
            close(pipefd[1]);
            in_fd=pipefd[0];
        }
    }
    if(bg==0)
    {
        while(!fpids.empty());
    }
    return status;
    
}

int execute(string input)
{
    
        char *temp = new char[input.size()+1];
        strcpy(temp,const_cast<char*>(input.c_str()));

        int status;
        fpids.clear();
        vector<char*> piped_commands=split(temp,'|');
        if(piped_commands.size()==0)
        {
            cout << "Error-No command found\n";
            status=EXIT_SUCCESS;
        }
        else 
        {   
            status=execute_pipe(piped_commands,0,1);
        }
        piped_commands.clear();
        return status;
}

void delep(char* file_path)
{
    int l=strlen(file_path);
    char* cmd=new char[(l+100)*sizeof(char)];
    memset(cmd,0,l+100);
    strcat(cmd,"lsof -t ");
    strcat(cmd,file_path);
    //cout << "\nPIDs which have the given file open(if any):\n";
    cout << endl;
    int status;
    status=execute_command(cmd,0,1);
    while(!fpids.empty());
    if(status!=EXIT_SUCCESS) 
    {
        cout << "delep error\n";
        return;
    }
    memset(cmd,0,l+100);
    strcat(cmd,"lslocks | grep ");
    strcat(cmd,file_path);
    strcat(cmd,"$ |sed s/^\\S*\\s*\\(\\S*\\).*/\\1/");
    cout << endl;
    cout << "\nPIDs which are holding a lock over the given file(if any):\n";
    status=execute(cmd);   
    if(status!=EXIT_SUCCESS) 
    {
        cout << "delep error\n";
        return;
    }
    cout << "\nDo you want to kill all the above processes(if any) and delete the file if exists[y/n]?";
    string s;
    cin >> s;
    if(s=="y"||s=="Y"){        
         memset(cmd,0,l+100);
         strcat(cmd,"lsof -t ");
         strcat(cmd,file_path);
         strcat(cmd," | xargs kill -9");
         status=execute(cmd);
         if(status!=EXIT_SUCCESS) 
         {
            cout << "delep error\n";
            return;
         }
         
         unlink(file_path);
         cout << "\nFile deleted if exists\n";
    }
    
}
vector<pid_t> getChildProcesses(pid_t parentPid) {
    vector<pid_t> childProcesses;
    char dirPath[1000];
    sprintf(dirPath, "/proc/%d/task/%d/children", parentPid,parentPid);
    std::ifstream infile(dirPath);
    int temp;
    while(infile>>temp) {
        childProcesses.push_back(temp);
    }
    infile.close();
    return childProcesses;
}
//function to get cpu usage time
long long int getCPUt(pid_t pid){
    char dirPath[1000];
    sprintf(dirPath, "/proc/%d/stat", pid);
    FILE* fp=fopen(dirPath,"r");
    long long int ut,st,cut,cst;
    fscanf(fp,"%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lld %lld %lld %lld",&ut, &st, &cut, &cst);
    return (ut+st+cut+cst);
}

// Function to detect the pID of all trouble
pid_t detectMalware(vector<pid_t> process) {
    //Ctime is sum of cpu time of all the children of a pid
    //time is cpu time of a particular pid
    vector <long long int>noc,time,Ctime;
    for(auto pid :process){
        time.push_back(getCPUt(pid));
        vector<pid_t>children=getChildProcesses(pid);
        noc.push_back(children.size());
        long long int temp=0;
        for(auto cpid:children){
            temp+=getCPUt(cpid);
        }
        Ctime.push_back(temp);
    }
    // Traverse the process path that we got to find suspicious processes
    long double maxratio=0;
    int index=0;
    for(int i=0;i<process.size()-1;i++){
        if(time[i]==0){
            index=i;
            break;
        }
        long double temp=(long double)Ctime[i]/time[i];
        if(maxratio<temp){
            maxratio=temp;
            index=i;
        }
    } 
    return process[index];
}
void sb(int argc,vector<char*> argv)
{
    //cout<<argc;
    if(argc < 2) {
        cout << "Usage: sb <pid> [-suggest]" << endl;
        return;
    }
    // Get the process id
    pid_t pid = atoi(argv[1]);
    vector<pid_t> process;
    cout << "Process tree path : " << endl;
    pid_t currentPid=pid;
    process.push_back(currentPid);
    cout << "Process " << currentPid<< "-->";
    while(currentPid!=1) {
        char dirPath[1000];
        sprintf(dirPath, "/proc/%d/status", currentPid);
        FILE *fp=fopen(dirPath,"r");
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "PPid:", 5) == 0) {
                currentPid= atoi(line + 5);
                break;
            }
        }
        process.push_back(currentPid);
        cout << currentPid<<"-->";
    }
    cout<<"\n";
    // If the -suggest flag is present, Now detect the root of all trouble
    if(argc > 2 && string(argv[2]) == "-suggest") {
        //using process vector of pids to find the malware
        pid_t malwarePid = detectMalware(process);
        if(malwarePid != 1) {
            cout << "Malware detected, PID of process thats causing all trouble: " << malwarePid << endl;
        } else {
            cout << "No malware is detected." << endl;
        }
    }
}
void loadHistory() 
{

    ifstream in(historyFile);
    if (!in.is_open()) {
        cout << "No previous history" << endl;
        return;
    }
    string line;
    while (getline(in, line)) {
    commandHistory.push_back(line);
    historyIndex = commandHistory.size();
    if (commandHistory.size() > MAX_HISTORY)
    {
        commandHistory.erase(commandHistory.begin());
        historyIndex--;
    }   
    }  
    in.close();
}

void addToHistory(string cmd)
{    
    commandHistory.push_back(cmd);
    ofstream out(historyFile, ios::app);
    if (!out.is_open()) {
        cout << "Error opening history file" << endl;
        return;
     }
    out << cmd << endl;
    out.close();
    historyIndex = commandHistory.size();
    // Truncate the history if it exceeds 1000 commands
    if (commandHistory.size() > MAX_HISTORY)
    {
        commandHistory.erase(commandHistory.begin());
        historyIndex--;
    }
}


int  uparrow(int count, int key)
{
    
    if (historyIndex >= 0)
    {
        
        
        if(historyIndex==0){
           // if(commandHistory.size()>0){    
            const char* neww = commandHistory[0].c_str();
            rl_replace_line(neww, 0);
            rl_redisplay();  
            rl_end_of_line(0, 0);  
           // }
        }
        else{   
        
            if(historyIndex==commandHistory.size()) 
            {
                latest_text.clear();
                latest_text = rl_line_buffer;  
            }   
            historyIndex--;
            const char* neww = commandHistory[historyIndex].c_str();
            rl_replace_line(neww, 0);
            rl_redisplay();
            rl_end_of_line(0, 0);
            //historyIndex--;     
            
        }
    }

    return 0;
}

int downarrow(int count, int key)
{
       
       if(historyIndex<=commandHistory.size()-1 && historyIndex>=0){
    
        if(historyIndex==commandHistory.size()-1){
            //const char* neww="";
            if(commandHistory.size()!=0){
            rl_replace_line(latest_text.c_str(), 0);
            rl_redisplay();
            rl_end_of_line(0, 0);
            //historyIndex=commandHistory.size();
            }//
            historyIndex++;
        }
        else {
            historyIndex++;
            const char* neww = commandHistory[historyIndex].c_str();
            rl_replace_line(neww, 0);
            rl_redisplay();
            rl_end_of_line(0, 0);
        }
       }
        //if(historyIndex<commandHistory.size()-1)
    return 0;
}

string seperate(string str, char c)
{
    string s1 = "";
    for (int i = 0; i < str.length(); i++)
    {
        if (str[i] != c) s1 = s1 + str[i];
        else s1 = s1 + " " + str[i] + " ";
    }
    return s1;
}
int checkinp(vector<char*>tokens, int k)
{
    for (int i = 0; i < k; i++)
    {
        if (strcmp(tokens[i], "<") == 0) return i; 
    }
    return -1;
}
int checkout(vector<char *>tokens, int k)
{
    for (int i = 0; i < k; i++)
    {
        if (strcmp(tokens[i], ">") == 0)  return i;    
    }
    return -1;
}

void remove(vector<char *>tokens, int k, int r)
{
    for (int j = r; j < k - 2; j++) tokens[j] = tokens[j + 2];
     tokens[k - 2] = NULL;
}

vector<string> match_files(string pattern) 
{
    glob_t results;
    glob(pattern.c_str(), GLOB_TILDE, NULL, &results);
    vector<string> files;
    for (unsigned int i = 0; i < results.gl_pathc; i++) {
    files.push_back(results.gl_pathv[i]);
    }
    globfree(&results);
    return files;
}

vector<char*> split(char* str, char delimiter) 
{
    vector<char*> result;

    //Strip leading and trailing spaces

    int start = 0, end = strlen(str) - 1;
    while (str[start] == ' ') start++;
    while (str[end] == ' ') end--;
    str[end + 1] = '\0';
    char* token;
    char s[2];
    s[0]=delimiter;
    s[1]='\0';
    token = strtok(str+start, s);
    int flag=0;
    while (token != nullptr) {
        //cout<<"|"<<token<<"|";
        char* result1 = strchr(token,'*');
        char* result2=strchr(token,'?');
        if(delimiter==' ' &&(result1!=nullptr || result2!= nullptr)&&strcmp(result[0],"sed")!=0){
            vector<string> files = match_files(token);
           // cout<<"+";
            for(int i=0 ; i<files.size();i++){
               
                result.push_back(const_cast<char*>(files[i].c_str()));
            }
        }
        else{
            //cout<<"*";
            result.push_back(token);
        }
        
        token = strtok(nullptr, s);
    }
    // result.erase(result.begin());
    return result;
}

