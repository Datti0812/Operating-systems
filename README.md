# Operating-systems/Linker
Coding labs for the course at NYU

Every operating system needs to perform linking, memory management, scheduling and IO operations to make itself robust and well structured.

As part of the curriculum I coded for all the above four main aspects of operating system in C++.

1. Linker will read the input and assigns tokens to each instruction given in the input. It validates whether the given instrution is valid or not and processes the vaild one's.
2. Scheduler takes the valid insructions and make processes and execute them based on different constraints. For example, it considers the time needed to complete a process depends on different types of schedulers. In the Scheduler program I developed 5 different types of schedulers and each work differently.
3. Memory management considers what type of scheduler is running and it prioritises each process relaying on the amount memory it occupies or requests the operating system. The processes will get executed according to the memory constraints placed by the operating system. 
4. IO opertions are prioritized by the mode of IO request interrupted the operating system. (For example moving the mouse or pressing a key in keyboard)

These programs are written in a way how the operating systems work in an outer scope. 
