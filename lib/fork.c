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
    if(err != FEC_WR){
        panic("err code is not write");
    }
    pde_t * pde = (pde_t *)uvpd;
    pde = &pde[PDX(addr)];
    pte_t *pte = &pde[PTX(addr)];
//    panic("*pte %p",*pte);
    if(!(*pte & PTE_COW)){
        panic("pte not PTE_COW");
    }
	// LAB 4: Your code here.
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
    int r  = 0;
    if(r = sys_page_alloc(thisenv->env_id,PFTEMP,PTE_U | PTE_W | PTE_P)){
        panic(" sys page alloc fail");
    }
    uint32_t begin = ROUNDDOWN(addr,PGSIZE);
    memcpy(PFTEMP,begin,PGSIZE);
    if( r = sys_page_unmap(thisenv->env_id, addr)){
        panic(" sys page unmap fail");
    }
    if(r = sys_page_map(thisenv->env_id,PFTEMP,thisenv->env_id,addr, PTE_U | PTE_W | PTE_P)){
        panic(" sys page map fail");
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
    envid_t dst_envid = sys_getenvid();
	// LAB 4: Your code here.
    void *addr = (void *)(pn * PGSIZE);

//    pde_t * src_pde = &((pde_t * uvpd)[PDX(addr)]);
    pte_t * src_pte = &uvpt[PTX(addr)];

    if(*src_pte & (PTE_W |PTE_COW)){
        * src_pte = ((*src_pte) & 0xFFFFF000) | PTE_COW | PTE_U | PTE_P;
    }

    if( r = sys_page_map(thisenv->env_id,addr,dst_envid,addr, PTE_COW | PTE_U | PTE_P)){
        panic(" sys page map fail %e",r);
    }
//	panic("duppage not implemented");
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
    set_pgfault_handler(pgfault);
    int envid = sys_exofork();
    if(envid == 0){
        thisenv = envs[ENVX(sys_getenvid())];
        return 0;
    } 
    for(uintptr_t addr = UTEXT; addr < USTACKTOP; addr += PGSIZE){
        duppage(envid,PGNUM(addr));
    }
    int r = sys_page_alloc(envid,UXSTACKTOP,PTE_U|PTE_W);
    if(r){
        panic("fork %e",r);
    }
    


	panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
