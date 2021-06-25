#include <iostream>
#include <unistd.h>
#include <string.h>
#include <string>
#include <fstream>
#include <vector>
#include <queue>
#include <sstream>

using namespace std;

size_t buffersize = 1024;
char delimiters[] = " \t\n";
const int PTE_count = 64;

unsigned long long total_cycles = 0;
unsigned long context_switch_count = 0;
unsigned long process_exit_count = 0;

enum action_type
{
    CONTEXT_SWITCH,
    PROCESS_EXIT,
    RW_ACCESS,
    MAP,
    UNMAP,
    PAGE_IN,
    PAGE_OUT,
    FILE_IN,
    FILE_OUT,
    ZERO,
    SEGV,
    SEGPROT
};

struct pstats
{
    unsigned long ins = 0;
    unsigned long fins = 0;
    unsigned long outs = 0;
    unsigned long fouts = 0;
    unsigned long maps = 0;
    unsigned long unmaps = 0;
    unsigned long zeros = 0;
    unsigned long segv = 0;
    unsigned long segprot = 0;
};

struct pte_t
{
    unsigned int VALID : 1;
    unsigned int REFERENCED : 1;
    unsigned int MODIFIED : 1;
    unsigned int WRITE_PROTECT : 1;
    unsigned int PAGEDOUT : 1;
    unsigned int FILE_MAPPED : 1;
    unsigned int frame_no : 7;
    unsigned int padding : 19;
};

void reset_entry(pte_t *pte)
{
    pte->VALID = 0;
    pte->REFERENCED = 0;
    pte->MODIFIED = 0;
    pte->WRITE_PROTECT = 0;
    pte->PAGEDOUT = 0;
    pte->FILE_MAPPED = 0;
    pte->frame_no = 0;
}

struct VMA
{
    unsigned int start_vpage;
    unsigned int end_vpage;
    unsigned write_proteced;
    unsigned file_mapped;
    unsigned int vma_virual_pages;
};
//VMA *vma_list;

struct frame_t
{
    int pid;
    bool free = true;
    unsigned frame_index;
    unsigned page_index;
    unsigned int age_counter : 32;
    unsigned long time_stamp = 0; // last instruction time
};

char *buffer = (char *)malloc(sizeof(char) * buffersize);
char *token;

//int *my_random;
vector<int> randvals;
FILE *file;

frame_t **frame_table;
int num_frames;
unsigned long instr_count = 0;

queue<frame_t *> free_frame_list;

int myrandom(int burst)
{
    static int ofs = 0;
    if (ofs >= randvals.size())
        ofs = 0;
    int res = (randvals[ofs] % burst);
    ofs += 1;
    return res;
}

class Process
{
public:
    int pid;
    vector<VMA *> vma_list;
    pte_t page_table[PTE_count];
    pstats *stat = new pstats;

    Process(int pid)
    {
        this->pid = pid;
        // reset the page table
        for (int i = 0; i < PTE_count; i++)
        {
            reset_entry(&this->page_table[i]);
        }
    }
};
vector<Process *> process_list;

void add_cycles(Process *process, action_type action)
{
    if (action == RW_ACCESS)
    {
        total_cycles += 1;
    }
    else if (action == CONTEXT_SWITCH)
    {
        total_cycles += 130;
        context_switch_count++;
    }
    else if (action == PROCESS_EXIT)
    {
        total_cycles += 1250;
        process_exit_count++;
    }
    else if (action == MAP)
    {
        total_cycles += 300;
        process->stat->maps++;
    }
    else if (action == UNMAP)
    {
        total_cycles += 400;
        process->stat->unmaps++;
    }
    else if (action == PAGE_IN)
    {
        total_cycles += 3100;
        process->stat->ins++;
    }
    else if (action == PAGE_OUT)
    {
        total_cycles += 2700;
        process->stat->outs++;
    }
    else if (action == FILE_IN)
    {
        total_cycles += 2800;
        process->stat->fins++;
    }
    else if (action == FILE_OUT)
    {
        total_cycles += 2400;
        process->stat->fouts++;
    }
    else if (action == ZERO)
    {
        total_cycles += 140;
        process->stat->zeros++;
    }
    else if (action == SEGV)
    {
        total_cycles += 340;
        process->stat->segv++;
    }
    else if (action == SEGPROT)
    {
        total_cycles += 420;
        process->stat->segprot++;
    }
}

