////////////////////////////////////////////////////////////////////////////////
// BlueBasic
////////////////////////////////////////////////////////////////////////////////
//
// Authors:
//      Tim Wilkinson <tim.j.wilkinson@gmail.com>
//
//      Original TinyBasicPlus
//        Mike Field <hamster@snap.net.nz>
//	      Scott Lawrence <yorgle@gmail.com>
//

#define kVersion "v0.2"

// v0.2: 2014-08-01
//      Seems like a good place to declare 0.2. Work is complete enough to have a pre-flashed device which can
//      be connected to over BLE and then program, including setting up services and characteristics, and attaching
//      these to the general i/o on the chip.
//
// v0.1: 2014-07-10
//      Start of Blue Basic. Heavily modified from Tiny Basic Plus (https://github.com/BleuLlama/TinyBasicPlus).
//      The goal is to put a Basic interpreter onto the TI CC254x Bluetooth LE chip.

#include "os.h"


////////////////////////////////////////////////////////////////////////////////

#ifndef BLUEBASIC_MEM
#define kRamSize   2048 /* arbitrary */
#else // BLUEBASIC_MEM
#define kRamSize   (BLUEBASIC_MEM)
#endif // BLUEBASIC_MEM

////////////////////////////////////////////////////////////////////////////////
// ASCII Characters
#define CR	'\r'
#define NL	'\n'
#define WS_TAB	'\t'
#define WS_SPACE   ' '
#define SQUOTE  '\''
#define DQUOTE  '\"'
#define CTRLC	0x03
#define CTRLH	0x08

typedef short unsigned LINENUM;

enum
{
  ERROR_OK = 0,
  ERROR_GENERAL,
  ERROR_EXPRESSION,
  ERROR_DIV0,
  ERROR_OOM,
  ERROR_TOOBIG,
  ERROR_BADPIN,
};

static const char* const error_msgs[] =
{
  "OK",
  "Error",
  "Bad expression",
  "Divide by zero",
  "Out of memory",
  "Too big",
  "Bad pin",
};

static const char initmsg[]           = "BlueBasic " kVersion;
static const char memorymsg[]         = " bytes free.";

#ifdef TARGET_CC254X
#define VAR_TYPE    long int
#else
#define VAR_TYPE    int
#endif
#define VAR_SIZE    (4)


// Start enum at 128 so we get instant token values.
// By moving the command list to an enum, we can easily remove sections
// above and below simultaneously to selectively obliterate functionality.
enum
{
  // -----------------------
  // Keywords
  //

  KW_CONSTANT = 0x80,
  KW_LIST,
  KW_LOAD,
  KW_SAVE,
  KW_DLOAD,
  KW_DSAVE,
  KW_MEM,
  KW_NEW,
  KW_RUN,
  KW_NEXT,
  KW_LET,
  KW_IF,
  KW_ELIF,
  KW_ELSE,
  KW_FI,
  KW_GOTO,
  KW_GOSUB,
  KW_RETURN,
  KW_REM,
  KW_SLASHSLASH,
  KW_FOR,
  KW_PRINT,
  KW_REBOOT,
  KW_END,
  KW_DIM,
  KW_TIMER,
  KW_DELAY,
  KW_DELAYMICROSECONDS,
  KW_AUTORUN,
  KW_PIN_P0,
  KW_PIN_P1,
  KW_PIN_P2,
  KW_GATT,
  KW_ADVERT,
  KW_SCAN,
  KW_BTSET,
  KW_PINMODE,
  KW_ATTACHINTERRUPT,
  KW_DETACHINTERRUPT,
  KW_SPI,
  KW_ANALOGREFERENCE,
  KW_ANALOGRESOLUTION,
  KW_WIRE,

  // -----------------------
  // Operators
  //

  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_REM,
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_GE,
  OP_NE,
  OP_GT,
  OP_EQEQ,
  OP_EQ,
  OP_LE,
  OP_LT,
  OP_NE_BANG,
  OP_LSHIFT,
  OP_RSHIFT,
  OP_UMINUS,
  
  // -----------------------
  // Functions
  //
  
  FUNC_ABS,
  FUNC_LEN,
  FUNC_RND,
  FUNC_MILLIS,
  FUNC_BATTERY,
  FUNC_HEX,
  
  // -----------------------

  ST_TO,
  ST_STEP,
  TI_STOP,
  TI_REPEAT,
  PM_PULLUP,
  PM_PULLDOWN,
  PM_INPUT,
  PM_ADC,
  PM_OUTPUT,
  PM_RISING,
  PM_FALLING,
  PM_INTERNAL,
  PM_EXTERNAL,
  PM_TIMEOUT,
  BLE_ONREAD,
  BLE_ONWRITE,
  BLE_ONCONNECT,
  BLE_ONDISCOVER,
  BLE_SERVICE,
  BLE_CHARACTERISTIC,
  BLE_READ,
  BLE_WRITENORSP,
  BLE_WRITE,
  BLE_NOTIFY,
  BLE_INDICATE,
  BLE_GENERAL,
  BLE_LIMITED,
  BLE_MORE,
  BLE_NAME,
  BLE_CUSTOM,
  BLE_FUNC_BTGET,
  BLE_ACTIVE,
  BLE_DUPLICATES,
  SPI_TRANSFER,
  SPI_MSB,
  SPI_LSB,
  SPI_MASTER,
  SPI_SLAVE,
};

enum
{
  CO_TRUE = 1,
  CO_FALSE,
  CO_ON,
  CO_OFF,
  CO_YES,
  CO_NO,
  CO_HIGH,
  CO_LOW,
  CO_ADVERT_ENABLED,
  CO_MIN_CONN_INTERVAL,
  CO_MAX_CONN_INTERVAL,
  CO_SLAVE_LATENCY,
  CO_TIMEOUT_MULTIPLIER,
  CO_DEV_ADDRESS,
  CO_TXPOWER,
  CO_RXGAIN,
  CO_LIM_DISC_INT_MIN,
  CO_LIM_DISC_INT_MAX,
  CO_GEN_DISC_INT_MIN,
  CO_GEN_DISC_INT_MAX,
};

// Constant map (so far all constants are <= 16 bits)
static const VAR_TYPE constantmap[] =
{
  1, // TRUE
  0, // FALSE
  1, // ON
  0, // OFF
  1, // YES
  0, // NO
  1, // HIGH
  0, // LOW
  CO_ADVERT_ENABLED,
  BLE_MIN_CONN_INTERVAL,
  BLE_MAX_CONN_INTERVAL,
  BLE_SLAVE_LATENCY,
  BLE_TIMEOUT_MULTIPLIER,
  BLE_BD_ADDR,
  BLE_TXPOWER,
  BLE_RXGAIN,
  TGAP_LIM_DISC_ADV_INT_MIN,
  TGAP_LIM_DISC_ADV_INT_MAX,
  TGAP_GEN_DISC_ADV_INT_MIN,
  TGAP_GEN_DISC_ADV_INT_MAX,
};

//
// See http://en.wikipedia.org/wiki/Operator_precedence
//
static const unsigned char operator_precedence[] =
{
   4, // OP_ADD
   4, // OP_SUB
   3, // OP_MUL
   3, // OP_DIV
   3, // OP_REM
   8, // OP_AND
  10, // OP_OR
   9, // OP_XOR
   6, // OP_GE,
   7, // OP_NE,
   6, // OP_GT,
   7, // OP_EQEQ,
   7, // OP_EQ,
   6, // OP_LE,
   6, // OP_LT,
   7, // OP_NE_BANG,
   5, // OP_LSHIFT,
   5, // OP_RSHIFT,
   2, // OP_UMINUS
};

enum
{
  EXPR_NORMAL = 0,
  EXPR_BRACES,
  EXPR_COMMA,
};

// WIRE commands
enum
{
  WIRE_PIN = 1,
  WIRE_OUTPUT,
  WIRE_INPUT,
  WIRE_INPUT_PULLUP,
  WIRE_INPUT_PULLDOWN,
  WIRE_ADC,
  WIRE_HIGH,
  WIRE_LOW,
  WIRE_TIMEOUT,
  WIRE_OUTPUT_BYTES,
  WIRE_OUTPUT_SHORT,
  WIRE_INPUT_BYTES,
  WIRE_INPUT_INT,
  WIRE_INPUT_WAIT_SHORT,
};

#define WIRE_COUNT_TO_USEC(C) (((C) * 370) >> 8) // Measured: each loop count takes 1.42us
#define WIRE_USEC_TO_COUNT(U) (((U) << 8) / 370)

#include "keyword_tables.h"

// Interpreter exit statuses
enum
{
  IX_BYE,
  IX_PROMPT,
  IX_OUTOFMEMORY
};

typedef struct
{
  char frame_type;
  unsigned short frame_size;
} stack_header;

typedef struct stack_for_frame
{
  stack_header header;
  char for_var;
  VAR_TYPE terminal;
  VAR_TYPE step;
  unsigned char *txtpos;
} stack_for_frame;

typedef struct stack_gosub_frame
{
  stack_header header;
  unsigned char *txtpos;
} stack_gosub_frame;

typedef struct stack_event_frame
{
  stack_header header;
} stack_event_frame;

typedef struct stack_variable_frame
{
  stack_header header;
  char type;
  char name;
  char oflags;
  struct gatt_variable_ref* ble;
  // .... bytes ...
} stack_variable_frame;

enum
{
  STACK_GOSUB_FLAG = 'G',
  STACK_FOR_FLAG = 'F',
  STACK_VARIABLE_FLAG = 'V',
  STACK_EVENT_FLAG = 'E',
  STACK_SERVICE_FLAG = 'S',
  STACK_VARIABLE_NORMAL = 'N'
};

enum
{
  VAR_NORMAL   = 0x00,
  VAR_VARIABLE = 0x01,
  // These flags are used inside the special struct
  VAR_INT      = 0x02,
  VAR_DIM_BYTE = 0x04,
};

static __data unsigned char* txtpos;
static __data unsigned char* program_end;
static __data unsigned char error_num;
static __data unsigned char* variables_begin;
static __data unsigned char* sp;

static unsigned char* list_line;
static unsigned char* program_start;
static LINENUM linenum;

static unsigned char cache_name;
static stack_variable_frame* cache_frame;
static unsigned char* cache_ptr;
static stack_variable_frame normal_variable = { { STACK_VARIABLE_FLAG, 0 }, VAR_INT, 0, 0, NULL };

#define VARIABLE_INT_ADDR(F)    (((VAR_TYPE*)variables_begin) + ((F) - 'A'))
#define VARIABLE_INT_GET(F)     (*VARIABLE_INT_ADDR(F))
#define VARIABLE_INT_SET(F,V)   (*VARIABLE_INT_ADDR(F) = (V))
#define VARIABLE_FLAGS_GET(F)   (vname = (F) - 'A', (((*(variables_begin + 26 * VAR_SIZE + vname / 8)) >> (vname % 8)) & 0x01))
#define VARIABLE_FLAGS_SET(F,V) do { vname = (F) - 'A'; unsigned char* v = variables_begin + 26 * VAR_SIZE + vname / 8; *v = (*v & (255 - (1 << (vname % 8)))) | ((V) << (vname % 8)); } while(0)

#ifdef SIMULATE_PINS
static unsigned char P0DIR, P1DIR, P2DIR;
static unsigned char P0SEL, P1SEL, P2SEL;
static unsigned char P0INP, P1INP, P2INP;
static unsigned char P0, P1, P2;
static unsigned char APCFG, ADCH, ADCL, ADCCON1, ADCCON3;
static unsigned char PICTL, P0IEN, P1IEN, P2IEN;
static unsigned char IEN1, IEN2;
static unsigned char U0BAUD, U0GCR, U0CSR, U0DBUF;
static unsigned char U1BAUD, U1GCR, U1CSR, U1DBUF;
static unsigned char PERCFG;
#endif

static unsigned char spiChannel;
static unsigned char analogReference;
static unsigned char analogResolution = 0x30; // 14-bits

static void pin_write(unsigned char pin, unsigned char val);
static VAR_TYPE pin_read(unsigned char major, unsigned char minor);
static unsigned char pin_parse(void);
static void pin_wire_parse(void);
static void pin_wire(unsigned char* start, unsigned char* end);
#define PIN_MAJOR(P)  ((P) >> 4)
#define PIN_MINOR(P)  ((P) & 15)

static VAR_TYPE expression(unsigned char mode);
#define EXPRESSION_STACK_SIZE 8
#define EXPRESSION_QUEUE_SIZE 8

unsigned char  ble_adbuf[31];
unsigned char* ble_adptr;
unsigned char  ble_isadvert;

typedef struct stack_service_frame
{
  stack_header header;
  gattAttribute_t* attrs;
  LINENUM connect;
} stack_service_frame;

typedef struct gatt_variable_ref
{
  unsigned char var;
  gattAttribute_t* attrs;
  unsigned char* cfg;
  LINENUM read;
  LINENUM write;
} gatt_variable_ref;

#define INVALID_CONNHANDLE 0xFFFF

static short find_quoted_string(void);
static char ble_build_service(void);
static char ble_get_uuid(void);
static unsigned char ble_read_callback(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char* len, unsigned short offset, unsigned char maxlen);
static unsigned char ble_write_callback(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char len, unsigned short offset);
static void ble_notify_assign(gatt_variable_ref* vref);

