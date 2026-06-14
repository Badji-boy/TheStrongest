
namespace Logger
{
    FILE* logFile = nullptr;
    char logBuffer[1024];


    void Log(const char* message)
    {
        if (logFile)
        {
            fprintf(logFile, "%s", message);
            fflush(logFile); 
        }

        OutputDebugStringA(message);
    }

    void Init()
    {
        // Открываем файл для записи (перезаписываем при каждом запуске)
        fopen_s(&logFile, "debug_log.txt", "w");
        if (logFile)
        {
            Log("=== Logging started ===\n");
            Log("Program initialized\n");
        }
    }

    void LogFormatted(const char* format, ... /*хоть сколько параметров*/)
    {
        if (!logFile) return;

        va_list args;
        va_start(args, format);
        vsnprintf(logBuffer, sizeof(logBuffer), format, args);
        va_end(args);

        Log(logBuffer);
    }

    void LogError(const char* function, HRESULT hr)
    {
        if (logFile)
        {
            LogFormatted("ERROR in %s: HRESULT = 0x%X\n", function, hr);
        }
    }

    void Shutdown()
    {
        if (logFile)
        {
            Log("=== Logging ended ===\n");
            fclose(logFile);
            logFile = nullptr;
        }
    }
}