void reset_all_referenced()
{
    for (int i = 0; i < num_frames; i++)
    {
        frame_t *curr_frame = frame_table[i];
        if (!curr_frame->free)
        {
            pte_t *curr_entry = &process_list[curr_frame->pid]->page_table[curr_frame->page_index];
            curr_entry->REFERENCED = 0;
        }
    }
}

class Pager
{
public:
    virtual frame_t *select_victim_frame() = 0;
};

class FIFO : public Pager
{
    int chron = 0;

public:
    frame_t *select_victim_frame()
    {
        frame_t *victim = frame_table[chron];
        chron = chron + 1;
        if (chron >= num_frames)
        {
            chron = 0;
        }
        return victim;
    }
};

class CLOCK : public Pager
{
public:
    int chron = 0;
    frame_t *select_victim_frame()
    {
        frame_t *curr_frame;
        while (true)
        {
            if (chron >= num_frames)
                chron = 0;

            curr_frame = frame_table[chron];
            Process *curr_proc = process_list[curr_frame->pid];
            pte_t *curr_entry = &curr_proc->page_table[curr_frame->page_index];

            chron++;

            if (curr_entry->REFERENCED)
                curr_entry->REFERENCED = 0;
            else
                break;
        }
        return curr_frame;
    }
};

class ESC : public Pager
{
public:
    int threshold = 49;
    int chron = 0;
    frame_t *select_victim_frame()
    {
        frame_t *victim = nullptr;
        int lowest_class = 100;
        for (int i = 0; i < num_frames; i++)
        {
            if (chron >= num_frames)
                chron = 0;

            frame_t *curr_frame = frame_table[chron];
            Process *curr_proc = process_list[curr_frame->pid];
            pte_t *curr_entry = &curr_proc->page_table[curr_frame->page_index];

            chron++;

            int entry_class = (2 * curr_entry->REFERENCED) + curr_entry->MODIFIED;
            if (victim == nullptr || entry_class < lowest_class)
            {
                victim = curr_frame;
                lowest_class = entry_class;
            }

            if (lowest_class == 0)
                break;
        }

        chron = victim->frame_index + 1;

        if (instr_count >= this->threshold)
        {
            this->threshold = instr_count + 50;
            reset_all_referenced();
        }

        return victim;
    }
};

class AGING : public Pager
{
public:
    int chron = 0;
    frame_t *select_victim_frame()
    {
        frame_t *victim = nullptr;
        for (int i = 0; i < num_frames; i++)
        {
            if (chron >= num_frames)
                chron = 0;

            frame_t *curr_frame = frame_table[chron];
            Process *curr_proc = process_list[curr_frame->pid];
            pte_t *curr_entry = &curr_proc->page_table[curr_frame->page_index];

            curr_frame->age_counter = curr_frame->age_counter >> 1;
            if (curr_entry->REFERENCED)
                curr_frame->age_counter = curr_frame->age_counter | 0x80000000;

            if (victim == nullptr || curr_frame->age_counter < victim->age_counter)
            {
                victim = curr_frame;
            }

            chron++;
        }
        chron = victim->frame_index + 1;
        reset_all_referenced();
        return victim;
    }
};

