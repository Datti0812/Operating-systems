#include<iostream>
#include <queue>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <algorithm>

using namespace::std;
FILE *file;
char *buffer;
size_t buffersize = 1024;
char *token;
char delimiters[] = " \t\n";
int head = 0;
int t = 0;
int num = 0;
int tot_shift_count = 0;
int flow = 1;
int short_dist = INT_MAX;
int int_less = INT_MIN;
int ati = 0;
int sstf = 0;
int dir = -1; 
int highest, high_count = 0;
int lowest, low_count = 0;
vector <int> tn;
int clook_count = 0;

struct IO
{
    int id;
    int arrival_time;
    int start_time;
    int end_time;
    int track_number;
};

vector <IO*> swap_queue;
queue <IO*> input_io_queue;
vector <IO*> change_queue;
vector <IO*> wait_queue;

class Scheduler
{
    public:
    virtual IO* get_IO()
    {
        return nullptr;
    }
};

// CLASS FIFO ----------------------------------------------------------------

class FIFO : public Scheduler
{
    public:
    IO *get_IO()
    {
        IO *return_io = nullptr;
        if(!wait_queue.empty())
        {
            return_io = wait_queue[0];
        }
        else if (wait_queue.empty())
        {
            return nullptr;
        }
        if (return_io != nullptr && (return_io->track_number > head))
        {
            flow = 1;
        }
        else
        { 
            flow = -1;
        }
        wait_queue.erase(wait_queue.begin() + 0);
        return return_io;
    }

};

// CLASS SSTF -------------------------------------------------------------

class SSTF : public Scheduler
{
    public:
    IO *get_IO()
    {
        IO *return_io = nullptr;
        if (wait_queue.empty())
        {
            return nullptr;
        }
        else if(!wait_queue.empty())
        {
            for(int i=0; i<wait_queue.size(); i++)
            {
                if(sstf == 0)
                {
                    return_io = wait_queue[i];
                    wait_queue.erase(wait_queue.begin() + (return_io->id));
                    break;
                }
                if (abs(wait_queue[i]->track_number - head) < short_dist)
                {
                    short_dist = abs(wait_queue[i]->track_number - head);
                    return_io = wait_queue[i];

                    if (return_io->track_number > head)
                    {
                        flow = 1;
                    }
                    else
                    { 
                        flow = -1;
                    }
                }
                else
                {
                    continue;
                }   
            }
        }
        if(sstf != 0)
        {
            for(int i =0; i < wait_queue.size(); i++)
            {
                if (wait_queue[i] == return_io)
                {
                    wait_queue.erase(wait_queue.begin() + i);
                }
            }
        }
        sstf = sstf + 1;
        short_dist = INT_MAX;
        return return_io;
    }
};

class LOOK : public Scheduler
{
    public:
    IO *get_IO()
    {
        IO *return_io = nullptr;
        if (wait_queue.empty())
        {
            return nullptr;
        }
        else if(!wait_queue.empty())
        {
            for(int i=0; i<wait_queue.size(); i++)
            {
                if(sstf == 0)
                {
                    return_io = wait_queue[i];
                    if (return_io->track_number > head)
                    {
                        flow = 1;
                    }
                    else
                    { 
                        flow = -1;
                    }
                    wait_queue.erase(wait_queue.begin() + (return_io->id));
                    break;
                }
                if(abs(wait_queue[i]->track_number - head) < short_dist)
                {
                    if ((wait_queue[i]->track_number - head) * flow >= 0)
                    {
                        return_io = wait_queue[i];
                        short_dist = abs(wait_queue[i]->track_number - head);
                    }
                    else
                    {
                        continue;
                    }
                }
            }
            if (return_io == nullptr)
            {
                flow = flow * (-1);
                return_io = get_IO();
            }
        }
        if(sstf != 0)
        {
            for(int i =0; i < wait_queue.size(); i++)
            {
                if (wait_queue[i] == return_io)
                {
                    wait_queue.erase(wait_queue.begin() + i);
                }
            }
        }
        sstf = sstf + 1;
        short_dist = INT_MAX;
        return return_io;
    }
};

