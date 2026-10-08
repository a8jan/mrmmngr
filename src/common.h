//---------------------------------------------------------------------------
#ifndef commonH
#define commonH
//---------------------------------------------------------------------------
#define PROG_NAME               "MrmMngr"
#define PROG_EXE_NAME           "mrmmngr"
#define PROG_VERSION            0x13
#define PROG_AUTHOR             "RM-TEAM 06/2022"

//---------------------------------------------------------------------------
#define BYTE                    unsigned char
#define WORD                    unsigned short int
#define DWORD                   unsigned int
#define BOOL                    int
#define FALSE                   0
#define TRUE                    !FALSE

//-----------------------------------------------------------------------------
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(__BORLANDC__)
  #define WIN                   1
  #define LINUX                 0
#else
  #define WIN                   0
  #define LINUX                 1
  #define stricmp               strcasecmp
  #define strupr(x)             char* uptr = x; while ((*uptr = toupper(*uptr))) uptr++;

  #ifndef MRM_DATA_DIR
    #define MRM_DATA_DIR        "/usr/local/share/mrmmngr"
  #endif

  #pragma GCC diagnostic ignored "-Warray-bounds"
#endif

//---------------------------------------------------------------------------
#define BUFFER_SIZE             65536
#define SECTOR_SIZE             128
#define BANK_SIZE               32768

#define MRM_SIZE_KB_MIN         32
#define MRM_SIZE_KB_MAX         8192
#define MRM_SIZE_KB_DFLT        512

#define MRM_DIR_ITEMS_MIN       4
#define MRM_DIR_ITEMS_MAX       256
#define MRM_DIR_ITEMS_DFLT      32
#define MRM_DIR_SIZE            (MRM_DIR_ITEMS_MAX * sizeof(DIR_ITEM))

#define MRM_SHORT_NAME_LEN      8
#define MRM_LONG_NAME_LEN       16

#define MRM_BOOT_FILE           "mrm-boot.bin"
#define MRM_BOOT_SIZE_MAX       (BANK_SIZE - MRM_DIR_SIZE)

#define MRM_LOADER_FILE         "mrm-ldr.bin"
#define MRM_LOADER_SIZE_MAX     246

#define MRM_UNPACK_FILE         "unpckpsc.bin"
#define MRM_UNPACK_SIZE_MAX     256

#define MRM_BOOT_FILLER_BYTE    0xFF
#define MRM_DIR_FILLER_BYTE     0xE5
#define MRM_DATA_FILLER_BYTE    0xFF

// atributy suboru
#define DI_ATTR_LIST            1
#define DI_ATTR_ALLRAM          2

#define FIND_END                (DATA_FILE *)1

//---------------------------------------------------------------------------
#define ERR_OK                  0
#define ERR_PRINT_USAGE         -1
#define ERR_OUT_OF_MEMORY       -2
#define ERR_UNKNOWN_OPTION      -3
#define ERR_BAD_OPTIONS         -4
#define ERR_NO_OPTION_VALUE     -5
#define ERR_OPEN_SRC_FILE       -6
#define ERR_ACCESS_SRC_FILE     -7
#define ERR_LENGTH_SRC_FILE     -8
#define ERR_SRC_FILE_TO_LONG    -9
#define ERR_SRC_FILE_OF_READ    -10
#define ERR_READ_SRC_FILE       -11
#define ERR_OPEN_DEST_FILE      -12
#define ERR_ACCESS_DEST_FILE    -13
#define ERR_WRITE_DEST_FILE     -14
#define ERR_BAD_ADDRESS         -15
#define ERR_BAD_VALUE           -16
#define ERR_BAD_LONG_NAME       -17
#define ERR_INPUT_FILE_MORE     -18
#define ERR_NO_OUTPUT_FILE      -19
#define ERR_BOOT_FILE_MORE      -20
#define ERR_INVALID_BOOT_BIN    -21
#define ERR_INVALID_FILE_MASK   -22
#define ERR_UNAMBIGUOUS_NAME    -23
#define ERR_FILE_EXIST          -24
#define ERR_FILE_NOT_EXIST      -25
#define ERR_DIRECTORY_FULL      -26
#define ERR_DISK_FULL           -27
#define ERR_BAD_GEOMETRY        -28
#define ERR_INVALID_CPLX_NAME   -29
#define ERR_INVALID_LOADER      -30