static const unsigned char ble_primary_service_uuid[] = { 0x00, 0x28 };
static const unsigned char ble_characteristic_uuid[] = { 0x03, 0x28 };
static const unsigned char ble_characteristic_description_uuid[] = { 0x01, 0x29 };
static const unsigned char ble_client_characteristic_config_uuid[] = { 0x02, 0x29 };

static LINENUM servicestart;
static unsigned short servicecount;
static unsigned char ble_uuid[16];
static unsigned char ble_uuid_len;

static const gattServiceCBs_t ble_service_callbacks =
{
  ble_read_callback,
  ble_write_callback,
  NULL
};

//
// Skip whitespace
//
static inline void ignore_blanks(void)
{
  if (*txtpos == WS_SPACE)
  {
    txtpos++;
  }
}

//
// Tokenize the human readable command line into something easier, smaller and faster.
//  Note. The tokenized form must always be smaller than the human form otherwise this
//  will break because it overwrites the buffer as it goes along.
//
static void tokenize(void)
{
  unsigned char c;
  unsigned char* writepos;
  unsigned char* readpos;
  unsigned char* scanpos;
  const unsigned char* table;
  
  writepos = txtpos;
  scanpos = txtpos;
  for (;;)
  {
    readpos = scanpos;
    table = KEYWORD_TABLE(readpos);
    for (;;)
    {
      c = *readpos;
      if (c == SQUOTE || c == DQUOTE)
      {
        *writepos++ = c;
        readpos++;
        for (;;)
        {
          const char nc = *readpos++;
          *writepos++ = nc;
          if (nc == c)
          {
            break;
          }
          else if (nc == NL)
          {
            writepos--;
            readpos--;
            break;
          }
        }
        scanpos = readpos;
        break;
      }
      else if (c == NL)
      {
        *writepos = NL;
        return;
      }
      while (c == *table)
      {
        table++;
        c = *++readpos;
      }
      if (*table >= 0x80)
      {
        // Match found
        if (writepos > txtpos && writepos[-1] == WS_SPACE)
        {
          writepos--;
        }
        *writepos++ = *table;
        if (*table == KW_CONSTANT)
        {
          *writepos++ = table[1];
        }
        // Skip whitespace
        while (c = *readpos, c == WS_SPACE || c == WS_TAB)
        {
          readpos++;
        }
        scanpos = readpos;
        break;
      }
      else
      {
        // No match found
        // Move to next possibility
        while (*table++ < 0x80)
          ;
        if (table[-1] == KW_CONSTANT)
        {
          table++;
        }
        if (*table == 0)
        {
          // Not found
          // Advance until next likely token start
          c = *scanpos;
          if (c >= 'A' && c <= 'Z')
          {
            do
            {
              *writepos++ = c;
              c = *++scanpos;
            } while (c >= 'A' && c <= 'Z');
          }
          else if (c == WS_TAB || c == WS_SPACE)
          {
            if (writepos > txtpos && writepos[-1] != WS_SPACE)
            {
              *writepos++ = WS_SPACE;
            }
            do
            {
              c = *++scanpos;
            } while (c == WS_SPACE || c == WS_TAB);
          }
          else
          {
            *writepos++ = c;
            scanpos++;
          }
          break;
        }
        readpos = scanpos;
      }
    }
  }
}

//
// Print a number (base 10)
//
void printnum(VAR_TYPE num)
{
  VAR_TYPE size = 10;

  if (num < 0)
  {
    OS_putchar('-');
    num = -num;
  }
  for (; size <= num; size *= 10)
    ;
  for (size /= 10; size != 0; size /= 10)
  {
    OS_putchar('0' + num / size % 10);
  }
}

static void testlinenum(void)
{
  unsigned char ch;

  ignore_blanks();

  linenum = 0;
  for (ch = *txtpos; ch >= '0' && ch <= '9'; ch = *++txtpos)
  {
    // Trap overflows
    if (linenum >= 0xFFFF / 10)
    {
      linenum = 0xFFFF;
      break;
    }
    linenum = linenum * 10 + ch - '0';
  }
}

//
// Find the beginning and limit of a quoted string.
//  Returns the length, of -1 is invalid.
//
static short find_quoted_string(void)
{
  short i = 0;
  unsigned char delim = *txtpos;
  if (delim != '"' && delim != '\'')
  {
    return -1;
  }
  txtpos++;
  for(; txtpos[i] != delim; i++)
  {
    if (txtpos[i] == NL)
    {
      return -1;
    }
  }
  return i;
}

//
// Print the string between quotation marks.
//
static unsigned char print_quoted_string(void)
{
  short i = find_quoted_string();
  if (i == -1)
  {
    return 0;
  }
  else
  {
    // Print the characters
    for (; i; i--)
    {
      OS_putchar(*txtpos++);
    }
    txtpos++;

    return 1;
  }
}

//
// Print a message + newline
//
void printmsg(const char *msg)
{
  while (*msg != 0)
  {
    OS_putchar(*msg++);
  }
  OS_putchar(NL);
}

//
// Find the line nearest the given linenum.
//
static unsigned char* findline(void)
{
  unsigned char *line;

  for (line = program_start; ; line += line[sizeof(LINENUM)])
  {
    if (line == program_end || *(LINENUM*)line >= linenum)
    {
      return line;
    }
  }
}

//
// Find the line contain the current txtpos.
//
static unsigned char* findtxtline(void)
{
  unsigned char *line;
  unsigned char* nline;
  
  for (line = program_start; ; line = nline)
  {
    nline = line + line[sizeof(LINENUM)];
    if (line == program_end || (txtpos >= line && txtpos < nline))
    {
      return line;
    }
  }
}

//
// Print the current BASIC line. The line is tokenized so
// it is expanded as it's printed, including the addition of whitespace
// to make it more readable.
//
static void printline(unsigned char indent)
{
  LINENUM line_num;
  unsigned char lc = WS_SPACE;

  line_num = *(LINENUM *)list_line;
  list_line += sizeof(LINENUM) + sizeof(char);

  // Output the line */
  printnum(line_num);
  while (indent-- != 0)
  {
    OS_putchar(WS_SPACE);
  }
  for (unsigned char c = *list_line++; c != NL; c = *list_line++)
  {
    if (c < 0x80)
    {
      OS_putchar(c);
    }
    else
    {
      // Decode the token (which is a bit non-trival and slow)
      const unsigned char* begin;
      const unsigned char* ptr;
      const unsigned char** k;

      for (k = keywords; *k; k++)
      {
        begin = *k;
        ptr = begin;
        while (*ptr != 0)
        {
          while (*ptr < 0x80)
          {
            ptr++;
          }
          if (*ptr != c || (c == KW_CONSTANT && ptr[1] != list_line[0]))
          {
            if (*ptr == KW_CONSTANT)
            {
              ptr++;
            }
            begin = ++ptr;
          }
          else
          {
            if (c == KW_CONSTANT)
            {
              list_line++;
            }
            if (lc != WS_SPACE)
            {
              OS_putchar(WS_SPACE);
            }
            for (c = *begin++; c < 0x80; c = *begin++)
            {
              OS_putchar(c);
            }
            c = begin[-2];
            if (c != '(')
            {
              OS_putchar(WS_SPACE);
              c = WS_SPACE;
            }
            goto found;
          }
        }
      }
    }
found:
    lc = c;
  }
  OS_putchar(NL);
}

//
// Parse a string into an integer.
// We limit the number of characters scanned and specific a base (upto 16)
//
static VAR_TYPE parse_int(unsigned char maxlen, unsigned char base)
{
  VAR_TYPE v = 0;

  while (maxlen-- > 0)
  {
    char ch = *txtpos++;
    if (ch >= '0' && ch <= '9')
    {
      ch -= '0';
    }
    else if (ch >= 'A' && ch <= 'F')
    {
      ch -= 'A' - 10;
    }
    else if (ch >= 'a' && ch <= 'f')
    {
      ch -= 'a' - 10;
    }
    else
    {
error:
      error_num = ERROR_EXPRESSION;
      txtpos--;
      break;
    }
    if (ch >= base)
    {
      goto error;
    }
    v = v * base + ch;
  }
  return v;
}

//
// Find the information on the stack about the given variable. Return the info in
// the passed argument, and return the address of the variable.
// This method handles the complexites of finding variables which are on the stack (DIMs)
// as well as the simple, preset, variables. Regardless, we return appropriate information
// so the caller doesn't need to worry about where the variable is stored.
//
static unsigned char* get_variable_frame(char name, stack_variable_frame** frame)
{
  unsigned char* tsp;
  unsigned char vname;

  if (name == cache_name)
  {
    *frame = cache_frame;
    return cache_ptr;
  }
  if (VARIABLE_FLAGS_GET(name) == VAR_VARIABLE)
  {
    for (tsp = sp; tsp < variables_begin; tsp += ((stack_header*)tsp)->frame_size)
    {
      if (((stack_header*)tsp)->frame_type == STACK_VARIABLE_FLAG && ((stack_variable_frame*)tsp)->name == name)
      {
        *frame = (stack_variable_frame*)tsp;
        if (((stack_variable_frame*)tsp)->type == VAR_INT)
        {
          return (unsigned char*)VARIABLE_INT_ADDR(name);
        }
        else
        {
          cache_name = name;
          cache_frame = (stack_variable_frame*)tsp;
          cache_ptr = tsp + sizeof(stack_variable_frame);
          return cache_ptr;
        }
      }
    }
  }
  *frame = &normal_variable;
  return (unsigned char*)VARIABLE_INT_ADDR(name);
}

//
// Create an array
//
void create_dim(unsigned char name, VAR_TYPE size, unsigned char* data)
{
  unsigned char vname;
  
  if (sp - sizeof(stack_variable_frame) - size < program_end)
  {
    error_num = ERROR_OOM;
  }
  else
  {
    sp -= sizeof(stack_variable_frame) + size;
    stack_variable_frame *f = (stack_variable_frame *)sp;
    f->header.frame_type = STACK_VARIABLE_FLAG;
    f->header.frame_size = sizeof(stack_variable_frame) + size;
    f->type = VAR_DIM_BYTE;
    f->name = name;
    f->oflags = VARIABLE_FLAGS_GET(name);
    f->ble = NULL;
    VARIABLE_FLAGS_SET(name, VAR_VARIABLE);
    if (data)
    {
      OS_memcpy(sp + sizeof(stack_variable_frame), data, size);
    }
    else
    {
      OS_memset(sp + sizeof(stack_variable_frame), 0, size);
    }
    cache_name = 0;
  }
}

// -------------------------------------------------------------------------------------------
//
// Expression evaluator
//
// -------------------------------------------------------------------------------------------

static VAR_TYPE* expression_operate(unsigned char op, VAR_TYPE* queueptr)
{
  if (op == OP_UMINUS)
  {
    queueptr[-1] = -queueptr[-1];
  }
  else
  {
    queueptr--;
    switch (op)
    {
      case OP_ADD:
        queueptr[-1] += queueptr[0];
        break;
      case OP_SUB:
        queueptr[-1] -= queueptr[0];
        break;
      case OP_MUL:
        queueptr[-1] *= queueptr[0];
        break;
      case OP_DIV:
        if (queueptr[0] == 0)
        {
          goto expr_div0;
        }
        queueptr[-1] /= queueptr[0];
        break;
      case OP_REM:
        if (queueptr[0] == 0)
        {
          goto expr_div0;
        }
        queueptr[-1] %= queueptr[0];
        break;
      case OP_AND:
        queueptr[-1] &= queueptr[0];
        break;
      case OP_OR:
        queueptr[-1] |= queueptr[0];
        break;
      case OP_XOR:
        queueptr[-1] ^= queueptr[0];
        break;
      case OP_GE:
        queueptr[-1] = queueptr[-1] >= queueptr[0];
        break;
      case OP_NE:
      case OP_NE_BANG:
        queueptr[-1] = queueptr[-1] != queueptr[0];
        break;
      case OP_GT:
        queueptr[-1] = queueptr[-1] > queueptr[0];
        break;
      case OP_EQEQ:
      case OP_EQ:
        queueptr[-1] = queueptr[-1] == queueptr[0];
        break;
      case OP_LE:
        queueptr[-1] = queueptr[-1] <= queueptr[0];
        break;
      case OP_LT:
        queueptr[-1] = queueptr[-1] < queueptr[0];
        break;
      case OP_LSHIFT:
        queueptr[-1] <<= queueptr[0];
        break;
      case OP_RSHIFT:
        queueptr[-1] >>= queueptr[0];
        break;
      default:
        error_num = ERROR_EXPRESSION;
        return NULL;
    }
  }
  return queueptr;
expr_div0:
  error_num = ERROR_DIV0;
  return NULL;
}

