#include<iostream>
#include<string>
#include<queue>
#include<vector>
#include<fstream>
#include<unistd.h>
#include<string.h>
#include<stack>
#include<algorithm>

using namespace std;
size_t BUFFERSIZE = 1024;
int maxprio = 4;
int pid;
FILE *file;
char *buffer;
char delimiters[] = " \t\n";
char *token;
int * my_random;
vector <vector<int> > IO_store;
enum trans_states{TRANS_TO_READY,TRANS_TO_RUN,TRANS_TO_BLOCK,TRANS_TO_PREEMPT,TRANS_TO_FINISHED};
enum states{CREATED, READY, RUNNING, BLOCKED, FINISHED};
enum preempt_reason{NO,Q_EXPIRE,BY_OTHERS};
class Event;

class Myrandom
{
    public:
    int my_burst;
    Myrandom(int burst)
    {
        my_burst = burst;
    }
    int myrand()
    {
        static int ofs = 1;
        int res;
        if (ofs > my_random[0])
            ofs = 1;
        res = 1 + (my_random[ofs] % my_burst);
        ofs = ofs + 1;
        return res;
    }

};

int myrandom(int burst)
{
    static int ofs= 1 ;
    if (ofs>my_random[ 0 ])
        ofs= 1 ;
    int res = 1 +(my_random[ofs]%burst); 
    ofs+= 1 ;
    return res;
}


class IO_block_time
{
    public:
    vector <vector<int> > IO_block;
    IO_block_time(vector <vector<int> > IO_stor)
    {
        IO_block = IO_stor;
    }

    double tot_IO_time()
    {
        sort(IO_block.begin(), IO_block.end());
        int end = 0 ;
        double tot = 0.0 ;
        for (vector <int> line:IO_block)
        {
            int front=line[0];
            int back=line[1];
            if (back<=end)
            {
                continue;
            }
            else if (front<=end)
            {
                tot = tot + (back-end);
                end = back;
            }
            else
            {
                tot = tot + (back-front);
                end = back;
            }
        }
        return tot;
    }
};





class Process {          //-------------------------------------------------// CLASS PROCESS ------------------------------------------------
    public:
    int Id;
    int AT,TC,CB,IO;
    int remain_time;
    states proc_state;
    int static_priority;
    int dynamic_priority;
    bool dynprio_change;
    int previous_time;
    int new_time;
    int tot_IO;
    int tot_CPU_wait;
    preempt_reason preempt_opt;
    Event* remain_event;
    int IO_burst;
    int CPU_burst;
    bool preempt_check;
    //Myrandom *my_random;
    int finish_time;
    

    Process (int pid, int at, int tc, int cb, int io)
    {
        Id=pid;
        AT=at;
        TC=tc;
        CB=cb;
        IO=io;
        CPU_burst=cb;
        IO_burst=io;
        proc_state = CREATED;
        //my_random = new Myrandom(maxprio);
        //static_priority = my_random->myrand();
        static_priority = myrandom(maxprio);
        dynamic_priority = static_priority - 1;
        dynprio_change = false;
        previous_time = 0;
        new_time = at;
        tot_IO = 0;
        tot_CPU_wait = 0;
        preempt_opt = NO;
        remain_event = nullptr;
        preempt_check = false;
        finish_time = 0;
        remain_time = tc;

    }

    void alter_dynprio()
    {
        dynamic_priority = dynamic_priority - 1;
        if (dynamic_priority == -1)
        {
            dynamic_priority = static_priority - 1;
            dynprio_change = true;
        }
    }
};
class Scheduler
{
    public:
    int quantum;
    string schedtype;

    Scheduler()
    {
        quantum = 10000;
    }
    virtual void addprocess(Process* process)
    {}
    virtual Process *get_process()
    {
        return nullptr;
    }

};

