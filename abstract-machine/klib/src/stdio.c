#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

static int buf[4096];
/* basic printf format
 * This is a brief description. For more information, see the manual page.
 * 格式串format的格式
 * %[#0- +]*[:NUM:]?\(.[:NUM:]\)?
 * NUM=[:Decimal:]|\*
 * 说明 格式分割符 '%' 紧跟0或多个标志字符，包括
 *   #  替代格式
 *     o         0开头的八进制格式
 *     x/X       0x/0X开头的十六进制
 *     aAeEfFgG  不会省略小数点
 *     gG        不会省略小数部分末尾的0
 *   0  0填充, 如果格式中出现了精度, 0标志会被忽略(仅整数) 或行为未定义. diouxX
 *   -  左对齐 覆盖0标志
 *   ' ' 正数前加空格
 *   +  正数前加+ 覆盖' '标志
 * 宽度标志
 *   NUM  最小字段宽度
 *   *    NUM的值由下一个参数指定
 * 精度标志 指示小数部分最小宽度(aAeEfF)或最大精度(gG)或整数部分最小宽度(diouxX)
 *          或字符串最大宽度(sS)
 *   .     当作 .0 处理
 *   .NUM  为负数时当作没有指定精度
 *   .*    NUM的值由下一个参数指定
 * 长度标志
 *   hh char
 *   h  short
 *   l  long
 *   ll long long
 *   j  intmax_t
 *   z  size_t
 *   t  ptrdiff_t
 * Conversions specifiers
 *   d,i
 *   o,u,x,X
 *   f,F
 *   e,E
 *   g,G
 *   a,A
 *   c
 *   s
 *   p
 *   n
 * 返回值
 *   成功 - 格式串扩充之后的长度
 *   失败 - 负数
 */

/* D
typedef struct {
  char *str;
} outtype;
static inline void io_putc(outtype *io, char ch) {
  if (io->str == NULL)
    putch(ch);
  else
    *io->str++ = ch;
}
int vioprintf_internel(const char *fmt, va_list ap, outtype *io) {
  int data = 0;
  while (*fmt != '\0') {
    if (*fmt == '%') {
    parse_loop:
      fmt++;
      switch (*fmt) {
      case 'd': {
        int num = va_arg(ap, int);
        if (num < 0) {
          io_putc(io, '-');
          num = -num;
          data++;
        }
        int len = 0;
        int tmp = num;
        do {
          buf[len++] = tmp % 10;
          tmp /= 10;
        } while (tmp);
        for (int i = len - 1; i >= 0; i--) {
          io_putc(io, buf[i] + '0');
        }
        data += len;
        break;
      }
      case 's': {
        char *str = va_arg(ap, char *);
        while (*str != '\0') {
          io_putc(io, *str++);
          data++;
        }
        break;
      }
      case 'c': {
        char ch = va_arg(ap, int);
        io_putc(io, ch);
        data++;
        break;
      }
      case 'p': {
        io_putc(io, '0');
        io_putc(io, 'x');
        data += 2;
        unsigned int num = va_arg(ap, unsigned int);
        int len = 0;
        unsigned int tmp = num;
        do {
          buf[len++] = tmp % 16;
          tmp /= 16;
        } while (tmp);
        for (int i = len - 1; i >= 0; i--) {
          if (buf[i] < 10) {
            io_putc(io, buf[i] + '0');
          } else {
            io_putc(io, buf[i] - 10 + 'a');
          }
        }
        data += len;
        break;
      }
      case 'x': {
        unsigned int num = va_arg(ap, unsigned int);
        int len = 0;
        unsigned int tmp = num;
        do {
          buf[len++] = tmp % 16;
          tmp /= 16;
        } while (tmp);
        for (int i = len - 1; i >= 0; i--) {
          if (buf[i] < 10) {
            io_putc(io, buf[i] + '0');
          } else {
            io_putc(io, buf[i] - 10 + 'a');
          }
        }
        break;
      }
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        goto parse_loop;
      default: {
        putch('`');
        putstr(fmt);
        putch('\'');
        panic("Not implemented (printf flag, see above)");
        break;
      }
      }
    } else {
      io_putc(io, *fmt);
    }
    fmt++;
  }
  return data;
}
*/