static VAR_TYPE expression(unsigned char mode)
{
  VAR_TYPE queue[EXPRESSION_QUEUE_SIZE];
  unsigned char stack[EXPRESSION_STACK_SIZE];
  unsigned char lastop = 1;

  // Done parse if we have a pending error
  if (error_num)
  {
    return 0;
  }
  
  VAR_TYPE* queueptr = queue;
  unsigned char* stackptr = stack;

  for (;;)
  {
    unsigned char op = *txtpos++;
    switch (op)
    {
      // Ignore whitespace
      case WS_SPACE:
        continue;

      case NL:
        txtpos--;
        goto done;
      
      default:
        // Parse number
        if (op >= '0' && op <= '9')
        {
          if (queueptr == &queue[EXPRESSION_QUEUE_SIZE])
          {
            goto expr_oom;
          }
          txtpos--;
          *queueptr++ = parse_int(255, 10);
          error_num = ERROR_OK;
          lastop = 0;
        }
        else if (op >= 'A' && op <= 'Z')
        {
          stack_variable_frame* frame;
          unsigned char* ptr = get_variable_frame(op, &frame);
          
          if (frame->type == VAR_DIM_BYTE)
          {
            if (stackptr >= &stack[EXPRESSION_STACK_SIZE - 1])
            {
              goto expr_oom;
            }
            *stackptr++ = op;
            lastop = 1;
          }
          else
          {
            if (queueptr == &queue[EXPRESSION_QUEUE_SIZE])
            {
              goto expr_oom;
            }
            *queueptr++ = *(VAR_TYPE*)ptr;
            lastop = 0;
          }
        }
        else
        {
          txtpos--;
          goto done;
        }
        break;
        
      case KW_CONSTANT:
        if (queueptr == &queue[EXPRESSION_QUEUE_SIZE])
        {
          goto expr_oom;
        }
        *queueptr++ = constantmap[*txtpos++ - CO_TRUE];
        lastop = 0;
        break;

      case FUNC_HEX:
        if (queueptr == &queue[EXPRESSION_QUEUE_SIZE])
        {
          goto expr_oom;
        }
        *queueptr++ = parse_int(255, 16);
        error_num = ERROR_OK;
        lastop = 0;
        break;

      case FUNC_MILLIS:
        ignore_blanks();
        if (*txtpos++ != ')')
        {
          goto expr_error;
        }
        if (queueptr == &queue[EXPRESSION_QUEUE_SIZE])
        {
          goto expr_oom;
        }
        *queueptr++ = (VAR_TYPE)OS_millis();
        lastop = 0;
        break;

      case FUNC_BATTERY:
      {
        VAR_TYPE top;

        ignore_blanks();
        if (*txtpos++ != ')')
        {
          goto expr_error;
        }
        if (queueptr == &queue[EXPRESSION_QUEUE_SIZE])
        {
          goto expr_oom;
        }
        ADCCON3 = 0x0F | 0x10 | 0x00; // VDD/3, 10-bit, internal voltage reference
#ifdef SIMULATE_PINS
        ADCCON1 = 0x80;
#endif
        while ((ADCCON1 & 0x80) == 0)
          ;
        top = ADCL;
        top |= ADCH << 8;
        top = top >> 6;
        // VDD can be in the range 2v to 3.6v. Internal reference voltage is 1.24v (per datasheet)
        // So we're measuring VDD/3 against 1.24v giving us (VDD x 511) / 3.72
        // or VDD = (ADC * 3.72 / 511). x 1000 to get result in mV.
        *queueptr++ = (top * 3720L) / 511L;
        lastop = 0;
        break;
      }
        
      case FUNC_LEN:
      {
        unsigned char ch = *txtpos;
        if (ch < 'A' || ch > 'Z' || txtpos[1] != ')')
        {
          goto expr_error;
        }
        if (queueptr == &queue[EXPRESSION_QUEUE_SIZE])
        {
          goto expr_oom;
        }
        stack_variable_frame* frame;
        txtpos += 2;
        get_variable_frame(ch, &frame);
        if (frame->type != VAR_DIM_BYTE)
        {
          goto expr_error;
        }
        *queueptr++ = frame->header.frame_size - sizeof(stack_variable_frame);
        lastop = 0;
        break;
      }

      case '(':
        if (stackptr == &stack[EXPRESSION_STACK_SIZE])
        {
          goto expr_oom;
        }
        *stackptr++ = op;
        lastop = 1;
        break;

      case FUNC_ABS:
      case FUNC_RND:
      case KW_PIN_P0:
      case KW_PIN_P1:
      case KW_PIN_P2:
      case BLE_FUNC_BTGET:
        if (stackptr >= &stack[EXPRESSION_STACK_SIZE - 1])
        {
          goto expr_oom;
        }
        *stackptr++ = op;
        *stackptr++ = '(';
        lastop = 1;
        break;

      case ',':
        for (;;)
        {
          if (stackptr == stack)
          {
            if (mode == EXPR_COMMA)
            {
              goto done;
            }
            goto expr_error;
          }
          unsigned const op2 = *--stackptr;
          if (op2 == '(')
          {
            stackptr++;
            break;
          }
          if ((queueptr = expression_operate(op2, queueptr)) <= queue)
          {
            goto expr_error;
          }
        }
        lastop = 0;
        break;

      case ')':
      {
        for (;;)
        {
          if (stackptr == stack)
          {
            if (mode == EXPR_BRACES)
            {
              goto done;
            }
            goto expr_error;
          }
          unsigned const op2 = *--stackptr;
          if (op2 == '(')
          {
            break;
          }
          if ((queueptr = expression_operate(op2, queueptr)) <= queue)
          {
            goto expr_error;
          }
        }
        if (stackptr > stack)
        {
          if (queueptr == queue)
          {
            goto expr_error;
          }
          const VAR_TYPE top = queueptr[-1];
          op = *--stackptr;
          switch (op)
          {
            case FUNC_ABS:
              queueptr[-1] = top < 0 ? -top : top;
              break;
            case FUNC_RND:
              queueptr[-1] = OS_rand() % top;
              break;
            case KW_PIN_P0:
              queueptr[-1] = pin_read(0, top);
              break;
            case KW_PIN_P1:
              queueptr[-1] = pin_read(1, top);
              break;
            case KW_PIN_P2:
              queueptr[-1] = pin_read(2, top);
              break;
            case BLE_FUNC_BTGET:
              if (top & 0x8000)
              {
                goto expr_error; // Not supported
              }
              GAPRole_GetParameter(top, (unsigned long*)&queueptr[-1], 0, NULL);
              break;

            default:
              if (op >= 'A' && op <= 'Z')
              {
                stack_variable_frame* frame;
                unsigned char* ptr = get_variable_frame(op, &frame);

                if (frame->type != VAR_DIM_BYTE || top < 0 || top >= frame->header.frame_size - sizeof(stack_variable_frame))
                {
                  goto expr_error;
                }
                queueptr[-1] = ptr[top];
              }
              else
              {
                stackptr++;
              }
              break;
          }
          lastop = 1;
        }
        break;
      }

      case OP_SUB:
        if (lastop)
        {
          // Unary minus
          op = OP_UMINUS;
        }
        // Fall through
      case OP_ADD:
      case OP_MUL:
      case OP_DIV:
      case OP_REM:
      case OP_AND:
      case OP_OR:
      case OP_XOR:
      case OP_GE:
      case OP_NE:
      case OP_GT:
      case OP_EQEQ:
      case OP_EQ:
      case OP_LE:
      case OP_LT:
      case OP_NE_BANG:
      case OP_LSHIFT:
      case OP_RSHIFT:
      {
        const unsigned char op1precedence = operator_precedence[op - OP_ADD];
        for (;;)
        {
          const unsigned char op2 = stackptr[-1] - OP_ADD;
          if (stackptr == stack || op2 >= sizeof(operator_precedence) || op1precedence < operator_precedence[op2])
          {
            break;
          }
          if ((queueptr = expression_operate(*--stackptr, queueptr)) <= queue)
          {
            goto expr_error;
          }
        }
        if (stackptr == &stack[EXPRESSION_STACK_SIZE])
        {
          goto expr_oom;
        }
        *stackptr++ = op;
        lastop = 1;
        break;
      }
    }
  }
done:
  while (stackptr > stack)
  {
    if ((queueptr = expression_operate(*--stackptr, queueptr)) <= queue)
    {
      goto expr_error;
    }
  }
  if (queueptr != &queue[1])
  {
    goto expr_error;
  }
  return queueptr[-1];
expr_error:
  if (!error_num)
  {
    error_num = ERROR_EXPRESSION;
  }
  return 0;
expr_oom:
  error_num = ERROR_OOM;
  return 0;
}

// -------------------------------------------------------------------------------------------
//
// The main interpreter
//
// -------------------------------------------------------------------------------------------

void interpreter_init()
{
  program_start = OS_malloc(kRamSize);
  OS_memset(program_start, 0, kRamSize);
  program_end = program_start;
  variables_begin = program_start + kRamSize - 26 * VAR_SIZE - 4; // 4 bytes = 32 bits of flags
  sp = variables_begin;
  interpreter_banner();
}

void interpreter_banner(void)
{
  printmsg(initmsg);
  printnum((VAR_TYPE)(sp - program_end));
  printmsg(memorymsg);
  printmsg(error_msgs[ERROR_OK]);
}

//
// Setup the interpreter.
//  Initialize everything we need to run the interpreter, and process any
//  'autorun' which may have been set.
//
void interpreter_setup(void)
{
  OS_init();
  interpreter_init();
  OS_prompt_buffer(program_end + sizeof(LINENUM), sp);
  
  if (OS_autorun_get())
  {
    short len = OS_file_open(0, 'r');
    if (len > 0)
    {
      program_end = program_start + len;
      OS_file_read(program_start, len);
    }
    OS_file_close();
    if (program_end > program_start)
    {
      OS_timer_start(DELAY_TIMER, OS_AUTORUN_TIMEOUT, 0, *(LINENUM*)program_start);
    }
    OS_prompt_buffer(program_end + sizeof(LINENUM), sp);
  }
}

//
// Run the main interpreter while we have new commands to execute
//
void interpreter_loop(void)
{
  while (OS_prompt_available())
  {
    interpreter_run(0, 0);
    OS_prompt_buffer(program_end + sizeof(LINENUM), sp);
  }
}