class FCFS : public Scheduler
{
    public:
        queue<Process*> runqueue;
    FCFS()
    {
        schedtype = "FCFS";
    }
    void addprocess(Process* process)
    {
        runqueue.push(process);
    }
    Process *get_process()
    {
        if (runqueue.empty() == true)
            return nullptr;
        else
        {       
            Process *new_process = runqueue.front();
            runqueue.pop();
            return new_process;
        }
        
    }
};

class LCFS : public Scheduler
{
    public:
        stack<Process*>runqueue;
    LCFS()
    {
        schedtype = "LCFS";
    }
    void addprocess(Process* process)
    {
        runqueue.push(process);
    }
    Process *get_process()
    {
        if (runqueue.empty() == true)
            return nullptr;
        else
        {     
            Process *new_process = runqueue.top();
            runqueue.pop();
            return new_process;
        }
    }
};

class SRTF : public Scheduler
{
    public:
        vector<Process*>runqueue;
    SRTF()
    {
        schedtype = "SRTF";
    }
    void addprocess(Process *process)
    {
        int flag_check = 0;
        for(int i=0; i<runqueue.size(); i++)
        {
            if (runqueue[i]->remain_time <= process->remain_time)
            {
                runqueue.insert(runqueue.begin()+i, process);
                flag_check = 1;
                break;
            }
        }
        if (flag_check == 0)
            runqueue.push_back(process);
    }
    Process *get_process()
    {
        if (runqueue.empty())
            return nullptr;
        else
        {
        
            Process *new_process = runqueue.back();
            runqueue.pop_back();
            return new_process;
        }
    }
};

class RR : public Scheduler
{
    public:
        queue<Process*>runqueue;
    RR(int quant)
    {
        schedtype = "RR";
        quantum = quant;
    }
    void addprocess(Process* process)
    {
        runqueue.push(process);
    }
    Process *get_process()
    {
        if (runqueue.empty() == true)
            return nullptr;
        else
        {
            Process *new_process = runqueue.front();
            runqueue.pop();
            return new_process;
        }
    }
};

class PRIO : public Scheduler
{
    public:
        queue<Process*> *active_q, *expired_q;
    PRIO(int quant)
    {
        schedtype = "PRIO";
        active_q = new queue<Process*>[maxprio];
        expired_q = new queue<Process*>[maxprio];
        quantum = quant;
    }
    void swap_queues()
    {
        queue<Process*> *temp = expired_q;
        expired_q = active_q;
        active_q = temp;
    }
    void addprocess (Process* process)
    {
        if (process->dynprio_change)
        {
            expired_q[process->dynamic_priority].push(process);
            process->dynprio_change = false ;
        }
        else
            active_q[process->dynamic_priority].push(process);
    }
    Process* get_process ()
    {
        int pri = 0 ;
        while(pri< maxprio)
        {
            if (!active_q[pri]. empty ())
                break ;
            pri++;
        }
        if (pri == maxprio)
            swap_queues();

        int new_prio=maxprio- 1;
        while (new_prio >= 0)
        {
            if (!active_q[new_prio]. empty())
            {
                Process* res = active_q[new_prio].front ();
                active_q[new_prio].pop();
                return res;
            }
            new_prio-= 1 ;
        }
    return nullptr ; }
};

class PREPRIO : public Scheduler
{
    public:
        queue<Process*> *active_q, *expired_q;
    PREPRIO (int quant)
    {
        schedtype = "PREPRIO" ;
        active_q = new queue<Process*>[maxprio];
        expired_q = new queue<Process*>[maxprio];
        quantum = quant;
    }
    void swap_queues()
    {
        queue<Process*>* temp = expired_q;
        expired_q = active_q;
        active_q = temp;
    }
    void addprocess (Process* process)
    {
        if (process->dynprio_change)
        {
            expired_q[process->dynamic_priority].push(process);
            process->dynprio_change= false;
        }
        else
            active_q[process->dynamic_priority].push(process);
    }
    Process* get_process ()
    {
        int pri = 0 ;
        while (pri<maxprio)
        {
            if (!active_q[pri].empty())
            break ;
            pri ++;
        }
        if (pri == maxprio)
            swap_queues();
        int new_prio = maxprio- 1 ;
        while (new_prio>= 0 )
        {
            if (!active_q[new_prio].empty())
            {
                Process* res = active_q[new_prio].front();
                active_q[new_prio].pop();
                return res;
            }
            new_prio-= 1;
        }
        return nullptr ;
    }
};



