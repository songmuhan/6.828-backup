#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H
#endif  // SOL >= 6
#include <kern/pci.h>

int e1000_attach(struct pci_func *);


#define E1000_STATUS   0x00008  /* Device Status - RO */
