#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "logs.h"
#include "utils.h"
#include "dnsparser.h"

#define MAX_PATH_BUFFER     256

static BOOL	PrintConsole = FALSE;
static BOOL	DebugPrint = FALSE;

static FILE	*LogFile = NULL;

static int	ThresholdLength = 0;
static int	CurrentLength = 0;
static char	FilePath[MAX_PATH_BUFFER];

static EFFECTIVE_LOCK   PrintLock;

int Log_Init(ConfigFileInfo *ConfigInfo, BOOL PrintScreen, BOOL PrintDebug)
{
    PrintConsole = PrintScreen;
    DebugPrint = PrintDebug;

	if( ConfigGetBoolean(ConfigInfo, "LogOn") == FALSE )
	{
		return 0;
	}

	if( snprintf(FilePath,
                 sizeof(FilePath),
                 "%s%cdnsforwarder.log",
                 ConfigGetRawString(ConfigInfo, "LogFileFolder"),
                 PATH_SLASH_CH
                 )
        >= sizeof(FilePath)
    )
    {
        return -36;
    }

	LogFile = fopen(FilePath, "r+");
	if( LogFile == NULL )
	{
	    /* File does not exist */
		LogFile = fopen(FilePath, "w");
		if( LogFile == NULL )
        {
            return -44;
        }

		CurrentLength = 0;
	} else {
		fseek(LogFile, 0, SEEK_END);
		CurrentLength = ftell(LogFile);
	}

	ThresholdLength = ConfigGetInt32(ConfigInfo, "LogFileThresholdLength");
	if( ThresholdLength <= 0 )
    {
        return -60;
    }

	EFFECTIVE_LOCK_INIT(PrintLock);

	return 0;
}

BOOL Log_Inited(void)
{
	return (!(LogFile == NULL)) || PrintConsole;
}

BOOL Log_DebugOn(void)
{
    return DebugPrint;
}

static void CheckLength(void)
{
    static int CurrentNumber = 0;

	if( CurrentLength >= ThresholdLength )
	{
        char FileRenamed[MAX_PATH_BUFFER + 8];

        fclose(LogFile);

        while( TRUE )
        {
            ++CurrentNumber;

            if( snprintf(FileRenamed,
                         sizeof(FileRenamed),
                         "%s.%d",
                         FilePath,
                         CurrentNumber
                         )
                >= sizeof(FileRenamed)
            )
            {
                return;
            }

            if( FileIsReadable(FileRenamed) == FALSE )
            {
                /* If `FileRenamed' does not exist */
                rename(FilePath, FileRenamed);
                LogFile = fopen(FilePath, "w");
                if( LogFile == NULL )
                {
                    return;
                }

                CurrentLength = 0;

                break;
            }
        }
	}
}

void Log_Print(const char *Type, const char *format, ...)
{
	va_list ap;
	char DateAndTime[32];

	if( !Log_Inited() )
    {
        return;
    }

    GetCurDateAndTime(DateAndTime, sizeof(DateAndTime));

	va_start(ap, format);

	EFFECTIVE_LOCK_GET(PrintLock);
	if( LogFile != NULL )
	{
        CheckLength();

        CurrentLength += fprintf(LogFile,
                                 "%s [%s] ",
                                 DateAndTime,
                                 Type == NULL ? "" : Type
                                 );
        CurrentLength += vfprintf(LogFile, format, ap);

        fflush(LogFile);
	}

	if( PrintConsole )
    {
        printf("%s [%s] ",
               DateAndTime,
               Type == NULL ? "" : Type
               );
        vprintf(format, ap);
    }

	EFFECTIVE_LOCK_RELEASE(PrintLock);

	va_end(ap);
}

void ShowRefusingMessage(IHeader *h, const char *Message)
{
    Log_Print(NULL,
              "[R][%s][%s][%s] %s.\n",
              h->Agent,
              DNSGetTypeName(h->Type),
              h->Domain,
              Message
              );
}

void ShowTimeOutMessage(IHeader *h, char Protocol)
{
    Log_Print(NULL,
              "[%c][%s][%s][%s] Timed out.\n",
              Protocol,
              h->Agent,
              DNSGetTypeName(h->Type),
              h->Domain
              );
}

void ShowErrorMessage(IHeader *h, char Protocol)
{
    if( PRINTON )
    {
        int ErrorNum;
        char ErrorMessage[320];

        ErrorNum = GET_LAST_ERROR();
        ErrorMessage[0] ='\0';
		GetErrorMsg(ErrorNum, ErrorMessage, sizeof(ErrorMessage));

        Log_Print(NULL,
                  "[%c][%s][%s][%s] An error occured : %d : %s .\n",
                  Protocol,
                  h->Agent,
                  DNSGetTypeName(h->Type),
                  h->Domain,
                  ErrorNum,
                  ErrorMessage
                  );
    }
}

void ShowNormalMessage(IHeader *h,
                       int PackageLength /* Excluding IHeader */,
                       char Protocol
                       )
{
    if( PRINTON )
    {
        char *Package = (char *)(h + 1);
        char InfoBuffer[1024];

		InfoBuffer[0] = '\0';
		GetAllAnswers(Package, PackageLength, InfoBuffer, sizeof(InfoBuffer));

        Log_Print(NULL,
                  "[%c][%s][%s][%s] : %d bytes\n%s",
                  Protocol,
                  h->Agent,
                  DNSGetTypeName(h->Type),
                  h->Domain,
                  PackageLength,
                  InfoBuffer
                  );
    }
}

void ShowBlockedMessage(IHeader *h,
                        int PackageLength /* Excluding IHeader */,
                        const char *Message
                        )
{
    if( PRINTON )
    {
        char *Package = (char *)(h + 1);
        char InfoBuffer[1024];

		InfoBuffer[0] = '\0';
		GetAllAnswers(Package, PackageLength, InfoBuffer, sizeof(InfoBuffer));

        Log_Print(NULL,
                  "[B][%s] %s :\n%s",
                  h->Domain,
                  Message == NULL ? "" : Message,
                  InfoBuffer
                  );
    }
}