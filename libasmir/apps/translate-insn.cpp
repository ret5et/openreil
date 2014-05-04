#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <set>
#include <iostream>
#include <string>

#include "asm_program.h"
#include "ir_printer.h"

extern "C" 
{
#include "libvex.h"
}

#include "irtoir.h"

using namespace std;

#define DBG_ASM
//#define DBG_BAP
#define DBG_REIL

#define MAX_REG_NAME_LEN  15
#define MAX_TEMP_NAME_LEN 15
#define MAX_INST_LEN 30

enum reil_op_t 
{ 
    I_NONE,
    I_JCC,      // conditional jump 
    I_STR,      // store value to register
    I_STM,      // store value to memory
    I_LDM,      // load value from memory
    I_ADD,      // addition
    I_SUB,      // substraction
    I_NEG,      // negation
    I_MUL,      // multiplication
    I_DIV,      // division
    I_MOD,      // modulus    
    I_SMUL,     // signed multiplication
    I_SDIV,     // signed division
    I_SMOD,     // signed modulus
    I_SHL,      // shift left
    I_SHR,      // shift right
    I_ROL,      // rotate left
    I_ROR,      // rotate right
    I_AND,      // binary and
    I_OR,       // binary or
    I_XOR,      // binary xor
    I_NOT,      // binary not
    I_BAND,     // logical and
    I_BOR,      // logical or
    I_BXOR,     // logical xor
    I_BNOT,     // logical not    
    I_EQ,       // equation
    I_L,        // less than
    I_LE,       // less or equal than
    I_SL,       // signed less than
    I_SLE,      // signed less or equal than    
    I_LCAST,    // low half of the integer
    I_HCAST,    // high half of the integer
    I_UCAST,    // cast to unsigned value
    I_SCAST     // cast with sign bit
};

enum reil_type_t
{
    A_NONE,
    A_REG,      // target architecture registry operand
    A_TEMP,     // temporary registry operand
    A_CONSTANT  // immediate value
};

typedef uint64_t reil_const_t;
typedef uint64_t reil_addr_t;
typedef uint16_t reil_inum_t;

enum reil_size_t { U1, U8, U16, U32, U64 };

struct reil_arg_t
{
    reil_type_t type;

    union
    {
        struct
        {
            reil_size_t size;
            char name[MAX_REG_NAME_LEN];

        } reg;

        struct
        {
            reil_size_t size;
            char name[MAX_TEMP_NAME_LEN];

        } temp;

        struct
        {
            reil_size_t size;
            reil_const_t val;

        } constant;
    };
};

struct reil_inst_t
{
    reil_addr_t addr;   // address of the original assembly instruction
    reil_inum_t inum;   // number of the IR subinstruction
    reil_op_t op;       // operation code
    reil_arg_t a, b, c; // arguments
};

typedef pair<int32_t, string> TEMPREG_BAP;

class CReilFromBilTranslator
{
public:
    
    CReilFromBilTranslator(); 
    ~CReilFromBilTranslator();

    void reset_state();    

    void process_bil(address_t addr, Stmt *s);
    void process_bil(address_t addr, bap_block_t *block);

private:

    string to_string_constant(reil_const_t val, reil_size_t size);
    string to_string_size(reil_size_t size);
    string to_string_operand(reil_arg_t *a);

    string tempreg_get_name(int32_t tempreg_num);
    int32_t tempreg_bap_find(string name);
    int32_t tempreg_alloc(void);
    
    reil_size_t convert_operand_size(reg_t typ);
    void convert_operand(Exp *exp, struct reil_arg_t *reil_arg);    

    void process_reil_inst(reil_inst_t *reil_inst);

    void free_bil_exp(Exp *exp);
    Exp *process_bil_exp(Exp *exp);    
    Exp *process_bil_inst(reil_op_t inst, Exp *c, Exp *exp);