class Event
{
    public:
    int timestamp;
    Process *process_event;
    trans_states tran_state;
    bool next;
    //vector<Event*> event_queue;

    Event(int time_stamp, Process *process, trans_states transtate)
    {
        timestamp = time_stamp;
        process_event = process;
        tran_state = transtate;
        next = false;
    }

  /*  Event *getEvent ()
    {
        if (!event_queue.empty())
            return event_queue.front();
        else
            return nullptr ;
    }

    void push_event(Event *event)
    {
        int flag_check = 0;
        for (int i=0; i<event_queue.size(); i++)
        {
            if (event_queue[i]->timestamp <= event->timestamp)
            {
                event_queue.insert(event_queue.begin(),event);
                flag_check = 1 ;
                break ;
            }
        }
        if (flag_check == 0 )
            event_queue.push_back(event);
    }
    void pop_event()
    {
        event_queue.erase(event_queue.begin());
    }  */
};

class DES
{
    public:
    vector<Event*> event_queue;
    
    Event *getEvent ()
    {
        if (!event_queue.empty())
            return event_queue.back();
        else
            return nullptr ;
    }
    void push_event(Event *event)
    {
        int flag_check = 0;
        for (int i=0; i<event_queue.size(); i++)
        {
            if (event_queue[i]->timestamp <= event->timestamp)
            {
                event_queue.insert(event_queue.begin()+i, event);
                flag_check = 1 ;
                break ;
            }
        }
        if (flag_check == 0 )
            event_queue.push_back(event);
    }
    void pop_event()
    {
        event_queue.pop_back();
    }
    bool queueEmpty (){
        return event_queue.empty();
    }
};

Scheduler *newtype(char instr, int quantum)
{
    Scheduler *new_sched = nullptr;
    if (instr == 'F' )  new_sched = new FCFS ();
    else if (instr == 'L' ) new_sched = new LCFS ();
    else if (instr == 'S' ) new_sched = new SRTF ();
    else if (instr == 'R' ) new_sched = new RR (quantum);
    else if (instr == 'P' ) new_sched = new PRIO (quantum);
    else if (instr == 'E' ) new_sched = new PREPRIO (quantum);

    if (instr != 'F' && instr != 'L' && instr != 'S' ){
        cout<<new_sched->schedtype<< " " <<new_sched->quantum<<endl; }
    else {
           cout<<new_sched->schedtype<<endl;
           }

    return new_sched;
};

