#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/error.h>
volatile uint32_t *E1000BASE;
#define REG(offset) (E1000BASE[offset / 4])

struct eth_packet_buffer
{
	char data[DATA_PACKET_BUFFER_SIZE];
};

// transmit buffers
struct eth_packet_buffer tx_queue_data[TX_QUEUE_SIZE];
struct e1000_tx_desc *tx_queue_desc;

int e1000_attach(struct pci_func *f){
    pci_func_enable(f);
    E1000BASE = mmio_map_region(f->reg_base[0],f->reg_size[0]);
    assert(REG(E1000_STATUS) == 0x80080783);

    struct PageInfo *page = page_alloc(ALLOC_ZERO);
    physaddr_t tx_descs_base = page2pa(page);
    tx_queue_desc = page2kva(page);

    REG(E1000_TDBAL) = tx_descs_base;
    REG(E1000_TDBAH) = 0;
    REG(E1000_TDLEN) = MAX_DESC_SIZE * sizeof(struct e1000_tx_desc);

    REG(E1000_TDH) = 0;
    REG(E1000_TDT) = 0;

    REG(E1000_TCTL) |= E1000_TCTL_EN | E1000_TCTL_PSP;
    REG(E1000_TCTL) |= E1000_TCTL_COLD & ( 0x40 << 12);  // set the cold)
    REG(E1000_TIPG) = 10; // page 313


    for(int i = 0; i < MAX_DESC_SIZE; i++){
		tx_queue_desc[i].addr = (uint64_t)PADDR(&tx_queue_data[i]);
        // Report Status on, and mark descriptor as end of packet
		tx_queue_desc[i].cmd = (1 << 3) | (1 << 0);
		// set Descriptor Done so we can use this descriptor
		tx_queue_desc[i].status |= E1000_TXD_STAT_DD;
    }

    return 0;
}
int tx_packet(char *buf, int size)
{
	assert(size <= ETH_MAX_PACKET_SIZE);
	int tail_indx = REG(E1000_TDT);
	if (!(tx_queue_desc[tail_indx].status & E1000_TXD_STAT_DD)) {
		return -E_NIC_BUSY; // queue is full
	}
	tx_queue_desc[tail_indx].status &= ~E1000_TXD_STAT_DD;
	memmove(&tx_queue_data[tail_indx].data, buf, size);
	tx_queue_desc[tail_indx].length = size;
	// update the TDT to "submit" this packet for transmission
	REG(E1000_TDT) = (tail_indx + 1) % TX_QUEUE_SIZE;
	return 0;
}