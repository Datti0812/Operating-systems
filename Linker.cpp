#include <stdio.h>
#include <string>
#include <string.h>
#include <iostream>
#include <regex>
#include <vector>
#include <map>
#include <set>

using namespace std;

size_t BUFFERSIZE = 1024;

vector<string> symkey_array;
int lineNumber = 0;
int lineOffset = 0;
int lineLength = -1;

map<string, int> symtable;
map<string, int> sym_check;
map<string, int> defsym_allmod;
vector<string> usesym_allmod;

#define UNUSED(expr) \
  do                 \
  {                  \
    (void)(expr);    \
  } while (0)

class Tokenizer
{
public:
  bool newline;
  FILE *file;
  char *buffer;

  Tokenizer(char *filename)
  {
    newline = true;
    file = nullptr;
    buffer = nullptr;
    file = fopen(filename, "r");
    if (file == nullptr)
    {
      printf("Error: file does not exist or not given!\n");
      exit(EXIT_FAILURE);
    }

    buffer = (char *)malloc(BUFFERSIZE);
  }

  ~Tokenizer()
  {
    fclose(file);
    free(buffer);
  } 

  char *getToken()
  {
    char *token;
    char delimiters[] = " \t\n";

    while (true)
    {
      while (newline)
      {
        int size = getline(&buffer, &BUFFERSIZE, file);
        if (size == -1)
        {
          lineOffset = lineLength;
          return nullptr;
        }

        lineLength = size;
        lineNumber = lineNumber+1;

        token = strtok(buffer, delimiters);

        if (token != nullptr)
        {
          lineOffset = token - buffer + 1;
          newline = false;
          return token; 
        }
      }

      token = strtok(nullptr, delimiters);
      if (token == nullptr)
      {
        newline = true;
        continue;
      }

      lineOffset = token - buffer + 1;
      return token;
    }
  }
  void __parseerror(int errcode)
  {
    static string errors[] = {
        "NUM_EXPECTED",           // Number expect
        "SYM_EXPECTED",           // Symbol Expected
        "ADDR_EXPECTED",          // Addressing Expected which is A/E/I/R
        "SYM_TOO_LONG",           // Symbol Name is too long
        "TOO_MANY_DEF_IN_MODULE", // > 16
        "TOO_MANY_USE_IN_MODULE", // > 16
        "TOO_MANY_INSTR",         // total num_instr exceeds memory size (512)
    };
    printf("Parse Error line %d offset %d: %s\n", lineNumber, lineOffset, errors[errcode].c_str());
    exit(EXIT_FAILURE);
  }

  int readInt()
  {
    char *token = this->getToken();
    int int_token = 0;
    if (token == nullptr)
      return -1; 
    string conv_token =  string(token);
    if (!isdigit(conv_token[0]))
        __parseerror(0);
    if ((token != nullptr) && (conv_token.length() > 16))
        return conv_token.length();
    else
        int_token = stoi(conv_token);
        return int_token;
  }

  string readSymbol()
  {
    char *token = this->getToken();
    if (token == nullptr)
        return "";
    string str_token = string(token);
    regex sym_exp("[a-zA-Z][a-zA-Z0-9]*");
    if (token != nullptr)
    {
      if (!isalpha(str_token[0]) || (!regex_match(str_token, sym_exp)))
        __parseerror(1);
      else if (str_token.length() > 16)
        __parseerror(3);
      else if (str_token == "")
          __parseerror(1);
      else
        return str_token;
    }
    }

  string readREIA() 
  {
    char *REIA = this->getToken();
    if (REIA != nullptr)
    {
      if (*REIA == 'R' || *REIA == 'E' || *REIA == 'I' || *REIA == 'A')
        return REIA;
      else
        __parseerror(1);
    }

    return "";
  }

  void createSymbol(string sym, int val, int abs_val)
  {
    if (symtable.find(sym) == symtable.end())
    {
      symtable[sym] = abs_val;
      symkey_array.push_back(sym);
      sym_check[sym] = val;
    }
    else
    {
      sym_check[sym] = 10000 + val;
      symtable[sym] = symtable[sym] + 10000;
    }
  }

  int Pass1()                                                     // Pass1 function
  {
    int module_count = 0;
    int base = 0;
   // bool return_val = false;
    int tot_instr_count = 0;
    
    while (true)
    {
      int defcount = this->readInt(); //------------------------------DEFINITION LIST
      if (defcount == -1)
        break;
      if (defcount > 16)
        __parseerror(4); 
      module_count = module_count + 1;
      for (int i = 0; i < defcount; i++)
      {
        string symbol = this->readSymbol();
        if (symbol == "")
          __parseerror(1);
        defsym_allmod[symbol] = module_count;
        

        int val = this->readInt();
        if (val == -1)
          __parseerror(0);
        int absolute_val = val + base;
        this->createSymbol(symbol, val, absolute_val); // this would change in pass2
      }

      int usecount = this->readInt(); //------------------------------------- USE LIST
      if (usecount == -1)
        __parseerror(0);
      if (usecount > 16)
        __parseerror(5); 

      for (int i = 0; i < usecount; i++)
      {
        string symbol = this->readSymbol();
        if (symbol == "") 
          __parseerror(1);
        usesym_allmod.push_back(symbol);
      }

      int instcount = this->readInt(); // ---------------------------------- PROGRAM TEXT
      
      if (instcount == -1)
        __parseerror(0);
      tot_instr_count = tot_instr_count + instcount;
      if (tot_instr_count > 512)
        __parseerror(6);
        
      for (int i = 0; i < instcount; i++)
      {
        string addr_mode = this->readREIA();
        if (addr_mode == "") 
        __parseerror(2);

        int instr = this->readInt();
/*        if (instr < 1000)
        {
            __parseerror(2);
            return_val = true;
        } */
        if (instr == -1)
          __parseerror(0);
      }
      
      
     for (auto &a : sym_check)
      {
        if (a.second < 10000)
        {
          if (a.second > instcount)
          {
            printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", module_count, a.first.c_str(), a.second, instcount - 1);
            symtable[a.first] = base; //treat the address given as 0 (relative to the module).
          }
        }
      }
      sym_check.clear();
      base = base + instcount;
    }
   // if (return_val)
  //      return -1;
 //   else
    return 0; // if no parse error rises
  }

