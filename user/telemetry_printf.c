#include "kernel/types.h"
#include "user/user.h"

#include <stdarg.h>

static char digits[] = "0123456789ABCDEF";

struct telemetry_sink {
  char buf[512];
  int len;
  int fallback;
};

static void
sink_flush(struct telemetry_sink *sink)
{
  if(sink->len == 0)
    return;

  if(!sink->fallback && telemetrywrite(sink->buf, sink->len) < 0)
    sink->fallback = 1;
  if(sink->fallback)
    write(1, sink->buf, sink->len);
  sink->len = 0;
}

static void
sink_putc(struct telemetry_sink *sink, char c)
{
  if(sink->len >= sizeof(sink->buf))
    sink_flush(sink);
  sink->buf[sink->len++] = c;
}

static void
printint(struct telemetry_sink *sink, long long xx, int base, int sgn, int width, int left_align, int zero_pad)
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
      sink_putc(sink, pad);
  }
  while(--i >= 0)
    sink_putc(sink, buf[i]);
  if(left_align && width > len) {
    for(int j = 0; j < width - len; j++)
      sink_putc(sink, ' ');
  }
}

static void
printstr(struct telemetry_sink *sink, char *s, int width, int left_align)
{
  int len = 0;
  char *p;

  if(s == 0)
    s = "(null)";

  p = s;
  while(*p++) len++;

  if(!left_align && width > len) {
    for(int j = 0; j < width - len; j++)
      sink_putc(sink, ' ');
  }
  for(; *s; s++)
    sink_putc(sink, *s);
  if(left_align && width > len) {
    for(int j = 0; j < width - len; j++)
      sink_putc(sink, ' ');
  }
}

static void
printptr(struct telemetry_sink *sink, uint64 x)
{
  sink_putc(sink, '0');
  sink_putc(sink, 'x');
  for(int i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    sink_putc(sink, digits[x >> (sizeof(uint64) * 8 - 4)]);
}

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

static void
telemetry_vprintf(const char *fmt, va_list ap)
{
  struct telemetry_sink sink = { .len = 0, .fallback = 0 };
  char *s;
  int c0, c1, c2, i, state;
  int width = 0, left_align = 0, zero_pad = 0;

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
        sink_putc(&sink, c0);
      }
    } else {
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
      if(c0 >= '0' && c0 <= '9'){
        width = parse_width(fmt, &i);
        c0 = fmt[i] & 0xff;
      }

      c1 = c2 = 0;
      if(c0) c1 = fmt[i+1] & 0xff;
      if(c1) c2 = fmt[i+2] & 0xff;

      if(c0 == 'd'){
        printint(&sink, va_arg(ap, int), 10, 1, width, left_align, zero_pad);
      } else if(c0 == 'l' && c1 == 'd'){
        printint(&sink, va_arg(ap, uint64), 10, 1, width, left_align, zero_pad);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
        printint(&sink, va_arg(ap, uint64), 10, 1, width, left_align, zero_pad);
        i += 2;
      } else if(c0 == 'u'){
        printint(&sink, va_arg(ap, uint32), 10, 0, width, left_align, zero_pad);
      } else if(c0 == 'l' && c1 == 'u'){
        printint(&sink, va_arg(ap, uint64), 10, 0, width, left_align, zero_pad);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
        printint(&sink, va_arg(ap, uint64), 10, 0, width, left_align, zero_pad);
        i += 2;
      } else if(c0 == 'x'){
        printint(&sink, va_arg(ap, uint32), 16, 0, width, left_align, zero_pad);
      } else if(c0 == 'l' && c1 == 'x'){
        printint(&sink, va_arg(ap, uint64), 16, 0, width, left_align, zero_pad);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
        printint(&sink, va_arg(ap, uint64), 16, 0, width, left_align, zero_pad);
        i += 2;
      } else if(c0 == 'p'){
        printptr(&sink, va_arg(ap, uint64));
      } else if(c0 == 'c'){
        char c = va_arg(ap, int);
        if(!left_align && width > 1) {
          for(int j = 0; j < width - 1; j++)
            sink_putc(&sink, ' ');
        }
        sink_putc(&sink, c);
        if(left_align && width > 1) {
          for(int j = 0; j < width - 1; j++)
            sink_putc(&sink, ' ');
        }
      } else if(c0 == 's'){
        s = va_arg(ap, char*);
        printstr(&sink, s, width, left_align);
      } else if(c0 == '%'){
        sink_putc(&sink, '%');
      } else {
        sink_putc(&sink, '%');
        sink_putc(&sink, c0);
      }
      state = 0;
    }
  }

  sink_flush(&sink);
}

void
telemetry_printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  telemetry_vprintf(fmt, ap);
}
