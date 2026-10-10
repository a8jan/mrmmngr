//---------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "common.h"
#if LINUX
  #include <unistd.h>
#endif
/*
#if WIN
  #include <dir.h>
#else
  #include <dirent.h>
#endif
*/
//---------------------------------------------------------------------------
// funkcne prototypy
int loadMrmInfoAndDir(WORK_PARAMS *pWP);
int writeMrm(WORK_PARAMS *pWP, BOOL bLoadFilesContent);
int prepareDataFile(WORK_PARAMS *pWP, DIR_ITEM *pDI);
int loadFileContent(WORK_PARAMS *pWP, DATA_FILE *pDF, BOOL bFromBuffer);
int loadAllFilesContent(WORK_PARAMS *pWP);
DATA_FILE* findFile(WORK_PARAMS *pWP, BOOL bWildcards);
DIR_ITEM* findEmptyDirItem(WORK_PARAMS *pWP);
int verifyBootProlog(WORK_PARAMS *pWP, BOOL bInImage);

//---------------------------------------------------------------------------
// Vytvorenie MRM suboru.
int createMrm(WORK_PARAMS *pWP)
{
  int iErr, iDirLen, iDataLen, iSecs, ii, iOffset;

  do {
    printf("Creating MRM image: %s\n", pWP->pszFnImg);

    pWP->iImageSize =  pWP->iImageSizeReq;
    pWP->iDirItems =  pWP->iDirItemsReq;
    pWP->iDirSecCount = pWP->iDirItems / MRM_DIR_ITEMS_MIN;

    // vytvor MRM image
    iErr = openImageFile(pWP, TRUE);
    if (iErr != ERR_OK)
      break;

    // nacitaj `mrm-boot.bin` (alebo iny zvoleny subor)
    iErr = loadFile(pWP->szFnBoot, pWP->pbtBoot, MRM_BOOT_SIZE_MAX);
    if (iErr < ERR_OK)
      break;
    pWP->iBootLen = iErr;

    // kontrola prologu v `mrm-boot.bin`
    iErr = verifyBootProlog(pWP, FALSE);
    if (iErr != ERR_OK)
      break;

    // z dlzky suboru `mrm-boot.bin` urci prvy sektor adresara
    pWP->iFirstDirSec = (pWP->iBootLen + SECTOR_SIZE - 1) / SECTOR_SIZE;
    // ak nie je dlzka suboru `mrm-boot.bin` nasobkom velkosti sektora, vypln zbytok sektora vyplnovym bytom
    if (pWP->iFirstDirSec * SECTOR_SIZE > pWP->iBootLen) {
      memset(pWP->pbtBoot, MRM_BOOT_FILLER_BYTE, pWP->iFirstDirSec * SECTOR_SIZE - pWP->iBootLen);
      pWP->iBootLen = pWP->iFirstDirSec * SECTOR_SIZE;
    }

    // nastav 4 definicne byty v Prologu
    // - prvy sektor adresara
    *(pWP->pbtBoot + 12) = (BYTE) pWP->iFirstDirSec;
    // - pocet sektorov adresara
    *(pWP->pbtBoot + 13) = (BYTE) pWP->iDirSecCount;
    // - celkova velkost MRM obrazu v sektoroch -1
    *((WORD *) (pWP->pbtBoot + 14)) = (WORD) (pWP->iImageSize / SECTOR_SIZE - 1);

    // uloz `mrm-boot.bin` s pripravenymi definicnymi bytami
    iErr = writeToImage(pWP, pWP->pbtBoot, 0, pWP->iFirstDirSec * SECTOR_SIZE);
    if (iErr != ERR_OK)
      break;

    // vypocitaj dlzku adresara a datovej oblasti MRM
    iDirLen = pWP->iDirItems * sizeof(DIR_ITEM);
    iDataLen = pWP->iImageSize - (pWP->iBootLen + iDirLen);

    // zapis zbytok obrazu disku

    // adresar
    memset(pWP->pbtBuff, MRM_DIR_FILLER_BYTE, SECTOR_SIZE);
    iSecs = iDirLen / SECTOR_SIZE;
    iOffset = pWP->iFirstDirSec * SECTOR_SIZE;
    for (ii = 0; ii < iSecs; ii++) {
      iErr = writeToImage(pWP, pWP->pbtBuff, iOffset, SECTOR_SIZE);
      if (iErr != ERR_OK)
        break;
      iOffset += SECTOR_SIZE;
    }
    if (iErr != ERR_OK)
      break;

    // datova oblast
    memset(pWP->pbtBuff, MRM_DATA_FILLER_BYTE, SECTOR_SIZE);
    iSecs = iDataLen / SECTOR_SIZE;
    for (ii = 0; ii < iSecs; ii++) {
      iErr = writeToImage(pWP, pWP->pbtBuff, iOffset, SECTOR_SIZE);
      if (iErr != ERR_OK)
        break;
      iOffset += SECTOR_SIZE;
    }
    if (iErr != ERR_OK)
      break;

    printf("MRM image size: %g kB (%d sectors)\n", pWP->iImageSize / 1024.0, pWP->iImageSize / SECTOR_SIZE);
    printf("MRM BOOT file: %s\n", pWP->szFnBoot);
    printf("MRM Boot size: %d bytes\n", pWP->iBootLen);
    printf("Directory items: %d (%d bytes)\n", pWP->iDirItems, iDirLen);
    printf("Capacity: %g kB (%d secotrs)\n", iDataLen / 1024.0, iDataLen / SECTOR_SIZE);

  } while (FALSE);

  return iErr;
}
//---------------------------------------------------------------------------
// Reconfiguracia geometrie MRM alebo zmena BOOT kodu.
int reconfigureMrm(WORK_PARAMS *pWP)
{
  int iErr;
  int iDirLenCur, iDataLenCur;
  int iBootLenNew, iFirstDirSecNew, iDirLenNew, iImageSizeNew, iDataLenNew;
  char szBackupFile[256];

  do {
    printf("Reconfiguring MRM image: %s\n", pWP->pszFnImg);

    // otvor MRM obraz
    iErr = openImageFile(pWP, FALSE);
    if (iErr != ERR_OK)
      break;

    // precitaj adresar
    iErr = loadMrmInfoAndDir(pWP);
    if (iErr != ERR_OK)
      break;

    // kontrola, ci sa vobec ma nieco zmenit
    if (!pWP->bBoot
        && pWP->iDirItems == pWP->iDirItemsReq
        && pWP->iImageSize == pWP->iImageSizeReq) {
      printf("No changes in Image Geometry nor Boot code!\n");
      break;
    }

    // vypisanie pozadovanych zmien
    if (pWP->bBoot)
      printf("  New MRM BOOT file: %s\n", pWP->szFnBoot);
    if (pWP->iDirItems != pWP->iDirItemsReq)
      printf("  Directory items: %d -> %d\n", pWP->iDirItems, pWP->iDirItemsReq);
    if (pWP->iImageSize != pWP->iImageSizeReq)
      printf("  MRM image size: %g kB -> %g kB\n", pWP->iImageSize / 1024.0, pWP->iImageSizeReq / 1024.0);
    printf("\n");

    if (pWP->bBoot) {
      // nacitaj `mrm-boot.bin` (alebo iny zvoleny subor)
      iErr = loadFile(pWP->szFnBoot, pWP->pbtBoot, MRM_BOOT_SIZE_MAX);
      if (iErr < ERR_OK)
        break;
      iBootLenNew = iErr;
    }
    else {
      iErr = readFromImage(pWP, pWP->pbtBoot, 0, pWP->iBootLen);
      if (iErr < ERR_OK)
        break;
      iBootLenNew = pWP->iBootLen;
    }

    // kontrola prologu v `mrm-boot.bin`
    iErr = verifyBootProlog(pWP, FALSE);
    if (iErr != ERR_OK)
      break;

    // z dlzky suboru `mrm-boot.bin` urci prvy sektor adresara
    iFirstDirSecNew = (iBootLenNew + SECTOR_SIZE - 1) / SECTOR_SIZE;
    // ak nie je dlzka suboru `mrm-boot.bin` nasobkom velkosti sektora, vypln zbytok sektora vyplnovym bytom
    if (iFirstDirSecNew * SECTOR_SIZE > iBootLenNew) {
      memset(pWP->pbtBoot, MRM_BOOT_FILLER_BYTE, iFirstDirSecNew * SECTOR_SIZE - iBootLenNew);
      iBootLenNew = iFirstDirSecNew * SECTOR_SIZE;
    }

    if (pWP->bBoot
        && pWP->iDirItems == pWP->iDirItemsReq
        && pWP->iImageSize == pWP->iImageSizeReq
        && iBootLenNew == pWP->iBootLen) {
      // nie je zmena geometrie a ani velkost BOOT kodu, tak sa iba prepiseme BOOT kod novym

      // nastav 4 definicne byty v Prologu
      // - prvy sektor adresara
      *(pWP->pbtBoot + 12) = (BYTE) pWP->iFirstDirSec;
      // - pocet sektorov adresara
      *(pWP->pbtBoot + 13) = (BYTE) (pWP->iDirItems / MRM_DIR_ITEMS_MIN);
      // - celkova velkost MRM obrazu v sektoroch -1
      *((WORD *) (pWP->pbtBoot + 14)) = (WORD) (pWP->iImageSize / SECTOR_SIZE - 1);

      // uloz `mrm-boot.bin` s pripravenymi definicnymi bytami
      iErr = writeToImage(pWP, pWP->pbtBoot, 0, pWP->iFirstDirSec * SECTOR_SIZE);
      break;
    }

    // vypocitaj aktualnu dlzku adresara a datovej oblasti MRM
    iDirLenCur = pWP->iDirItems * sizeof(DIR_ITEM);
    iDataLenCur = (pWP->iFirstFreeSec * SECTOR_SIZE) - (pWP->iBootLen + iDirLenCur);
    // vypocitaj potrebnu velkost MRM pola nove dlzku adresara a BOOTu
    iDirLenNew = pWP->iDirItemsReq * sizeof(DIR_ITEM);
    iImageSizeNew = iDataLenCur + (iBootLenNew + iDirLenNew);
    iDataLenNew = pWP->iImageSizeReq - (iBootLenNew + iDirLenNew);

    // ak sa aktualny obsah nevojde do novej pozadovanej velkosti, je to chyba
    if ((pWP->iDirItems != pWP->iDirItemsReq && pWP->iTotalFiles > pWP->iDirItemsReq)
        || iImageSizeNew > pWP->iImageSizeReq) {
      iErr = printError(ERR_BAD_GEOMETRY, NULL, NULL);
      break;
    }

    // z MRM precitaj vsetky subory do pamate
    iErr = loadAllFilesContent(pWP);
    if (iErr != ERR_OK)
      return iErr;

    // uzatvor povodny MRM subor
    iErr = closeImageFile(pWP);
    if (iErr != ERR_OK)
      return iErr;

    // vyvor zaloznu kopiu MRM obrazu
    strcpy(szBackupFile, pWP->pszFnImg);
    strcat(szBackupFile, ".tmp");
    iErr = copyFile(szBackupFile, pWP->pszFnImg);
    if (iErr != ERR_OK)
      return iErr;

    // nanovo vytvor cielovy obraz MRM
    iErr = openImageFile(pWP, TRUE);
    if (iErr != ERR_OK)
      break;

    pWP->iBootLen = iBootLenNew;
    pWP->iFirstDirSec = iFirstDirSecNew;
    pWP->iDirItems = pWP->iDirItemsReq;
    pWP->iImageSize = pWP->iImageSizeReq;
    pWP->iDirSecCount = pWP->iDirItems / MRM_DIR_ITEMS_MIN;

    // nastav 4 definicne byty v Prologu
    // - prvy sektor adresara
    *(pWP->pbtBoot + 12) = (BYTE) pWP->iFirstDirSec;
    // - pocet sektorov adresara
    *(pWP->pbtBoot + 13) = (BYTE) pWP->iDirSecCount;
    // - celkova velkost MRM obrazu v sektoroch -1
    *((WORD *) (pWP->pbtBoot + 14)) = (WORD) (pWP->iImageSize / SECTOR_SIZE - 1);

    // uloz `mrm-boot.bin` s pripravenymi definicnymi bytami
    iErr = writeToImage(pWP, pWP->pbtBoot, 0, pWP->iFirstDirSec * SECTOR_SIZE);
    if (iErr != ERR_OK)
      break;

    // zapis adresar a subory do noveho MRM
    iErr = writeMrm(pWP, FALSE);
    if (iErr != ERR_OK)
      break;

    printf("MRM image size: %g kB (%d sectors)\n", pWP->iImageSize / 1024.0, pWP->iImageSize / SECTOR_SIZE);
    printf("MRM BOOT size: %d bytes\n", pWP->iBootLen);
    printf("Directory items: %d (%d bytes)\n", pWP->iDirItems, iDirLenNew);
    printf("Capacity: %g kB (%d secotrs)\n", iDataLenNew / 1024.0, iDataLenNew / SECTOR_SIZE);

  } while (FALSE);

  if (iErr == ERR_OK) {
    // vymaz zalozny subor
    if (remove(szBackupFile) != 0 && errno == EACCES)
      printError(ERR_ACCESS_DEST_FILE, szBackupFile, NULL);
  }

  return iErr;
}
//---------------------------------------------------------------------------
// Zobrazenie informacii o MRM obraze
int showMrmInfo(WORK_PARAMS *pWP)
{
  int iErr;
  int iDirLen, iDataLen, iUsedSpace, iFreeSpace;

  do {
    printf("MRM image info: %s\n", pWP->pszFnImg);

    // otvor MRM obraz
    iErr = openImageFile(pWP, FALSE);
    if (iErr != ERR_OK)
      break;

    // precitaj adresar
    iErr = loadMrmInfoAndDir(pWP);
    if (iErr != ERR_OK)
      break;

    iDirLen = pWP->iDirItems * sizeof(DIR_ITEM);
    iDataLen = pWP->iImageSize - pWP->iBootLen - iDirLen;
    iFreeSpace = pWP->iImageSize - pWP->iFirstFreeSec * SECTOR_SIZE;
    iUsedSpace = iDataLen - iFreeSpace;

    printf("MRM image size: %g kB (%d sectors)\n", pWP->iImageSize / 1024.0, pWP->iImageSize / SECTOR_SIZE);
    printf("MRM BOOT size: %d bytes\n", pWP->iBootLen);
    printf("Directory items: %d (%d bytes)\n", pWP->iDirItems, iDirLen);
    printf("Capacity: %g kB (%d secotrs)\n", iDataLen / 1024.0, iDataLen / SECTOR_SIZE);
    printf("\nFiles: %d\n", pWP->iTotalFiles);
    printf("Used space: %g kB\n", iUsedSpace / 1024.0);
    printf("Free space: %g kB", iFreeSpace / 1024.0);

  } while (FALSE);

  return iErr;
}
//---------------------------------------------------------------------------
// Vypisanie adresara MRM podla masky.
int printCatalogue(WORK_PARAMS *pWP)
{
  int iErr, ii, jj, iDispItems, iDispItemsLen, iFreeSpace;
  int iaItems[MRM_DIR_ITEMS_MAX];
  char cc;
  DIR_ITEM *pDI, *aDI;

  // otvor MRM obraz
  iErr = openImageFile(pWP, FALSE);
  if (iErr != ERR_OK)
    return iErr;

  // precitaj adresar
  iErr = loadMrmInfoAndDir(pWP);
  if (iErr != ERR_OK)
    return iErr;

  // spocitaj polozky adresara pre zobrazenie
  pWP->iFilesByMask = 0;
  pDI = (DIR_ITEM *) pWP->pbtDir;
  for (ii = 0; ii < pWP->iDirItems; ii++, pDI++) {
    // porovnaj meno suboru s maskou
    if (compareFileNameMask(pWP, pDI, TRUE))
      iaItems[pWP->iFilesByMask++] = ii;
  }

  printf("Catalogue of '%s', mask: '%s'\n\n", pWP->pszFnImg, pWP->szMaskPar);
  printf("Idx  T Name      Long name         Start           Size          AL\n");
  printf("-------------------------------------------------------------------\n");

  iDispItems = 0;
  iDispItemsLen = 0;
  aDI = (DIR_ITEM *) pWP->pbtDir;
  for (ii = 0; ii < pWP->iFilesByMask; ii++) {
    if (ii > 0 && (ii % 20) == 0)
      pressEnter();

    pDI = &aDI[iaItems[ii]];
    // index v adresari
    printf("%*d  ", 3, iaItems[ii] + 1);
    // typ
    cc = pDI->cType;
    if (!isValidChar(cc))
      cc = ' ';
    printf("%c ", cc);
    // name
    for (jj = 0; jj < 8; jj++) {
      cc = pDI->cName[jj];
      if (!isValidChar(cc))
        cc = ' ';
      printf("%c", cc);
    }
    printf("  ");
    // long name
    for (jj = 0; jj < 16; jj++) {
      cc = pDI->cLongName[jj];
      if (!isValidChar(cc))
        cc = ' ';
      printf("%c", cc);
    }
    // pociatocna adresa, dlzka, atributy
    printf("  %5d (%04Xh)  %5d (%04Xh)  %c%c\n", pDI->wMemAdr, pDI->wMemAdr, pDI->wLength, pDI->wLength,
           (pDI->btAttrs & 2) ? 'A' : '-', (pDI->btAttrs & 1) ? 'L' : '-');

    iDispItems++;
    iDispItemsLen += (((pDI->wLength + SECTOR_SIZE - 1) / SECTOR_SIZE) * SECTOR_SIZE);
  }

  iFreeSpace = pWP->iImageSize - pWP->iFirstFreeSec * SECTOR_SIZE;
  printf("\n%g kB in %d file(s), %g kB free", iDispItemsLen / 1024.0, iDispItems, iFreeSpace / 1024.0);

  return iErr;
}
//---------------------------------------------------------------------------
// Pridanie jedneho suboru do MRM.
int addFile(WORK_PARAMS *pWP)
{
  int iErr, iFileLen, iFileLenSec, iUnpackerFileLen, iNeedSpace;
  DATA_FILE *pDF;
  DIR_ITEM *pDI;
  char szFn[256], *p;

  // ak bol pouzity prepinac -fn, pouzije sa specifikovane meno
  if (*pWP->szMaskParN != '\0') {
    strcpy(pWP->szMaskPar, pWP->szMaskParN);
    strcpy(pWP->szMaskPrep, pWP->szMaskPrepN);
  }

  // otvor MRM image
  iErr = openImageFile(pWP, FALSE);
  if (iErr != ERR_OK)
    return iErr;

  // precitaj adresar
  iErr = loadMrmInfoAndDir(pWP);
  if (iErr != ERR_OK)
    return iErr;

  if (pWP->bReplaceFile)
    printf("Replacing file: %s\n", pWP->szMaskPar);
  else
    printf("Adding file: %s\n", pWP->szMaskPar);

  // vyhladaj, ci uz subor s danym menom existuje
  pWP->pDFfind = NULL;
  pDF = findFile(pWP, FALSE);
  if (pDF && !pWP->bOverwrite && !pWP->bReplaceFile) // ak existuje a nie je dovoleny prepis, je to chyba
    return printError(ERR_FILE_EXIST, pWP->szMaskPar, NULL);
  if (!pDF && pWP->bReplaceFile) // ak neexistuje a nahradzuje sa subor, je to chyba
    return printError(ERR_FILE_NOT_EXIST, pWP->szMaskPar, NULL);

  if (pDF && pWP->bOverwrite && !pWP->bReplaceFile)
    printf("File already exist, will be overwritten.\n");

  // pre pakovane data PSC obrazku treba najprv pripravit rozpakovaciu rutinu
  iUnpackerFileLen = 0;
  if (pWP->bPackedScreen) {
    // priprav cestu k `unpckpsc.bin`
    strcpy(szFn, pWP->szFnBoot);
    p = strrchr(szFn, '/');
  #if WIN
    if (p == NULL)
      p = strrchr(szFn, '\\');
  #endif
    if (p)
      *++p = '\0';
    else
      *szFn = '\0';
    strcat(szFn, MRM_UNPACK_FILE);

    // nacitaj Unpacker do buffra
    iErr = loadFile(szFn, pWP->pbtBuff, MRM_UNPACK_SIZE_MAX);
    if (iErr < ERR_OK)
      return iErr;
    iUnpackerFileLen = iErr;
  }

  // precitaj subor do buffra
  iErr = loadFile(pWP->szFilePath, pWP->pbtBuff + iUnpackerFileLen, BUFFER_SIZE - iUnpackerFileLen - 1);
  if (iErr < ERR_OK)
    return iErr;

  printf("Source file: %s\n", pWP->szFilePath);

  // dlzka suboru - vyplnenie oblasti posledneho sektoru vyplnovym bytom
  iFileLen = iErr + iUnpackerFileLen;
  iFileLenSec = (iFileLen + SECTOR_SIZE - 1) & ~(SECTOR_SIZE - 1);
  if (iFileLen != iFileLenSec)
    memset(pWP->pbtBuff + iFileLen, MRM_DATA_FILLER_BYTE, iFileLenSec - iFileLen);

  // kontrola na volne miesto pre novy subor
  iNeedSpace = iFileLenSec;
  if (pDF)
    iNeedSpace = iFileLenSec - (((int) pDF->pDI->wLength + SECTOR_SIZE - 1) & ~(SECTOR_SIZE - 1));
  if (iNeedSpace > 0 && pWP->iFirstFreeSec * SECTOR_SIZE + iNeedSpace > pWP->iImageSize) {
    iErr = printError(ERR_DISK_FULL, NULL, NULL);
    if (iErr != ERR_OK)
      return iErr;
  }

  if (!pDF) {
    // ak subor s danym menom neexistuje, musi existovat volna polozka v adresari
    pDI = findEmptyDirItem(pWP);
    if (!pDI)
      return printError(ERR_DIRECTORY_FULL, NULL, NULL);

    // vyplnenie struktury adresarovej polozky
    memcpy(pDI->cName, pWP->szMaskPrep, MRM_SHORT_NAME_LEN);
    pDI->cType = pWP->szMaskPrep[MRM_SHORT_NAME_LEN];
    memcpy(pDI->cLongName, pWP->szLongName, MRM_LONG_NAME_LEN);
    pDI->wSector = 0;
    pDI->wMemAdr = (WORD) pWP->iStartAdr;
    pDI->wLength = (WORD) iFileLen;
    pDI->btAttrs = ((pWP->bAttrList) ? DI_ATTR_LIST : 0)
                 | ((pWP->bAttrAllRam) ? DI_ATTR_ALLRAM : 0);

    // pripravenie datovej struktury
    iErr = prepareDataFile(pWP, pDI);
    if (iErr != ERR_OK)
      return iErr;
    pDF = pWP->pDFlast;
    pDF->iIndex = (pDF->pPrev) ? pDF->pPrev->iIndex + 1 : 0;
  }
  else {
    // ak subor s danym menom existuje, aktualizuje sa jeho adresarova polozka
    if (pWP->bAddFile) {
      memcpy(pDF->pDI->cName, pWP->szMaskPrep, MRM_SHORT_NAME_LEN);
      pDF->pDI->cType = pWP->szMaskPrep[MRM_SHORT_NAME_LEN];
      memcpy(pDF->pDI->cLongName, pWP->szLongName, MRM_LONG_NAME_LEN);
      pDF->pDI->wMemAdr = (WORD) pWP->iStartAdr;
      pDF->pDI->btAttrs = ((pWP->bAttrList) ? DI_ATTR_LIST : 0)
                        | ((pWP->bAttrAllRam) ? DI_ATTR_ALLRAM : 0);
    }
    pDF->pDI->wSector = 0;
    pDF->pDI->wLength = (WORD) iFileLen;
  }

  pDF->iFileOffset = 0;
  pDF->iSize = iFileLenSec;
  pDF->bNewContent = TRUE;

  // priprav obsah suboru
  iErr = loadFileContent(pWP, pDF, TRUE);
  if (iErr < ERR_OK)
    return iErr;

  // vypisanie informacii o pridavanom subore
  if (pWP->bAddFile) {
    if (pWP->bPackedScreen)
      printf("Unpack routine inserted before file\n");
    if (*pWP->szLongName != '\0')
      printf("Long name: %s\n", pWP->szLongName);
    printf("Size: %d bytes (%d sectors)\n", iFileLen, iFileLenSec / SECTOR_SIZE);
    printf("Start: %d\n", (int) pDF->pDI->wMemAdr);
    printf("Attributes: ");
    if (!pWP->bAttrList && !pWP->bAttrAllRam)
      printf("None");
    else {
      if (pWP->bAttrList)
        printf("List");
      if (pWP->bAttrList && pWP->bAttrAllRam)
        printf(", ");
      if (pWP->bAttrAllRam)
        printf("AllRAM");
      printf("\n");
    }
  }
  else
    printf("New size: %d bytes (%d sectors)\n", iFileLen, iFileLenSec / SECTOR_SIZE);

  // zapis suboru do MRM suboru
  iErr = writeMrm(pWP, TRUE);

  return iErr;
}
//---------------------------------------------------------------------------
int addFileCplx(WORK_PARAMS *pWP)
{
  int iErr, i;
  char szName[13];
  DIR_ITEM di;

  printf("Adding file with complex name.\n");

  // rozparsuj komplexne meno subooru
  iErr = parseComplexFileName(&di, pWP->szFilePath);
  if (iErr != ERR_OK)
    return iErr;

  // priprav meno 8.1
  strncpy(szName, di.cName, MRM_SHORT_NAME_LEN);
  szName[MRM_SHORT_NAME_LEN] = '\0';
  trim(szName);
  i = (int) strlen(szName);
  szName[i++] = '.';
  szName[i++] = di.cType;
  szName[i] = '\0';
  iErr = prepareFileName81(pWP, szName, "-fc", FALSE);
  if (iErr != ERR_OK)
    return iErr;

  // vymaz pripdne dodatocne meno
  *pWP->szMaskParN = '\0';
  *pWP->szMaskPrepN = '\0';
  // priprav dlhe meno
  strncpy(pWP->szLongName, di.cLongName, MRM_LONG_NAME_LEN);
  pWP->szLongName[MRM_LONG_NAME_LEN] = '\0';
  // nastav pociatocnu adresu
  pWP->iStartAdr = di.wMemAdr;
  // nastav atributy
  pWP->bAttrList = (di.btAttrs & DI_ATTR_LIST) ? TRUE : FALSE;
  pWP->bAttrAllRam = (di.btAttrs & DI_ATTR_ALLRAM) ? TRUE : FALSE;

  // a pridaj subor
  pWP->bAddFile = TRUE;
  iErr = addFile(pWP);

  return iErr;
}
//---------------------------------------------------------------------------
int addFileCplxList(WORK_PARAMS *pWP)
{
  int iErr = ERR_OK;
  char szNameCplx[256], *p;
  FILE *f;

  printf("Adding files from list stored in file: %s\n", pWP->szFilePath);

  do {
    f = fopen(pWP->szFilePath, "rt");
    if (f == NULL) {
      iErr = printError(ERR_OPEN_SRC_FILE, pWP->szFilePath, NULL);
      break;
    }

    while (TRUE) {
      printf("\n");

      memset(szNameCplx, 0, sizeof(szNameCplx));
      p = fgets(szNameCplx, sizeof(szNameCplx) - 1, f);
      if (p == NULL)  // koniec suboru
        break;

      trim(p);
      if (!*p)
        continue;

      // pridaj subor do MRM
      strcpy(pWP->szFilePath, p);
      iErr = addFileCplx(pWP);
      if (iErr != ERR_OK)
        break;
    }

  } while (FALSE);

  if (f != NULL)
    fclose(f);

  return iErr;
}
//---------------------------------------------------------------------------
// Zmena parametrov jedneho suboru.
int changeFileParameters(WORK_PARAMS *pWP)
{
  int iErr;
  DATA_FILE *pDF, *pDF2;

  // otvor MRM image
  iErr = openImageFile(pWP, FALSE);
  if (iErr != ERR_OK)
    return iErr;

  // precitaj adresar
  iErr = loadMrmInfoAndDir(pWP);
  if (iErr != ERR_OK)
    return iErr;

  printf("Changing parameters for file: %s\n", pWP->szMaskPar);

  // vyhladaj subor
  pWP->pDFfind = NULL;
  pDF = findFile(pWP, FALSE);
  if (!pDF) // ak neexistuje, je to chyba
    return printError(ERR_FILE_NOT_EXIST, pWP->szMaskPar, NULL);

  // ak bol pouzity prepinac -fn, zmeni sa meno
  if (*pWP->szMaskParN != '\0') {
    strcpy(pWP->szMaskPar, pWP->szMaskParN);
    strcpy(pWP->szMaskPrep, pWP->szMaskPrepN);

    printf("New name: %s\n", pWP->szMaskPar);

    // subor s pozadovanym novym menom nesmie existovat
    pWP->pDFfind = NULL;
    pDF2 = findFile(pWP, FALSE);
    if (pDF2)
      return printError(ERR_FILE_EXIST, pWP->szMaskPar, NULL);

    // zmen meno
    memcpy(pDF->pDI->cName, pWP->szMaskPrep, MRM_SHORT_NAME_LEN);
    pDF->pDI->cType = pWP->szMaskPrep[MRM_SHORT_NAME_LEN];
  }

  // zmena dlheho mena
  if (*pWP->szLongName != '\0') {
    printf("New long name: %s\n", pWP->szLongName);
    memcpy(pDF->pDI->cLongName, pWP->szLongName, MRM_LONG_NAME_LEN);
  }

  // zmena pociatocnej adresy
  if (pWP->iStartAdr != -1) {
    printf("New start: %d (%04Xh)\n", pWP->iStartAdr, pWP->iStartAdr);
    pDF->pDI->wMemAdr = (WORD) pWP->iStartAdr;
  }

  // zmena atributov
  if (pWP->bAttrList)
    pDF->pDI->btAttrs ^= DI_ATTR_LIST;
  if (pWP->bAttrAllRam)
    pDF->pDI->btAttrs ^= DI_ATTR_ALLRAM;

  if (pWP->bAttrList || pWP->bAttrAllRam) {
    printf("New attributes: ");
    if (!pDF->pDI->btAttrs)
      printf("None");
    else {
      if (pDF->pDI->btAttrs & DI_ATTR_LIST)
        printf("List");
      if ((pDF->pDI->btAttrs & (DI_ATTR_LIST | DI_ATTR_ALLRAM)) == (DI_ATTR_LIST | DI_ATTR_ALLRAM))
        printf(", ");
      if (pDF->pDI->btAttrs & DI_ATTR_ALLRAM)
        printf("AllRAM");
    }
    printf("\n");
  }

  iErr = writeToImage(pWP, (BYTE *) pDF->pDI, pWP->iFirstDirSec * SECTOR_SIZE + pDF->iIndex * sizeof(DIR_ITEM), sizeof(DIR_ITEM));

  return iErr;
}
//---------------------------------------------------------------------------
// Zmazanie suborov z MRM.
int deleteFiles(WORK_PARAMS *pWP)
{
  int iErr, iDeleted;
  DATA_FILE *pDF;
  char szName[16];

  // otvor MRM image
  iErr = openImageFile(pWP, FALSE);
  if (iErr != ERR_OK)
    return iErr;

  // precitaj adresar
  iErr = loadMrmInfoAndDir(pWP);
  if (iErr != ERR_OK)
    return iErr;

  printf("Delete files by mask: %s\n\n", pWP->szMaskPar);

  iDeleted = 0;
  pWP->pDFfind = NULL;
  while (TRUE) {
    // vyhladaj subor
    pDF = findFile(pWP, TRUE);
    if (!pDF) // skonci, ak sa uz dalsi subor podla masky nenasiel
      break;

    // vypis meno mazaneho suboru
    compactFileName(pDF->pDI, szName, FALSE);
    printf("Deleting file: %s\n", szName);

    // oznac subor, ako zmazany
    pDF->bDeleted = TRUE;
    iDeleted++;
  }

  if (iDeleted == 0)
    printf("No files found to delete by given mask!\n");
  else {
    // zapis novy obsah MRM
    iErr = writeMrm(pWP, TRUE);
    if (iErr != ERR_OK)
      return iErr;

    printf("\n%d file(s) deleted.\n", iDeleted);
  }

  return iErr;
}
//---------------------------------------------------------------------------
// Extrahovanie suborov z MRM.
int extractFiles(WORK_PARAMS *pWP)
{
  int iErr, iExtracted, i;
  DATA_FILE *pDF;
  char szFn[256], szName[256];
  FILE *fiCplx;

  // otvor MRM image
  iErr = openImageFile(pWP, FALSE);
  if (iErr != ERR_OK)
    return iErr;

  // precitaj adresar
  iErr = loadMrmInfoAndDir(pWP);
  if (iErr != ERR_OK)
    return iErr;

  printf("Extracting files by mask: %s\n", pWP->szMaskPar);
  printf("Target path: %s\n", pWP->szFilePath);

  // ak je to pozadovane, otvor subor pre zapis mien s komplexnymi menami suborov
  fiCplx = NULL;
  if (*pWP->szCplxListFn != '\0') {
    fiCplx = fopen(pWP->szCplxListFn, "wt");
    if (fiCplx == NULL) {
      iErr = printError(ERR_OPEN_DEST_FILE, pWP->szCplxListFn, NULL);
      return iErr;
    }
    printf("List file: %s\n", pWP->szCplxListFn);
  }
  printf("\n");

  iExtracted = 0;
  pWP->pDFfind = NULL;
  while (TRUE) {
    // vyhladaj subor
    pDF = findFile(pWP, TRUE);
    if (!pDF)
      break;

    // vypis meno extrahovaneho suboru
    compactFileName(pDF->pDI, szName, pWP->bExtrFilesCplx);
    printf("Extracting file: %s\n", szName);

    // precitaj obsah suboru z MRM do buffra
    iErr = loadFileContent(pWP, pDF, FALSE);
    if (iErr != ERR_OK)
      break;

    // priprav meno cieloveho suboru na disku
    memset(szFn, 0, 256);
    strcpy(szFn, pWP->szFilePath);
    strcat(szFn, szName);
    i = (int) strlen(szFn);
    if (isMetaChar(szFn[i - 1], FALSE)) {
      szFn[i - 1] = '\0';
      strcat(szFn, "bin");
    }

    // uloz subor
    iErr = saveFile(szFn, pDF->pbtContent, (int) pDF->pDI->wLength, FALSE);
    if (iErr != ERR_OK)
      break;

    // ak je to pozadovane, zapis komplexne meno tohto suboru do suboru so zoznamom
    if (fiCplx) {
      if (fprintf(fiCplx, "./%s\n", szName) == EOF) {
        iErr = printError(ERR_WRITE_DEST_FILE, pWP->szCplxListFn, NULL);
        if (iErr != ERR_OK)
          break;
      }
    }

    iExtracted++;
  }

  if (fiCplx)
    fclose(fiCplx);

  if (iErr != ERR_OK)
    return iErr;

  if (iExtracted == 0)
    printf("No files found to extract by given mask!\n");
  else
    printf("\n%d file(s) extracted.\n", iExtracted);

  return iErr;
}
//---------------------------------------------------------------------------
// Vytvorenie a pridanie suboru Loadera
int makeLoader(WORK_PARAMS *pWP)
{
  int iErr, iLdrLen;
  char szFn[256], *p;

  printf("Creating and adding Loader file into image: %s\n", pWP->szMaskPar);

  strcpy(szFn, pWP->szFnBoot);
  p = strrchr(szFn, '/');
#if WIN
  if (p == NULL)
    p = strrchr(szFn, '\\');
#endif
  if (p)
    *++p = '\0';
  else {
    p = szFn;
    *p = '\0';
  }
  strcat(szFn, MRM_LOADER_FILE);

  // nacitaj Loader do buffra
  iErr = loadFile(szFn, pWP->pbtBuff, MRM_LOADER_SIZE_MAX);
  if (iErr < ERR_OK)
    return iErr;
  iLdrLen = iErr;

  printf("Picture file: %s\n", pWP->szMaskParP);
  printf("Code file: %s\n", pWP->szMaskParN);
  if (pWP->iStartAdr >= 0)
    printf("Custom start: %d (%04Xh)\n", pWP->iStartAdr, pWP->iStartAdr);
  if (pWP->iLdrConf & 3) {
    printf("Load Monitor: ");
    if ((pWP->iLdrConf & 3) == 3)
      printf("%s", pWP->szMaskParM);
    else if (pWP->iLdrConf & 1)
      printf("MONIT1.?");
    else
      printf("MONIT2.?");
    if (pWP->iLdrConf & 4 && (pWP->iLdrConf & 3) != 3)
      printf(" (No Init)");
    printf("\n");
  }
  if (pWP->bSuppresCLS)
    printf("CLS suppressed\n");
  printf("\n");

  // meno suboru obrazku
  *(pWP->pbtBuff + 3) = pWP->szMaskPrepP[MRM_SHORT_NAME_LEN];
  memcpy(pWP->pbtBuff + 4, pWP->szMaskPrepP, MRM_SHORT_NAME_LEN);
  // meno suboru kodu hry
  *(pWP->pbtBuff + 12) = pWP->szMaskPrepN[MRM_SHORT_NAME_LEN];
  memcpy(pWP->pbtBuff + 13, pWP->szMaskPrepN, MRM_SHORT_NAME_LEN);
  // start adresa
  if (pWP->iStartAdr >= 0)
    *((WORD *) (pWP->pbtBuff + 21)) = (WORD) pWP->iStartAdr;
  // konfiguracia Loadera
  if (pWP->bSuppresCLS)
    pWP->iLdrConf |= 8;
  *(pWP->pbtBuff + 23) = (BYTE) pWP->iLdrConf;
  // meno suboru Monitora
  if (*pWP->szMaskParM != '\0') {
    *(pWP->pbtBuff + 24) = pWP->szMaskPrepM[MRM_SHORT_NAME_LEN];
    memcpy(pWP->pbtBuff + 25, pWP->szMaskPrepM, MRM_SHORT_NAME_LEN);
  }

  // uloz pripraveny Loader do docasneho suboru
#if WIN
  *p = '\0';
  tmpnam(p); // vytvor unikatne docasne meno
#else
  {
    // tmpnam() vracia v Linuxe absolutnu cestu,
    // pouzi mkstemp() v $TMPDIR alebo /tmp
    const char *pszTmpDir = getenv("TMPDIR");
    int fd;

    if (pszTmpDir == NULL || *pszTmpDir == '\0')
      pszTmpDir = "/tmp";
    snprintf(szFn, sizeof(szFn), "%s/mrmldrXXXXXX", pszTmpDir);
    fd = mkstemp(szFn); // vytvor unikatny docasny subor
    if (fd < 0)
      return printError(ERR_OPEN_DEST_FILE, szFn, NULL);
    close(fd);
  }
#endif
  iErr = saveFile(szFn, pWP->pbtBuff, iLdrLen, FALSE);
  if (iErr != ERR_OK) {
    remove(szFn);
    return iErr;
  }

  // pridaj subor Loadera do obrazu
  strcpy(pWP->szFilePath, szFn);  // meno zdrojoveho suboru je docasny subor s Loaderom
  *pWP->szMaskParN = '\0';        // vymaz meno suboru kodu hry, aby sa
  *pWP->szMaskPrepN = '\0';       // nepouzilo pri pridavani
  pWP->iStartAdr = 0x7F00;        // pociatocna adresa
  pWP->bAttrList = TRUE;          // subor bude v zozname

  pWP->bAddFile = TRUE;           // budeme pridavat
  iErr = addFile(pWP);

  // odstran docasny subor
  remove(szFn);

  return iErr;
}
//---------------------------------------------------------------------------
// Zobrazenie informacii o Loaderi
int showLoaderInfo(WORK_PARAMS *pWP)
{
  int iErr, iVal;
  DATA_FILE *pDF;
  BYTE *pbt;
  char szStr[16];

  // otvor MRM image
  iErr = openImageFile(pWP, FALSE);
  if (iErr != ERR_OK)
    return iErr;

  // precitaj adresar
  iErr = loadMrmInfoAndDir(pWP);
  if (iErr != ERR_OK)
    return iErr;

  printf("Loader file info: %s\n", pWP->szMaskPar);

  // vyhladaj subor
  pWP->pDFfind = NULL;
  pDF = findFile(pWP, FALSE);
  if (!pDF) // ak neexistuje, je to chyba
    return printError(ERR_FILE_NOT_EXIST, pWP->szMaskPar, NULL);

  iErr = loadFileContent(pWP, pDF, FALSE);
  if (iErr != ERR_OK)
    return iErr;
  pbt = pDF->pbtContent;

  if (*pbt != 0xC3 || *(pbt + 2) != (pDF->pDI->wMemAdr >> 8) || pDF->pDI->btAttrs != DI_ATTR_LIST)
    return printError(ERR_INVALID_LOADER, pWP->szMaskPar, NULL);

  printf("Directory index: %d\n", pDF->iIndex);
  printf("Loader file size: %d bytes\n", (int) pDF->pDI->wLength);
  printf("Loader load address: %d (%04Xh)\n", (int) pDF->pDI->wMemAdr, (int) pDF->pDI->wMemAdr);
  // subor obrazku
  memset(szStr, 0, sizeof(szStr));
  memcpy(szStr, pbt + 4, 8);
  trim(szStr);
  printf("Picture file: %s.%c\n", szStr, (char) *(pbt + 3));
  // subor hlavneho kodu
  memset(szStr, 0, sizeof(szStr));
  memcpy(szStr, pbt + 13, 8);
  trim(szStr);
  printf("Code file: %s.%c\n", szStr, (char) *(pbt + 12));
  // ina start adresa
  iVal = (int) *((WORD *) (pbt + 21));
  if (iVal != 0xFFFF)
    printf("Code custom start: %d (%04Xh)\n", iVal, iVal);
  // konfiguracne bity
  iVal = (int) *(pbt + 23);
  if (iVal & 3) {
    printf("Load Monitor: ");
    if ((iVal & 3) == 3) {
      // vlastny Monitor
      memset(szStr, 0, sizeof(szStr));
      memcpy(szStr, pbt + 25, 8);
      trim(szStr);
      printf("%s.%c", szStr, (char) *(pbt + 24));
    }
    else if (iVal & 1)
      printf("MONIT1.?");
    else
      printf("MONIT2.?");
    if (iVal & 4 && (iVal & 3) != 3)
      printf(" (No Init)");
    printf("\n");
  }
  if (iVal & 8)
    printf("CLS suppressed\n");

  return iErr;
}
//---------------------------------------------------------------------------
// Ziskanie informacii o MRM a precitanie adresara.
int loadMrmInfoAndDir(WORK_PARAMS *pWP)
{
  int iErr, ii, iSec;
  DIR_ITEM *pDI;

  do {
    // precitaj prolog MRM disku
    iErr = readFromImage(pWP, pWP->pbtBoot, 0, 16);
    if (iErr != ERR_OK)
      break;

    // kontrola prologu
    iErr = verifyBootProlog(pWP, TRUE);
    if (iErr != ERR_OK)
      break;

    // precitaj z prologu definicne data
    pWP->iFirstDirSec = (int) *(pWP->pbtBoot + 12);
    pWP->iDirSecCount = (int) *(pWP->pbtBoot + 13);
    pWP->iDirItems = pWP->iDirSecCount * MRM_DIR_ITEMS_MIN;
    pWP->iImageSize = (((int) *((WORD *) (pWP->pbtBoot + 14))) + 1) * SECTOR_SIZE;
    pWP->iBootLen = pWP->iFirstDirSec * SECTOR_SIZE;
    pWP->iFirstFreeSec = pWP->iFirstDirSec + pWP->iDirSecCount;

    // precitaj adresar
    iErr = readFromImage(pWP, pWP->pbtDir, pWP->iFirstDirSec * SECTOR_SIZE, pWP->iDirItems * sizeof(DIR_ITEM));
    if (iErr != ERR_OK)
      break;

    // uvolni existujuce DATA_FILE struktury a alokovanu pamat
    freeDataFiles(pWP);

    // spracuj adresarove polozky
    pWP->iTotalFiles = 0;
    pDI = (DIR_ITEM *) pWP->pbtDir;
    for (ii = 0; ii < pWP->iDirItems; ii++, pDI++) {
      // iba platne polozky
      if (!isValidChar(pDI->cType) || !isValidChar(*pDI->cName))
        continue;

      // priprav datovu strukturu suboru
      iErr = prepareDataFile(pWP, pDI);
      if (iErr != ERR_OK)
        break;
      pWP->pDFlast->iIndex = ii;

      // aktualizuj dalsie cislo sektoru a celkovy pocet suborov
      iSec = pDI->wSector + (pDI->wLength + SECTOR_SIZE - 1) / SECTOR_SIZE;
      if (iSec > pWP->iFirstFreeSec)
        pWP->iFirstFreeSec = iSec;
      pWP->iTotalFiles++;
    }

  } while (FALSE);

  return iErr;
}
//---------------------------------------------------------------------------
// Zapisanie suborov do MRM.
int writeMrm(WORK_PARAMS *pWP, BOOL bLoadFilesContent)
{
  int iErr = ERR_OK, iDirOffset, iDataOffset, iDirIdx;
  DIR_ITEM *pDI;
  DATA_FILE *pDF;

  if (bLoadFilesContent) {
    // naitanie obsahov suborov do pamate, kedze sa obraz moze uplne zmenit pri mazani suborov
    iErr = loadAllFilesContent(pWP);
    if (iErr != ERR_OK)
      return iErr;
  }

  // priprava offsetov pre zapis adresara a dat suborov
  iDirIdx = 0;
  iDirOffset = pWP->iFirstDirSec * SECTOR_SIZE;
  iDataOffset = (pWP->iFirstDirSec + pWP->iDirSecCount) * SECTOR_SIZE;
  pDF = pWP->pDFfirst;
  while (pDF) {
    if (!pDF->bDeleted) { // zmazane subory sa vynechaju
      // zapis adresarovej polozky
      pDI = pDF->pDI;
      pDI->wSector = (WORD) (iDataOffset / SECTOR_SIZE);
      iErr = writeToImage(pWP, (BYTE *) pDI, iDirOffset, sizeof(DIR_ITEM));
      if (iErr != ERR_OK)
        break;
      iDirOffset += sizeof(DIR_ITEM);

      // zapis obsahu suboru do datovej oblasti
      pDF->iFileOffset = iDataOffset;
      iErr = writeToImage(pWP, pDF->pbtContent, iDataOffset, pDF->iSize);
      if (iErr != ERR_OK)
        break;
      iDataOffset += pDF->iSize;

      iDirIdx++;
    }
    pDF = pDF->pNext;
  }
  if (iErr != ERR_OK)
    return iErr;

  // vyplnenie nevyuzitych poloziek adresara vyplnovym bytom
  memset(pWP->pbtBuff, MRM_DIR_FILLER_BYTE, sizeof(DIR_ITEM));
  while (iDirIdx < pWP->iDirItems) {
    iErr = writeToImage(pWP, pWP->pbtBuff, iDirOffset, sizeof(DIR_ITEM));
    if (iErr != ERR_OK)
      break;
    iDirOffset += sizeof(DIR_ITEM);
    iDirIdx++;
  }
  if (iErr != ERR_OK)
    return iErr;

  // vyplnenie nevyuzitej datovej oblasti vyplnovym bytom
  memset(pWP->pbtBuff, MRM_DATA_FILLER_BYTE, SECTOR_SIZE);
  while (iDataOffset < pWP->iImageSize) {
    iErr = writeToImage(pWP, pWP->pbtBuff, iDataOffset, SECTOR_SIZE);
    if (iErr != ERR_OK)
      break;
    iDataOffset += SECTOR_SIZE;
  }

  return iErr;
}
//---------------------------------------------------------------------------
void freeDataFiles(WORK_PARAMS *pWP)
{
  DATA_FILE *pDF1, *pDF2;

  pDF1 = pWP->pDFfirst;
  while (pDF1) {
    pDF2 = pDF1->pNext;
    if (pDF1->pbtContent)
      free(pDF1->pbtContent);
    free(pDF1);
    pDF1 = pDF2;
  }
  pWP->pDFfind = NULL;
  pWP->pDFlast = NULL;
  pWP->pDFfind = NULL;
}
//---------------------------------------------------------------------------
// Pripravenie datovej struktury suboru.
int prepareDataFile(WORK_PARAMS *pWP, DIR_ITEM *pDI)
{
  DATA_FILE *pDF;
  int iErr;

  // alokuj pamat pre novu datovu strukturu suboru
  pDF = NULL; // dolezite !!!
  iErr = allocBuffer((void *) &pDF, sizeof(DATA_FILE));
  if (iErr != ERR_OK)
    return iErr;

  // vypln datovu strukturu suboru
  pDF->pDI = pDI;
  pDF->iIndex = -1;
  pDF->iFileOffset = pDI->wSector * SECTOR_SIZE;
  pDF->iSize = (pDI->wLength + SECTOR_SIZE - 1) & ~(SECTOR_SIZE - 1);
  pDF->pbtContent = NULL;
  pDF->pPrev = pWP->pDFlast;
  pDF->pNext = NULL;
  if (pWP->pDFlast) {
    pWP->pDFlast->pNext = pDF;
    pWP->pDFlast = pDF;
  }
  else {
    pWP->pDFfirst = pDF;
    pWP->pDFlast = pDF;
  }

  return ERR_OK;
}
//---------------------------------------------------------------------------
// Nacitanie obsahu suboru z MRM do buffra.
int loadFileContent(WORK_PARAMS *pWP, DATA_FILE *pDF, BOOL bFromBuffer)
{
  int iErr;

  // uvolni pripadne existujuci buffer
  if (pDF->pbtContent)
    free(pDF->pbtContent);
  pDF->pbtContent = NULL;

  // alokuj buffer pre obsah suboru
  iErr = allocBuffer((void *) &pDF->pbtContent, pDF->iSize);
  if (iErr != ERR_OK)
    return iErr;

  // nacitaj obsah suboru z buffra alebo z MRM
  if (bFromBuffer)
    memcpy(pDF->pbtContent, pWP->pbtBuff, pDF->iSize);
  else
    iErr = readFromImage(pWP, pDF->pbtContent, pDF->iFileOffset, pDF->iSize);

  return iErr;
}
//---------------------------------------------------------------------------
// Nacitanie obsahu vsetkych suborov.
int loadAllFilesContent(WORK_PARAMS *pWP)
{
  int iErr;
  DATA_FILE *pDF;

  iErr = ERR_OK;
  pDF = pWP->pDFfirst;
  while (pDF) {
    if (!pDF->bNewContent && !pDF->bDeleted) {
      iErr = loadFileContent(pWP, pDF, FALSE);
      if (iErr != ERR_OK)
        break;
    }
    pDF = pDF->pNext;
  }

  return iErr;
}
//---------------------------------------------------------------------------
// Vyhladanie suboru podla masky.
DATA_FILE* findFile(WORK_PARAMS *pWP, BOOL bWildcards)
{
  DATA_FILE *pDF;
  DIR_ITEM *pDI;

  if (pWP->pDFfind == FIND_END)
    return NULL;

  pDF = (pWP->pDFfind) ? pWP->pDFfind : pWP->pDFfirst;
  while (pDF) {
    pDI = pDF->pDI;
    // porovnaj meno suboru s maskou
    if (compareFileNameMask(pWP, pDI, bWildcards)) {
      pWP->pDFfind = pDF->pNext;
      if (!pWP->pDFfind)
        pWP->pDFfind = FIND_END;
      break;
    }
    pDF = pDF->pNext;
  }

  return pDF;
}
//---------------------------------------------------------------------------
// Vyhladanie volnej adresarovej polozky.
DIR_ITEM* findEmptyDirItem(WORK_PARAMS *pWP)
{
  int ii;
  DIR_ITEM *pDI;

  pDI = (DIR_ITEM *) pWP->pbtDir;
  for (ii = 0; ii < pWP->iDirItems; ii++, pDI++) {
    // najdi neobsadenu polozku
    if (!isValidChar(pDI->cType) || !isValidChar(*pDI->cName))
      break;
  }

  return (ii < pWP->iDirItems) ? pDI : NULL;
}
//---------------------------------------------------------------------------
// Kontrola prologu MRM Bootu.
int verifyBootProlog(WORK_PARAMS *pWP, BOOL bInImage)
{
  int iErr = ERR_OK;
  BYTE btaProlog[] = { 0xCD, 0x00, 0x8C, 0x10, 0x00 };

  // kontrola prologu `mrm-boot.bin`
  if (memcmp(pWP->pbtBoot, btaProlog, sizeof(btaProlog)) != 0
      || *(pWP->pbtBoot + 9) != 0xC3) {
    iErr = printError(ERR_INVALID_BOOT_BIN, (bInImage) ? NULL : pWP->szFnBoot, NULL);
  }

  return iErr;
}
//---------------------------------------------------------------------------