//
// The main interpreter engine
//
unsigned char interpreter_run(LINENUM gofrom, unsigned char canreturn)
{
  error_num = ERROR_OK;

  if (gofrom)
  {
    stack_event_frame *f;

    linenum = gofrom;
    if (canreturn)
    {
      if (sp - sizeof(stack_event_frame) < program_end)
      {
        goto qoom;
      }
      
      sp -= sizeof(stack_event_frame);
      f = (stack_event_frame *)sp;
      f->header.frame_type = STACK_EVENT_FLAG;
      f->header.frame_size = sizeof(stack_event_frame);
    }
    txtpos = findline() + sizeof(LINENUM) + sizeof(char);
    if (txtpos >= program_end)
    {
      goto print_error_or_ok;
    }
    goto interperate;
  }

  txtpos = program_end + sizeof(LINENUM);
  tokenize();

  {
    unsigned char linelen;
    unsigned char* start;

    // Move it to the end of program_memory
    // Find the end of the freshly entered line
    for(linelen = 0; txtpos[linelen++] != NL;)
      ;
    OS_rmemcpy(sp - linelen, txtpos, linelen);
    txtpos = sp - linelen;

    // Now see if we have a line number
    testlinenum();
    ignore_blanks();
    if (linenum == 0)
    {
      goto direct;
    }
    if (linenum == 0xFFFF)
    {
      goto qwhat;
    }

    // Allow space for line header
    txtpos -= sizeof(LINENUM) + sizeof(char);

    // Calculate the length of what is left, including the (yet-to-be-populated) line header
    linelen = sp - txtpos;

    // Now we have the number, add the line header.
    *((LINENUM*)txtpos) = linenum;
    txtpos[sizeof(LINENUM)] = linelen;

    // Merge it into the rest of the program
    start = findline();

    // If a line with that number exists, then remove it
    if (start != program_end && *((LINENUM *)start) == linenum)
    {
      unsigned char olinelen = start[sizeof(LINENUM)];
      program_end -= olinelen;
      OS_memcpy(start, start + olinelen, program_end - start);
    }

    if (txtpos[sizeof(LINENUM) + sizeof(char)] == NL) // If the line has no txt, it was just a delete
    {
      return IX_PROMPT;
    }

    // Make room for new line if we can
    if ((unsigned short)(txtpos - program_end) < linelen)
    {
      return IX_OUTOFMEMORY;
    }

    // Move program up to make space
    OS_rmemcpy(start + linelen, start, program_end - start);
    // Insert new line
    OS_memcpy(start, txtpos, linelen);
    program_end += linelen;
  }

  return IX_PROMPT;

// -- Commands ---------------------------------------------------------------

direct:
  txtpos = program_end + sizeof(LINENUM);
  if (*txtpos == NL)
  {
    return IX_PROMPT;
  }
  else
  {
    goto interperate;
  }

// ---------------------------------------------------------------------------

execnextline:
  while (*txtpos != NL)
  {
    txtpos++;
  }
  // Fall through ...
  
run_next_statement:
  if (*txtpos != NL)
  {
    error_num = ERROR_GENERAL;
    goto print_error_or_ok;
  }
  txtpos += sizeof(LINENUM) + sizeof(char) + sizeof(char);
  if (txtpos >= program_end) // Out of lines to run
  {
    goto print_error_or_ok;
  }
interperate:
  switch (*txtpos++)
  {
    default:
      if (*--txtpos < 0x80)
      {
        goto assignment;
      }
      break;
    case KW_CONSTANT:
      goto qwhat;
    case KW_LIST:
      goto list;
    case KW_LOAD:
      goto load;
    case KW_SAVE:
      goto save;
    case KW_DLOAD:
      goto cmd_dload;
    case KW_DSAVE:
      goto cmd_dsave;
    case KW_MEM:
      goto mem;
    case KW_NEW:
      if (*txtpos != NL)
      {
        goto qwhat;
      }
      program_end = program_start;
      // Fall through
    case KW_RUN:
      while (sp < variables_begin)
      {
        if (((stack_header*)sp)->frame_type == STACK_SERVICE_FLAG)
        {
          gattAttribute_t* attr;
          GATTServApp_DeregisterService(((stack_service_frame*)sp)->attrs[0].handle, &attr);
        }
        sp += ((stack_header*)sp)->frame_size;
      }
      txtpos = program_start + sizeof(LINENUM) + sizeof(char);
      if (txtpos >= program_end)
      {
        goto print_error_or_ok;
      }
      goto interperate;
    case KW_NEXT:
      goto next;
    case KW_LET:
      goto assignment;
    case KW_IF:
    case KW_ELIF:
      goto cmd_elif;
    case KW_ELSE:
      goto cmd_else;
    case KW_FI:
      goto run_next_statement;
    case KW_GOTO:
      linenum = expression(EXPR_NORMAL);
      if (error_num || *txtpos != NL)
      {
        goto qwhat;
      }
      txtpos = findline() + sizeof(LINENUM) + sizeof(char);
      if (txtpos >= program_end)
      {
        goto print_error_or_ok;
      }
      goto interperate;
    case KW_GOSUB:
      goto cmd_gosub;
    case KW_RETURN:
      goto gosub_return;
    case KW_REM:
    case KW_SLASHSLASH:
      goto execnextline;	// Ignore line completely
    case KW_FOR:
      goto forloop;
    case KW_PRINT:
      goto print;
    case KW_REBOOT:
      OS_reboot();
    case KW_END:
      if (*txtpos != NL)
      {
        goto qwhat;
      }
      goto print_error_or_ok;
    case KW_DIM:
      goto cmd_dim;
    case KW_TIMER:
      goto cmd_timer;
    case KW_DELAY:
      goto cmd_delay;
    case KW_DELAYMICROSECONDS:
      goto cmd_delaymicroseconds;
    case KW_AUTORUN:
      goto cmd_autorun;
    case KW_PIN_P0:
    case KW_PIN_P1:
    case KW_PIN_P2:
      txtpos--;
      goto assignpin;
    case KW_GATT:
      goto ble_gatt;
    case KW_ADVERT:
      ble_isadvert = 1;
      goto ble_advert;
    case KW_SCAN:
      ble_isadvert = 0;
      goto ble_scan;
    case KW_BTSET:
      goto cmd_btset;
    case KW_PINMODE:
      goto cmd_pinmode;
    case KW_ATTACHINTERRUPT:
      goto cmd_attachint;
    case KW_DETACHINTERRUPT:
      goto cmd_detachint;
    case KW_SPI:
      goto cmd_spi;
    case KW_ANALOGREFERENCE:
      goto cmd_analogref;
    case KW_ANALOGRESOLUTION:
      goto cmd_analogresolution;
    case KW_WIRE:
      goto cmd_wire;
  }
  goto qwhat;

// -- Errors -----------------------------------------------------------------
  
qoom:
  error_num = ERROR_OOM;
  goto print_error_or_ok;
  
qtoobig:
  error_num = ERROR_TOOBIG;
  goto print_error_or_ok;
  
qbadpin:
  error_num = ERROR_BADPIN;
  goto print_error_or_ok;

qwhat:
  if (!error_num)
  {
    error_num = ERROR_GENERAL;
  }
  // Fall through ...

print_error_or_ok:
  printmsg(error_msgs[error_num]);
  if (txtpos < program_end && error_num != ERROR_OK)
  {
    list_line = findtxtline();
    OS_putchar('>');
    OS_putchar('>');
    OS_putchar(WS_SPACE);
    printline(1);
    OS_putchar(NL);
  }
  return error_num == ERROR_OOM ? IX_OUTOFMEMORY : IX_PROMPT;
  

// --- Commands --------------------------------------------------------------

  VAR_TYPE val;

cmd_elif:
    val = expression(EXPR_NORMAL);
    if (error_num || *txtpos != NL)
    {
      goto qwhat;
    }
    if (val)
    {
      goto run_next_statement;
    }
    else
    {
      unsigned char nest = 0;
      txtpos++;
      for (;;)
      {
        if (txtpos >= program_end)
        {
          printmsg(error_msgs[ERROR_OK]);
          return IX_PROMPT;
        }
        switch (txtpos[sizeof(LINENUM) + sizeof(char)])
        {
          case KW_IF:
            nest++;
            break;
          case KW_ELSE:
            if (!nest)
            {
              txtpos += sizeof(LINENUM) + sizeof(char) + 1;
              goto run_next_statement;
            }
            break;
          case KW_ELIF:
            if (!nest)
            {
              txtpos += sizeof(LINENUM) + sizeof(char);
              goto interperate;
            }
            break;
          case KW_FI:
            if (!nest)
            {
              txtpos += sizeof(LINENUM) + sizeof(char) + 1;
              goto run_next_statement;
            }
            nest--;
            break;
          default:
            break;
        }
        txtpos += txtpos[sizeof(LINENUM)];
      }
    }
 
cmd_else:
  {
    unsigned char nest = 0;
    ignore_blanks();
    if (*txtpos != NL)
    {
      goto qwhat;
    }
    txtpos++;
    for (;;)
    {
      if (txtpos >= program_end)
      {
        printmsg(error_msgs[ERROR_OK]);
        return IX_PROMPT;
      }
      if (txtpos[sizeof(LINENUM) + sizeof(char)] == KW_IF)
      {
        nest++;
      }
      else if (txtpos[sizeof(LINENUM) + sizeof(char)] == KW_FI)
      {
        if (!nest)
        {
          txtpos += sizeof(LINENUM) + sizeof(char) + 1;
          goto run_next_statement;
        }
        nest--;
      }
      txtpos +=	txtpos[sizeof(LINENUM)];
    }
  }

forloop:
  {
    unsigned char var;
    VAR_TYPE initial;
    VAR_TYPE step;
    VAR_TYPE terminal;
    stack_for_frame *f;

    if (*txtpos < 'A' || *txtpos > 'Z')
    {
      goto qwhat;
    }
    var = *txtpos++;
    ignore_blanks();
    if (*txtpos != OP_EQ)
    {
      goto qwhat;
    }
    txtpos++;

    initial = expression(EXPR_NORMAL);
    if (error_num)
    {
      goto qwhat;
    }

    if (*txtpos++ != ST_TO)
    {
      goto qwhat;
    }

    terminal = expression(EXPR_NORMAL);
    if (error_num)
    {
      goto qwhat;
    }

    if (*txtpos++ == ST_STEP)
    {
      step = expression(EXPR_NORMAL);
      if (error_num)
      {
        goto qwhat;
      }
    }
    else
    {
      txtpos--;
      step = 1;
    }
    ignore_blanks();
    if (*txtpos != NL)
    {
      goto qwhat;
    }

    if (sp - sizeof(stack_for_frame) < program_end)
    {
      goto qoom;
    }

    sp -= sizeof(stack_for_frame);
    f = (stack_for_frame *)sp;
    VARIABLE_INT_SET(var, initial);
    f->header.frame_type = STACK_FOR_FLAG;
    f->header.frame_size = sizeof(stack_for_frame);
    f->for_var = var;
    f->terminal = terminal;
    f->step = step;
    f->txtpos = txtpos;
    goto run_next_statement;
  }

cmd_gosub:
  {
    stack_gosub_frame *f;

    linenum = expression(EXPR_NORMAL);
    if (error_num || *txtpos != NL)
    {
      goto qwhat;
    }
    if (sp + sizeof(stack_gosub_frame) < program_end)
    {
      goto qoom;
    }
    sp -= sizeof(stack_gosub_frame);
    f = (stack_gosub_frame *)sp;
    f->header.frame_type = STACK_GOSUB_FLAG;
    f->header.frame_size = sizeof(stack_gosub_frame);
    f->txtpos = txtpos;
    txtpos = findline() + sizeof(LINENUM) + sizeof(char);
    if (txtpos >= program_end)
    {
      goto print_error_or_ok;
    }
    goto interperate;
  }
  
next:
  // Find the variable name
  if (*txtpos < 'A' || *txtpos > 'Z' || txtpos[1] != NL)
  {
    goto qwhat;
  }

gosub_return:
  // Now walk up the stack frames and find the frame we want, if present
  while (sp < variables_begin)
  {
    switch (((stack_header*)sp)->frame_type)
    {
      case STACK_GOSUB_FLAG:
        if (txtpos[-1] == KW_RETURN)
        {
          stack_gosub_frame *f = (stack_gosub_frame *)sp;
          txtpos = f->txtpos;
          sp += f->header.frame_size;
          goto run_next_statement;
        }
        // This is not the loop you are looking for... so walk back up the stack
        break;
      case STACK_EVENT_FLAG:
        if (txtpos[-1] == KW_RETURN)
        {
          sp += ((stack_header*)sp)->frame_size;
          return IX_PROMPT;
        }
        break;
      case STACK_FOR_FLAG:
        // Flag, Var, Final, Step
        if (txtpos[-1] == KW_NEXT)
        {
          stack_for_frame *f = (stack_for_frame *)sp;
          // Is the the variable we are looking for?
          if (*txtpos == f->for_var)
          {
            VAR_TYPE v = VARIABLE_INT_GET(f->for_var) + f->step;
            VARIABLE_INT_SET(f->for_var, v);
            // Use a different test depending on the sign of the step increment
            if ((f->step > 0 && v <= f->terminal) || (f->step < 0 && v >= f->terminal))
            {
              // We have to loop so don't pop the stack
              txtpos = f->txtpos;
            }
            else
            {
              // We've run to the end of the loop. drop out of the loop, popping the stack
              sp += f->header.frame_size;
              txtpos++;
            }
            goto run_next_statement;
          }
        }
        // This is not the loop you are looking for... so Walk back up the stack
        break;
      case STACK_VARIABLE_FLAG:
      {
          unsigned char vname;
          stack_variable_frame* f = (stack_variable_frame*)sp;
          VARIABLE_FLAGS_SET(f->name, f->oflags);
          cache_name = 0;
        }
        break;
      case STACK_SERVICE_FLAG:
        {
          gattAttribute_t* attr;
          GATTServApp_DeregisterService(((stack_service_frame*)sp)->attrs[0].handle, &attr);
        }
        break;
      default:
        goto qoom;
    }
    sp += ((stack_header*)sp)->frame_size;
  }
  // Didn't find the variable we've been looking for
  goto qwhat;

//
// PX(Y) = Z
// Assign a value to a pin.
//
assignpin:
  {
    unsigned char pin;
    
    pin = pin_parse();
    if (error_num)
    {
      goto qwhat;
    }
    ignore_blanks();
    if (*txtpos != OP_EQ)
    {
      goto qwhat;
    }
    txtpos++;
    val = expression(EXPR_NORMAL);
    if (error_num)
    {
      goto qwhat;
    }
    pin_write(pin, val);
    
    goto run_next_statement;
  }

//
// X = Y
// Assign a value to a variable.
//
assignment:
  {
    unsigned char var;
    stack_variable_frame* frame;
    unsigned char* ptr;
    VAR_TYPE idx;

    var = *txtpos;
    if (var < 'A' || var > 'Z')
    {
      goto qwhat;
    }
    txtpos++;

    ptr = get_variable_frame(var, &frame);
    
    switch (frame->type)
    {
      case VAR_INT:
        if (*txtpos != OP_EQ)
        {
          goto qwhat;
        }
        txtpos++;
        val = expression(EXPR_NORMAL);
        if (error_num)
        {
          goto qwhat;
        }
        *(VAR_TYPE*)ptr = val;
        goto var_done;

      case VAR_DIM_BYTE:
        ignore_blanks();
        if (*txtpos != '(')
        {
          goto qwhat;
        }
        txtpos++;
        idx = expression(EXPR_BRACES);
        if (error_num)
        {
          goto qwhat;
        }
        if (idx < 0 || idx >= frame->header.frame_size - sizeof(stack_variable_frame) || *txtpos != OP_EQ)
        {
          goto qwhat;
        }
        txtpos++;
        val = expression(EXPR_NORMAL);
        if (error_num)
        {
          goto qwhat;
        }
        ptr[idx] = (unsigned char)val;
var_done:
        if (frame->ble)
        {
          ble_notify_assign(frame->ble);
        }
        goto run_next_statement;

      default:
        goto qwhat;
    }
  }

//
// LIST [<linenr>]
// List the current program starting at the optional line number.
//
list:
  {
    unsigned char indent = 1;

    testlinenum();

    // Should be EOL
    if (*txtpos != NL)
    {
      goto qwhat;
    }

    // Find the line
    list_line = findline();
    while(list_line != program_end)
    {
      switch (list_line[sizeof(LINENUM) + sizeof(char)])
      {
        case KW_ELSE:
        case KW_ELIF:
          indent--;
          // Fall through
        case KW_IF:
        case KW_FOR:
          printline(indent);
          indent++;
          break;
        case KW_NEXT:
        case KW_FI:
          indent--;
          // Fall through
        default:
          printline(indent);
          break;
      }
    }
  }
  goto run_next_statement;

print:
  for (;;)
  {
    if (*txtpos == NL)
    {
      break;
    }
    else if (!print_quoted_string())
    {
      if (*txtpos == '"' || *txtpos == '\'')
      {
        goto qwhat;
      }
      else if (*txtpos == ',' || *txtpos == WS_SPACE)
      {
        txtpos++;
      }
      else
      {
        VAR_TYPE e;
        e = expression(EXPR_COMMA);
        if (error_num)
        {
          goto qwhat;
        }
        printnum(e);
      }
    }
  }
  OS_putchar(NL);
  goto run_next_statement;

//
// MEM
// Print the current free memory.
//
mem:
  printnum((VAR_TYPE)(sp - program_end));
  printmsg(memorymsg);
  goto run_next_statement;

//
// LOAD
// Load any program in the persistent store back into memory.
//
load:
  {
    short len;
    // clear the program
    program_end = program_start;
    
    // load from a file into memory
    len = OS_file_open(0, 'r');
    if (len > 0)
    {
      program_end = program_start + len;
      OS_file_read(program_start, len);
    }
    OS_file_close();
    txtpos = program_end;
    goto print_error_or_ok;
  }

//
// SAVE
// Save the current program to persistent store.
// 
save:
  OS_file_open(0, 'w');
  OS_file_write(program_start, program_end - program_start);
  OS_file_close();
  txtpos = program_end;
  goto print_error_or_ok;

//
// DLOAD <1-15> <var>
// Load the value in the numbered store into <var> (which may be a DIM).
// The store persists across reboots/power-on/off events.
//
cmd_dload:
  {
    unsigned char chan;
    stack_variable_frame* vframe;
    unsigned char var;
    unsigned char* ptr;
    
    chan = expression(EXPR_NORMAL);
    if (error_num || chan == 0)
    {
      goto qwhat;
    }

    ignore_blanks();
    var = *txtpos++;
    if (var < 'A' || var > 'Z')
    {
      txtpos--;
      goto dsave_error;
    }
    if (*txtpos != WS_SPACE && *txtpos != NL)
    {
      goto dsave_error;
    }

    if (OS_file_open(chan, 'r') < 0)
    {
      goto qwhat;
    }
    ptr = get_variable_frame(var, &vframe);
    OS_file_read(ptr, vframe->type == VAR_DIM_BYTE ? vframe->header.frame_size - sizeof(stack_variable_frame) : VAR_SIZE);
    OS_file_close();

    goto run_next_statement;
  }

//
// DSAVE <1-15> <var>
// Save the value of <var> (which may be a DIM) to the numbered store. This
// store persists across reboots and power-off/on.
//
cmd_dsave:
  {
    unsigned char chan;
    stack_variable_frame* vframe;
    unsigned char var;
    unsigned char* ptr;

    chan = expression(EXPR_NORMAL);
    if (error_num || chan == 0)
    {
      goto qwhat;
    }

    ignore_blanks();
    var = *txtpos++;
    if (var < 'A' || var > 'Z')
    {
      txtpos--;
      goto dsave_error;
    }
    if (*txtpos != WS_SPACE && *txtpos != NL)
    {
      goto dsave_error;
    }

    if (OS_file_open(chan, 'w') < 0)
    {
      goto qwhat;
    }
    ptr = get_variable_frame(var, &vframe);
    OS_file_write(ptr, vframe->type == VAR_DIM_BYTE ? vframe->header.frame_size - sizeof(stack_variable_frame) : VAR_SIZE);
    OS_file_close();

    goto run_next_statement;
dsave_error:
    OS_file_close();
    goto qwhat;
  }

//
// DIM <var>(<size>)
// Converts a variable into an array of bytes of the given size.
//
cmd_dim:
  {
    VAR_TYPE size;
    unsigned char name;

    if (*txtpos < 'A' || *txtpos > 'Z')
    {
      goto qwhat;
    }
    name = *txtpos++;
    ignore_blanks();
    if (*txtpos != '(')
    {
      goto qwhat;
    }
    txtpos++;
    size = expression(EXPR_BRACES);
    if (error_num)
    {
      goto qwhat;
    }
    create_dim(name, size, NULL);
    if (error_num)
    {
      goto qwhat;
    }
  }
  goto run_next_statement;

//
// TIMER <timer number> <timeout ms> [REPEAT] GOSUB <linenum>
// Creates an optionally repeating timer which will call a specific subroutine everytime it fires.
//
cmd_timer:
  {
    unsigned char timer;
    VAR_TYPE timeout;
    unsigned char repeat = 0;
    LINENUM subline;
    
    timer = (unsigned char)expression(EXPR_COMMA);
    if (error_num)
    {
      goto qwhat;
    }
    ignore_blanks();
    if (*txtpos == TI_STOP)
    {
      txtpos++;
      OS_timer_stop(timer);
    }
    else
    {
      timeout = expression(EXPR_NORMAL);
      if (error_num)
      {
        goto qwhat;
      }
      ignore_blanks();
      if (*txtpos == TI_REPEAT)
      {
        txtpos++;
        repeat = 1;
      }
      if (*txtpos != KW_GOSUB)
      {
        goto qwhat;
      }
      txtpos++;
      subline = (LINENUM)expression(EXPR_NORMAL);
      if (error_num)
      {
        goto qwhat;
      }
      if (!OS_timer_start(timer, timeout, repeat, subline))
      {
        goto qwhat;
      }
    }
  }
  goto run_next_statement;

//
// DELAY <timeout>
// Pauses execution for timeout-ms 
//
cmd_delay:
  val = expression(EXPR_NORMAL);
  if (error_num || val < 0)
  {
    goto qwhat;
  }
  while (*txtpos++ != NL)
    ;
  OS_timer_start(DELAY_TIMER, val, 0, *(LINENUM*)txtpos);
  return IX_PROMPT;
  
cmd_delaymicroseconds:
  val = expression(EXPR_NORMAL);
  if (error_num || val < 0)
  {
    goto qwhat;
  }
  OS_delaymicroseconds(val);
  goto run_next_statement;

//
// AUTORUN ON|OFF
// When on, will load the save program at boot time and execute it.
//
cmd_autorun:
  {
    VAR_TYPE v = expression(EXPR_NORMAL);
    if (error_num)
    {
      goto qwhat;
    }
    OS_autorun_set(v ? 1 : 0);
  }
  goto run_next_statement;

cmd_pinmode:
  {
    unsigned char pinMode;
    unsigned char cmd[3];
    
    cmd[0] = WIRE_PIN;
    cmd[1] = pin_parse();
    if (error_num)
    {
      goto qwhat;
    }

    ignore_blanks();
    switch (*txtpos++)
    {
      case PM_INPUT:
        pinMode = *txtpos++;
        if (pinMode == NL)
        {
          txtpos--;
          cmd[2] = WIRE_INPUT;
        }
        else if (pinMode == PM_PULLUP)
        {
          cmd[2] = WIRE_INPUT_PULLUP;
        }
        else if (pinMode == PM_PULLDOWN)
        {
          cmd[2] = WIRE_INPUT_PULLDOWN;
        }
        else
        {
          txtpos--;
          goto qwhat;
        }
        break;
      case PM_ADC:
        if (PIN_MAJOR(cmd[1]) != 0)
        {
          goto qwhat;
        }
        cmd[2] = WIRE_ADC;
        break;
      case PM_OUTPUT:
        cmd[2] = WIRE_OUTPUT;
        break;
      default:
        txtpos--;
        goto qwhat;
    }
    
    pin_wire(cmd, cmd + sizeof(cmd));
    if (error_num)
    {
      goto qwhat;
    }
  }
  goto run_next_statement;

//
// ATTACHINTERRUPT <pin> RISING|FALLING GOSUB <linenr>
// Attach an interrupt handler to the given pin. When the pin either falls or rises, the specified
// subroutine will be called.
//
cmd_attachint:
  {
    unsigned short pin;
    LINENUM line;
    unsigned char falling = 0;
    unsigned char i;
    
    pin = pin_parse();
    if (error_num)
    {
      goto qwhat;
    }
    if (PIN_MAJOR(pin) == 2 && PIN_MINOR(pin) >= 4)
    {
      goto qbadpin;
    }
    ignore_blanks();
    switch (*txtpos++)
    {
      case PM_RISING:
        falling = 0;
        break;
      case PM_FALLING:
        falling = 1;
        break;
      default:
        txtpos--;
        goto qwhat;
    }
    if (*txtpos++ != KW_GOSUB)
    {
      txtpos--;
      goto qwhat;
    }
    line = expression(EXPR_NORMAL);
    if (error_num)
    {
      goto qwhat;
    }
    
    if (!OS_interrupt_attach(pin, line))
    {
      goto qbadpin;
    }

    i = 1 << PIN_MINOR(pin);
    if (PIN_MAJOR(pin) == 0)
    {
      PICTL = (PICTL & 0x01) | falling;
      P0IEN |= i;
      IEN1 |= 1 << 5;
    }
    else if (PIN_MAJOR(pin) == 1)
    {
      if (PIN_MINOR(pin) < 4)
      {
        PICTL = (PICTL & 0x02) | (falling << 1);
        P1IEN |= i;
        IEN2 |= 1 << 4;
      }
      else
      {
        PICTL = (PICTL & 0x04) | (falling << 2);
        P1IEN |= i;
        IEN2 |= 1 << 4;
      }
    }
    else
    {
      PICTL = (PICTL & 0x08) | (falling << 3);
      P2IEN |= i;
      IEN2 |= 1 << 1;
    }
  }
  goto run_next_statement;

//
// DETACHINTERRUPT <pin>
// Detach the interrupt handler from the pin.
//
cmd_detachint:
  {
    unsigned short pin;
    unsigned char i;
    
    pin = pin_parse();
    if (error_num)
    {
      goto qwhat;
    }
    
    i = ~(1 << PIN_MINOR(pin));
    if (PIN_MAJOR(pin) == 0)
    {
      P0IEN &= i;
      if (P0IEN == 0)
      {
        IEN1 &= ~(1 << 5);
      }
    }
    else if (PIN_MAJOR(pin) == 1)
    {
      P1IEN &= i;
      if (P1IEN == 0)
      {
        IEN2 &= ~(1 << 4);
      }
    }
    else
    {
      P2IEN &= i;
      if (P2IEN == 0)
      {
        IEN2 &= ~(1 << 1);
      }
    }

    if (!OS_interrupt_detach(pin))
    {
      goto qbadpin;
    }
    goto run_next_statement;
  }
  
//
// WIRE ([<pin>] OUTPUT|INPUT [PULLUP|PULLDOWN] HIGH|LOW [<array>|<value, value, ...>])*
//
cmd_wire:
  pin_wire_parse();
  if (error_num)
  {
    goto qwhat;
  }
  goto run_next_statement;

  unsigned char ch;
ble_gatt:
  switch(*txtpos++)
  {
    case BLE_SERVICE:
      servicestart = *(LINENUM*)(txtpos - 5); // <lineno:2> <len:1> GATT:1 SERVICE:1
      servicecount = 1;
      break;
    case BLE_READ:
    case BLE_WRITE:
    case BLE_WRITENORSP:
    case BLE_NOTIFY:
    case BLE_INDICATE:
      {
        servicecount++;
        for (;;)
        {
          switch (*txtpos++)
          {
            case BLE_NOTIFY:
            case BLE_INDICATE:
              servicecount++;
              break;
            case BLE_READ:
            case BLE_WRITE:
            case BLE_WRITENORSP:
              break;
            default:
              goto value_done;
          }
        }
value_done:
        ch = *--txtpos;
        if (ch >= 'A' && ch <= 'Z')
        {
          unsigned char vname;
          stack_variable_frame* vframe;
          get_variable_frame(ch, &vframe);
          if (vframe->type == STACK_VARIABLE_NORMAL)
          {
            if (sp - sizeof(stack_variable_frame) < program_end)
            {
              goto qoom;
            }
            sp -= sizeof(stack_variable_frame);
            vframe = (stack_variable_frame*)sp;
            vframe->header.frame_type = STACK_VARIABLE_FLAG;
            vframe->header.frame_size = sizeof(stack_variable_frame);
            vframe->type = VAR_INT;
            vframe->name = ch;
            vframe->oflags = VARIABLE_FLAGS_GET(ch);
            vframe->ble = NULL;
            VARIABLE_FLAGS_SET(ch, VAR_VARIABLE);
          }
        }
        else
        {
          goto qwhat;
        }
      }
      break;
    case BLE_CHARACTERISTIC:
      servicecount += 2; // NB. We include an extra for the description. Optimize later!
      break;
    case KW_END:
      switch (ble_build_service())
      {
        default:
          break;
        case 1:
          goto qwhat;
        case 2:
          goto qoom;
      }
      break;
    default:
      goto qwhat;
  }
  goto execnextline;

//
// SCAN <time> LIMITED|GENERAL [ACTIVE] [DUPLICATES] ONDISCOVER GOSUB <linenum>
//  or
// SCAN LIMITED|GENERAL|NAME "..."|CUSTOM "..."|END
//
ble_scan:
  if (*txtpos < 0x80)
  {
    unsigned char active = 0;
    unsigned char dups = 0;
    unsigned char mode = 0;

    val = expression(EXPR_NORMAL);
    if (error_num)
    {
      goto qwhat;
    }

    ignore_blanks();
    switch (*txtpos)
    {
      case BLE_GENERAL:
        mode |= 1;
        txtpos++;
        if (*txtpos == BLE_LIMITED)
        {
          mode |= 2;
          txtpos++;
        }
        break;
      case BLE_LIMITED:
        mode |= 2;
        if (*txtpos == BLE_GENERAL)
        {
          mode |= 1;
          txtpos++;
        }

        break;
      default:
        goto qwhat;
    }

    if (*txtpos == BLE_ACTIVE)
    {
      txtpos++;
      active = 1;
    }
    if (*txtpos == BLE_DUPLICATES)
    {
      txtpos++;
      dups = 1;
    }

    if (txtpos[0] != BLE_ONDISCOVER || txtpos[1] != KW_GOSUB)
    {
      goto qwhat;
    }
    txtpos += 2;

    linenum = expression(EXPR_NORMAL);
    if (error_num)
    {
      goto qwhat;
    }
    blueBasic_discover.linenum = linenum;
    GAPRole_SetParameter(TGAP_GEN_DISC_SCAN, val, 0, NULL);
    GAPRole_SetParameter(TGAP_LIM_DISC_SCAN, val, 0, NULL);
    GAPRole_SetParameter(TGAP_FILTER_ADV_REPORTS, !dups, 0, NULL);
    GAPObserverRole_CancelDiscovery();
    GAPObserverRole_StartDiscovery(mode, !!active, 0);
    goto run_next_statement;
  }
  // Fall through ...

ble_advert:
  if (!ble_adptr)
  {
    ble_adptr = ble_adbuf;
  }
  ch = *txtpos;
  switch (ch)
  {
    case BLE_LIMITED:
      if (ble_adptr + 3 > ble_adbuf + sizeof(ble_adbuf))
      {
        goto qtoobig;
      }
      *ble_adptr++ = 2;
      *ble_adptr++ = GAP_ADTYPE_FLAGS;
      *ble_adptr++ = GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED;
      txtpos++;
      break;
    case BLE_GENERAL:
      if (ble_adptr + 3 > ble_adbuf + sizeof(ble_adbuf))
      {
        goto qtoobig;
      }
      *ble_adptr++ = 2;
      *ble_adptr++ = GAP_ADTYPE_FLAGS;
      *ble_adptr++ = GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED;
      txtpos++;
      break;
    case BLE_NAME:
      {
        ch = *++txtpos;
        if (ch != SQUOTE && ch != DQUOTE)
        {
          goto qwhat;
        }
        else
        {
          short len;
          short alen;

          len = find_quoted_string();
          if (len == -1)
          {
            goto qwhat;
          }
          if (ble_adptr + 3 > ble_adbuf + sizeof(ble_adbuf))
          {
            goto qtoobig;
          }
          else
          {
            alen = ble_adbuf + sizeof(ble_adbuf) - ble_adptr - 2;
            if (alen > len)
            {
              *ble_adptr++ = len + 1;
              *ble_adptr++ = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
              alen = len;
            }
            else
            {
              *ble_adptr++ = alen + 1;
              *ble_adptr++ = GAP_ADTYPE_LOCAL_NAME_SHORT;
            }
            OS_memcpy(ble_adptr, txtpos, alen);
            ble_adptr += alen;
            GGS_SetParameter(GGS_DEVICE_NAME_ATT, len < GAP_DEVICE_NAME_LEN ? len : GAP_DEVICE_NAME_LEN, txtpos);
            txtpos += len + 1;
          }
        }
      }
      break;
    case BLE_CUSTOM:
      {
        unsigned char* lenptr;
        if (ble_adptr == NULL)
        {
          goto qwhat;
        }
        if (ble_adptr + 1 > ble_adbuf + sizeof(ble_adbuf))
        {
          goto qtoobig;
        }
        lenptr = ble_adptr++;

        for (;;)
        {
          ch = *++txtpos;
          if (ch == NL)
          {
            *lenptr = (unsigned char)(ble_adptr - lenptr - 1);
            break;
          }
          else if (ch == SQUOTE || ch == DQUOTE)
          {
            unsigned short v;
            txtpos++;
            for (;;)
            {
              ignore_blanks();
              v = parse_int(2, 16);
              if (error_num)
              {
                if (*txtpos == ch)
                {
                  error_num = ERROR_OK;
                  break;
                }
                goto qwhat;
              }
              if (ble_adptr + 1 > ble_adbuf + sizeof(ble_adbuf))
              {
                goto qtoobig;
              }
              *ble_adptr++ = (unsigned char)v;
            }
          }
          else if (ch >= 'A' && ch <= 'Z')
          {
            stack_variable_frame* frame;
            unsigned char* ptr;
            
            if (txtpos[1] != NL && txtpos[1] != WS_SPACE)
            {
              goto qwhat;
            }

            ptr = get_variable_frame(ch, &frame);
            if (frame->type != VAR_DIM_BYTE)
            {
              goto qwhat;
            }
            if (ble_adptr + frame->header.frame_size - sizeof(stack_variable_frame) > ble_adbuf + sizeof(ble_adbuf))
            {
              goto qtoobig;
            }
            for (ch = sizeof(stack_variable_frame); ch < frame->header.frame_size; ch++)
            {
              *ble_adptr++ = *ptr++;
            }
          }
          else if (ch != WS_SPACE)
          {
            goto qwhat;
          }
        }
      }
      break;
    case KW_END:
      if (ble_adptr == ble_adbuf)
      {
        ble_adptr = NULL;
        goto qwhat;
      }
      if (ble_isadvert)
      {
        GAPRole_SetParameter(_GAPROLE(BLE_ADVERT_DATA), 0, ble_adptr - ble_adbuf, ble_adbuf);
        GAPRole_SetParameter(_GAPROLE(BLE_ADVERT_ENABLED), 1, 0, NULL);
      }
      else
      {
        GAPRole_SetParameter(_GAPROLE(BLE_SCAN_RSP_DATA), 0, ble_adptr - ble_adbuf, ble_adbuf);
      }
      ble_adptr = NULL;
      txtpos++;
      break;
    default:
      if (ch != SQUOTE && ch != DQUOTE)
      {
        goto qwhat;
      }
      else
      {
        ch = ble_get_uuid();
        if (!ch)
        {
          goto qwhat;
        }
        if (ble_adptr + 2 + ble_uuid_len > ble_adbuf + sizeof(ble_adbuf))
        {
          goto qtoobig;
        }
        *ble_adptr++ = ble_uuid_len + 1;
        switch (ble_uuid_len)
        {
          case 2:
            *ble_adptr++ = GAP_ADTYPE_16BIT_COMPLETE;
            break;
          case 4:
            *ble_adptr++ = GAP_ADTYPE_32BIT_COMPLETE;
            break;
          case 16:
            *ble_adptr++ = GAP_ADTYPE_128BIT_COMPLETE;
            break;
          default:
            goto qwhat;
        }
        OS_memcpy(ble_adptr, ble_uuid, ble_uuid_len);
        ignore_blanks();
        if (*txtpos == BLE_MORE)
        {
          ble_adptr[-1] &= 0xFE;
          txtpos++;
        }
        ble_adptr += ble_uuid_len;
      }
      break;
  }
  goto run_next_statement;

//
// BTSET <paramater> <value>|<array>
//
cmd_btset:
  {
    unsigned short param;
    unsigned char* ptr;

    param = (unsigned short)expression(EXPR_NORMAL);
    if (error_num)
    {
      goto qwhat;
    }
    // Expects an array
    if (param & 0x8000)
    {
      stack_variable_frame* vframe;
      ptr = get_variable_frame(*txtpos, &vframe);
      if (vframe->type != VAR_DIM_BYTE)
      {
        goto qwhat;
      }
      if (GAPRole_SetParameter(_GAPROLE(param), 0, vframe->header.frame_size - sizeof(stack_variable_frame), ptr) != SUCCESS)
      {
        goto qwhat;
      }
    }
    // Expects an int
    else
    {
      val = expression(EXPR_NORMAL);
      if (error_num)
      {
        goto qwhat;
      }
      if (GAPRole_SetParameter(_GAPROLE(param), val, 0, NULL) != SUCCESS)
      {
        goto qwhat;
      }
    }
  }
  goto run_next_statement;

//
// SPI MASTER <port 0|1|2|3> <mode 0|1|2|3> LSB|MSB <speed>
//  or
// SPI TRANSFER Px(y) <array>
//
cmd_spi:
  {
    if (*txtpos == SPI_MASTER)
    {
      // Setup
      unsigned char port;
      unsigned char mode;
      unsigned char msblsb;
      unsigned char speed;

      txtpos++;
      port = expression(EXPR_COMMA);
      mode = expression(EXPR_COMMA);
      if (error_num || port > 3 || mode > 3)
      {
        goto qwhat;
      }
      msblsb = *txtpos;
      if (msblsb != SPI_MSB && msblsb != SPI_LSB)
      {
        goto qwhat;
      }
      txtpos++;
      speed = expression(EXPR_NORMAL);
      if (error_num)
      {
        goto qwhat;
      }

      mode = (mode << 6) | (msblsb == SPI_MSB ? 0x20 : 0x00);
      switch (speed)
      {
        case 1: // E == 15, M = 0
          mode |= 15;
          break;
        case 2: // E = 16
          mode |= 16;
          break;
        case 4: // E = 17
          mode |= 17;
          break;
        default:
          goto qwhat;
      }
      
      if ((port & 2) == 0)
      {
        spiChannel = 0;
        if ((port & 1) == 0)
        {
          P0SEL |= 0x2C;
        }
        else
        {
          P1SEL |= 0x2C;
        }
        PERCFG = (PERCFG & 0xFE) | (port & 1);
        U0BAUD = 0;
        U0GCR = mode;
        U0CSR = 0x00; // SPI mode, recv enable, SPI master
      }
      else
      {
        spiChannel = 1;
        if ((port & 1) == 0)
        {
          P0SEL |= 0x38;
        }
        else
        {
          P1SEL |= 0xE0;
        }
        PERCFG = (PERCFG & 0xFD) | ((port & 1) << 1);
        U1BAUD = 0;
        U1GCR = mode;
        U1CSR = 0x00;
      }
    }
    else if (*txtpos == SPI_TRANSFER)
    {
      // Transfer
      unsigned char pin;
      stack_variable_frame* vframe;
      unsigned char* ptr;
      unsigned short len;
      
      txtpos++;
      pin = pin_parse();
      if (error_num)
      {
        goto qwhat;
      }
      ignore_blanks();
      ch = *txtpos;
      if (ch < 'A' || ch > 'Z')
      {
        goto qwhat;
      }
      ptr = get_variable_frame(ch, &vframe);
      if (vframe->type != VAR_DIM_BYTE)
      {
        goto qwhat;
      }
      txtpos++;

      // .. transfer ..
      pin_write(pin, 0);
      if (spiChannel == 0)
      {
        for (len = vframe->header.frame_size - sizeof(stack_variable_frame); len; len--, ptr++)
        {
          U0CSR &= 0xF9; // Clear flags
          U0DBUF = *ptr;
#ifdef SIMULATE_PINS
          U0CSR |= 0x02;
#endif
          while ((U0CSR & 0x02) != 0x02)
            ;
          *ptr = U0DBUF;
        }
      }
      else
      {
        for (len = vframe->header.frame_size - sizeof(stack_variable_frame); len; len--, ptr++)
        {
          U1CSR &= 0xF9;
          U1DBUF = *ptr;
#ifdef SIMULATE_PINS
          U1CSR |= 0x02;
#endif
          while ((U1CSR & 0x02) != 0x02)
            ;
          *ptr = U1DBUF;
        }
      }
      pin_write(pin, 1);
    }
    else
    {
      goto qwhat;
    }
  }
  goto run_next_statement;

//
// ANALOGREFERENCE INTERNAL|EXTERNAL
//
cmd_analogref:
  switch (*txtpos)
  {
    case PM_INTERNAL:
      analogReference = 0x00;
      break;
    case PM_EXTERNAL:
      analogReference = 0x40;
      break;
    default:
      goto qwhat;
  }
  txtpos++;
  goto run_next_statement;

//
// ANALOGRESOLUTION 8|10|12|14
//  Set the number of bits returned from an ADC operation.
//
cmd_analogresolution:
  val = expression(EXPR_NORMAL);
  switch (val)
  {
    case 8:
      analogResolution = 0x00;
      break;
    case 10:
      analogResolution = 0x10;
      break;
    case 12:
      analogResolution = 0x20;
      break;
    case 14:
      analogResolution = 0x30;
      break;
    default:
      goto qwhat;
  }
  analogResolution = val;
  goto run_next_statement;
}