class CLOOK : public Scheduler
{
    public:
    IO *get_IO()
    {
        IO *return_io = nullptr;
        if (wait_queue.empty())
        {
            return nullptr;
        }
        else if(!wait_queue.empty())
        {
            for(int i=0; i<wait_queue.size(); i++)
            {
                if(flow == 1)
                {
                    if((wait_queue[i]->track_number - head) * flow >= 0)
                    {
                        if(abs(wait_queue[i]->track_number - head) < short_dist)
                        {
                            short_dist = wait_queue[i]->track_number - head;
                            return_io = wait_queue[i];
                        }
                    }
                    else
                    {
                        continue;
                    }                    
                }
                else if(dir == -1 && flow == -1)
                {
                    if ((wait_queue[i]->track_number - head) < ati)
                    {
                        ati = (wait_queue[i]->track_number - head);
                        return_io = wait_queue[i];
                    }
                    else
                    {
                        continue;
                    }
                     
                }
            }
            if(return_io == nullptr)
            {
                flow = -flow;
                dir = -1;
                return_io = get_IO();
            }
            if(flow == -1 && return_io != nullptr)
            {
                dir = 1;
            }
        }
        for(int i =0; i < wait_queue.size(); i++)
        {
            if (wait_queue[i] == return_io)
            {
                wait_queue.erase(wait_queue.begin() + i);
            }
        }
        
        ati = 0;
        short_dist = INT_MAX;
        return return_io;
    }
};

class FLOOK : public Scheduler
{
    public:
    IO *get_IO()
    {
        IO *return_io = nullptr;
        if(swap_queue.empty() && !wait_queue.empty())
        {
            swap_queue.swap(wait_queue);
        }
        if(swap_queue.empty() && return_io == nullptr && wait_queue.empty())
        {
            return nullptr;
        }
        
        for(int i=0; i<swap_queue.size(); i++)
        {
            if(sstf == 0)
            {
                return_io = swap_queue[i];
                if (return_io->track_number > head)
                {
                    flow = 1;
                }
                else
                { 
                    flow = -1;
                }
                swap_queue.erase(swap_queue.begin() + (return_io->id));
                break;
            }
            int check = (swap_queue[i]->track_number - head) * flow;
            if(abs(swap_queue[i]->track_number - head) < short_dist)
            {
                if(check >= 0)
                {
                    short_dist = abs(swap_queue[i]->track_number - head);
                    return_io = swap_queue[i];
                }
            }
        }
        if(return_io == nullptr)
        {
            if(!swap_queue.empty())
            {
                flow = -flow;
                return_io = get_IO();
            }   
            else if (swap_queue.empty() && !wait_queue.empty()) 
            {
                return_io = get_IO();
            }
        }
        if(sstf != 0 && return_io != nullptr)
        {
            for(int i =0; i < swap_queue.size(); i++)
            {
                if (swap_queue[i] == return_io)
                {
                    swap_queue.erase(swap_queue.begin() + i);
                }
            }
        }
        sstf = sstf + 1;
        short_dist = INT_MAX;
        return return_io;
    }
};

Scheduler *get_algo(char instr)
{
    Scheduler *return_algo_type = nullptr;
    if (instr == 'i') return_algo_type = new FIFO();
    if (instr == 'j') return_algo_type = new SSTF();
    if (instr == 's') return_algo_type = new LOOK();
    if (instr == 'c') return_algo_type = new CLOOK();
    if (instr == 'f') return_algo_type = new FLOOK();

    return return_algo_type;
}