//---------------------------------------------------------------------------
#pragma pack(push, 1)
typedef struct DirItem {
  char cType;
  char cName[MRM_SHORT_NAME_LEN];
  WORD wSector;
  WORD wMemAdr;
  WORD wLength;
  BYTE btAttrs;
  char cLongName[MRM_LONG_NAME_LEN];
} DIR_ITEM;
#pragma pack(pop)

// sizeof(DIR_ITEM) != 32 !!! (zaporna velkost pola = chyba pri preklade)
typedef char DIR_ITEM_SIZE_CHECK[(sizeof(DIR_ITEM) == 32) ? 1 : -1];

//---------------------------------------------------------------------------
typedef struct tagDataFile {
  DIR_ITEM *pDI;              // pointer na adresarovu polozku
  int iIndex;                 // index adresarovej polozky
  int iFileOffset;            // offset suboru v MRM
  int iSize;                  // dlzka suboru
  BYTE *pbtContent;           // pointer na buffer s obsahom suboru
  BOOL bNewContent;           // priznak noveho obsahu suboru
  BOOL bDeleted;              // subor urceny na zmazanie
  struct tagDataFile *pPrev;  // pointer na predoslu strukturu
  struct tagDataFile *pNext;  // pointer na dalsiu strukturu
} DATA_FILE;

//---------------------------------------------------------------------------
typedef struct WorkParams {
  char *pszFnImg;       // pointer na cestu/meno k suboru obrazu
  FILE *fiImage;        // pointer na otvoreny subor MRM
  BYTE *pbtBoot;        // pointer na buffer pre nacitany mrm-boot.bin
  BYTE *pbtDir;         // pointer na buffer pre adresar
  BYTE *pbtBuff;        // pointer na pracovny buffer
  char szFnBoot[256];   // cesta/meno suboru s Boot kodom
  BOOL bBoot;           // bol specifikovany BOOT subor
  DATA_FILE *pDFfirst;  // pointer na struktury suborov v MRM
  DATA_FILE *pDFlast;   // pointer na struktury suborov v MRM
  DATA_FILE *pDFfind;   // pointer na struktury suborov v MRM
  BOOL bCreateMrm;      // vytvorenie noveho MRM
  BOOL bReconfigureMrm; // prekonfigurovanie MRM
  BOOL bShowMrmInfo;    // zobrazenie informacii o MRM
  BOOL bCatalogue;      // zobrazenie katalogu MRM
  BOOL bAddFile;        // pridanie suborov do MRM
  BOOL bAddFileCplx;    // pridanie suborov do MRM - komplexne meno
  BOOL bAddFileCplxLst; // pridanie suborov do MRM so zoznamu v textovom subore
  BOOL bReplaceFile;    // nahradenie suboru v MRM
  BOOL bChangeFilePar;  // zmena parametrov suboru
  BOOL bDeleteFiles;    // zmazanie suborov
  BOOL bExtractFiles;   // extrahovanie suborov
  BOOL bExtrFilesCplx;  // extrahovanie suborov s komplexnymi menami
  BOOL bMakeLoader;     // vytvorenie loadera
  BOOL bShowLoaderInfo; // sobrazenie informacii o loaderi
  int iImageSizeReq;    // pozadovana velkost obrazu MRM v bytoch
  int iImageSize;       // velkost obrazu MRM v bytoch
  int iBootLen;         // velkost Boot kodu
  int iFirstDirSec;     // prvy sektor adresara
  int iDirSecCount;     // pocet sektorov adresara
  int iDirItemsReq;     // pocet pozadovanych poloziek adresara
  int iDirItems;        // pocet poloziek adresara
  int iTotalFiles;      // pocet platnych suborov
  int iFilesByMask;     // pocet suborov podla masky
  int iFirstFreeSec;    // prvy volny sektor
  char szFilePath[256]; // cesta/meno/maska suboru, ktory sa ma pridat/extrahovat
  char szLongName[MRM_LONG_NAME_LEN + 1];  // dlhe meno suboru v MRM pre zobrazenie (16 znakov)
  char szMaskPar[13];   // maska z parametra
  char szMaskPrep[13];  // pripravena maska na test
  char szMaskParN[13];  // maska z parametra - pre nove meno suboru
  char szMaskPrepN[13]; // pripravena maska na test - pre nove meno suboru
  char szMaskParP[13];  // maska z parametra - pre meno suboru obrazku
  char szMaskPrepP[13]; // pripravena maska na test - pre meno suboru obrazku
  char szMaskParM[13];  // maska z parametra - pre meno suboru Monitora
  char szMaskPrepM[13]; // pripravena maska na test - pre meno suboru Monitora
  char szCplxListFn[256]; // cesta/meno suboru pre zapis zoznamu suborov s komplexnymi menami
  int iStartAdr;        // nahravacia/startovcia adresa suboru
  int iLdrConf;         // konfiguracia Loadera
  BOOL bSuppresCLS;     // potlacenie CLS v Loaderi
  BOOL bAttrList;       // atribut List - subor, ktory sa zobrazuje v zozname spustitelnych suborov
  BOOL bAttrAllRam;     // atribut AllRAM
  BOOL bOverwrite;      // prepisanie existujuceho suboru
  BOOL bPackedScreen;   // priznak, ze pred subor treba pridat rozpakovaciu rutinu PSC
} WORK_PARAMS;