    vector<TEMPREG_BAP> tempreg_bap;
    int32_t tempreg_count;
    reil_inum_t inst_count;
    address_t inst_addr;
};

class CReilTranslator
{
public:

    CReilTranslator(bfd_architecture arch);
    ~CReilTranslator();

    void process_inst(address_t addr, uint8_t *data, int size);

private:

    void set_inst_addr(address_t addr);

    CReilFromBilTranslator *translator;
    uint8_t inst_buffer[MAX_INST_LEN];
    asm_program_t *prog;
};

const char *reil_inst_name[] = 
{
    "NONE", "JCC", 
    "STR", "STM", "LDM", 
    "ADD", "SUB", "NEG", "MUL", "DIV", "MOD", "SMUL", "SDIV", "SMOD", 
    "SHL", "SHR", "ROL", "ROR", 
    "AND", "OR", "XOR", "NOT", "BAND", "BOR", "BXOR", "BNOT",
    "EQ", "L", "LE", "SL", "SLE", "CAST_LO", "CAST_HI", "CAST_U", "CAST_S"
};

reil_op_t reil_inst_map_binop[] = 
{
    /* PLUS     */ I_ADD, 
    /* MINUS    */ I_SUB,   
    /* TIMES    */ I_MUL,  
    /* DIVIDE   */ I_DIV,
    /* MOD      */ I_MOD,      
    /* LSHIFT   */ I_SHL,   
    /* RSHIFT   */ I_SHR,  
    /* ARSHIFT  */ I_NONE,
    /* LROTATE  */ I_ROL,  
    /* RROTATE  */ I_ROR,  
    /* LOGICAND */ I_BAND, 
    /* LOGICOR  */ I_BOR,
    /* BITAND   */ I_AND,  
    /* BITOR    */ I_OR,       
    /* XOR      */ I_XOR,      
    /* EQ       */ I_EQ,
    /* NEQ      */ I_NONE,  
    /* GT       */ I_NONE,       
    /* LT       */ I_L,       
    /* GE       */ I_NONE,
    /* LE       */ I_LE, 
    /* SDIVIDE  */ I_SDIV, 
    /* SMOD     */ I_SMOD   
};

reil_op_t reil_inst_map_unop[] = 
{
    /* NEG      */ I_NEG,
    /* NOT      */ I_BNOT 
};

template<class T>
string _to_string(T i)
{
    stringstream s;
    s << i;
    return s.str();
}

#define RELATIVE ((exp_type_t)((uint32_t)EXTENSION + 1))

class Relative : public Exp 
{
public:
    Relative(reg_t t, const_val_t val); 
    Relative(const Relative& other);
    virtual Relative *clone() const;
    virtual ~Relative() {}
    static void destroy(Constant *expr);

    virtual string tostring() const;
    virtual void accept(IRVisitor *v) { }
    reg_t typ;
    const_val_t val;  
};

Relative::Relative(reg_t t, const_val_t v)
  : Exp(RELATIVE), typ(t), val(v) { }

Relative::Relative(const Relative& other)
  : Exp(RELATIVE), typ(other.typ), val(other.val) { }

Relative *Relative::clone() const
{
    return new Relative(*this);
}

string Relative::tostring() const
{
    return string("$+") + _to_string(val);
}

void Relative::destroy(Constant *expr)
{
    assert(expr);
    delete expr;
}

CReilFromBilTranslator::CReilFromBilTranslator()
{
    reset_state();
}

CReilFromBilTranslator::~CReilFromBilTranslator()
{
    
}

void CReilFromBilTranslator::reset_state()
{
    tempreg_bap.clear();
    tempreg_count = inst_count = 0;
}

string CReilFromBilTranslator::to_string_constant(reil_const_t val, reil_size_t size)
{
    stringstream s;
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    uint64_t u64 = 0;

    switch (size)
    {
    case U1: if (val == 0) s << "0"; else s << "1"; break;
    case U8: u8 = (uint8_t)val; s << dec << (int)u8; break;
    case U16: u16 = (uint16_t)val; s << u16; break;
    case U32: u32 = (uint32_t)val; s << u32; break;
    case U64: u64 = (uint64_t)val; s << u64; break;
    default: assert(0);
    }
    
    return s.str();
}