void simulation(DES *des, char instr, int quant)
{

    //Scheduler *new_scheduler = new Scheduler();
    Scheduler *new_scheduler = new Scheduler();
    new_scheduler = newtype(instr,quant);
    Event* eve;
    Event* new_eve;
    Process * running = nullptr;
    vector <int> local;
    int total_time;
    while (des->getEvent() != nullptr)
    {
        eve = des->getEvent();
        des->pop_event();
        if (eve->next)
            continue ;
        Process *proc = eve->process_event;
        int CURRENT_TIME = eve->timestamp;
        proc->previous_time = CURRENT_TIME - proc->new_time;
        
        bool call_scheduler;
        switch (eve->tran_state) {
       
        case TRANS_TO_READY:

            if (proc->proc_state == BLOCKED) {
                proc->dynamic_priority = proc->static_priority - 1 ;
                proc->tot_IO += proc->previous_time;
            }

            if (instr == 'E' ) {
                proc->preempt_check = true ;
            }

            if (proc->preempt_check && running != nullptr &&
            proc->dynamic_priority > running->dynamic_priority &&
                running->remain_event!= nullptr &&
                running->remain_event->timestamp != CURRENT_TIME)
            {
                running->remain_event->next= true ;
                new_eve = new Event (CURRENT_TIME,running,TRANS_TO_PREEMPT);
                running->remain_event = new_eve;
                running->preempt_opt = BY_OTHERS;
                des-> push_event (new_eve);
            }
            proc->proc_state = READY;
            new_scheduler->addprocess(proc);
            proc->new_time = CURRENT_TIME;
            call_scheduler = true;
            break ;
            
        case TRANS_TO_RUN:
            
            if (proc->preempt_opt == NO)
                        proc->CPU_burst = min ( myrandom (proc->CB), proc->remain_time);
                           proc->preempt_opt = NO;
                           proc->proc_state = RUNNING;
                           proc->new_time = CURRENT_TIME;
                           proc->tot_CPU_wait += proc->previous_time;
                           running = proc;
            
            if (proc->remain_time <= min (proc->CPU_burst, new_scheduler->quantum)) {
                new_eve = new Event (CURRENT_TIME + proc->remain_time, proc, TRANS_TO_FINISHED);
            }
            
            else if (proc->CPU_burst <= new_scheduler->quantum) {
                new_eve = new Event (CURRENT_TIME + proc->CPU_burst, proc, TRANS_TO_BLOCK);
            }
            
            else {
                proc->preempt_opt = Q_EXPIRE;
                new_eve = new Event (CURRENT_TIME + new_scheduler->quantum, proc, TRANS_TO_PREEMPT);
            }
            proc->remain_event = new_eve;
            des-> push_event (new_eve);
            break ;
           
        case TRANS_TO_BLOCK:
            
            proc->IO_burst = myrandom (proc->IO);
            proc->remain_time -= proc->previous_time;
            proc->proc_state = BLOCKED;
            proc->new_time = CURRENT_TIME;
            running = nullptr;
            new_eve = new Event (CURRENT_TIME + proc->IO_burst, proc, TRANS_TO_READY);
            local.push_back(CURRENT_TIME);
            total_time = CURRENT_TIME + proc->IO_burst;
            local.push_back(total_time);
            IO_store. push_back (local);
            local.clear();
            proc->remain_event = new_eve;
            des-> push_event(new_eve);
            call_scheduler = true ; break ;
            
        case TRANS_TO_PREEMPT:

            proc->remain_time -= proc->previous_time;
            proc->CPU_burst -= proc->previous_time;
            running = nullptr ;
            proc->proc_state = READY;
            proc-> alter_dynprio();
            proc->new_time = CURRENT_TIME;
            new_scheduler->addprocess(proc);
            call_scheduler = true;
            break;
        
        case TRANS_TO_FINISHED:

            running = nullptr;
            proc->remain_time = 0;
            proc->CPU_burst = 0;
            proc->proc_state = FINISHED;
            proc->new_time = CURRENT_TIME;
            call_scheduler = true;
            break;
        }
    
        delete eve;
        eve = nullptr;

        if (call_scheduler)
        {
            if (des->getEvent()!= nullptr && des->getEvent()->timestamp==CURRENT_TIME)
                continue ;
            call_scheduler = false ;
            if (running == nullptr)
            {
                running = new_scheduler->get_process();
                if (running == nullptr)
                    continue ;
                new_eve = new Event (CURRENT_TIME,running,TRANS_TO_RUN);
                proc->remain_event = new_eve;
                des-> push_event (new_eve); 
            }
        }
    }
    delete new_scheduler;
    new_scheduler = nullptr;
}
   