//---------------------------------------------------------------------------
// utils.c
void pressEnter(void);
int printError(int iErr, void *pvPar, void *pvPar2);
int allocBuffer(void **ppvBuff, int iSize);
int parseNumber(const char *pszStr);
char* trimLeft(char *pStr);
char* trimRight(char *pStr);
char* trim(char *pStr);
BOOL isValidChar(char cc);
BOOL isMetaChar(char cc, BOOL bExtra);
char* appendEncChar(char cc, char *pszDest);
BOOL decodeString(char *pszDest, int iMaxDestLen, const char *pszSrc);

// file.c
int prepareFilePath(WORK_PARAMS *pWP, const char *pszFilePath, const char *pszOpt, BOOL bWildcards, BOOL bOptAdd);
int prepareFilePathEx(WORK_PARAMS *pWP, char *pszDestFileName, const char *pszFilePath, const char *pszOpt,
                      BOOL bWildcards, BOOL bOptAdd, BOOL b81Name);
int prepareFileName81(WORK_PARAMS *pWP, const char *pszName, const char *pszOpt, BOOL bWildcards);
int prepareFileName81M(const char *pszName, const char *pszOpt, BOOL bWildcards, char *pszMaskPrep, char *pszMaskPar);
BOOL compareFileNameMask(WORK_PARAMS *pWP, DIR_ITEM *pDI, BOOL bWildcards);
void compactFileName(DIR_ITEM *pDI, char *pszName, BOOL bComplexName);
int parseComplexFileName(DIR_ITEM *pDI, char *pszComplexName);
int openImageFile(WORK_PARAMS *pWP, BOOL bCreate);
int closeImageFile(WORK_PARAMS *pWP);
int writeToImage(WORK_PARAMS *pWP, BYTE *pbtBuff, int iOffset, int iLen);
int readFromImage(WORK_PARAMS *pWP, BYTE *pbtBuff, int iOffset, int iToRead);
int saveFile(const char *pszFileName, BYTE *pbtBuff, int iLen, BOOL bAppend);
int loadFile(const char *pszFileName, BYTE *pbtBuff, int iMaxLen);
int copyFile(const char *pszFileNameDest, const char *pszFileNameSrc);

// mrm.c
int createMrm(WORK_PARAMS *pWP);
int reconfigureMrm(WORK_PARAMS *pWP);
int showMrmInfo(WORK_PARAMS *pWP);
int printCatalogue(WORK_PARAMS *pWP);
int addFile(WORK_PARAMS *pWP);
int addFileCplx(WORK_PARAMS *pWP);
int addFileCplxList(WORK_PARAMS *pWP);
int changeFileParameters(WORK_PARAMS *pWP);
int deleteFiles(WORK_PARAMS *pWP);
int extractFiles(WORK_PARAMS *pWP);
int makeLoader(WORK_PARAMS *pWP);
int showLoaderInfo(WORK_PARAMS *pWP);
void freeDataFiles(WORK_PARAMS *pWP);

//---------------------------------------------------------------------------
#endif
//---------------------------------------------------------------------------