//
// Parse the immediate text into a PIN value.
// Pins are of the form PX(Y) and are returned as 0xXY
//
static unsigned char pin_parse(void)
{
  unsigned char major;
  unsigned char minor;
  
  major = *txtpos++;
  if (major < KW_PIN_P0 || major > KW_PIN_P2)
  {
    txtpos--;
    goto badpin;
  }
  minor = expression(EXPR_BRACES);
  if (error_num || minor > 7)
  {
    goto badpin;
  }
#ifdef ENABLE_DEBUG_INTERFACE
  if (major == KW_PIN_P2 && (minor == 1 || minor == 2))
  {
    goto badpin;
  }
#endif
#ifdef OS_UART_PORT
#if OS_UART_PORT == HAL_UART_PORT_1
  if (major == KW_PIN_P1 && minor >= 4)
  {
    goto badpin;
  }
#endif
#endif
  return ((major - KW_PIN_P0) << 4) | minor;
badpin:
  error_num = ERROR_BADPIN;
  return 0;
}

//
// Write pin
//
void pin_write(unsigned char pin, unsigned char val)
{
  unsigned char bit = 1 << PIN_MINOR(pin);

  switch (PIN_MAJOR(pin))
  {
    case 0:
      P0 = (P0 & ~bit) | (val ? bit : 0);
      break;
    case 1:
      P1 = (P1 & ~bit) | (val ? bit : 0);
      break;
    default:
      P2 = (P2 & ~bit) | (val ? bit : 0);
      break;
  }
}