string CReilFromBilTranslator::to_string_size(reil_size_t size)
{
    switch (size)
    {
    case U1: return string("1");
    case U8: return string("8");
    case U16: return string("16");
    case U32: return string("32");
    case U64: return string("64");
    }

    assert(0);
}

#define STR_ARG_EMPTY " "
#define STR_VAR(_name_, _t_) "(" + (_name_) + ", " + to_string_size((_t_)) + ")"
#define STR_CONST(_val_, _t_) "(" + to_string_constant((_val_), (_t_)) + ", " + to_string_size((_t_)) + ")"

string CReilFromBilTranslator::to_string_operand(reil_arg_t *a)
{
    switch (a->type)
    {
    case A_NONE: return string(STR_ARG_EMPTY);
    case A_REG: return STR_VAR(string(a->reg.name), a->reg.size);
    case A_TEMP: return STR_VAR(string(a->temp.name), a->temp.size);
    case A_CONSTANT: return STR_CONST(a->constant.val, a->constant.size);
    }    

    assert(0);
}

string CReilFromBilTranslator::tempreg_get_name(int32_t tempreg_num)
{
    char number[15];
    sprintf(number, "%.2d", tempreg_num);

    string tempreg_name = string("V_");
    tempreg_name += number;

    return tempreg_name;
}

int32_t CReilFromBilTranslator::tempreg_bap_find(string name)
{
    vector<TEMPREG_BAP>::iterator it;

    // find temporary registry number by BAP temporary registry name
    for (it = tempreg_bap.begin(); it != tempreg_bap.end(); ++it)
    {
        if (it->second == name)
        {
            return it->first;
        }
    }

    return -1;
}

int32_t CReilFromBilTranslator::tempreg_alloc(void)
{
    while (true)
    {
        vector<TEMPREG_BAP>::iterator it;
        bool found = false;
        int32_t ret = tempreg_count;

        // check if temporary registry number was reserved for BAP registers
        for (it = tempreg_bap.begin(); it != tempreg_bap.end(); ++it)
        {
            if (it->first == tempreg_count)
            {
                found = true;
                break;
            }
        }   

        tempreg_count += 1;     
        if (!found) return ret;
    }

    assert(0);
    return -1;
}

reil_size_t CReilFromBilTranslator::convert_operand_size(reg_t typ)
{
    switch (typ)
    {
    case REG_1: return U1;
    case REG_8: return U8;
    case REG_16: return U16;
    case REG_32: return U32;
    case REG_64: return U64;
    default: assert(0);
    }    
}

void CReilFromBilTranslator::convert_operand(Exp *exp, struct reil_arg_t *reil_arg)
{
    if (exp == NULL) return;

    assert(exp->exp_type == TEMP || exp->exp_type == CONSTANT || exp->exp_type == RELATIVE);

    if (exp->exp_type == CONSTANT)
    {
        // special handling for canstants
        Constant *constant = (Constant *)exp;
        reil_arg->type = A_CONSTANT;
        reil_arg->constant.size = convert_operand_size(constant->typ);
        reil_arg->constant.val = constant->val;
        return;
    }

    Temp *temp = (Temp *)exp;    
    string ret = temp->name;

    const char *c_name = ret.c_str();
    if (strncmp(c_name, "R_", 2) && strncmp(c_name, "V_", 2) && strncmp(c_name, "pc_", 3))
    {
        // this is a BAP temporary registry
        int32_t tempreg_num = tempreg_bap_find(temp->name);
        if (tempreg_num == -1)
        {
            // there is no alias for this registry, create it
            tempreg_num = tempreg_alloc();
            tempreg_bap.push_back(make_pair(tempreg_num, temp->name));

#ifdef DBG_TEMPREG

            printf("Temp reg %d reserved for %s\n", tempreg_num, name.c_str());
#endif
        }
        else
        {

#ifdef DBG_TEMPREG

            printf("Temp reg %d found for %s\n", tempreg_num, name.c_str());   
#endif
        }

        ret = tempreg_get_name(tempreg_num);
    }

    if (!strncmp(c_name, "R_", 2))
    {
        // architecture register
        reil_arg->type = A_REG;
        reil_arg->reg.size = convert_operand_size(temp->typ);
        strncpy(reil_arg->reg.name, ret.c_str(), MAX_REG_NAME_LEN - 1);
    }
    else
    {
        // temporary register
        reil_arg->type = A_TEMP;
        reil_arg->temp.size = convert_operand_size(temp->typ);
        strncpy(reil_arg->temp.name, ret.c_str(), MAX_TEMP_NAME_LEN - 1);
    }
}