  void pass2(map<string,int> symtable) // -------------------------------------------Pass 2 function
  {
      
    int total_instr = 0;
    int module_count = 0;
    int base = 0;
    vector<string> uselist;
    set<string> sym_used;
    //map<string, int> symtable2;
    cout << "Symbol Table" << endl;

    for (string key:symkey_array)
    {
      if (symtable[key] < 10000)
      {
        cout << key + "=" << symtable[key] << endl;
      }
      else
      {
        symtable[key] = symtable[key] % 10000;
        cout << key + "=" << symtable[key] << " Error: This variable is multiple times defined; first value used" << endl;
      }
    }
    cout << "\n";
    cout << "Memory Map" << endl;

    while (!feof(file))
    {
      uselist.clear();
      int defcount = this->readInt(); // ---------------------------DEFINITION LIST

      if (defcount == -1)
        break;

      for (int i = 0; i < defcount; i++)
      {
        string symbol = this->readSymbol();
        this->readInt();

        /*if (symtable2.find(symbol) == symtable2.end())
          symtable2[symbol] = module_count;*/
      }

      int usecount = this->readInt(); // ----------------------------------USE LIST

      for (int i = 0; i < usecount; i++)
      {
        string symbol = this->readSymbol();
        uselist.push_back(symbol);
      }

      int instcount = this->readInt(); // ------------------------------PROGRAM TEXT
      total_instr = total_instr + instcount;

      for (int i = 0; i < instcount; i++)
      {

        string addr_mode = this->readSymbol();
        int instr = this->readInt();
        int instr_calc = instr - (instr % 1000);

        if (addr_mode == "R")
        {
          if (instr >= 10000)
          {
            instr = 9999;
            printf("%.3d: %.4d Error: Illegal opcode; treated as 9999\n", base + i, instr);
          }
          else if (instr % 1000 >= instcount)
            printf("%.3d: %.4d Error: Relative address exceeds module size; zero used\n", base + i, instr_calc + base);
          else
            printf("%.3d: %.4d\n", base + i, instr + base);
        }

        if (addr_mode == "I")
        {
          if (instr >= 10000)
          {
            instr = 9999;
            printf("%.3d: %.4d Error: Illegal immediate value; treated as 9999\n", base + i, instr);
          }
          else
            printf("%.3d: %.4d\n", base + i, instr);
        }

        if (addr_mode == "A") 
        {
          if (instr >= 10000)
          {
            instr = 9999;
            printf("%.3d: %.4d Error: Illegal opcode; treated as 9999\n", base + i, instr);
          }
          else if (instr % 1000 >= 512) 
            printf("%.3d: %.4d Error: Absolute address exceeds machine size; zero used\n", base + i, instr_calc);
          else
            printf("%.3d: %.4d\n", base + i, instr);
        }

        if (addr_mode == "E")
        {
          int instr_operand = instr % 1000;
          if (instr >= 10000)
          {
            instr = 9999;
            printf("%.3d: %.4d Error: Illegal opcode; treated as 9999\n", base + i, instr);
          }
          else if (instr_operand >= usecount)
            printf("%.3d: %.4d Error: External address exceeds length of uselist; treated as immediate\n", base + i, instr);
          else
          {
            string get_symbol = uselist[instr_operand];
            sym_used.insert(get_symbol);

            if (symtable.find(get_symbol) != symtable.end())
            {
              int defsym_val = symtable[get_symbol];
              printf("%.3d: %.4d\n", base + i, instr_calc + defsym_val);
            }
            else
              printf("%.3d: %.4d Error: %s is not defined; zero used\n", base + i, instr_calc, get_symbol.c_str());
          }
        }
      }

      for (int i = 0; i < usecount; i++)
      {
        string symbol = uselist[i];
        if (sym_used.find(symbol) == sym_used.end()){
            module_count = module_count + 1;
        printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", module_count, symbol.c_str());}
      else
        module_count = module_count + 1;
      }
      base = total_instr;
    }
    cout << "\n";
    for (auto &a: defsym_allmod)
    {
        vector <string>::iterator it = find(usesym_allmod.begin(), usesym_allmod.end(), a.first);
        if (it != usesym_allmod.end())
            continue;
        else
            printf("Warning: Module %d: %s was defined but never used\n", a.second, a.first.c_str());
    }
  }
};

int main(int argc, char **argv)
{
  UNUSED(argc);
  // call function from other file
  //cout<< argv[1]<<endl;

  Tokenizer *tokenizer = new Tokenizer(argv[1]);

  // get all tokens in the file

  int result_pass1 = tokenizer->Pass1();

  if (result_pass1 == 0)
  {
    Tokenizer *tokenizer1 = new Tokenizer(argv[1]);
    tokenizer1->pass2(symtable);
  }

  /*char *token = tokenizer->getToken();
  while (token != nullptr)
  {
    cout << "Token: " << lineNumber << ":" << lineOffset << " : " << token << endl;
    token = tokenizer->getToken();
  }*/

  //printf("Final Spot in File : line=%d offset=%d\n", lineNumber, lineOffset);

 
  delete tokenizer;

  return 0;
}
