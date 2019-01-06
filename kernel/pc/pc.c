# include "zjunix/pc.h"

//初始化进程模块
//将进程调度所用的bitmap全部初始化为0
//初始化时没有当前执行进程，shell也未创建
//然后创建idle进程，并进入进程调度。
void init_pc()
{
	ready_group = 0;
	for (int i = 0; i < MAX_LEVEL / 8; i++)
	{
		ready_table[i] = 0;
	}
	INIT_LIST_HEAD(&wait_list);
	for (int i = 0; i < MAX_LEVEL; i++)
	{
		all_pcs[i] = 0;
	}
	cur_pc = 0;
	shell = 0;

	//将调度函数注册到7号中断（时间中断）与5号系统调用
	register_interrupt_handler(7, pc_schedule);
	register_syscall(5, pc_schedule);

	if(create_pc("idle", IDLE_ID, (void*)idle, 0, 0, 0))
	{
		kernel_printf("Init idle success!\n");
		asm volatile(
			"li $v0, 1000000\n\t"
			"mtc0 $v0, $11\n\t"
			"mtc0 $zero, $9");
	}
	else
	{
		kernel_printf("Init idle failed!\n");
		return;
	}
}

//idle进程所用代码
//每过一段时间输出，表明idle正在被运行。
void idle()
{
	int count = 0;
	while(1)
	{
		count++;
		if(count == 10000)
		{
			kernel_printf("Idle is running!\n");
			count = 0;
		}
	}
}

//初始优先级为7，变为20之后，新的进程优先级确实不能是20
//新的进程相应的id也不是20
//原来如果有id为20的，他的优先级只要没改，是不能变过去的
//外界调用这个函数创建新进程，创建好了之后触发进程调度
//all_pc按照优先级作为索引，所以要寻找对应id的进程要遍历
int exec_from_kernel(u_int argc, void *args, int wait, u_byte new_id, int user, int test)
{
	if (create_pc(args, new_id, (void*)entry(test, 0), 0, 0, user))
	{
		kernel_printf("Create process failed!");
		return 1;
	}
	else
	{
		if (wait)
		{
			wait_for_newpc(new_id);
		}
		else
		{
			asm volatile(
			"li $v0, 5\n\t"
			"syscall\n\t"
			"nop\n\t");
		}
	}
}

int create_pc(char *name, u_byte new_id, void(*entry)(u_int argc, void *args), u_int argc, void *args, int user)
{
	u_int init_gp;
	pc_union *new_pc_union;

	new_pc_union = kmalloc(sizeof(pc_union));
	new_pc_union->pc.state = P_INIT;
	kernel_strcpy(new_pc_union->pc.name, name);
	if (cal_prio(&(new_pc_union->pc), new_id))
	{
		return 1;
	}
	else
	{
		new_pc_union->pc.id = new_id;
		new_pc_union->pc.parent_id = cur_pc->pc.id;
		new_pc_union->pc.address_id = new_id;
		kernel_memset(&(new_pc_union->pc.context), 0, sizeof(context));
		new_pc_union->pc.context.epc = (u_int)entry;
		new_pc_union->pc.context.sp = (u_int)new_pc_union + KERNEL_STACK_SIZE;
		asm volatile("la %0, _gp\n\t" : "=r"(init_gp));
		new_pc_union->pc.context.gp = init_gp;
		new_pc_union->pc.context.a0 = argc;
		new_pc_union->pc.context.a1 = (u_int)args;
		new_pc_union->pc.pc_files = 0;
		if(user)
		{
			new_pc_union->pc.mm = mm_create();
		}
		else
		{
			new_pc_union->pc.mm = 0;
		}
		INIT_LIST_HEAD(&(new_pc_union->pc.node));
		if (new_id == SHELL_ID)
		{
			shell = new_pc_union;
		}
		else
		{
			all_pcs[new_pc_union->pc.prio] = new_pc_union;
		}
		turn_to_ready(&(new_pc_union->pc));
		kernel_printf("Create %d process success!", new_pc_union->pc.id);
		return 0;
	}
}

