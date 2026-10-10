//---------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "common.h"
#if LINUX
  #include <unistd.h>
#endif

//---------------------------------------------------------------------------
void pressEnter(void)
{
  int cc;

#if LINUX
  // v skripte alebo pri presmerovani necakaj
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
    return;
#endif

  printf("\nPress ENTER ...\n");
  do {
    cc = getchar();
  } while (cc != '\n' && cc != EOF);
}
//---------------------------------------------------------------------------
int printError(int iErr, const void *pvPar1, const void *pvPar2)
{
  switch (iErr) {
    case ERR_OUT_OF_MEMORY :
      fprintf(stderr, "Out of memory!");
      break;

    case ERR_UNKNOWN_OPTION :
      fprintf(stderr, "Unknown option: %s", (const char *) pvPar1);
      break;

    case ERR_BAD_OPTIONS :
      fprintf(stderr, "Bad combination of options: %s %s", (const char *) pvPar1, (const char *) pvPar2);
      break;

    case ERR_NO_OPTION_VALUE :
      fprintf(stderr, "Missing option value(s) '%s'!", (const char *) pvPar1);
      break;

    case ERR_OPEN_SRC_FILE :
      fprintf(stderr, "Can't open file '%s' for reading!", (const char *) pvPar1);
      break;

    case ERR_ACCESS_SRC_FILE :
      fprintf(stderr, "Can't access file '%s' for reading!", (const char *) pvPar1);
      break;

    case ERR_LENGTH_SRC_FILE :
      fprintf(stderr, "Can't retrieve length of the file '%s'!", (const char *) pvPar1);
      break;

    case ERR_SRC_FILE_OF_READ :
      fprintf(stderr, "Read beyond end of the file '%s'.", (const char *) pvPar1);
      break;

    case ERR_SRC_FILE_TO_LONG :
      fprintf(stderr, "Source file '%s' is too long! Max length %d bytes.", (const char *) pvPar1, *(const int *) pvPar2);
      break;

    case ERR_READ_SRC_FILE :
      fprintf(stderr, "Can't read from the file '%s'!", (const char *) pvPar1);
      break;

    case ERR_OPEN_DEST_FILE :
      fprintf(stderr, "Can't open file '%s' for writing!", (const char *) pvPar1);
      break;

    case ERR_ACCESS_DEST_FILE :
      fprintf(stderr, "Can't access output file '%s' for writing!", (const char *) pvPar1);
      break;

    case ERR_WRITE_DEST_FILE :
      fprintf(stderr, "Can't write to the file '%s'!", (const char *) pvPar1);
      break;

    case ERR_BAD_ADDRESS :
      fprintf(stderr, "Value %d is out of range <0, 65535>: %s", *(const int *) pvPar2, (const char *) pvPar1);
      break;

    case ERR_BAD_VALUE :
      fprintf(stderr, "Bad option value: %s %s", (const char *) pvPar1, (const char *) pvPar2);
      break;

    case ERR_BAD_LONG_NAME :
      fprintf(stderr, "Long name can have up to 16 chars!");
      break;

    case ERR_INPUT_FILE_MORE :
      fprintf(stderr, "Only one source file can be specified! '%s' or '%s'?", (const char *) pvPar1, (const char *) pvPar2);
      break;

    case ERR_NO_OUTPUT_FILE :
      fprintf(stderr, "Output file not specified!");
      break;

    case ERR_BOOT_FILE_MORE :
      fprintf(stderr, "Only one BOOT file can be specified! '%s' or '%s'?", (const char *) pvPar1, (const char *) pvPar2);
      break;

    case ERR_INVALID_BOOT_BIN :
      if (pvPar1)
        fprintf(stderr, "Invalid BOOT binary file! '%s'", (const char *) pvPar1);
      else
        fprintf(stderr, "Invalid BOOT in MRM image!");
      break;

    case ERR_INVALID_FILE_MASK :
      fprintf(stderr, "Invalid file name/mask '%s' in option `%s`", (const char *) pvPar1, (const char *) pvPar2);
      break;

    case ERR_UNAMBIGUOUS_NAME :
      fprintf(stderr, "File name '%s' in option `%s` must be unambiguous!", (const char *) pvPar1, (const char *) pvPar2);
      break;

    case ERR_FILE_EXIST :
      fprintf(stderr, "File '%s' alreday exist!", (const char *) pvPar1);
      break;

    case ERR_FILE_NOT_EXIST :
      fprintf(stderr, "File '%s' not exist!", (const char *) pvPar1);
      break;

    case ERR_DIRECTORY_FULL :
      fprintf(stderr, "Directory full!");
      break;

    case ERR_DISK_FULL :
      fprintf(stderr, "Disk full!");
      break;

    case ERR_BAD_GEOMETRY :
      fprintf(stderr, "Current MRM content do not fit into new MRM geometry (size/directory/boot)!");
      break;

    case ERR_INVALID_CPLX_NAME :
      fprintf(stderr, "Invalid complex file name '%s'", (const char *) pvPar1);
      break;

    case ERR_INVALID_LOADER :
      fprintf(stderr, "File '%s' is not valid standard Loader file!", (const char *) pvPar1);
      break;
  }

  fprintf(stderr, "\n\n");

  return iErr;
}
//---------------------------------------------------------------------------
int allocBuffer(void **ppvBuff, int iSize)
{
  if (*ppvBuff)
    return ERR_OK;

  *ppvBuff = (void *) malloc(iSize);
  if (!*ppvBuff)
    return printError(ERR_OUT_OF_MEMORY, NULL, NULL);

  memset(*ppvBuff, 0, iSize);
  return ERR_OK;
}
//---------------------------------------------------------------------------
int parseNumber(const char *pszStr)
{
  char *p;
  int iVal = strtol(pszStr, &p, 0);
  if (iVal < 0 || (iVal == 0 && *p != '\0'))
    iVal = -1;
  return iVal;
}
//---------------------------------------------------------------------------
char* trimLeft(char *pszStr)
{
  char *p, *q;

  if (isspace(*pszStr)) {
    p = pszStr;
    while (isspace(*p))
      p++;

    q = pszStr;
    while (*p)
      *q++ = *p++;
    *q = '\0';
  }

  return pszStr;
}
//---------------------------------------------------------------------------
char* trimRight(char *pszStr)
{
  char *p;

  p = pszStr + strlen(pszStr);
  while (p > pszStr) {
    p--;
    if (isspace(*p))
      *p = '\0';
    else
      break;
  }

  return pszStr;
}
//---------------------------------------------------------------------------
char* trim(char *pszStr)
{
  return trimLeft(trimRight(pszStr));
}
//---------------------------------------------------------------------------
BOOL isValidChar(char cc)
{
  return (cc >= '!' && cc <= '~');
}
//---------------------------------------------------------------------------
BOOL isMetaChar(char cc, BOOL bExtra)
{
  BOOL bIsMetaChar;

  switch (cc) {
    case '#' :
    case '_' :
    case ' ' :
      bIsMetaChar = bExtra;
      break;

    case '\\' :
    case '/' :
    case ':' :
    case '*' :
    case '?' :
    case '"' :
    case '<' :
    case '>' :
    case '|' :
      bIsMetaChar = TRUE;
      break;

    default :
      bIsMetaChar = FALSE;
      break;
  }

  return bIsMetaChar;
}
//---------------------------------------------------------------------------
char* appendEncChar(char cc, char *pszDest)
{
  char *p = pszDest;
  if (isMetaChar(cc, TRUE))
    p += sprintf(p, "#%02X", (int) cc);
  else
    *p++ = cc;

  return p;
}
//---------------------------------------------------------------------------
BOOL decodeString(char *pszDest, int iMaxDestLen, const char *pszSrc)
{
  char cc, *p, *q;
  int iCnt, iVal;

  iCnt = 0;
  p = (char *) pszSrc;
  q = pszDest;
  while (*p && iCnt < iMaxDestLen) {
    cc = *p++;
    if (cc == '#') {
      cc = *p++;
      if (!isxdigit(cc))
        return FALSE;
      cc = (char) toupper(cc);
      if (cc > '9')
        iVal = cc - 'A' + 10;
      else
        iVal = cc - '0';
      iVal *= 16;

      cc = *p++;
      if (!isxdigit(cc))
        return FALSE;
      cc = (char) toupper(cc);
      if (cc > '9')
        iVal += (cc - 'A' + 10);
      else
        iVal += (cc - '0');

      if (iVal < ' ' || iVal > '~')
        return FALSE;

      cc = (char) iVal;
    }

    *q++ = cc;
    iCnt++;
  }
  *q = '\0';

  return (*p == '\0');
}
//---------------------------------------------------------------------------

