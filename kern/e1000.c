#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/error.h>
volatile uint32_t *E1000BASE;
#define REG(offset) (E1000BASE[offset / 4])
#define TX_QUEUE_SIZE 64
#define RX_QUEUE_SIZE 128

struct eth_packet_buffer
{
	char data[DATA_PACKET_BUFFER_SIZE];
};

// transmit buffers
struct eth_packet_buffer tx_queue_data[TX_QUEUE_SIZE];
struct e1000_tx_desc *tx_queue_desc;

// receive buffers
struct eth_packet_buffer rx_queue_data[RX_QUEUE_SIZE];
struct e1000_rx_desc *rx_queue_desc;
int e1000_attach(struct pci_func *f){
    pci_func_enable(f);
    E1000BASE = mmio_map_region(f->reg_base[0],f->reg_size[0]);
    assert(REG(E1000_STATUS) == 0x80080783);

    struct PageInfo *page = page_alloc(ALLOC_ZERO);
    physaddr_t tx_descs_base = page2pa(page);
    tx_queue_desc = page2kva(page);

    REG(E1000_TDBAL) = tx_descs_base;
    REG(E1000_TDBAH) = 0;
    REG(E1000_TDLEN) = TX_QUEUE_SIZE * sizeof(struct e1000_tx_desc);

    REG(E1000_TDH) = 0;
    REG(E1000_TDT) = 0;

    REG(E1000_TCTL) |= E1000_TCTL_EN | E1000_TCTL_PSP;
    REG(E1000_TCTL) |= E1000_TCTL_COLD & ( 0x40 << 12);  // set the cold)
    REG(E1000_TIPG) = 10; // page 313


    for(int i = 0; i < TX_QUEUE_SIZE; i++){
		tx_queue_desc[i].addr = (uint64_t)PADDR(&tx_queue_data[i]);
        // Report Status on, and mark descriptor as end of packet
		tx_queue_desc[i].cmd = (1 << 3) | (1 << 0);
		// set Descriptor Done so we can use this descriptor
		tx_queue_desc[i].status |= E1000_TXD_STAT_DD;
    }

	// Receive initialization
	// MAC address 52:54:00:12:34:56
	REG(E1000_RAL) = 0x12005452;
	*(uint16_t *)&REG(E1000_RAH) = 0x5634;
	REG(E1000_RAH) |= E1000_RAH_AV; // set the address valid bit
	// MTA initialized to 0b
	REG(E1000_MTA) = 0;
	// Allocate memory for the receive descriptor list & init registers
	struct PageInfo *rx_desc_pg = page_alloc(ALLOC_ZERO);
	rx_queue_desc = page2kva(rx_desc_pg);
	physaddr_t rx_desc_base = page2pa(rx_desc_pg);
	REG(E1000_RDBAL) = rx_desc_base;
	REG(E1000_RDBAH) = 0;
	REG(E1000_RDLEN) = RX_QUEUE_SIZE * sizeof(struct e1000_rx_desc);

	// initialize head and tail such that (tail + 1) % size = head
	REG(E1000_RDH) = 0;
	REG(E1000_RDT) = RX_QUEUE_SIZE - 1;
	for (int i = 0; i < RX_QUEUE_SIZE; i++) {
		rx_queue_desc[i].addr = (uint64_t)PADDR(&rx_queue_data[i]);
		// clear Descriptor Done so we know we are not allowed to read it
		rx_queue_desc[i].status &= ~E1000_RXD_STAT_DD;
	}
	// enable and strip CRC
	REG(E1000_RCTL) |= E1000_RCTL_EN | E1000_RCTL_SECRC;


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

int rx_packet(char *buf, int size)
{
	int next_indx = (REG(E1000_RDT) + 1) % RX_QUEUE_SIZE;
	if (!(rx_queue_desc[next_indx].status & E1000_TXD_STAT_DD)) {
		return -E_RX_EMPTY; // queue is empty
	}
	rx_queue_desc[next_indx].status &= ~E1000_TXD_STAT_DD;
	int rx_size = MIN(rx_queue_desc[next_indx].length, size);
	memmove(buf, rx_queue_data[next_indx].data, rx_size);
	REG(E1000_RDT) = next_indx;
	return rx_size;
}