class WORKING_SET : public Pager
{
public:
    int chron = 0;
    frame_t *select_victim_frame()
    {
        frame_t *victim = nullptr;
        for (int i = 0; i < num_frames; i++)
        {
            if (chron >= num_frames)
                chron = 0;

            frame_t *curr_frame = frame_table[chron];
            Process *curr_proc = process_list[curr_frame->pid];
            pte_t *curr_entry = &curr_proc->page_table[curr_frame->page_index];

            chron++;

            int age = instr_count - curr_frame->time_stamp;
            if (curr_entry->REFERENCED)
            {
                curr_frame->time_stamp = instr_count;
                curr_entry->REFERENCED = 0;
                if (victim == nullptr)
                {
                    victim = curr_frame;
                }
            }
            else
            {
                if (age >= 50)
                {
                    victim = curr_frame;
                    break;
                }
                else
                {
                    if (victim == nullptr || curr_frame->time_stamp < victim->time_stamp)
                    {
                        victim = curr_frame;
                    }
                }
            }
        }
        chron = victim->frame_index + 1;
        return victim;
    }
};

class RANDOM : public Pager
{
public:
    frame_t *select_victim_frame()
    {
        int rand = myrandom(num_frames);
        frame_t *victim = frame_table[rand];
        return victim;
    }
};

Pager *pager;
Pager *get_algo(char instr)
{
    Pager *return_algo_type = nullptr;
    if (instr == 'f')
        return_algo_type = new FIFO();
    else if (instr == 'c')
        return_algo_type = new CLOCK();
    else if (instr == 'e')
        return_algo_type = new ESC();
    else if (instr == 'r')
        return_algo_type = new RANDOM();
    else if (instr == 'a')
        return_algo_type = new AGING();
    else if (instr == 'w')
        return_algo_type = new WORKING_SET();

    return return_algo_type;
}

frame_t *allocate_frame_from_free_list()
{
    frame_t *out = nullptr;
    if (!free_frame_list.empty())
    {
        out = free_frame_list.front();
        free_frame_list.pop();
    }
    return out;
}

frame_t *get_frame()
{
    frame_t *frame = allocate_frame_from_free_list();
    if (frame == nullptr)
        frame = pager->select_victim_frame();
    return frame;
}

// want to skip over all lines that begin with #
ssize_t myReadLine(char *buf, size_t bufsz, FILE *file)
{
    ssize_t size = getline(&buf, &bufsz, file);
    while (size > 0 && buf[0] == '#')
        size = getline(&buf, &bufsz, file);

    return size;
}

bool get_instr(char *op, int *vpage, FILE *file)
{
    int size = myReadLine(buffer, buffersize, file);
    if (size > 0)
    {
        token = strtok(buffer, delimiters);
        *op = *token;
        token = strtok(nullptr, delimiters);
        *vpage = atoi(token);
        return true;
    }
    return false;
}

VMA *get_vma(Process *process, int vpage)
{
    for (auto curr_vma : process->vma_list)
    {
        if (vpage <= curr_vma->end_vpage && curr_vma->start_vpage <= vpage)
            return curr_vma;
    }
    return nullptr;
}

void free_frames(Process *curr_proc)
{
    for (int i = 0; i < 64; i++)
    {
        pte_t *curr_entry = &curr_proc->page_table[i];
        if (curr_entry->VALID)
        {
            add_cycles(curr_proc, UNMAP);
            cout << " UNMAP " << curr_proc->pid << ":" << i << '\n';
            if (curr_entry->MODIFIED)
            {
                if (curr_entry->FILE_MAPPED)
                {
                    add_cycles(curr_proc, FILE_OUT);
                    cout << " FOUT" << '\n';
                }
            }

            frame_t *frame = frame_table[curr_entry->frame_no];
            frame->age_counter = 0;
            frame->pid = -1;
            frame->free = true;
            frame->page_index = -1;
            frame->time_stamp = 0;
            free_frame_list.push(frame);
        }

        reset_entry(curr_entry);
        curr_entry = nullptr;
    }
}