void CReilFromBilTranslator::process_reil_inst(reil_inst_t *reil_inst)
{

#ifdef DBG_REIL

    printf("%.8llx.%.2x ", reil_inst->addr, reil_inst->inum);  
    printf("%7s ", reil_inst_name[reil_inst->op]);
    printf("%16s, ", to_string_operand(&reil_inst->a).c_str());
    printf("%16s, ", to_string_operand(&reil_inst->b).c_str());
    printf("%16s  ", to_string_operand(&reil_inst->c).c_str());
    printf("\n");    

#endif

}

void CReilFromBilTranslator::free_bil_exp(Exp *exp)
{
    if (exp) 
    {
        // free temp expression that was returned by process_bil_exp()
        Temp::destroy(reinterpret_cast<Temp *>(exp));
    }
}

Exp *CReilFromBilTranslator::process_bil_exp(Exp *exp)
{
    Exp *ret = exp;

    if (exp->exp_type != TEMP && exp->exp_type != CONSTANT)
    {
        assert(exp->exp_type == BINOP ||
               exp->exp_type == UNOP || 
               exp->exp_type == CAST);

        // expand complex expression and store result to the new temporary value
        return process_bil_inst(I_STR, NULL, exp);
    }    

    return NULL;
}

Exp *CReilFromBilTranslator::process_bil_inst(reil_op_t inst, Exp *c, Exp *exp)
{
    reil_inst_t reil_inst;
    Exp *a = NULL, *b = NULL;
    Exp *a_temp = NULL, *b_temp = NULL, *exp_temp = NULL;

    assert(exp);
    assert(inst == I_STR || inst == I_JCC);
    
    memset(&reil_inst, 0, sizeof(reil_inst));
    reil_inst.op = inst;
    reil_inst.addr = inst_addr;

    if (c && c->exp_type == MEM)
    {
        // check for the store to memory
        assert(reil_inst.op == I_STR);

        Mem *mem = (Mem *)c;    
        reil_inst.op = I_STM;

        // parse address expression
        Exp *addr = process_bil_exp(mem->addr);
        if (addr)
        {
            c = addr;
        }
        else
        {
            c = mem->addr;
        }

        // parse value expression
        if (exp_temp = process_bil_exp(exp))
        {
            exp = exp_temp;
        }
    }
    else if (c && c->exp_type == NAME)
    {
        // check for the jump
        assert(reil_inst.op == I_JCC);

        Name *name = (Name *)c;
        c = new Temp(REG_32, name->name);
    }

    if (reil_inst.op == I_STR) assert(c == NULL || c->exp_type == TEMP);
    if (reil_inst.op == I_STM) assert(c == NULL || (c->exp_type == TEMP || c->exp_type == CONSTANT));
    
    // get a and b operands values from expression
    if (exp->exp_type == BINOP)
    {
        assert(reil_inst.op == I_STR);

        // store result of binary operation
        BinOp *binop = (BinOp *)exp;
        reil_inst.op = reil_inst_map_binop[binop->binop_type];
        
        assert(reil_inst.op != I_NONE);

        a = binop->lhs;
        b = binop->rhs;        
    }
    else if (exp->exp_type == UNOP)
    {
        assert(reil_inst.op == I_STR);

        // store result of unary operation
        UnOp *unop = (UnOp *)exp;   
        reil_inst.op = reil_inst_map_unop[unop->unop_type];

        assert(reil_inst.op != I_NONE);

        a = unop->exp;
    }    
    else if (exp->exp_type == CAST)
    {
        assert(reil_inst.op == I_STR);

        // store with type cast
        Cast *cast = (Cast *)exp;
        if (cast->cast_type == CAST_HIGH)
        {
            // use high half
            reil_inst.op = I_HCAST;
        }
        else if (cast->cast_type == CAST_LOW)
        {
            // use low half
            reil_inst.op = I_LCAST;
        }
        else if (cast->cast_type == CAST_UNSIGNED)
        {
            // cast to unsigned value of bigger size
            reil_inst.op = I_UCAST;
        }
        else
        {
            assert(0);
        }

        a = cast->exp;
    }
    else if (exp->exp_type == MEM)
    {
        assert(reil_inst.op == I_STR);

        // read from memory and store
        Mem *mem = (Mem *)exp;
        reil_inst.op = I_LDM;

        if (a_temp = process_bil_exp(mem->addr))
        {
            a = a_temp;
        }
        else
        {
            a = mem->addr;
        }
    }     
    else if (exp->exp_type == TEMP || exp->exp_type == CONSTANT)
    {
        // store constant or register
        a = exp;
    }        
    else
    {
        assert(0);
    }

    // parse operand a expression
    if (a && (a_temp = process_bil_exp(a)))
    {
        a = a_temp;
    }   

    // parse operand b expression
    if (b && (b_temp = process_bil_exp(b)))
    {
        b = b_temp;
    }    

    assert(a);
    assert(a == NULL || a->exp_type == TEMP || a->exp_type == CONSTANT);
    assert(b == NULL || b->exp_type == TEMP || b->exp_type == CONSTANT);    

    if (c == NULL)
    {
        // allocate temporary value to store result
        reg_t tempreg_type;
        string tempreg_name;

        // determinate type for new value by type of result
        if (exp->exp_type == CAST)
        {
            Cast *cast = (Cast *)exp;
            tempreg_type = cast->typ;
        }
        else if (a->exp_type == TEMP)
        {
            Temp *temp = (Temp *)a;
            tempreg_type = temp->typ;
        }
        else if (a->exp_type == CONSTANT)
        {
            Constant *constant = (Constant *)a;
            tempreg_type = constant->typ;
        }
        else
        {
            assert(0);
        }        
        
        tempreg_name = tempreg_get_name(tempreg_alloc());
        c = new Temp(tempreg_type, tempreg_name);
    }        

    reil_inst.inum = inst_count;
    inst_count += 1;

    // make REIL operands from BIL expressions
    convert_operand(a, &reil_inst.a);
    convert_operand(b, &reil_inst.b);
    convert_operand(c, &reil_inst.c);

    // handle assembled REIL instruction
    process_reil_inst(&reil_inst);

    free_bil_exp(a_temp);
    free_bil_exp(b_temp);
    free_bil_exp(exp_temp);

    return c;
}