//
// Read pin
//
static VAR_TYPE pin_read(unsigned char major, unsigned char minor)
{
  switch (major)
  {
    case 0:
      if (APCFG & (1 << minor))
      {
        VAR_TYPE val;
        ADCCON3 = minor | analogResolution | analogReference;
#ifdef SIMULATE_PINS
        ADCCON1 = 0x80;
#endif
        while ((ADCCON1 & 0x80) == 0)
          ;
        val = ADCL;
        val |= ADCH << 8;
        return val >> (8 - (analogResolution / 8));
      }
      return (P0 >> minor) & 1;
    case 1:
      return (P1 >> minor) & 1;
    default:
      return (P2 >> minor) & 1;
  }
}

//
// Execute a wire command. This manipulates a pin(s) input and output quickly
// and can be used for signalling devices.
//
static void pin_wire_parse(void)
{
  unsigned char* ptr = program_end;
  unsigned char input = 0;

  for (;;)
  {
    const unsigned char op = *txtpos++;
    switch (op)
    {
      case NL:
        txtpos--;
        pin_wire(program_end, ptr);
        return;
      case WS_SPACE:
      case ',':
        break;
      case KW_PIN_P0:
      case KW_PIN_P1:
      case KW_PIN_P2:
        txtpos--;
        *ptr++ = WIRE_PIN;
        *ptr++ = pin_parse();
        if (error_num)
        {
          goto wire_error;
        }
        break;
      case PM_OUTPUT:
        *ptr++ = WIRE_OUTPUT;
        input = 0;
        break;
      case PM_INPUT:
        *ptr++ = WIRE_INPUT;
        input = 1;
        break;
      case PM_PULLUP:
      case PM_PULLDOWN:
        if (ptr > program_end && ptr[-1] == WIRE_INPUT)
        {
          ptr[-1] = (op == PM_PULLUP ? WIRE_INPUT_PULLUP : WIRE_INPUT_PULLDOWN);
        }
        else
        {
          goto wire_error;
        }
        break;
      case PM_TIMEOUT:
        *ptr++ = WIRE_TIMEOUT;
        *(unsigned short*)ptr = WIRE_USEC_TO_COUNT(expression(EXPR_COMMA));
        ptr += sizeof(unsigned short);
        if (error_num)
        {
          goto wire_error;
        }
        break;
      default:
        if (op >= 'A' && op <= 'Z')
        {
          stack_variable_frame* vframe;
          unsigned char* vptr = get_variable_frame(op, &vframe);
          if (vframe->type == VAR_DIM_BYTE)
          {
            *ptr++ = (input ? WIRE_INPUT_BYTES : WIRE_OUTPUT_BYTES);
            *ptr++ = vframe->header.frame_size - sizeof(stack_variable_frame);
            OS_memset(vptr, 0, vframe->header.frame_size - sizeof(stack_variable_frame));
            *(unsigned char**)ptr = vptr;
            ptr += sizeof(unsigned char*);
            break;
          }
          else if (input)
          {
            *vptr = 0;
            *ptr++ = WIRE_INPUT_INT;
            *(unsigned char**)ptr = vptr;
            ptr += sizeof(unsigned char*);
            break;
          }
        }
        txtpos--;
        VAR_TYPE val = expression(EXPR_COMMA);
        if (error_num)
        {
          goto wire_error;
        }
        if (ptr > program_end && ptr[-1] == WIRE_OUTPUT)
        {
          *ptr++ = (val ? WIRE_HIGH : WIRE_LOW);
        }
        else if (input)
        {
          *ptr++ = WIRE_INPUT_WAIT_SHORT;
          *(unsigned short*)ptr = val;
          ptr += sizeof(unsigned short);
        }
        else
        {
          *ptr++ = WIRE_OUTPUT_SHORT;
          *(unsigned short*)ptr = val;
          ptr += sizeof(unsigned short);
        }
        break;
    }
  }
wire_error:
  if (!error_num)
  {
    error_num = ERROR_GENERAL;
  }
}