int main(int argc, char **argv)
{
    char *find_algo;
    int opt_char;
    char *OPFS;
    while ((opt_char = getopt(argc, argv, "o:a:f:")) != -1)
    {
        switch (opt_char)
        {
        case 'a':
            find_algo = optarg;
            break;

        case 'f':
            num_frames = atoi(optarg);
            break;

        case 'o':
            OPFS = optarg;
            break;
        }
    }

    // frame_t *ft = new frame_t[num_frames];
    // frame_table = &ft;
    frame_table = (frame_t **)malloc(sizeof(frame_t *) * num_frames);
    for (int k = 0; k < num_frames; k++)
    {
        frame_table[k] = (frame_t *)malloc(sizeof(frame_t));
        memset(frame_table[k], 0, sizeof(frame_t));
        frame_table[k]->frame_index = k;
        frame_table[k]->free = true;
        free_frame_list.push(frame_table[k]);
    }

    string str = find_algo;
    // char algo = str[0];
    pager = get_algo(str[0]);

    ifstream randfile(argv[optind + 1]);
    int temp;
    int rfile_count;
    randfile >> rfile_count;
    //randvals.push_back(temp);
    for (int i = 0; i < rfile_count; i++)
    {
        randfile >> temp;
        randvals.push_back(temp);
    }

    // int num_of_vmas = 0;
    // int cur_processnum = 0;
    // int completed_no_vmas = 0;

    file = fopen(argv[optind], "r");

    // get processes and vmas here

    int size = myReadLine(buffer, buffersize, file);
    int num_processes = atoi(buffer);
    for (int i = 0; i < num_processes; i++)
    {
        // create a process
        Process *newProc = new Process(i);
        size = myReadLine(buffer, buffersize, file);
        int num_vmas = atoi(buffer);
        for (int j = 0; j < num_vmas; j++)
        {
            size = myReadLine(buffer, buffersize, file);
            VMA *newVMA = new VMA;

            token = strtok(buffer, delimiters);
            newVMA->start_vpage = atoi(token);
            token = strtok(nullptr, delimiters);
            newVMA->end_vpage = atoi(token);
            token = strtok(nullptr, delimiters);
            newVMA->write_proteced = atoi(token);
            token = strtok(nullptr, delimiters);
            newVMA->file_mapped = atoi(token);

            newProc->vma_list.push_back(newVMA);
            newVMA = nullptr;
        }

        process_list.push_back(newProc);
        newProc = nullptr;
    }

    // while (size > 0)
    // {
    //     cout << buffer << endl;
    //     size = myReadLine(buffer, buffersize, file);
    // }

    Process *curr_proc;
    int vpage;
    char op;
    while (get_instr(&op, &vpage, file))
    {
        cout << instr_count << ": ==> " << op << " " << vpage << '\n';

        // handle c
        switch (op)
        {
        // handle context switch
        case ('c'):
            curr_proc = process_list[vpage];
            add_cycles(curr_proc, CONTEXT_SWITCH);
            break;
        // handle proc exit
        case ('e'):
            cout << "EXIT current process " << curr_proc->pid << '\n';
            add_cycles(curr_proc, PROCESS_EXIT);

            free_frames(curr_proc);
            curr_proc = nullptr;
            break;
        // handle read write access
        default:
            add_cycles(curr_proc, RW_ACCESS);
            pte_t *pte = &curr_proc->page_table[vpage];
            if (!pte->VALID)
            {
                // get the corresponding VMA for this page
                VMA *vma_for_pte = get_vma(curr_proc, vpage);
                if (vma_for_pte == nullptr)
                {
                    cout << " SEGV";
                    add_cycles(curr_proc, SEGV);
                    cout << endl;
                    instr_count++;
                    continue;
                }
                else
                {
                    pte->FILE_MAPPED = vma_for_pte->file_mapped;
                    pte->WRITE_PROTECT = vma_for_pte->write_proteced;
                }

                frame_t *frame = get_frame();

                if (!frame->free)
                {

                    cout << " UNMAP " << frame->pid << ":" << frame->page_index << '\n';
                    add_cycles(process_list[frame->pid], UNMAP);

                    pte_t *victim_pte = &process_list[frame->pid]->page_table[frame->page_index];
                    victim_pte->VALID = 0;

                    if (victim_pte->MODIFIED)
                    {
                        switch (victim_pte->FILE_MAPPED)
                        {
                        case 1:
                            victim_pte->MODIFIED = 0;
                            add_cycles(process_list[frame->pid], FILE_OUT);
                            cout << " FOUT" << '\n';
                            break;
                        default:
                            victim_pte->MODIFIED = 0;
                            victim_pte->PAGEDOUT = 1;
                            add_cycles(process_list[frame->pid], PAGE_OUT);
                            cout << " OUT" << '\n';
                        }
                    }
                }

                switch (pte->FILE_MAPPED)
                {
                case 1:
                {
                    add_cycles(curr_proc, FILE_IN);
                    cout << " FIN" << '\n';
                    break;
                }
                default:
                {
                    switch (pte->PAGEDOUT)
                    {
                    case 1:
                        add_cycles(curr_proc, PAGE_IN);
                        cout << " IN" << '\n';
                        break;
                    default:
                        add_cycles(curr_proc, ZERO);
                        cout << " ZERO" << '\n';
                    }
                }
                }
                add_cycles(curr_proc, MAP);
                cout << " MAP " << frame->frame_index << '\n';
                pte->frame_no = frame->frame_index;
                pte->VALID = 1;
                frame->page_index = vpage;
                frame->pid = curr_proc->pid;
                frame->free = false;
                frame->age_counter = 0;
            }

            pte->REFERENCED = 1;
            if (op == 'w')
            {
                switch (pte->WRITE_PROTECT)
                {
                case 1:
                {
                    add_cycles(curr_proc, SEGPROT);
                    cout << " SEGPROT" << '\n';
                    break;
                }
                default:
                    pte->MODIFIED = 1;
                }
            }
        }

        cout.flush();

        instr_count++;
    }

    //TODO print process tables
    for (int i = 0; i < process_list.size(); i++)
    {
        // print table
        Process *proc = process_list[i];
        printf("PT[%d]:", proc->pid);

        for (int j = 0; j < 64; j++)
        {
            cout << " ";
            pte_t *curr_entry = &proc->page_table[j];
            if (curr_entry->VALID)
            {
                cout << j << ":";
                if (curr_entry->REFERENCED)
                    cout << "R";
                else
                    cout << "-";
                if (curr_entry->MODIFIED)
                    cout << "M";
                else
                    cout << "-";
                if (curr_entry->PAGEDOUT)
                    cout << "S";
                else
                    cout << "-";
            }
            else
            {
                cout << (curr_entry->PAGEDOUT ? "#" : "*");
            }
        }
        cout << endl;
    }

    //TODO print frame table
    printf("FT:");
    for (int i = 0; i < num_frames; i++)
    {
        printf(" ");
        if (!frame_table[i]->free)
        {
            printf("%d:%d", frame_table[i]->pid, frame_table[i]->page_index);
        }
        else
        {
            printf("*");
        }
    }
    printf("\n");

    //TODO print all process counters
    for (int i = 0; i < process_list.size(); i++)
    {
        pstats *stats = process_list[i]->stat;
        printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n",
               process_list[i]->pid,
               stats->unmaps,
               stats->maps,
               stats->ins,
               stats->outs,
               stats->fins,
               stats->fouts,
               stats->zeros,
               stats->segv,
               stats->segprot);
    }

    //TODO print TOTAL COST
    printf("TOTALCOST %lu %lu %lu %llu %lu\n",
           instr_count,
           context_switch_count,
           process_exit_count,
           total_cycles,
           sizeof(pte_t));
}