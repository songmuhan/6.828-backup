#include <kern/e1000.h>
#include <kern/pmap.h>


volatile uint32_t *E1000BASE;
#define REG(offset) (E1000BASE[offset / 4])

// LAB 6: Your driver code here
int e1000_attach(struct pci_func *f){
    pci_func_enable(f);
    E1000BASE = mmio_map_region(f->reg_base[0],f->reg_size[0]);
    assert(REG(E1000_STATUS) == 0x80080783);
    return 0;
}