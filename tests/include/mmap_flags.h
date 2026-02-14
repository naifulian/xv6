// mmap_flags.h - Common mmap flags for tests
#ifndef MMAP_FLAGS_H
#define MMAP_FLAGS_H

// mmap protection flags
#define PROT_READ  0x01
#define PROT_WRITE 0x02

// mmap mapping flags (simplified for xv6 kernel)
// Note: Current xv6 kernel implementation requires MAP_ANONYMOUS | MAP_PRIVATE
#define MAP_ANONYMOUS 0x20
#define MAP_PRIVATE   0x02

#endif // MMAP_FLAGS_H