void CReilFromBilTranslator::process_bil(address_t addr, Stmt *s)
{    
    tempreg_count = 0;
    inst_addr = addr;

#ifdef DBG_BAP
        
    printf("%s\n", s->tostring().c_str());

#endif

    switch (s->stmt_type)
    {
    case MOVE:    
        {
            // move statement
            Move *move = (Move *)s;
            process_bil_inst(I_STR, move->lhs, move->rhs);
            break;    
        }       
    
    case JMP:
        {
            // jump statement
            Jmp *jmp = (Jmp *)s;
            Constant c(REG_1, 1);
            process_bil_inst(I_JCC, jmp->target, &c);
            break;
        }

    case CJMP:
        {
            // conditional jump statement
            CJmp *cjmp = (CJmp *)s;
            process_bil_inst(I_JCC, cjmp->t_target, cjmp->cond);
            break;
        }

    case CALL:
    case RETURN:
        {            
            printf("Statement %d is not implemented\n", s->stmt_type);
            assert(0);
        }

    case EXPSTMT:
    case COMMENT:
    case SPECIAL:
    case LABEL:
    case VARDECL:

        break;
    }  

#if defined(DBG_BAP) && defined(DBG_REIL)

    printf("\n");

#endif  

}

void CReilFromBilTranslator::process_bil(address_t addr, bap_block_t *block)
{
    reset_state();

    for (int i = 0; i < block->bap_ir->size(); i++)
    {
        // enumerate BIL statements
        Stmt *s = block->bap_ir->at(i);

        // convert statement to REIL code
        process_bil(addr, s);
    }
}

