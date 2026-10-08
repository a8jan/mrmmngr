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
int processArguments(int argc, char* argv[], WORK_PARAMS **ppWP);
void version(void);
void usage(void);

//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  WORK_PARAMS *pWP = NULL;
  int iErr;

  do {
    version();

    if (argc <= 2) {
      iErr = ERR_PRINT_USAGE;
      break;
    }

    // spracovanie a kontrola vstupnych parametrov
    iErr = processArguments(argc, argv, &pWP);
    if (iErr != ERR_OK)
      break;

    // alokovanie bufffrov

    iErr = allocBuffer((void *) &pWP->pbtBuff, BUFFER_SIZE);
    if (iErr != ERR_OK)
      break;

    iErr = allocBuffer((void *) &pWP->pbtBoot, MRM_BOOT_SIZE_MAX);
    if (iErr != ERR_OK)
      break;

    iErr = allocBuffer((void *) &pWP->pbtDir, MRM_DIR_SIZE);
    if (iErr != ERR_OK)
      break;

    // vykonanie pozadovanej operacie

    if (pWP->bCatalogue)
      iErr = printCatalogue(pWP);
    else if (pWP->bCreateMrm)
      iErr = createMrm(pWP);
    else if (pWP->bShowMrmInfo)
      iErr = showMrmInfo(pWP);
    else if (pWP->bReconfigureMrm)
      iErr = reconfigureMrm(pWP);
    else if (pWP->bAddFile || pWP->bReplaceFile)
      iErr = addFile(pWP);
    else if (pWP->bAddFileCplx)
      iErr = addFileCplx(pWP);
    else if (pWP->bAddFileCplxLst)
      iErr = addFileCplxList(pWP);
    else if (pWP->bChangeFilePar)
      iErr = changeFileParameters(pWP);
    else if (pWP->bDeleteFiles)
      iErr = deleteFiles(pWP);
    else if (pWP->bExtractFiles)
      iErr = extractFiles(pWP);
    else if (pWP->bMakeLoader)
      iErr = makeLoader(pWP);
    else if (pWP->bShowLoaderInfo)
      iErr = showLoaderInfo(pWP);

  } while (FALSE);

  if (iErr == ERR_PRINT_USAGE) {
    usage();
    iErr = 0;
  }
  else if (!iErr)
    printf("\n");

  if (pWP) {
    // uzatvorenie MRM obrazu
    closeImageFile(pWP);

    // uvolnenie alokovanej pamate
    freeDataFiles(pWP);
    if (pWP->pbtDir)
      free(pWP->pbtDir);
    if (pWP->pbtBoot)
      free(pWP->pbtBoot);
    if (pWP->pbtBuff)
      free(pWP->pbtBuff);
    if (pWP)
      free(pWP);
  }

#ifdef _DEBUG
  getchar();