static void pin_wire(unsigned char* ptr, unsigned char* end)
{
  // We now have an fast expression to manipulate the pins. Do it.
  // We inline all the pin operations to keep the time down.
  unsigned char major = 0;
  unsigned char dbit = 1;
  unsigned char high = 0;
  unsigned short timeout = 256;
  while (ptr < end)
  {
    switch (*ptr++)
    {
      case WIRE_PIN:
      {
        unsigned char pin = *ptr++;
        major = PIN_MAJOR(pin);
        dbit = 1 << PIN_MINOR(pin);
        switch (major)
        {
          case 0:
            P0SEL &= ~dbit;
            break;
          case 1:
            P1SEL &= ~dbit;
            break;
          case 2:
            P2SEL &= ~dbit;
            break;
        }
        break;
      }
      case WIRE_OUTPUT:
        switch (major)
        {
          case 0:
            P0DIR |= dbit;
            APCFG &= ~dbit;
            high = (P0 & dbit) ? 1 : 0;
            break;
          case 1:
            P1DIR |= dbit;
            high = (P1 & dbit) ? 1 : 0;
            break;
          case 2:
            P2DIR |= dbit;
            high = (P2 & dbit) ? 1 : 0;
            break;
        }
        break;
      case WIRE_INPUT:
        switch (major)
        {
          case 0:
            P0DIR &= ~dbit;
            P0INP |= dbit;
            APCFG &= ~dbit;
            break;
          case 1:
            P1DIR &= ~dbit;
            P1INP |= dbit;
            break;
          case 2:
            P2DIR &= ~dbit;
            P2INP |= dbit;
            break;
        }
        break;
      case WIRE_INPUT_PULLUP:
        switch (major)
        {
          case 0:
            P0DIR &= ~dbit;
            P0INP &= ~dbit;
            APCFG &= ~dbit;
            P2INP &= ~(1 << 5);
            break;
          case 1:
            P1DIR &= ~dbit;
            P1INP &= ~dbit;
            P2INP &= ~(1 << 6);
            break;
          case 2:
            P2DIR &= ~dbit;
            P2INP &= ~dbit;
            P2INP &= ~(1 << 7);
            break;
        }
        break;
      case WIRE_INPUT_PULLDOWN:
        switch (major)
        {
          case 0:
            P0DIR &= ~dbit;
            P0INP &= ~dbit;
            APCFG &= ~dbit;
            P2INP |= 1 << 5;
            break;
          case 1:
            P1DIR &= ~dbit;
            P1INP &= ~dbit;
            P2INP |= 1 << 6;
            break;
          case 2:
            P2DIR &= ~dbit;
            P2INP &= ~dbit;
            P2INP |= 1 << 7;
            break;
        }
        break;
      case WIRE_ADC:
        APCFG |= dbit;
        break;
      case WIRE_TIMEOUT:
        timeout = *(unsigned short*)ptr;
        ptr += sizeof(unsigned short);
        break;
      case WIRE_HIGH:
      wire_high:
        switch (major)
        {
          case 0:
            P0 |= dbit;
            break;
          case 1:
            P1 |= dbit;
            break;
          case 2:
            P2 |= dbit;
            break;
        }
        high = 1;
        break;
      case WIRE_LOW:
      wire_low:
        switch (major)
        {
          case 0:
            P0 &= ~dbit;
            break;
          case 1:
            P1 &= ~dbit;
            break;
          case 2:
            P2 &= ~dbit;
            break;
        }
        high = 0;
        break;
      case WIRE_OUTPUT_SHORT:
        OS_delaymicroseconds(*(unsigned short*)ptr);
        ptr += sizeof(unsigned short);
        if (ptr < end && *ptr == WIRE_OUTPUT_SHORT)
        {
          high = !high;
          if (high)
          {
            goto wire_high;
          }
          else
          {
            goto wire_low;
          }
        }
        break;
      case WIRE_OUTPUT_BYTES:
      {
        unsigned char len = *ptr++;
        unsigned char* data = *(unsigned char**)ptr;
        ptr += sizeof(unsigned char);
        while (len--)
        {
          OS_delaymicroseconds(*data++);
          if (len)
          {
            high = !high;
            if (high)
            {
              switch (major)
              {
                case 0:
                  P0 &= ~dbit;
                  break;
                case 1:
                  P1 &= ~dbit;
                  break;
                case 2:
                  P2 &= ~dbit;
                  break;
              }
            }
            else
            {
              switch (major)
              {
                case 0:
                  P0 &= ~dbit;
                  break;
                case 1:
                  P1 &= ~dbit;
                  break;
                case 2:
                  P2 &= ~dbit;
                  break;
              }
            }
          }
        }
      }
      case WIRE_INPUT_BYTES:
      {
        unsigned short count;
        unsigned char v;
        unsigned char len = *ptr++;
        unsigned char* data = *(unsigned char**)ptr;
        ptr += sizeof(unsigned char*);
        while (len--)
        {
          count = 1;
          switch (major)
          {
            case 0:
              v = P0 & dbit;
              for (; v == (P0 & dbit) && count < timeout; count++)
                ;
              break;
            case 1:
              v = P1 & dbit;
              for (; v == (P1 & dbit) && count < timeout; count++)
                ;
              break;
            case 2:
              v = P2 & dbit;
              for (; v == (P2 & dbit) && count < timeout; count++)
                ;
              break;
          }
          count = WIRE_COUNT_TO_USEC(count);
          if (count > 255)
          {
            count = 0;
          }
          *data++ = count;
        }
        break;
      }
      case WIRE_INPUT_INT:
      {
        unsigned short count = 1;
        unsigned char v;
        switch (major)
        {
          case 0:
            v = P0 & dbit;
            for (; v == (P0 & dbit) && count < timeout; count++)
              ;
            break;
          case 1:
            v = P1 & dbit;
            for (; v == (P1 & dbit) && count < timeout; count++)
              ;
            break;
          case 2:
            v = P2 & dbit;
            for (; v == (P2 & dbit) && count < timeout; count++)
              ;
            break;
        }
        **(VAR_TYPE**)ptr = WIRE_COUNT_TO_USEC(count);
        ptr += sizeof(unsigned char*);
        break;
      }
      case WIRE_INPUT_WAIT_SHORT:
        OS_delaymicroseconds(*(unsigned short*)ptr);
        ptr += sizeof(unsigned short);
        break;
      default:
        goto wire_error;
    }
  }
  return;
wire_error:
  if (!error_num)
  {
    error_num = ERROR_GENERAL;
  }
  return;
}