int kill_pc(u_byte kill_id)
{
	pc_union *kill_pc = 0;
	list *pos;
	pc_union *next;
	if(kill_id == SHELL_ID)
	{
		kernel_printf("shell can not be killed!\n");
		return 1;
	}
	else
	{
		kill_pc = find_by_id(kill_id);
		if (!kill_pc)
		{
			if (kill_pc->pc.id == cur_pc->pc.id)
			{
				kernel_printf("Current process can not be killed!\n");
				return 1;
			}
			if (kill_pc->pc.id = IDLE_ID)
			{
				kernel_printf("Idle process can not be killed!\n");
				return 1;
			}

			disable_interrupts();
			if (kill_pc->pc.state < 0)
			{
				list_for_each(pos, &wait_list)
				{
					next = container_of(pos, pc_union, pc.node);
					if (next->pc.id == kill_pc->pc.id)
					{
						list_del(&(next->pc.node));
						INIT_LIST_HEAD(&(next->pc.node));
					}
				}
			}

			turn_to_unready(kill_pc, P_END);
			if (kill_pc->pc.pc_files != 0)
			{
				pc_files_delete(&(kill_pc->pc));
			}
			if (kill_pc->pc.mm != 0)
			{
				mm_delete(kill_pc->pc.mm);
			}
			all_pcs[kill_pc->pc.prio] = 0;
			kfree(kill_pc);
			enable_interrupts();
			asm volatile(
				"li $v0, 5\n\t"
				"syscall\n\t"
				"nop\n\t");
			return 0;
		}
		else
		{
			kernel_printf("ID not found!\n");
			return 1;
		}
	}
}

void activate_mm(pc* pc)
{
	// init_pgtable();
	//在cp0中设置进程的ASID， 用于TLB匹配
	set_tlb_asid(pc->address_id);
}

void pc_files_delete(pc* pc) 
{
	//可能有问题，不知道如何查看文件是否打开
	fs_close(pc->pc_files);
	kfree(&(pc->pc_files));
}

void print_all_pcs()
{
	for(int i = 0; i < MAX_LEVEL;i++)
	{
		if(!(i % 8))
		{
			kernel_printf("\n");
		}
		if(all_pcs[i] == 0)
		{
			kernel_printf("null ");
		}
		if(all_pcs[i] != 0)
		{
			kernel_printf("%d %d | ", all_pcs[i]->pc.state, all_pcs[i]->pc.id);
		}
	}
}

void print_wait() 
{
    pc_union *next;
    list *pos;
    kernel_printf("wait list:\n");
	list_for_each(pos, &wait_list)
	{
		next = container_of(pos, pc_union, pc.node);
		kernel_printf("%d ", next->pc.id);
	}
}

int entry(unsigned int argc, void *args)
{
	kernel_printf("\n============number %d process============\n", cur_pc->pc.id);
	int count = 100000;
	int count_exit = 0;
	while(1)
	{
		if(count == 100000)
		{
			kernel_printf("%d process is running!\n", cur_pc->pc.id);
			count = 0;
			count_exit++;
			if(count_exit == 100 && cur_pc->pc.id != 8 && cur_pc->pc.id != 7)
			{
				break;
			}
			if(cur_pc->pc.id == 7 && count_exit == 50)
			{
				change_prio(cur_pc, 20);
			}
			if(cur_pc->pc.id == 7 && count_exit == 80)
			{
				change_prio(cur_pc, 7);
			}
			if(cur_pc->pc.id == 6 && count_exit == 10)
			{
				break;
			}
		}
		count++;
	}
}

void wait(u_byte id)
{
	if (id < MAX_LEVEL)
	{
		pc_union *target = find_by_id(id);
		if (target && target->pc.state != P_END)
		{
			turn_to_unready(&(cur_pc->pc), -id);
			list_add_tail(&(cur_pc->pc.node), &wait);
			asm volatile(
			"li $v0, 5\n\t"
			"syscall\n\t"
			"nop\n\t");
		}
		else
		{
			kernel_printf("This process you wait has exited!\n");
			return;
		}
	}
	else
	{
		kernel_printf("Illegal process ID!\n");
		return;
	}
}

void end_pc()
{
	if (cur_pc->pc.id == SHELL_ID)
	{
		kernel_printf("Shell process can not be exited!\n");
		return;
	}
	if (cur_pc->pc.id = IDLE_ID)
	{
		kernel_printf("Idle process can not be exited!\n");
		return;
	}
	turn_to_unready(cur_pc, P_END);
	if(cur_pc->pc.pc_files != 0)
	{
		pc_files_delete(cur_pc->pc.pc_files);
	}
	if(cur_pc->pc.mm != 0)
	{
		mm_delete(cur_pc->pc.mm);	
	}
	all_pcs[cur_pc->pc.prio] = 0;
	kfree(cur_pc);
	asm volatile(
		"li $v0, 5\n\t"
		"syscall\n\t"
		"nop\n\t");
}

