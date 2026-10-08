//---------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "common.h"

//---------------------------------------------------------------------------
int prepareFilePath(WORK_PARAMS *pWP, const char *pszFilePath, const char *pszOpt,
                    BOOL bWildcards, BOOL bOptAdd)
{
  return prepareFilePathEx(pWP, pWP->szFilePath, pszFilePath, pszOpt, bWildcards, bOptAdd, TRUE);
}
//---------------------------------------------------------------------------
int prepareFilePathEx(WORK_PARAMS *pWP, char *pszDestFileName, const char *pszFilePath,
                      const char *pszOpt, BOOL bWildcards, BOOL bOptAdd, BOOL b81Name)
{
  int i;
  char *p, *q, szName[256];

  p = strrchr(pszFilePath, '/');
#if WIN
  if (!p)
    p = strrchr(pszFilePath, '\\');
#endif
  if (p && *(p + 1) == '\0')
    return printError(ERR_INVALID_FILE_MASK, pszFilePath, pszOpt);

  if (!p) {
#if WIN
    strcpy(pszDestFileName, ".\\"); // *pszDestFileName = '\0';
#else
    strcpy(pszDestFileName, "./");
#endif
    if (bOptAdd)
      strcat(pszDestFileName, pszFilePath);
    strcpy(szName, pszFilePath);
  }
  else {
    strcpy(szName, p + 1);
    if (bOptAdd)
      strcpy(pszDestFileName, pszFilePath);
    else {
      i = p - pszFilePath + 1;
      strncpy(pszDestFileName, pszFilePath, i);
      pszDestFileName[i] = '\0';
    }
  }

  if (!b81Name)
    return ERR_OK;

  p = szName;
  q = strchr(p, '.');
  if (q)
    i = q - p;
  else
    i = strlen(p);
  if (i == 0 || (q && *(q + 1) == '\0'))
    return printError(ERR_INVALID_FILE_MASK, pszFilePath, pszOpt);

  if (q && i > MRM_SHORT_NAME_LEN) {
    szName[MRM_SHORT_NAME_LEN] = '.';
    szName[MRM_SHORT_NAME_LEN + 1] = *(q + 1);
    szName[MRM_SHORT_NAME_LEN + 2] = '\0';
  }
  else if (!q && i > MRM_SHORT_NAME_LEN)
    szName[MRM_SHORT_NAME_LEN] = '\0';
  else if (q)
    *(q + 2) = '\0';

  return prepareFileName81(pWP, szName, pszOpt, bWildcards);
}
//---------------------------------------------------------------------------
// Preveri platnost `pszName` a pripravi ho do `pWP->szMaskPrep` a `pWP->szMaskPar`.
int prepareFileName81(WORK_PARAMS *pWP, const char *pszName, const char *pszOpt, BOOL bWildcards)
{
  return prepareFileName81M(pszName, pszOpt, bWildcards, pWP->szMaskPrep, pWP->szMaskPar);
}
//---------------------------------------------------------------------------
// Preveri platnost `pszName` a pripravi ho do `pszMaskPrep` a `pszMaskPar`.
int prepareFileName81M(const char *pszName, const char *pszOpt, BOOL bWildcards,
                       char *pszMaskPrep, char *pszMaskPar)
{
  char *p, *q;
  int j;

  p = strchr(pszName, '.');
  if ((p && (p == pszName || (p - pszName) > MRM_SHORT_NAME_LEN || *(p + 1) == '\0' || strlen(p + 1) > 1))
      || (!p && strlen(pszName) > MRM_SHORT_NAME_LEN)) {
    return printError(ERR_INVALID_FILE_MASK, pszName, pszOpt);
  }

  // meno suboru
  q = (char *) pszName;
  for (j = 0; j < MRM_SHORT_NAME_LEN; j++) {
    if (*q == '\0' || *q == '.')
      pszMaskPrep[j] = ' ';
    else if (*q == '*')
      pszMaskPrep[j] = '?';
    else
      pszMaskPrep[j] = *q++;
  }

  *(pszMaskPrep + MRM_SHORT_NAME_LEN) = '\0';

  if (!bWildcards
      && (strchr(pszMaskPrep, '?') != NULL || !p))
    return printError(ERR_UNAMBIGUOUS_NAME, pszName, pszOpt);

  if (p)
    *(pszMaskPrep + MRM_SHORT_NAME_LEN) = *(p + 1);
  else
    *(pszMaskPrep + MRM_SHORT_NAME_LEN) = ' ';
  *(pszMaskPrep + MRM_SHORT_NAME_LEN + 1) = '\0';

  strupr(pszMaskPrep);

  j = MRM_SHORT_NAME_LEN - 1;
  while (pszMaskPrep[j] == ' ')
    j--;
  j++;
  strncpy(pszMaskPar, pszMaskPrep, j);
  if (p) {
    pszMaskPar[j++] = '.';
    pszMaskPar[j++] = pszMaskPrep[MRM_SHORT_NAME_LEN];
  }
  pszMaskPar[j] = '\0';

  return ERR_OK;
}
//---------------------------------------------------------------------------
BOOL compareFileNameMask(WORK_PARAMS *pWP, DIR_ITEM *pDI, BOOL bWildcards)
{
  int ii;
  char *pM, *pN;

  // iba platne polozky
  if (!isValidChar(pDI->cType) || !isValidChar(*pDI->cName))
    return FALSE;

  // typ musi zodpovedat maske
  pM = pWP->szMaskPrep + MRM_SHORT_NAME_LEN;
  if (bWildcards && *pM != ' ' && *pM != pDI->cType)
    return FALSE;
  if (!bWildcards && *pM != pDI->cType)
    return FALSE;

  // meno musi zodpovedat maske
  pM = &pWP->szMaskPrep[0];
  pN = &pDI->cName[0];
  for (ii = 0; ii < MRM_SHORT_NAME_LEN; ii++, pM++, pN++) {
    if (bWildcards && *pM == '?')
      continue;
    if (*pM != toupper(*pN))
      break;
  }

  return (ii == MRM_SHORT_NAME_LEN);
}
//---------------------------------------------------------------------------
void compactFileName(DIR_ITEM *pDI, char *pszName, BOOL bComplexName)
{
  int i;
  char szN[32], *p, *q, cc;

  strncpy(pszName, pDI->cName, MRM_SHORT_NAME_LEN);
  *(pszName + MRM_SHORT_NAME_LEN) = '\0';
  trim(pszName);
  if (pDI->cType != ' ') {
    i = strlen(pszName);
    pszName[i++] = '.';
    pszName[i++] = pDI->cType;
    pszName[i] = '\0';
  }
  if (!bComplexName)
    return;

  // priprav komplexne meno suboru v tvare
  //   N_L_S_A.T
  //     N=kratke meno
  //     L=dlhe meno
  //     S=start adresa
  //     A=atributy (ako cislo)
  //     T=typ
  // specialne "meta" znaky budu v tvare #HH
  strcpy(szN, pszName);
  q = pszName;
  p = szN;
  // kratke meno
  while (*p) {
    cc = *p++;
    if (cc == '.')
      break;
    q = appendEncChar(cc, q);
  }
  // dlhe meno
  *q++ = '_';
  if (*pDI->cLongName != ' ') {
    strncpy(szN, pDI->cLongName, MRM_LONG_NAME_LEN);
    *(szN + MRM_LONG_NAME_LEN) = '\0';
    trim(szN);
    p = szN;
    while (*p)
      q = appendEncChar(*p++, q);
  }
  // start adresa, atributy
  q += sprintf(q, "_%d_%d.", (int) pDI->wMemAdr, (int) pDI->btAttrs);
  // pripona
  q = appendEncChar(pDI->cType, q);
  *q = '\0';
}
//---------------------------------------------------------------------------
int parseComplexFileName(DIR_ITEM *pDI, char *pszComplexName)
{
  char szCplxName[256], szStr[64], *p, *q;
  int iVal;

  memset(pDI, 0, sizeof(DIR_ITEM));

  p = strrchr(pszComplexName, '/');
#if WIN
  if (!p)
    p = strrchr(pszComplexName, '\\');
#endif
  if (p)
    p++;
  else
    p = pszComplexName;
  strcpy(szCplxName, p);
  p = szCplxName;

  // "rozparsuj" komplexne meno suboru v tvare
  //   N_L_S_A.T

  // kratke meno
  q = strchr(p, '_');
  if (!q)
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);
  *q = '\0';
  if (!decodeString(szStr, MRM_SHORT_NAME_LEN, p) || strlen(szStr) == 0)
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);
  while (strlen(szStr) < MRM_SHORT_NAME_LEN)
    strcat(szStr, " ");
  memcpy(pDI->cName, szStr, MRM_SHORT_NAME_LEN);

  // dlhe meno
  p = q + 1;
  q = strchr(p, '_');
  if (!q)
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);
  *q = '\0';
  if (!decodeString(szStr, MRM_LONG_NAME_LEN, p))
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);
  while (strlen(szStr) < MRM_LONG_NAME_LEN)
    strcat(szStr, " ");
  memcpy(pDI->cLongName, szStr, MRM_LONG_NAME_LEN);

  // start adresa
  p = q + 1;
  q = strchr(p, '_');
  if (!q)
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);
  *q = '\0';
  iVal = parseNumber(p);
  if (iVal < 0 || iVal > 65535)
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);
  pDI->wMemAdr = (WORD) iVal;

  // atributy
  p = q + 1;
  q = strchr(p, '.');
  if (!q)
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);
  *q = '\0';
  iVal = parseNumber(p);
  if (iVal < 0 || iVal > 255)
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);
  pDI->btAttrs = (BYTE) iVal;

  // pripona
  p = q + 1;
  if (!decodeString(szStr, 1, p))
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);

  if (!isValidChar(*szStr))
    return printError(ERR_INVALID_CPLX_NAME, pszComplexName, NULL);
  pDI->cType = *szStr;

  return ERR_OK;
}
//---------------------------------------------------------------------------
int openImageFile(WORK_PARAMS *pWP, BOOL bCreate)
{
  closeImageFile(pWP);

  if (bCreate) {
    if (remove(pWP->pszFnImg) != 0 && errno == EACCES) // vymaz existujuci subor
      return printError(ERR_ACCESS_DEST_FILE, pWP->pszFnImg, NULL);

    pWP->fiImage = fopen(pWP->pszFnImg, "a+b"); // vytvor subor obrazu MRM
    if (!pWP->fiImage)
      return printError(ERR_OPEN_DEST_FILE, pWP->pszFnImg, NULL);
    fclose(pWP->fiImage);
  }

  pWP->fiImage = fopen(pWP->pszFnImg, "r+b"); // otvor subor obrazu MRM
  if (!pWP->fiImage)
    return printError(ERR_OPEN_DEST_FILE, pWP->pszFnImg, NULL);

  return ERR_OK;
}
//---------------------------------------------------------------------------
int closeImageFile(WORK_PARAMS *pWP)
{
  if (pWP->fiImage)
    fclose(pWP->fiImage);
  pWP->fiImage = NULL;
  return ERR_OK;
}
//---------------------------------------------------------------------------
int writeToImage(WORK_PARAMS *pWP, BYTE *pbtBuff, int iOffset, int iLen)
{
  int iErr = ERR_OK;

  if (iLen <= 0)
    return iErr;

  do {
    if (fseek(pWP->fiImage, (long) iOffset, SEEK_SET) != 0) {
      iErr = printError(ERR_ACCESS_DEST_FILE, pWP->pszFnImg, NULL);
      break;
    }
    if (fwrite(pbtBuff, 1, iLen, pWP->fiImage) != (size_t) iLen) {
      iErr = printError(ERR_WRITE_DEST_FILE, pWP->pszFnImg, NULL);
      break;
    }
  } while (FALSE);

  return iErr;
}
//---------------------------------------------------------------------------
int readFromImage(WORK_PARAMS *pWP, BYTE *pbtBuff, int iOffset, int iToRead)
{
  int iErr = ERR_OK, iLen;

  do {
    if (fseek(pWP->fiImage, 0L, SEEK_END) != 0) {
      iErr = printError(ERR_ACCESS_SRC_FILE, pWP->pszFnImg, NULL);
      break;
    }
    iLen = ftell(pWP->fiImage);
    if (iLen < 0) {
      iErr = printError(ERR_LENGTH_SRC_FILE, pWP->pszFnImg, NULL);
      break;
    }
    if (iOffset >= iLen || (iOffset + iToRead) > iLen) {
      iErr = printError(ERR_SRC_FILE_OF_READ, pWP->pszFnImg, NULL);
      break;
    }
    if (fseek(pWP->fiImage, (long) iOffset, SEEK_SET) != 0) {
      iErr = printError(ERR_ACCESS_SRC_FILE, pWP->pszFnImg, NULL);
      break;
    }
    if (fread(pbtBuff, 1, iToRead, pWP->fiImage) != (size_t) iToRead) {
      iErr = printError(ERR_READ_SRC_FILE, pWP->pszFnImg, NULL);
      break;
    }
  } while (FALSE);

  return iErr;
}
//---------------------------------------------------------------------------
int saveFile(const char *pszFileName, BYTE *pbtBuff, int iLen, BOOL bAppend)
{
  int iErr = ERR_OK;
  FILE *f;

  if (iLen <= 0)
    return iErr;

  do {
    if (bAppend)
      f = fopen(pszFileName, "ab");
    else
      f = fopen(pszFileName, "wb");
    if (f == NULL) {
      iErr = printError(ERR_OPEN_DEST_FILE, pszFileName, NULL);
      break;
    }
    if (!bAppend && fseek(f, 0L, SEEK_SET) != 0) {
      iErr = printError(ERR_ACCESS_DEST_FILE, pszFileName, NULL);
      break;
    }
    if (fwrite(pbtBuff, 1, iLen, f) != (size_t) iLen) {
      iErr = printError(ERR_WRITE_DEST_FILE, pszFileName, NULL);
      break;
    }
  } while (FALSE);

  if (f != NULL)
    fclose(f);

  return iErr;
}
//---------------------------------------------------------------------------
int loadFile(const char *pszFileName, BYTE *pbtBuff, int iMaxLen)
{
  int iErr = ERR_OK, iLen;
  FILE *f;

  do {
    f = fopen(pszFileName, "rb");
    if (f == NULL) {
      iErr = printError(ERR_OPEN_SRC_FILE, pszFileName, NULL);
      break;
    }
    if (fseek(f, 0L, SEEK_END) != 0) {
      iErr = printError(ERR_ACCESS_SRC_FILE, pszFileName, NULL);
      break;
    }
    iLen = ftell(f);
    if (iLen < 0) {
      iErr = printError(ERR_LENGTH_SRC_FILE, pszFileName, NULL);
      break;
    }
    if (iLen > iMaxLen) {
      iErr = printError(ERR_SRC_FILE_TO_LONG, pszFileName, &iMaxLen);
      break;
    }
    if (fseek(f, 0L, SEEK_SET) != 0) {
      iErr = printError(ERR_ACCESS_SRC_FILE, pszFileName, NULL);
      break;
    }
    if (fread(pbtBuff, 1, iLen, f) != (size_t) iLen) {
      iErr = printError(ERR_READ_SRC_FILE, pszFileName, NULL);
      break;
    }
  } while (FALSE);

  if (f != NULL)
    fclose(f);

  if (iErr < ERR_OK)
    return iErr;

  return iLen;
}
//---------------------------------------------------------------------------
int copyFile(const char *pszFileNameDest, const char *pszFileNameSrc)
{
  int iErr, iLen;
  FILE *fiSrc, *fiDest;
  BYTE btBuff[BUFSIZ];
  size_t n;

  iErr = ERR_OK;
  fiSrc = NULL;
  fiDest = NULL;
  do {
    // vymaz existujuci subor
    if (remove(pszFileNameDest) != 0 && errno == EACCES) {
      iErr = printError(ERR_ACCESS_DEST_FILE, pszFileNameDest, NULL);
      break;
    }

    // vytvor cielovy subor
    fiDest = fopen(pszFileNameDest, "wb");
    if (!fiDest) {
      iErr = printError(ERR_OPEN_DEST_FILE, pszFileNameDest, NULL);
      break;
    }

    // otvor zdrojovy subor
    fiSrc = fopen(pszFileNameSrc, "rb");
    if (!fiSrc) {
      iErr = printError(ERR_OPEN_SRC_FILE, pszFileNameSrc, NULL);
      break;
    }
    // zisti dlzku suboru
    if (fseek(fiSrc, 0L, SEEK_END) != 0) {
      iErr = printError(ERR_ACCESS_SRC_FILE, pszFileNameSrc, NULL);
      break;
    }
    iLen = ftell(fiSrc);
    if (iLen < 0) {
      iErr = printError(ERR_LENGTH_SRC_FILE, pszFileNameSrc, NULL);
      break;
    }
    if (fseek(fiSrc, 0L, SEEK_SET) != 0) {
      iErr = printError(ERR_ACCESS_SRC_FILE, pszFileNameSrc, NULL);
      break;
    }

    // skopiruj subor
    while (iLen > 0) {
      n = fread(btBuff, sizeof(BYTE), sizeof(btBuff), fiSrc);
      if (n > 0) {
        if (fwrite(btBuff, sizeof(BYTE), n, fiDest) != n) {
          iErr = printError(ERR_WRITE_DEST_FILE, pszFileNameDest, NULL);
          break;
        }
      }
      iLen -= n;
    }

  } while (FALSE);

  if (fiSrc)
    fclose(fiSrc);
  if (fiDest)
    fclose(fiDest);

  return iErr;
}
//---------------------------------------------------------------------------

