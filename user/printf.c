#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#include <stdarg.h>

static char digits[] = "0123456789ABCDEF";

static void
putc(int fd, char c)
{
  write(fd, &c, 1);
}

static void
printint(int fd, long long xx, int base, int sgn, int width, int left_align, int zero_pad)
{
  char buf[20];
  int i, neg;
  unsigned long long x;
  int len;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  len = i;

  if(!left_align && width > len) {
    char pad = zero_pad ? '0' : ' ';
    for(int j = 0; j < width - len; j++)
      putc(fd, pad);
  }

  while(--i >= 0)
    putc(fd, buf[i]);

  if(left_align && width > len) {
    for(int j = 0; j < width - len; j++)
      putc(fd, ' ');
  }
}

static void
printstr(int fd, char *s, int width, int left_align)
{
  int len = 0;
  char *p = s;
  
  if(s == 0)
    s = "(null)";
  
  while(*p++) len++;
  p = s;
  
  if(!left_align && width > len) {
    for(int j = 0; j < width - len; j++)
      putc(fd, ' ');
  }
  
  for(; *s; s++)
    putc(fd, *s);
  
  if(left_align && width > len) {
    for(int j = 0; j < width - len; j++)
      putc(fd, ' ');
  }
}

static void
printptr(int fd, uint64 x) {
  int i;
  putc(fd, '0');
  putc(fd, 'x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    putc(fd, digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Parse width from format string
static int
parse_width(const char *fmt, int *i)
{
  int width = 0;
  while(fmt[*i] >= '0' && fmt[*i] <= '9') {
    width = width * 10 + (fmt[*i] - '0');
    (*i)++;
  }
  return width;
}

// Print to the given fd. Supports %d, %x, %p, %c, %s with width and alignment.
// Format: %[flags][width]type
// flags: '-' for left align, '0' for zero padding
// width: field width
void
vprintf(int fd, const char *fmt, va_list ap)
{
  char *s;
  int c0, c1, c2, i, state;
  int width, left_align, zero_pad;

  state = 0;
  for(i = 0; fmt[i]; i++){
    c0 = fmt[i] & 0xff;
    if(state == 0){
      if(c0 == '%'){
        state = '%';
        width = 0;
        left_align = 0;
        zero_pad = 0;
      } else {
        putc(fd, c0);
      }
    } else if(state == '%'){
      // Parse flags
      if(c0 == '-'){
        left_align = 1;
        i++;
        c0 = fmt[i] & 0xff;
      }
      if(c0 == '0' && !left_align){
        zero_pad = 1;
        i++;
        c0 = fmt[i] & 0xff;
      }
      
      // Parse width
      if(c0 >= '0' && c0 <= '9'){
        width = parse_width(fmt, &i);
        c0 = fmt[i] & 0xff;
      }
      
      c1 = c2 = 0;
      if(c0) c1 = fmt[i+1] & 0xff;
      if(c1) c2 = fmt[i+2] & 0xff;
      
      if(c0 == 'd'){
        printint(fd, va_arg(ap, int), 10, 1, width, left_align, zero_pad);
      } else if(c0 == 'l' && c1 == 'd'){
        printint(fd, va_arg(ap, uint64), 10, 1, width, left_align, zero_pad);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
        printint(fd, va_arg(ap, uint64), 10, 1, width, left_align, zero_pad);
        i += 2;
      } else if(c0 == 'u'){
        printint(fd, va_arg(ap, uint32), 10, 0, width, left_align, zero_pad);
      } else if(c0 == 'l' && c1 == 'u'){
        printint(fd, va_arg(ap, uint64), 10, 0, width, left_align, zero_pad);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
        printint(fd, va_arg(ap, uint64), 10, 0, width, left_align, zero_pad);
        i += 2;
      } else if(c0 == 'x'){
        printint(fd, va_arg(ap, uint32), 16, 0, width, left_align, zero_pad);
      } else if(c0 == 'l' && c1 == 'x'){
        printint(fd, va_arg(ap, uint64), 16, 0, width, left_align, zero_pad);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
        printint(fd, va_arg(ap, uint64), 16, 0, width, left_align, zero_pad);
        i += 2;
      } else if(c0 == 'p'){
        printptr(fd, va_arg(ap, uint64));
      } else if(c0 == 'c'){
        char c = va_arg(ap, int);
        if(!left_align && width > 1) {
          for(int j = 0; j < width - 1; j++)
            putc(fd, ' ');
        }
        putc(fd, c);
        if(left_align && width > 1) {
          for(int j = 0; j < width - 1; j++)
            putc(fd, ' ');
        }
      } else if(c0 == 's'){
        s = va_arg(ap, char*);
        printstr(fd, s, width, left_align);
      } else if(c0 == '%'){
        putc(fd, '%');
      } else {
        putc(fd, '%');
        putc(fd, c0);
      }

      state = 0;
    }
  }
}

void
fprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fd, fmt, ap);
}

void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
}
