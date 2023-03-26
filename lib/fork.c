// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>
// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//



static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
    // todo

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).
    if(!(err & FEC_WR)){
        cprintf("err code is not write, err %p, FEC_WR %p\n",err,FEC_WR);
        panic("pgfault");
    } 
    pte_t pte = uvpt[PGNUM(addr)];
//    panic("*pte %p",*pte);
    if(!(pte & PTE_COW)){
        panic("pte not PTE_COW");
    }
    // cprintf(" lib/fork pgfault, va %p\n",addr);
	// LAB 4: Your code here.
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    r = sys_page_alloc(0,PFTEMP,PTE_U | PTE_W | PTE_P);
    if(r){
        panic(" sys page alloc fail");
    }
    addr = (void *)ROUNDDOWN((uint32_t)addr,PGSIZE);
    memmove(PFTEMP,addr,PGSIZE);
    r = sys_page_map(0,PFTEMP,0,addr, PTE_U | PTE_W | PTE_P);
    if(r){
        panic(" sys page map fail");
    }
    
    r = sys_page_unmap(0, PFTEMP);
    if(r){
        panic(" sys page unmap fail");
    }
//	panic("pgfault not implemented");
}


//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//


static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	// LAB 4: Your code here.
    void *addr = (void *)(pn * PGSIZE);

//    pde_t * src_pde = &((pde_t * uvpd)[PDX(addr)]);
    pte_t pte = uvpt[PGNUM(addr)];
    assert(pte != 0);
//    cprintf(" - duppage : va %08p,pte %08p\n",addr,pte);

//    cprintf(" - duppage : va%p,pte 0x%08x\n",addr,pte);
/*
    int perm = pte & 0xFFF;
    if(pte & (PTE_W | PTE_COW)){
        perm = PTE_COW | PTE_U | PTE_P;
        r = sys_page_map(0,addr,0,addr,perm);
        if(r){
                panic(" - duppage ");
        }
    }
    r = sys_page_map(0,addr,envid,addr,perm);
    if(r){
        panic(" - duppage ");
    }
	return 0;
*/
    if(pte & (PTE_W | PTE_COW)){
        if((r = sys_page_map(0,addr,envid,addr,(PTE_COW | PTE_U | PTE_P)))){
                panic(" - duppage ");
        }
        if((r = sys_page_map(0,addr,0,addr,(PTE_COW | PTE_U | PTE_P)))){
                panic(" - duppage ");
        }



    }else{
        r = sys_page_map(0,addr,envid,addr,(PTE_U | PTE_P));
        if(r){
            panic(" - duppage ");
        }
    }

	return 0;
}


//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
    cprintf(" - lib/fork: begin ...\n");
   // sys_show_user_page_map(thisenv->env_id);
    set_pgfault_handler(pgfault);
    int envid = sys_exofork();
    if(envid == 0){
    //    cprintf(" - lib/fork: child return\n");
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    } 
    if(envid < 0){
        panic(" envid < 0 ");
    }
	for (uintptr_t addr = 0; addr < USTACKTOP; addr += PGSIZE){
//        cprintf(" - lib/fork: addr %08p, userstacktop %08p\n",addr,USTACKTOP);
//        pde_t pde = uvpd[PDX(addr)];
        if ( (uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) ) {
            // dup page to child
//            cprintf(" - va %p pde %p pte %p\n",addr,uvpd[PDX(addr)],uvpt[PGNUM(addr)]);
            duppage(envid, PGNUM(addr));
        }
    }
    //sys_show_user_page_map(envid);
    cprintf(" - lib/fork: %04x duppage to %04x \n",thisenv->env_id,envid);
    
    int r = sys_page_alloc(envid,(void *)UXSTACKTOP - PGSIZE,PTE_U|PTE_W|PTE_P);
    if(r){
        panic("fork fail here\n");
    }
    extern void _pgfault_upcall(void); //缺页处理
	if ((r = sys_env_set_pgfault_upcall(envid, _pgfault_upcall)) < 0)
           return r; //为儿子设置一个缺页处理分支
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)//设置成可运行
           return r;
    cprintf(" - lib/fork: return\n");
    return envid;
}
// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}