#endif

  return -iErr;
}
//---------------------------------------------------------------------------
int processArguments(int argc, char* argv[], WORK_PARAMS **ppWP)
{
  int iErr, i, iV;
  WORK_PARAMS *pWP = NULL;
  char *p, szStr[256];

  // spracovanie a kontrola vstupnych parametrov
  do {
    *ppWP = NULL;
    iErr = allocBuffer((void *) &pWP, sizeof(WORK_PARAMS));
    if (iErr != ERR_OK)
      break;
    *ppWP = pWP;

    // meno obrazu MRM musi byt prvym parametrom
    pWP->pszFnImg = argv[1];

    // inicializacia niektorych pracovnych parametrov
    pWP->iImageSizeReq = -1;
    pWP->iDirItemsReq = -1;
    pWP->iStartAdr = -1;
    pWP->iLdrConf = -1;

    // spracovanie prepinacov
    for (i = 2; i < argc; i++) {
      if (strcmp(argv[i], "-c") == 0) {
        pWP->bCatalogue = TRUE;
        if (++i == argc)
          iErr = prepareFileName81(pWP, "*", argv[i - 1], TRUE);
        else
          iErr = prepareFileName81(pWP, argv[i], argv[i - 1], TRUE);
        if (iErr != ERR_OK)
          break;
      }
      else if (strcmp(argv[i], "-ic") == 0) {
        pWP->bCreateMrm = TRUE;
      }
      else if (strcmp(argv[i], "-ir") == 0) {
        pWP->bReconfigureMrm = TRUE;
      }
      else if (strcmp(argv[i], "-ii") == 0) {
        pWP->bShowMrmInfo = TRUE;
      }
      else if (strcmp(argv[i], "-ib") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        if (*pWP->szFnBoot != '\0') {
          iErr = printError(ERR_BOOT_FILE_MORE, pWP->szFnBoot, argv[i]);
          break;
        }
        strcpy(pWP->szFnBoot, argv[i]);
        pWP->bBoot = TRUE;
      }
      else if (strcmp(argv[i], "-is") == 0) {
        if (++i == argc)
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
        else {
          pWP->iImageSizeReq = parseNumber(argv[i]);
          if (pWP->iImageSizeReq < MRM_SIZE_KB_MIN || pWP->iImageSizeReq > MRM_SIZE_KB_MAX
              || (pWP->iImageSizeReq % MRM_SIZE_KB_MIN) != 0) {
            iErr = printError(ERR_BAD_VALUE, argv[i - 1], argv[i]);
            break;
          }
          pWP->iImageSizeReq *= 1024;
        }
      }
      else if (strcmp(argv[i], "-id") == 0) {
        if (++i == argc)
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
        else {
          pWP->iDirItemsReq = parseNumber(argv[i]);
          if (pWP->iDirItemsReq < MRM_DIR_ITEMS_MIN || pWP->iDirItemsReq > MRM_DIR_ITEMS_MAX
              || (pWP->iDirItemsReq % MRM_DIR_ITEMS_MIN) != 0) {
            iErr = printError(ERR_BAD_VALUE, argv[i - 1], argv[i]);
            break;
          }
        }
      }
      else if (strcmp(argv[i], "-fa") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        if (*pWP->szFilePath != '\0') {
          iErr = printError(ERR_INPUT_FILE_MORE, pWP->szFilePath, argv[i]);
          break;
        }
        iErr = prepareFilePath(pWP, argv[i], "-fa", FALSE, TRUE);
        if (iErr != ERR_OK)
          break;
        pWP->bAddFile = TRUE;
      }
      else if (strcmp(argv[i], "-fr") == 0) {
        ++i;
        if ((i + 1) >= argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }

        iErr = prepareFileName81M(argv[i], argv[i - 1], FALSE, pWP->szMaskPrepN, pWP->szMaskParN);
        if (iErr != ERR_OK)
          break;

        iErr = prepareFilePath(pWP, argv[i + 1], "-fr", FALSE, TRUE);
        if (iErr != ERR_OK)
          break;
        i++;
        pWP->bReplaceFile = TRUE;
      }
      else if (strcmp(argv[i], "-fp") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iErr = prepareFileName81(pWP, argv[i], argv[i - 1], FALSE);
        if (iErr != ERR_OK)
          break;
        pWP->bChangeFilePar = TRUE;
      }
      else if (strcmp(argv[i], "-fd") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iErr = prepareFileName81(pWP, argv[i], argv[i - 1], TRUE);
        if (iErr != ERR_OK)
          break;
        pWP->bDeleteFiles = TRUE;
      }
      else if (strcmp(argv[i], "-fx") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iErr = prepareFilePath(pWP, argv[i], "-fx", TRUE, FALSE);
        if (iErr != ERR_OK)
          break;
        pWP->bExtractFiles = TRUE;
      }
      else if (strcmp(argv[i], "-fc") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iErr = prepareFilePath(pWP, argv[i], "-fc", FALSE, TRUE);
        if (iErr != ERR_OK)
          break;
        pWP->bAddFileCplx = TRUE;
      }
      else if (strcmp(argv[i], "-fl") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iErr = prepareFilePath(pWP, argv[i], "-fl", FALSE, TRUE);
        if (iErr != ERR_OK)
          break;
        pWP->bAddFileCplxLst = TRUE;
      }
      else if (strcmp(argv[i], "-ml") == 0) {
        ++i;
        if ((i + 2) >= argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iErr = prepareFileName81(pWP, argv[i], argv[i - 1], FALSE);
        if (iErr != ERR_OK)
          break;
        iErr = prepareFileName81M(argv[i + 1], argv[i - 1], FALSE, pWP->szMaskPrepP, pWP->szMaskParP);
        if (iErr != ERR_OK)
          break;
        iErr = prepareFileName81M(argv[i + 2], argv[i - 1], FALSE, pWP->szMaskPrepN, pWP->szMaskParN);
        if (iErr != ERR_OK)
          break;
        i += 2;
        pWP->bMakeLoader = TRUE;
      }
      else if (strcmp(argv[i], "-li") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iErr = prepareFileName81(pWP, argv[i], argv[i - 1], FALSE);
        if (iErr != ERR_OK)
          break;
        pWP->bShowLoaderInfo = TRUE;
      }
      else if (strcmp(argv[i], "-fn") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iErr = prepareFileName81M(argv[i], argv[i - 1], FALSE, pWP->szMaskPrepN, pWP->szMaskParN);
        if (iErr != ERR_OK)
          break;
      }
      else if (strcmp(argv[i], "-ln") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iV = strlen(argv[i]);
        if (iV > MRM_LONG_NAME_LEN) {
          iErr = printError(ERR_BAD_LONG_NAME, NULL, NULL);
          break;
        }
        strcpy(pWP->szLongName, argv[i]);
        while (iV < MRM_LONG_NAME_LEN)
          pWP->szLongName[iV++] = ' ';
        pWP->szLongName[iV] = '\0';
      }
      else if (strcmp(argv[i], "-sa") == 0) {
        if (++i == argc)
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
        else {
          pWP->iStartAdr = parseNumber(argv[i]);
          if (pWP->iStartAdr < 0 || pWP->iStartAdr > 65535) {
            iErr = printError(ERR_BAD_ADDRESS, argv[i - 1], argv[i]);
            break;
          }
        }
      }
      else if (strcmp(argv[i], "-al") == 0) {
        pWP->bAttrList = TRUE;
        continue;
      }
      else if (strcmp(argv[i], "-ar") == 0) {
        pWP->bAttrAllRam = TRUE;
        continue;
      }
      else if (strcmp(argv[i], "-ow") == 0) {
        pWP->bOverwrite = TRUE;
        continue;
      }
      else if (strcmp(argv[i], "-psc") == 0) {
        pWP->bPackedScreen = TRUE;
        continue;
      }
      else if (strcmp(argv[i], "-mon") == 0) {
        if (++i == argc)
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
        else {
          if (!strcmp(argv[i], "1"))
            pWP->iLdrConf = 1;
          else if (!strcmp(argv[i], "1X"))
            pWP->iLdrConf = 5;
          else if (!strcmp(argv[i], "2"))
            pWP->iLdrConf = 2;
          else if (!strcmp(argv[i], "2X"))
            pWP->iLdrConf = 6;
          else {
            iErr = prepareFileName81M(argv[i], argv[i - 1], FALSE, pWP->szMaskPrepM, pWP->szMaskParM);
            if (iErr != ERR_OK)
              break;
            pWP->iLdrConf = 3;
          }
          if (pWP->iLdrConf < 0) {
            iErr = printError(ERR_BAD_VALUE, argv[i - 1], argv[i]);
            break;
          }
        }
      }
      else if (strcmp(argv[i], "-scs") == 0) {
        pWP->bSuppresCLS = TRUE;
        continue;
      }
      else if (strcmp(argv[i], "-cfn") == 0) {
        pWP->bExtrFilesCplx = TRUE;
        continue;
      }
      else if (strcmp(argv[i], "-cfnl") == 0) {
        if (++i == argc) {
          iErr = printError(ERR_NO_OPTION_VALUE, argv[i - 1], NULL);
          break;
        }
        iErr = prepareFilePathEx(pWP, pWP->szCplxListFn, argv[i], "-cfnl", FALSE, TRUE, FALSE);
        if (iErr != ERR_OK)
          break;
      }
      else {
        iErr = printError(ERR_UNKNOWN_OPTION, argv[i], NULL);
        break;
      }
    }

    if (iErr != ERR_OK)
      break;

    // kontrola nespravnych kombinacii prepinacov
    iV = 0;
    *szStr = '\0';
    if (pWP->bCatalogue) {
      iV++;
      strcat(szStr, "-c ");
    }
    if (pWP->bCreateMrm) {
      iV++;
      strcat(szStr, "-ic ");
    }
    if (pWP->bReconfigureMrm) {
      iV++;
      strcat(szStr, "-ir ");
    }
    if (pWP->bShowMrmInfo) {
      iV++;
      strcat(szStr, "-ii ");
    }
    if (pWP->bAddFile) {
      iV++;
      strcat(szStr, "-fa ");
    }
    if (pWP->bAddFileCplx) {
      iV++;
      strcat(szStr, "-fc ");
    }
    if (pWP->bAddFileCplxLst) {
      iV++;
      strcat(szStr, "-fl ");
    }
    if (pWP->bReplaceFile) {
      iV++;
      strcat(szStr, "-fr ");
    }
    if (pWP->bChangeFilePar) {
      iV++;
      strcat(szStr, "-fp ");
    }
    if (pWP->bDeleteFiles) {
      iV++;
      strcat(szStr, "-fd ");
    }
    if (pWP->bExtractFiles) {
      iV++;
      strcat(szStr, "-fx ");
    }
    if (pWP->bMakeLoader) {
      iV++;
      strcat(szStr, "-ml ");
    }
    if (pWP->bShowLoaderInfo) {
      iV++;
      strcat(szStr, "-li ");
    }
    if (iV > 1) {
      iErr = printError(ERR_BAD_OPTIONS, szStr, "");
      break;
    }

    if (pWP->bCatalogue
        && (*pWP->szFnBoot != '\0' || *pWP->szLongName != '\0' || *pWP->szMaskParN != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1 || pWP->iStartAdr != -1 || pWP->iLdrConf != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bOverwrite || pWP->bSuppresCLS || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-c");
      break;
    }

    if (pWP->bCreateMrm
        && (*pWP->szLongName != '\0' || *pWP->szMaskPar != '\0' || *pWP->szMaskParN != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iStartAdr != -1 || pWP->iLdrConf != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bOverwrite || pWP->bSuppresCLS || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-ic");
      break;
    }

    if (pWP->bReconfigureMrm
        && (*pWP->szLongName != '\0' || *pWP->szMaskPar != '\0' || *pWP->szMaskParN != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iStartAdr != -1 || pWP->iLdrConf != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bOverwrite || pWP->bSuppresCLS || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0'
            || (pWP->iDirItemsReq == -1 && pWP->iImageSizeReq == -1 && !pWP->bBoot))) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-ir");
      break;
    }

    if (pWP->bShowMrmInfo
        && (*pWP->szFnBoot != '\0' || *pWP->szLongName != '\0' || *pWP->szMaskParN != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1 || pWP->iStartAdr != -1 || pWP->iLdrConf != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bOverwrite || pWP->bSuppresCLS || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-ii");
      break;
    }

    if (pWP->bAddFile
        && (*pWP->szFnBoot != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1
            || pWP->iLdrConf != -1 || pWP->bSuppresCLS || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-fa");
      break;
    }

    if (pWP->bAddFileCplx
        && (*pWP->szFnBoot != '\0' || *pWP->szLongName != '\0' || *pWP->szMaskParN != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1 || pWP->iStartAdr != -1 || pWP->iLdrConf != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bSuppresCLS || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-fc");
      break;
    }

    if (pWP->bAddFileCplxLst
        && (*pWP->szFnBoot != '\0' || *pWP->szLongName != '\0' || *pWP->szMaskParN != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1 || pWP->iStartAdr != -1 || pWP->iLdrConf != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bSuppresCLS || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-fl");
      break;
    }

    if (pWP->bReplaceFile
        && (*pWP->szFnBoot != '\0' || *pWP->szLongName != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iStartAdr != -1 || pWP->iLdrConf != -1 || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bOverwrite || pWP->bSuppresCLS || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-fr");
      break;
    }

    if (pWP->bChangeFilePar
        && (*pWP->szFnBoot != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1
            || pWP->bOverwrite || pWP->bSuppresCLS || pWP->iLdrConf != -1 || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0'
            || (pWP->iStartAdr < 0 && !pWP->bAttrList && !pWP->bAttrAllRam && !*pWP->szMaskParN && !*pWP->szLongName))) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-fp");
      break;
    }

    if (pWP->bDeleteFiles
        && (*pWP->szFnBoot != '\0' || *pWP->szLongName != '\0' || *pWP->szMaskParN != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1 || pWP->iStartAdr != -1 || pWP->iLdrConf != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bOverwrite || pWP->bSuppresCLS || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-fd");
      break;
    }

    if (pWP->bExtractFiles
        && (*pWP->szFnBoot != '\0' || *pWP->szLongName != '\0' || *pWP->szMaskParN != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1 || pWP->iStartAdr != -1 || pWP->iLdrConf != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bOverwrite || pWP->bSuppresCLS || pWP->bPackedScreen)) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-fx");
      break;
    }

    if (pWP->bMakeLoader
        && (*pWP->szFnBoot != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bPackedScreen || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-ml");
      break;
    }

    if (pWP->bShowLoaderInfo
        && (*pWP->szFnBoot != '\0' || *pWP->szLongName != '\0' || *pWP->szMaskParN != '\0' || *pWP->szMaskParP != '\0'
            || pWP->iImageSizeReq != -1 || pWP->iDirItemsReq != -1 || pWP->iStartAdr != -1 || pWP->iLdrConf != -1
            || pWP->bAttrList || pWP->bAttrAllRam || pWP->bOverwrite || pWP->bSuppresCLS || pWP->bPackedScreen
            || pWP->bExtrFilesCplx || *pWP->szCplxListFn != '\0')) {
      iErr = printError(ERR_BAD_OPTIONS, "for", "-li");
      break;
    }

    // nastavenie default hodnot premennych, ak neboli mastavene prepinacmi

    if (*pWP->szFnBoot == '\0') {
      // standardna cesta k MRM BOOT suboru
#if LINUX
      // pri spustitelnom subore, inak v MRM_DATA_DIR
      p = NULL;
      iV = (int) readlink("/proc/self/exe", pWP->szFnBoot,
                          sizeof(pWP->szFnBoot) - sizeof(MRM_BOOT_FILE));
      if (iV > 0) {
        pWP->szFnBoot[iV] = '\0';
        p = strrchr(pWP->szFnBoot, '/');
      }
      if (p) {
        strcpy(p + 1, MRM_BOOT_FILE);
        if (access(pWP->szFnBoot, R_OK) != 0)
          p = NULL;
      }
      if (p == NULL)
        strcpy(pWP->szFnBoot, MRM_DATA_DIR "/" MRM_BOOT_FILE);
#else
      p = strrchr(argv[0], '/');
#if WIN
      if (p == NULL)
        p = strrchr(argv[0], '\\');
#endif
      if (p) {
        iV = p - argv[0] + 1;
        strncpy(pWP->szFnBoot, argv[0], iV);
        pWP->szFnBoot[iV] = '\0';
      }
      strcat(pWP->szFnBoot, MRM_BOOT_FILE);
#endif
    }

    if (pWP->bCreateMrm || pWP->bReconfigureMrm) {
      if (pWP->iImageSizeReq < 0)
        pWP->iImageSizeReq = MRM_SIZE_KB_DFLT * 1024;

      if (pWP->iDirItemsReq < 0)
        pWP->iDirItemsReq = MRM_DIR_ITEMS_DFLT;
    }

    if (pWP->bAddFile) {
      if (pWP->iStartAdr < 0)
        pWP->iStartAdr = 0;

      if (*pWP->szLongName == '\0') {
        memset(pWP->szLongName, ' ', MRM_LONG_NAME_LEN);
        pWP->szLongName[MRM_LONG_NAME_LEN] = '\0';
      }
    }

    if (pWP->bMakeLoader) {
      if (pWP->iLdrConf < 0)
        pWP->iLdrConf = 0;

      if (*pWP->szLongName == '\0') {
        memset(pWP->szLongName, ' ', MRM_LONG_NAME_LEN);
        pWP->szLongName[MRM_LONG_NAME_LEN] = '\0';
      }
    }

  } while (FALSE);

  return iErr;
}
//---------------------------------------------------------------------------
void version(void)
{
  printf("%s v%d.%d - %s\n\n",
         PROG_NAME, PROG_VERSION / 16, PROG_VERSION % 16, PROG_AUTHOR);
}
//---------------------------------------------------------------------------
void usage(void)
{
  printf("Utility to manage MRM images.\n");
  printf("Usage:\n");
  printf("  %s imgname.mrm -c [fmask[.T]]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -ic [-is num] [-id num] [-ib [path/]bootfile.ext]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -ir [-is num] [-id num] [-ib [path/]bootfile.ext]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -ii\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -fa [path/]filename.ext [-fn fname.T] [-ln lname] [-sa num] [-al] [-ar] [-ow] [-psc]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -fc [path/]complex-file-name [-ow]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -fl [path/]complex-file-names-list [-ow]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -fr fname.T [path/]filename.ext\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -fp fname.T [-fn newfn.T] [-ln lname] [-sa num] [-al] [-ar]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -fd fmask[.T]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -fx [path/]fmask[.T] [-cfn [-cfnl [path/]complex-file-names-list]]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -ml fname.T pname.T cname.T [-ln lname] [-sa num] [-mon MX] [-scs] [-ow]\n", PROG_EXE_NAME);
  printf("  %s imgname.mrm -li fname.T\n", PROG_EXE_NAME);
//  printf("  %s imgname.mrm -split part-size\n", PROG_EXE_NAME);

  pressEnter();

  printf("    -c    show catalogue of the MRM image\n");
  printf("    -ic   create new MRM image\n");
  printf("    -ir   reconfigure MRM image\n");
  printf("    -ii   show image info\n");
  printf("    -is   image size in kB [%d]\n", MRM_SIZE_KB_DFLT);
  printf("    -id   number of directory items [%d]\n", MRM_DIR_ITEMS_DFLT);
  printf("    -ib   use this MRM Booter file [mrm-boot.bin]\n");
  printf("    -fa   add file into image\n");
  printf("    -fc   add file into image by complex source file name\n");
  printf("    -fl   add files into image by list of files in text file\n");
  printf("    -fr   replace file in image\n");
  printf("    -fp   change file parameters in image\n");
  printf("    -fd   delete file(s) from image\n");
  printf("    -fx   extract file(s) from image\n");
  printf("    -cfn  create complex file names for extracted files\n");
  printf("    -cfnl create also text file with list of complex file names of extracted files\n");
  printf("    -ml   make loader file\n");
  printf("    -li   show loader file info\n");
  printf("    -fn   file name (max. 8 chars + one char type)\n");
  printf("    -ln   long name to display (max. 16 chars)\n");
  printf("    -sa   start address to load [0]\n");
  printf("    -al   set/toggle List attribute\n");
  printf("    -ar   set/toggle AllRAM attribute\n");
  printf("    -ow   overwrite already existing file\n");
  printf("    -psc  insert PSC unpak routine before picture data\n");
  printf("    -mon  monitor type in Loader; can be 1, 1X, 2 or 2X or mname.T\n");
  printf("    -scs  suppress clear screen in Loader\n");

  pressEnter();

  printf("`lname` must be in double quotes when contains spaces.\n");
  printf("`fname.T`, `newfn.T`, `pname.T`, `cname.T` and `mname.T` are MRM 8.1 file names\n");
  printf("and must be in double quotes when contains spaces or >/< redir chars.\n");
  printf("`fmask[.T]` is MRM 8.1 file name that may contain wildcards.\n");
  printf("`T` must be single character. If not used in mask, all file types regard.\n");
  printf("`complex-file-name` has form `fname_lname_sa_attr.T` (special chars as #HH)\n");
  printf("`complex-file-names-list` is text file with complex file names to add\n");
  printf("All numeric parameters must be nonnegative integers and can be in three forms:\n");
  printf("  decimal (e.g. 12345)\n");
  printf("  octal (e.g. 01234)\n");
  printf("  hexadecimal (e.g. 0x1234)\n\n");
}
//---------------------------------------------------------------------------