CReilTranslator::CReilTranslator(bfd_architecture arch)
{
    // initialize libasmir
    translate_init();

    // allocate a fake bfd instance
    prog = asmir_new_asmp_for_arch(arch);
    assert(prog);

    // create code segment for instruction buffer 
    prog->segs = (section_t *)bfd_alloc(prog->abfd, sizeof(section_t));
    assert(prog->segs);
            
    prog->segs->data = inst_buffer;
    prog->segs->datasize = MAX_INST_LEN;                        
    prog->segs->section = NULL;
    prog->segs->is_code = true;
    set_inst_addr(0);

    translator = new CReilFromBilTranslator();
    assert(translator);
}

CReilTranslator::~CReilTranslator()
{
    asmir_close(prog);
}

void CReilTranslator::set_inst_addr(address_t addr)
{
    prog->segs->start_addr = addr;
    prog->segs->end_addr = prog->segs->start_addr + MAX_INST_LEN;
}

void CReilTranslator::process_inst(address_t addr, uint8_t *data, int size)
{
    if (addr) set_inst_addr(addr);
    memcpy(inst_buffer, data, min(size, MAX_INST_LEN));

    // translate to VEX
    bap_block_t *block = generate_vex_ir(prog, addr);
    assert(block);

#ifdef DBG_ASM

    string asm_code = asmir_string_of_insn(prog, block->inst);
    printf("# %s\n\n", asm_code.c_str());

#endif

    // tarnslate to BAP
    generate_bap_ir_block(prog, block);                

    // generate REIL
    CReilFromBilTranslator translator;
    translator.process_bil(addr, block);

    for (int i = 0; i < block->bap_ir->size(); i++)
    {
        // free BIL code
        Stmt *s = block->bap_ir->at(i);
        Stmt::destroy(s);
    }

    delete block->bap_ir;
    delete block;        

    // free VEX memory
    // asmir_close() is also doing that
    vx_FreeAll();
}

//======================================================================
//
// Main
//
//======================================================================

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("USAGE: tarnslate-insn.exe [arch] [hex_bytes]\n");
        return 0;
    }

    enum bfd_architecture arch;
    uint8_t inst[MAX_INST_LEN];
    memset(inst, 0, sizeof(inst));

    if (!strcmp(argv[1], "-x86"))
    {
        // set target architecture
        arch = bfd_arch_i386;
    }
    else
    {
        printf("ERROR: Invalid architecture\n");
        return -1;
    }

    for (int i = 2; i < argc; i++)
    {
        // get user specified bytes to dissassembly
        inst[i - 2] = strtol(argv[i], NULL, 16);
        if (errno == EINVAL)
        {
            printf("ERROR: Invalid hex byte\n");
            return -1;
        }
    }

    CReilTranslator translator(arch);
    translator.process_inst(0, inst, MAX_INST_LEN);

    return 0;
}