// MAIN function --------------------------------------------------------------
int main(int argc , char** argv)
{
    queue <IO*> output_io_queue;
    int prev;
    int tot_turnaround_time = 0;
    int total_waiting_time = 0;
    int waiting_time;
    int maxim_waiting_time = INT_MIN;
    int current_time = 1;
    int total_move_count = 0;
    char *algo_type;
    int opt_char;
    Scheduler *sched;
    int num_of_IO;
    IO *cur_input_io = nullptr;
    IO *front_io = nullptr;
    int count = 0;
    int final_end;
    
    while((opt_char = getopt(argc, argv, "s:"))!= -1)
    {
        switch (opt_char)
        {
        case 's':
            algo_type = optarg;
            break;
        }
    }
    string str = algo_type; 
   // char algo = str[0];
    sched = get_algo(str[0]);

    file = fopen(argv[optind],"r");
    int size = getline(&buffer, &buffersize, file);
    while(size != -1)
    {
        token = nullptr;
        string line = buffer;
        if (line[0] == '#')
        {
            size = getline(&buffer, &buffersize, file);
            continue;
        }
        else
        {
            IO *io_iter = new IO;
            token = strtok(buffer, delimiters);
            int k = atoi(token);
            io_iter->arrival_time = k;
            token = strtok(nullptr, delimiters);
            int n = atoi(token);
            io_iter->track_number = n;
            io_iter->id = count;
            input_io_queue.push(io_iter);
            change_queue.push_back(io_iter);
            num_of_IO = num_of_IO + 1;
            count = count + 1;
            int flag1 = 0;
            if (flag1 == 0)
            {
                size = getline(&buffer, &buffersize, file);
                continue;
            }
        }
    }
    while(!input_io_queue.empty() || cur_input_io != nullptr || !wait_queue.empty())
    {
        //front_io = input_io_queue.front();
        if(!input_io_queue.empty() && current_time == input_io_queue.front()->arrival_time)
        {
            wait_queue.push_back(input_io_queue.front());
            input_io_queue.pop();
        }
        if (cur_input_io != nullptr)
        {
            if (head != cur_input_io->track_number) 
            {        
                head = head + flow;
                total_move_count = total_move_count + 1;
            }
        }
        if (cur_input_io != nullptr)
        {
            if (head == cur_input_io->track_number) 
            {
                cur_input_io->end_time = current_time;
                change_queue[cur_input_io->id] = cur_input_io;
                cur_input_io = nullptr;
            } 
        }
        while (cur_input_io == nullptr && (!swap_queue.empty() || !wait_queue.empty()))
        {
            cur_input_io = sched->get_IO();
            if (cur_input_io == nullptr)
            {
                break;
            }
            else if(cur_input_io != nullptr)
            {
                cur_input_io->start_time = current_time;
            }
            bool set = cur_input_io->track_number == head;
            if (set)
            {
                cur_input_io->end_time = current_time;
                cur_input_io = nullptr;
            }
            /*else 
            {
                if (flow == 1) 
                    head = head + 1;
                else 
                    head = head - 1;
                tot_shift_count = tot_shift_count + 1;
            }   */
        }
        current_time = current_time + 1;
        final_end = current_time-1;
    }
    for(int i=0; i < num_of_IO; i++)
    {
        IO *out_IO = change_queue[i];
        /*if (i == 0)
        {
            out_IO->arrival_time = cur_input_io->arrival_time;
            out_IO->start_time = cur_input_io->arrival_time;
            out_IO->end_time = cur_input_io->end_time + 1;
            prev = cur_input_io->end_time;
            output_io_queue.push(out_IO);
        }
        else
        {
            out_IO->arrival_time = cur_input_io->arrival_time;
            out_IO->start_time = output_io_queue.back()->end_time;
            out_IO->end_time = abs(cur_input_io->end_time - prev) + out_IO->start_time;
            prev = cur_input_io->end_time;
            output_io_queue.push(out_IO);
        }*/
        tot_turnaround_time = tot_turnaround_time + out_IO->end_time - out_IO->arrival_time;
        waiting_time = out_IO->start_time - out_IO->arrival_time;
        total_waiting_time = total_waiting_time + waiting_time;
        maxim_waiting_time = max (maxim_waiting_time, waiting_time);
        printf("%5d: %5d %5d %5d\n", i, out_IO->arrival_time, out_IO->start_time, out_IO->end_time);
        cur_input_io = nullptr;
        current_time = out_IO->end_time;
        out_IO = nullptr;
    }
    double avg_turnaround = (double)tot_turnaround_time/num_of_IO;
    double avg_waittime = (double)total_waiting_time/num_of_IO;
    printf("SUM: %d %d %.2lf %.2lf %d\n", final_end, total_move_count, avg_turnaround, avg_waittime, maxim_waiting_time);
}