void pc_schedule(int state, int cause, context *pt_context)
{
	static u_int TIME_SLOT = 0;
	if ((cause >> (8 + 7) & 1) && !(TIME_SLOT % 5)) // 每5个时间片调用一次shell
	{
		pc_union *next = shell;			
		TIME_SLOT = 0;
		copy_context(pt_context, &(cur_pc->pc.context));
		cur_pc = next;
		copy_context(&(cur_pc->pc.context), pt_context);
		asm volatile("mtc0 $zero, $9\n\t");
	}
	else if ((cause >> (8 + 7) & 1) && (TIME_SLOT % 5))//时间中断进来，但是时间片还不够
	{
		TIME_SLOT++;
		asm volatile("mtc0 $zero, $9\n\t");
	}
	else if (!(cause >> (8 + 7 & 1)))//系统调用
	{
		pc_union *next = find_next();
		if (next->pc.mm != 0)
		{//激活地址空间
			activate_mm(next);
		}
		copy_context(pt_context, &(cur_pc->pc.context));
		cur_pc = next;
		copy_context(&(cur_pc->pc.context), pt_context);
	}
	else
	{
		kernel_printf("Error from schedule!\n");
		return;
	}
}

//计算优先级，计算low3, high3, 并判别优先级是否合法
//第一个参数为需要计算优先级的pcb
//第二个参数优先级
int cal_prio(pc *target, u_byte prio)
{
	if(prio == SHELL_ID && !kernel_strcmp("shell", target->name))
	{//shell进程
		target->prio = prio;
		target->high3 = prio >> 3;
		target->low3 = prio & 0x07;
		return 0;
	}
	else
	{//normal进程
		if(prio < MAX_LEVEL && prio >= 0)
		{
			target->prio = prio;
			target->high3 = prio >> 3;
			target->low3 = prio & 0x07;
			return 0;
		}
		else
		{
			kernel_printf("Error priority!");
			return 1;
		}
	}
}

void turn_to_ready(pc* target)
{
	ready_group |= only_indexbit_is1_table_for_3[target->high3];
	ready_table[target->high3] |= only_indexbit_is1_table_for_3[target->low3];
	target->state = P_READY;
}

void turn_to_unready(pc* target, int state)
{
	list *pos;
	pc_union *next;
	ready_table[target->high3] &= ~only_indexbit_is1_table_for_3[target->low3];
	if(ready_table[target->high3] == 0)
	{
		ready_group &= ~only_indexbit_is1_table_for_3[target->high3];
	}
	target->state = state;
	if(state == P_END)
	{
		list_for_each(pos, &wait_list)
		{
			next = container_of(pos, pc_union, pc.node);
			if (next->pc.state == -(target->id))
			{
				turn_to_ready(&(next->pc));
				list_del(&(next->pc.node));
				INIT_LIST_HEAD(&(next->pc.node));
			}
		}
	}
}

pc_union *find_next()
{
	int y = where_lowest1_table_for_8[ready_group];
	u_byte prio = (u_byte) ((y << 3) + where_lowest1_table_for_8[ready_table[y]]);
	return all_pcs[prio];
}

pc_union *find_by_id(u_byte id)
{
	if(id == SHELL_ID)
	{
		return shell;
	}
	else
	{
		for(int i = 0; i<MAX_LEVEL; i++)
		{
			if(all_pcs[i]->pc.id == id)
			{
				return all_pcs[i];
			}
		}
		return 0;
	}
}

void change_prio(pc_union *target, int new_prio)
{
	disable_interrupts();
	int old_prio = target->pc.prio;
	if(!cal_prio(&(target->pc), new_prio))
	{
		all_pcs[old_prio] = 0;
		all_pcs[new_prio] = target;
		asm volatile(
		"li $v0, 5\n\t"
		"syscall\n\t"
		"nop\n\t");
	}
	enable_interrupts();
}

static void copy_context(context* src, context* dest)
{
	dest->epc = src->epc;
	dest->at = src->at;
	dest->v0 = src->v0;
	dest->v1 = src->v1;
	dest->a0 = src->a0;
	dest->a1 = src->a1;
	dest->a2 = src->a2;
	dest->a3 = src->a3;
	dest->t0 = src->t0;
	dest->t1 = src->t1;
	dest->t2 = src->t2;
	dest->t3 = src->t3;
	dest->t4 = src->t4;
	dest->t5 = src->t5;
	dest->t6 = src->t6;
	dest->t7 = src->t7;
	dest->s0 = src->s0;
	dest->s1 = src->s1;
	dest->s2 = src->s2;
	dest->s3 = src->s3;
	dest->s4 = src->s4;
	dest->s5 = src->s5;
	dest->s6 = src->s6;
	dest->s7 = src->s7;
	dest->t8 = src->t8;
	dest->t9 = src->t9;
	dest->hi = src->hi;
	dest->lo = src->lo;
	dest->gp = src->gp;
	dest->sp = src->sp;
	dest->fp = src->fp;
	dest->ra = src->ra;
}