//
// Build a new BLE service and register it with the system
//
static char ble_build_service(void)
{
  unsigned char* line;
  unsigned char* origsp;
  gattAttribute_t* attributes;
  unsigned char ch;
  unsigned char cmd;
  short val;
  short count = 0;
  unsigned char* chardesc = NULL;
  unsigned char chardesclen = 0;
  stack_service_frame* frame;
  LINENUM onconnect = 0;

  linenum = servicestart;
  line = findline();
  txtpos = line + sizeof(LINENUM) + sizeof(char);

  origsp = sp;
  // Allocate space on the stack for the service attributes
  sp -= sizeof(gattAttribute_t) * servicecount + sizeof(unsigned short);
  if (sp < program_end)
  {
    goto oom;
  }
  attributes = (gattAttribute_t*)(sp + sizeof(unsigned short));
  
  for (count = 0; count < servicecount; count++)
  {
    // Ignore all GATT commands (we did them already)
    if (*txtpos++ != KW_GATT)
    {
      continue;
    }
    cmd = *txtpos++;

    if (cmd == KW_END)
    {
      break;
    }
    
    // Some defaults
    attributes[count].type.uuid = NULL;
    attributes[count].type.len = 2;
    attributes[count].permissions = GATT_PERMIT_READ;
    attributes[count].handle = 0;
    *(unsigned char**)&attributes[count].pValue = NULL;

    if (cmd == BLE_SERVICE || cmd == BLE_CHARACTERISTIC)
    {
      ch = *txtpos;
      if (ch == '"' || ch == '\'')
      {
        ch = ble_get_uuid();
        if (!ch)
        {
          goto error;
        }
        if (cmd == BLE_SERVICE)
        {
          attributes[count].type.uuid = ble_primary_service_uuid;
          sp -= ble_uuid_len + sizeof(gattAttrType_t);
          if (sp < program_end)
          {
            goto oom;
          }
          OS_memcpy(sp + sizeof(gattAttrType_t), ble_uuid, ble_uuid_len);
          ((gattAttrType_t*)sp)->len = ble_uuid_len;
          ((gattAttrType_t*)sp)->uuid = sp + sizeof(gattAttrType_t);
          *(unsigned char**)&attributes[count].pValue = sp;
          
          ignore_blanks();
          if (*txtpos == BLE_ONCONNECT)
          {
            if (txtpos[1] == KW_GOSUB)
            {
              txtpos += 2;
              onconnect = expression(EXPR_NORMAL);
              if (error_num)
              {
                goto error;
              }
            }
            else
            {
              goto error;
            }
          }
        }

        if (cmd == BLE_CHARACTERISTIC)
        {
          attributes[count].type.uuid = ble_characteristic_uuid;

          ignore_blanks();
          
          // Description
          val = find_quoted_string();
          if (val != -1)
          {
            chardesc = txtpos;
            chardesclen = (unsigned char)val;
            txtpos += val + 1;
          }
          else
          {
            chardesc = NULL;
            chardesclen = 0;
          }
        }
      }
      else
      {
        goto error;
      }
    }
    else if (cmd >= BLE_READ && cmd <= BLE_INDICATE)
    {
      txtpos--;
      ch = 0;
      for (;;)
      {
        switch (*txtpos++)
        {
          case BLE_READ:
            ch |= GATT_PROP_READ;
            break;
          case BLE_WRITE:
            ch |= GATT_PROP_WRITE;
            break;
          case BLE_NOTIFY:
            ch |= GATT_PROP_NOTIFY;
            break;
          case BLE_INDICATE:
            ch |= GATT_PROP_INDICATE;
            break;
          case BLE_WRITENORSP:
            ch |= GATT_PROP_WRITE_NO_RSP;
            break;
          default:
            txtpos--;
            goto done;
        }
      }
done:
      sp--;
      if (sp < program_end)
      {
        goto oom;
      }
      sp[0] = ch;
      *(unsigned char**)&attributes[count - 1].pValue = sp;
      
      ch = *txtpos;
      if (ch < 'A' || ch > 'Z')
      {
        goto error;
      }

      gatt_variable_ref* vref;
      stack_variable_frame* vframe;
      
      txtpos++;
      if (*txtpos != WS_SPACE && *txtpos != NL && *txtpos < 0x80)
        goto error;
      
      sp -= ble_uuid_len + sizeof(gatt_variable_ref);
      if (sp < program_end)
      {
        goto oom;
      }
      vref = (gatt_variable_ref*)(sp + ble_uuid_len);
      vref->read = 0;
      vref->write = 0;
      vref->var = ch;
      vref->attrs = attributes;
      vref->cfg = NULL;
      
      OS_memcpy(sp, ble_uuid, ble_uuid_len);
      *(unsigned char**)&attributes[count].pValue = (unsigned char*)vref;
      attributes[count].permissions = GATT_PERMIT_READ | GATT_PERMIT_WRITE;
      attributes[count].type.uuid = sp;
      attributes[count].type.len = ble_uuid_len;

      for (;;)
      {
        ignore_blanks();
        ch = *txtpos;
        if (ch == BLE_ONREAD || ch == BLE_ONWRITE)
        {
          txtpos++;
          if (*txtpos != KW_GOSUB)
          {
            goto error;
          }
          txtpos++;
          linenum = expression(EXPR_NORMAL);
          if (error_num)
          {
            goto error;
          }
          if (ch == BLE_ONREAD)
          {
            vref->read = linenum;
          }
          else
          {
            vref->write = linenum;
          }
        }
        else if (ch == NL)
        {
          break;
        }
        else
        {
          goto error;
        }
      }

      if ((attributes[count - 1].pValue[0] & (GATT_PROP_INDICATE|GATT_PROP_NOTIFY)) != 0)
      {
        sp -= sizeof(gattCharCfg_t) * GATT_MAX_NUM_CONN;
        if (sp < program_end)
        {
          goto oom;
        }
        GATTServApp_InitCharCfg(INVALID_CONNHANDLE, (gattCharCfg_t*)sp);

        count++;
        attributes[count].type.uuid = ble_client_characteristic_config_uuid;
        attributes[count].type.len = 2;
        attributes[count].permissions = GATT_PERMIT_READ | GATT_PERMIT_WRITE;
        attributes[count].handle = 0;
        *(unsigned char**)&attributes[count].pValue = sp;
        vref->cfg = sp;

        sp -= sizeof(stack_service_frame);
        if (sp < program_end)
        {
          goto oom;
        }
        frame = (stack_service_frame*)sp;
        frame->header.frame_type = STACK_SERVICE_FLAG;
        frame->header.frame_size = origsp - sp;
        get_variable_frame(vref->var, &vframe);
        vframe->ble = vref;
        sp += sizeof(stack_service_frame);
      }
 
      if (chardesclen > 0)
      {
        count++;
        attributes[count].type.uuid = ble_characteristic_description_uuid;
        attributes[count].type.len = 2;
        attributes[count].permissions = GATT_PERMIT_READ;
        attributes[count].handle = 0;

        sp -= chardesclen + 1;
        if (sp < program_end)
        {
          goto oom;
        }
        OS_memcpy(sp, chardesc, chardesclen);
        sp[chardesclen] = 0;
        *(unsigned char**)&attributes[count].pValue = sp;
        chardesclen = 0;
      }
    }
    else
    {
      goto error;
    }

    line += line[sizeof(LINENUM)];
    txtpos = line + sizeof(LINENUM) + sizeof(char);
  }

  // Build a stack header for this service
  sp -= sizeof(stack_service_frame);
  if (sp < program_end)
  {
    goto oom;
  }
  frame = (stack_service_frame*)sp;
  frame->header.frame_type = STACK_SERVICE_FLAG;
  frame->header.frame_size = origsp - sp;
  frame->attrs = attributes;
  frame->connect = onconnect;
  ((unsigned short*)attributes)[-1] = count;

  // Register the service
  ch = GATTServApp_RegisterService((gattAttribute_t*)attributes, count, &ble_service_callbacks);
  if (ch != SUCCESS)
  {
    goto error;
  }

  return 0;

error:
  sp = origsp;
  txtpos = line + sizeof(LINENUM) + sizeof(char);
  return 1;
oom:
  sp = origsp;
  txtpos = line + sizeof(LINENUM) + sizeof(char);
  return 2;
}

//
// Parse a string into a UUID
//  This is fairly lax in what it considers valid.
//
static char ble_get_uuid(void)
{
  VAR_TYPE v = 0;
  unsigned char* end;
  
  end = txtpos + find_quoted_string();
  if (end < txtpos)
  {
    return 0;
  }

  ble_uuid_len = 0;
  while (txtpos < end)
  {
    if (ble_uuid_len >= sizeof(ble_uuid))
    {
      return 0;
    }
    else if (*txtpos == '-')
    {
      txtpos++;
    }
    else
    {
      v = parse_int(2, 16);
      if (error_num)
      {
        return 0;
      }
      ble_uuid[ble_uuid_len++] = (unsigned char)v;
    }
  }
#ifdef TARGET_CC254X
  // Reverse
  for (v = 0; v < ble_uuid_len / 2; v++)
  {
    unsigned char c = ble_uuid[v];
    ble_uuid[v] = ble_uuid[ble_uuid_len - v - 1];
    ble_uuid[ble_uuid_len - v - 1] = c;
  }
#endif
  txtpos++;
  return ble_uuid_len > 0;
}

//
// Calculate the maximum read/write offset for the specified variable
//
static unsigned short ble_max_offset(gatt_variable_ref* vref, unsigned short offset, unsigned char maxlen)
{
  stack_variable_frame* frame;
  unsigned char moffset = offset + maxlen;

  get_variable_frame(vref->var, &frame);

  if (frame->type == VAR_DIM_BYTE)
  {
    if (moffset > frame->header.frame_size - sizeof(stack_variable_frame))
    {
      moffset = frame->header.frame_size - sizeof(stack_variable_frame);
    }
  }
  else
  {
    if (moffset > VAR_SIZE)
    {
      moffset = VAR_SIZE;
    }
  }

  return moffset;
}

//
// When a BLE characteristic is read, we process the incoming request from
// the appropriate BASIC variable. If an ONREAD event is specified we notify the user
// of the read request *before* we do the actual read.
//
static unsigned char ble_read_callback(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char* len, unsigned short offset, unsigned char maxlen)
{
  gatt_variable_ref* vref;
  unsigned char moffset;
  unsigned char* v;
  stack_variable_frame* frame;
  
  vref = (gatt_variable_ref*)attr->pValue;
  moffset = ble_max_offset(vref, offset, maxlen);
  if (!moffset)
  {
    return FAILURE;
  }

  if (vref->read)
  {
    interpreter_run(vref->read, 1);
  }

  v = get_variable_frame(vref->var, &frame);
#ifdef TARGET_CC254X
  if (frame->type == VAR_DIM_BYTE)
  {
    OS_memcpy(value, v + offset, moffset - offset);
  }
  else
  {
    for (unsigned char i = offset; i < moffset; i++)
    {
      value[i - offset] = v[moffset - i - 1];
    }
  }
#else
  OS_memcpy(value, v + offset, moffset - offset);
#endif

  *len = moffset - offset;
  return SUCCESS;
}

//
// When a BLE characteristic is written, we process the incomign data and update
// the appropriate BASIC variable. If an ONWRITE event is specified we notify the user
// of the change *after* the write.
//
static unsigned char ble_write_callback(unsigned short handle, gattAttribute_t* attr, unsigned char* value, unsigned char len, unsigned short offset)
{
  gatt_variable_ref* vref;
  unsigned char moffset;
  unsigned char* v;
  stack_variable_frame* frame;
  
  if (attr->type.uuid == ble_client_characteristic_config_uuid)
  {
    return GATTServApp_ProcessCCCWriteReq(handle, attr, value, len, offset, GATT_CLIENT_CFG_NOTIFY);
  }
 
  vref = (gatt_variable_ref*)attr->pValue;
  moffset = ble_max_offset(vref, offset, len);
  if (moffset != offset + len)
  {
    return FAILURE;
  }
  
  v = get_variable_frame(vref->var, &frame);

#ifdef TARGET_CC254X
  if (frame->type == VAR_DIM_BYTE)
  {
    OS_memcpy(v + offset, value, moffset - offset);
  }
  else
  {
    for (unsigned char i = offset; i < moffset; i++)
    {
      v[moffset - i - 1] = value[i - offset];
    }
  }
#else
  OS_memcpy(v + offset, value, moffset - offset);
#endif

  if (vref->write)
  {
    interpreter_run(vref->write, 1);
  }

  if (attr[1].type.uuid == ble_client_characteristic_config_uuid)
  {
    ble_notify_assign(vref);
  }
  
  return SUCCESS;
}

//
// Send a BLE NOTIFY event
//
static void ble_notify_assign(gatt_variable_ref* vref)
{
  GATTServApp_ProcessCharCfg((gattCharCfg_t*)vref->cfg, (unsigned char*)vref, 0, (gattAttribute_t*)vref->attrs, ((unsigned short*)vref->attrs)[-1], INVALID_TASK_ID);
}

//
// BLE connection management. If the ONCONNECT event was specified when a service
// was created, we forward any connection changes up to the user code.
//
void ble_connection_status(unsigned short connHandle, unsigned char changeType, signed char rssi)
{
  unsigned char* ptr;
  stack_service_frame* vframe;
  unsigned char j;
  unsigned char f;
  short i;

  for (ptr = sp; ptr < variables_begin; ptr += ((stack_header*)ptr)->frame_size)
  {
    vframe = (stack_service_frame*)ptr;
    if (vframe->header.frame_type == STACK_SERVICE_FLAG)
    {
      if (vframe->connect)
      {
        VARIABLE_INT_SET('H', connHandle);
        VARIABLE_INT_SET('S', changeType);
        if (changeType == LINKDB_STATUS_UPDATE_STATEFLAGS)
        {
          f = 0;
          for (j = 0x01; j < 0x20; j <<= 1) // No direct way to read flag bits!
          {
            if (linkDB_State(connHandle, j))
            {
              f |= j;
            }
          }
          VARIABLE_INT_SET('V', f);
        }
        else if (changeType == LINKDB_STATUS_UPDATE_RSSI)
        {
          VARIABLE_INT_SET('V', rssi);
        }
        interpreter_run(vframe->connect, 1);
      }
      if (changeType == LINKDB_STATUS_UPDATE_REMOVED || (changeType == LINKDB_STATUS_UPDATE_STATEFLAGS && !linkDB_Up(connHandle)))
      {
        for (i = ((short*)vframe->attrs)[-1]; i; i--)
        {
          if (vframe->attrs[i].type.uuid == ble_client_characteristic_config_uuid)
          {
            GATTServApp_InitCharCfg(connHandle, (gattCharCfg_t*)vframe->attrs[i].pValue);
          }
        }
      }
    }
  }
}

#ifdef TARGET_CC254X

extern void interpreter_devicefound(unsigned char addtype, unsigned char* address, signed char rssi, unsigned char eventtype, unsigned char len, unsigned char* data)
{
  if (blueBasic_discover.linenum)
  {
    unsigned char* osp = sp;
    error_num = ERROR_OK;
    VARIABLE_INT_SET('A', addtype);
    VARIABLE_INT_SET('R', rssi);
    VARIABLE_INT_SET('E', eventtype);
    create_dim('B', 8, address);
    create_dim('V', len, data);
    if (!error_num)
    {
      interpreter_run(blueBasic_discover.linenum, 1);
    }
    sp = osp;
  }
}

#endif // TARGET_CC254X