struct outobj {
  char *p;     // buffer
  size_t size; // buffer size
};

int voprintf_internel(struct outobj *out, const char *fmt, va_list ap) {
  int data = 0;
#define OUTCH(ch)                                                              \
  do {                                                                         \
    if (out->p) {                                                              \
      if (out->size > 0) {                                                     \
        *out->p++ = ch;                                                        \
        out->size--;                                                           \
      }                                                                        \
    } else {                                                                   \
      putch(ch);                                                               \
    }                                                                          \
    data++;                                                                    \
  } while (0)
  while (*fmt != '\0') {
    if (*fmt == '%') {
    parse_loop:
      fmt++;
      switch (*fmt) {
      case 'd': {
        int num = va_arg(ap, int);
        if (num < 0) {
          OUTCH('-');
          unsigned int unum = num;
          if (unum == -unum) {
            // int is int32_t
            OUTCH('2');
            OUTCH('1');
            OUTCH('4');
            OUTCH('7');
            OUTCH('4');
            OUTCH('8');
            OUTCH('3');
            OUTCH('6');
            OUTCH('4');
            OUTCH('8');
            break;
          }
          num = -num;
        }
        int len = 0;
        int tmp = num;
        do {
          buf[len++] = tmp % 10;
          tmp /= 10;
        } while (tmp);
        for (int i = len - 1; i >= 0; i--) {
          OUTCH(buf[i] + '0');
        }
        break;
      }
      case 's': {
        char *str = va_arg(ap, char *);
        while (*str != '\0') {
          OUTCH(*str);
          str++;
        }
        break;
      }
      case 'c': {
        char ch = va_arg(ap, int);
        OUTCH(ch);
        break;
      }
      case 'p':
        OUTCH('0');
        OUTCH('x');
#define SHIFT(a, b) (((a) >> (4 * (b))) & 0xf)
#define OUTNUM(val) OUTCH(val >= 10 ? val - 10 + 'a' : val + '0')
      case 'x': {
        unsigned int num = va_arg(ap, unsigned int);
        int i;
        if (num == 0) {
          OUTCH('0');
          break;
        }
        for (i = sizeof num * 2 - 1; i >= 0; i--) {
          if (SHIFT(num, i)) {
            OUTNUM(SHIFT(num, i));
            --i;
            break;
          }
        }
        for (; i >= 0; i--) OUTNUM(SHIFT(num, i));
#undef OUTNUM
        break;
      }
#undef SHIFT
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        goto parse_loop;
      default: {
        putch('`');
        putstr(fmt);
        putch('\'');
        putch('\n');
        panic("Not implemented (printf flag, see above)");
        break;
      }
      }
    } else {
      OUTCH(*fmt);
    }
#undef OUTCH
    fmt++;
  }
  return data;
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  struct outobj out = {.p = NULL};
  int res = voprintf_internel(&out, fmt, ap);
  va_end(ap);
  return res;
}

int vsprintf(char *str, const char *fmt, va_list ap) {
  struct outobj out = {.p =str, .size = -1};
  int res = voprintf_internel(&out, fmt, ap);
  *out.p = 0;
  return res;
}

int sprintf(char *str, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(str, fmt, ap);
  va_end(ap);
  return ret;
}

int snprintf(char *str, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  struct outobj out = {.p = str, .size = n > 0 ? n - 1 : 0};
  int ret = voprintf_internel(&out, fmt, ap);
  if (n > 0)
    *out.p = 0;
  va_end(ap);
  return ret;
}

int vsnprintf(char *str, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
// vim: fileencoding=utf-8
