#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "vm.h"
#include "virtio.h"
#include "defs.h"

#define R1(r) ((volatile uint32 *)(VIRTIO1 + (r)))

struct telemetry_queue {
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;
};

static struct {
  struct spinlock lock;
  int ready;
  struct telemetry_queue rxq;
  struct telemetry_queue txq;
  char txbuf[512];
  uint16 tx_used_idx;
} telemetry_console;

static int
telemetry_queue_init(int queue_id, struct telemetry_queue *queue)
{
  *R1(VIRTIO_MMIO_QUEUE_SEL) = queue_id;
  if(*R1(VIRTIO_MMIO_QUEUE_READY))
    return -1;

  uint32 max = *R1(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0 || max < NUM)
    return -1;

  queue->desc = kalloc();
  queue->avail = kalloc();
  queue->used = kalloc();
  if(!queue->desc || !queue->avail || !queue->used)
    return -1;

  memset(queue->desc, 0, PGSIZE);
  memset(queue->avail, 0, PGSIZE);
  memset(queue->used, 0, PGSIZE);

  *R1(VIRTIO_MMIO_QUEUE_NUM) = NUM;
  *R1(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)queue->desc;
  *R1(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)queue->desc >> 32;
  *R1(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)queue->avail;
  *R1(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)queue->avail >> 32;
  *R1(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)queue->used;
  *R1(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)queue->used >> 32;
  *R1(VIRTIO_MMIO_QUEUE_READY) = 1;
  return 0;
}

void
telemetry_console_init(void)
{
  uint32 status = 0;
  uint32 features;

  initlock(&telemetry_console.lock, "telemetry_console");
  telemetry_console.ready = 0;

  if(*R1(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
     *R1(VIRTIO_MMIO_VERSION) != 2 ||
     *R1(VIRTIO_MMIO_DEVICE_ID) != VIRTIO_ID_CONSOLE ||
     *R1(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551)
    return;

  *R1(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R1(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R1(VIRTIO_MMIO_STATUS) = status;

  *R1(VIRTIO_MMIO_DEVICE_FEATURES_SEL) = 0;
  features = *R1(VIRTIO_MMIO_DEVICE_FEATURES);
  *R1(VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 0;
  features &= ~(1 << VIRTIO_CONSOLE_F_SIZE);
  features &= ~(1 << VIRTIO_CONSOLE_F_MULTIPORT);
  features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
  *R1(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R1(VIRTIO_MMIO_STATUS) = status;
  if((*R1(VIRTIO_MMIO_STATUS) & VIRTIO_CONFIG_S_FEATURES_OK) == 0)
    return;

  if(telemetry_queue_init(0, &telemetry_console.rxq) < 0)
    return;
  if(telemetry_queue_init(1, &telemetry_console.txq) < 0)
    return;

  telemetry_console.tx_used_idx = telemetry_console.txq.used->idx;

  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R1(VIRTIO_MMIO_STATUS) = status;
  telemetry_console.ready = 1;
}

int
telemetry_console_write(char *buf, int n)
{
  if(!telemetry_console.ready)
    return -1;
  if(n <= 0)
    return 0;
  if(n > sizeof(telemetry_console.txbuf))
    n = sizeof(telemetry_console.txbuf);

  acquire(&telemetry_console.lock);

  memmove(telemetry_console.txbuf, buf, n);
  telemetry_console.txq.desc[0].addr = (uint64)telemetry_console.txbuf;
  telemetry_console.txq.desc[0].len = n;
  telemetry_console.txq.desc[0].flags = 0;
  telemetry_console.txq.desc[0].next = 0;
  telemetry_console.txq.avail->ring[telemetry_console.txq.avail->idx % NUM] = 0;
  __sync_synchronize();
  telemetry_console.txq.avail->idx += 1;
  __sync_synchronize();
  *R1(VIRTIO_MMIO_QUEUE_NOTIFY) = 1;

  while(telemetry_console.txq.used->idx == telemetry_console.tx_used_idx)
    ;
  telemetry_console.tx_used_idx = telemetry_console.txq.used->idx;

  release(&telemetry_console.lock);
  return n;
}
