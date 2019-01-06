#ifndef _ZJUNIX_PC_H
#define _ZJUNIX_PC_H

#include <zjunix/list.h>
#include <zjunix/vfs/vfs.h>
#include <zjunix/fs/fat.h>

#define  KERNEL_STACK_SIZE  4096
#define  PC_NAME_LEN   32

#define P_INIT 1
#define P_READY 2
#define P_RUNNING 3
#define P_END 4

#define MAX_LEVEL 64
#define IDLE_ID MAX_LEVEL - 1
#define SHELL_ID MAX_LEVEL

typedef unsigned char u_byte;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long long u_long;

typedef struct
{
	unsigned int epc;
	unsigned int at;
	unsigned int v0, v1;
	unsigned int a0, a1, a2, a3;
	unsigned int t0, t1, t2, t3, t4, t5, t6, t7;
	unsigned int s0, s1, s2, s3, s4, s5, s6, s7;
	unsigned int t8, t9;
	unsigned int hi, lo;
	unsigned int gp, sp, fp, ra;
}context;

typedef struct list_head list;

typedef struct
{
	u_byte id;
	u_byte parent_id;
	unsigned char name[PC_NAME_LEN];
	int address_id;
	int state;

	u_byte prio;
	u_byte high3;
	u_byte low3;

	context context;
	struct mm_struct *mm;
	FILE *pc_files;
	list node;
}pc;

typedef union
{
	pc pc;
	unsigned char kernel_stack[KERNEL_STACK_SIZE];
}pc_union;

//找出这个8位数最低位的1在哪一位
extern const u_byte where_lowest1_table_for_8[256];
extern const u_byte only_indexbit_is1_table_for_3[8];
extern list wait_list;
extern pc_union *cur_pc;
extern pc_union *all_pcs[MAX_LEVEL];
extern pc_union *shell;

extern u_byte ready_table[MAX_LEVEL / 8];
extern u_byte ready_group;

void wait_for_newpc(u_byte id);
void init_pc();
void idle();
int exec_from_kernel(u_int argc, void *args, int wait, u_byte new_id, int user, int test);
int create_pc(char *name, u_byte new_id, void(*entry)(u_int argc, void *args), u_int argc, void *args, int user);
int kill_pc(u_byte kill_id);
void activate_mm(pc* pc);
void pc_files_delete(pc* pc);
void print_all_pcs();
void print_wait();
int entry(unsigned int argc, void *args);
void end_pc();
void pc_schedule(unsigned int state, unsigned int cause, context *pt_context);
int cal_prio(pc *target, u_byte prio);
void turn_to_ready(pc* target);
void turn_to_unready(pc* target, int state);
pc_union *find_next();
pc_union *find_by_id(u_byte id);
void change_prio(pc_union *target, int new_prio);
static void copy_context(context* src, context* dest);

#endif  // !_ZJUNIX_PC_H

////volatile:不会被优化掉
//mfc：从后面读出来，存到前面的寄存器里
//mtc：从前面读出来，存到后面的寄存器里
//asm语句，用%0 %1代表外界变量
//=r代表将括号里的变量写入占位符
//r代表将寄存器值读入括号里的变量

//控制台进程
//idle进程优先级最低 没的跑就跑idle