int main(int argc, char **argv)
{
    queue<Process> event_queue;
    FILE *file;
    int flag_check= 0 ;
    char *sched_type = nullptr;
    int opt_char;
    while ((opt_char = getopt(argc, argv, "vs:")) != - 1 )
    {
        switch (opt_char)
        {
            case 'v':
                flag_check= 1 ;
                break;
            case 't':
                break;
            case 'e':
                break;
            case 's':
                sched_type= optarg;
                break;
        }
    }

    string schedType = sched_type;
    int quantum = 0;
    char instruct = schedType[0];

    int idx = (schedType.find(":"));
    if (idx != -1)
    {
       quantum = stoi(schedType.substr(1, idx));
        maxprio = stoi(schedType.substr(idx+1));
    }
    else if (instruct!= 'F' && instruct!= 'L' && instruct!= 'S'){
        quantum = stoi(schedType.substr(1)); }

    int num;
    ifstream getfile(argv[ optind + 1 ]);
    getfile>>num;
    my_random = new int[num+1];
    my_random[0] = num;
    for (int i= 1 ;i<num+ 1 ;i++)
        getfile>>my_random[i];

    file = fopen (argv[optind],"r");
    int size = getline(&buffer, &BUFFERSIZE, file);
    int pid = 0;
    vector<Process> process_list;
    vector<int> each_val;

    while (size != -1)
        {
        token = strtok(buffer, delimiters);
        each_val.push_back(atoi(token));
        for ( int i= 1 ;i< 4 ;i++)
        {
            token = strtok ( nullptr, delimiters );
            if (token!= NULL )
            each_val.push_back(atoi(token));
        }
        Process process = Process(pid,each_val[0],each_val[1],each_val[2],each_val[3]);
        process_list.push_back(process);
        pid+= 1 ;
        each_val.clear();
        size = getline(&buffer, &BUFFERSIZE, file);
        }
   fclose (file);
   DES *des = new DES();
   for (auto &l : process_list)
   {
    Event *new_event = new Event(l.AT, &l, TRANS_TO_READY);
    l.remain_event = new_event;
    des->push_event(new_event);
   }
   simulation(des,instruct,quantum);

    int i = 0;
    int final_last = 0;
    double cpu_utility_time = 0;
    double total_IO_util = 0;
    double tot_TT = 0;
    double tot_CPU_wait = 0;
    IO_block_time *a = nullptr;
            
    for (Process p:process_list)
    {
        p.finish_time = p.AT + p.TC + p.tot_IO + p.tot_CPU_wait; //pid: AT TC CB IO PRIO | FT TT IT CW
        int mean_time = p.finish_time-p.AT;
        printf ( "%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n" , p.Id, p.AT, p.TC, p.CB, p.IO, p.static_priority,
                p.finish_time, mean_time, p.tot_IO, p.tot_CPU_wait); 
        final_last = max(final_last,p.finish_time);
        cpu_utility_time = cpu_utility_time + p.TC;
        tot_TT = tot_TT + p.finish_time-p.AT;
        tot_CPU_wait = tot_CPU_wait + p.tot_CPU_wait;
        i = i + 1 ;
    }
    a = new IO_block_time(IO_store);
    total_IO_util = a->tot_IO_time();
    //total_IO_util = tot_IO_time(IO_store);
    double cpu_utility_per = (cpu_utility_time/final_last)*100;
    double IO_util_per = (total_IO_util/final_last)*100;
    double process_count = process_list.size();
    double avg_TT = (tot_TT/process_count);
    double avg_CPU_wait = (tot_CPU_wait/process_count);
    double throughput = (process_count/final_last)*100;
//Finishing time of the last event
//CPU utilization(%) = cpu_utility_time/final_last
//IO utilization(%) = total_IO_util/final_last
//Average turnaround time among processes = tot_TT/process_count //Average cpu waiting time among processes = tot_CPU_wait/process_count
//Throughput of number processes per 100 time units = process_count/final_last*100
    printf ( "SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n" , final_last, cpu_utility_per, IO_util_per, avg_TT,avg_CPU_wait, throughput);
    }
