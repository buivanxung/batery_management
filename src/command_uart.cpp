#include "bat_man.h"
#include "flash_files.h"
#include "bat_handle.h"
#include "logger.h"
#include "stm8_comm.h"

#define CLI_MAX_ARGS 8
#define CLI_BUFFER_SIZE 64
#define UART_RX_BUFFER_SIZE 1024

typedef void (*cli_func_t)(int argc, char **argv);

typedef struct
{
    const char *name;
    cli_func_t func;
    const char *help;
} cli_command_t;

QueueHandle_t _queueLed;
QueueHandle_t _queueAudio;
QueueHandle_t _queueMotor;
SPIFlash *_flash;

/* Upload state */
bool uploadMode = false;
char uploadName[16];
uint32_t uploadLength;
bool cliEcho = true;
bool silentMode = false;  // Disable logging during binary upload

/* ===================== TOKENIZER ===================== */
int cli_tokenize(char *input, char **argv)
{
    int argc = 0;

    while (*input && argc < CLI_MAX_ARGS)
    {
        // Skip spaces
        while (*input == ' ')
            input++;
        if (*input == '\0')
            break;

        argv[argc++] = input;

        // Find end of word
        while (*input && *input != ' ')
            input++;

        if (*input)
        {
            *input = '\0';
            input++;
        }
    }

    return argc;
}

/* ===================== COMMANDS ===================== */

void cmd_help(int argc, char **argv);
void cmd_led(int argc, char **argv);
void cmd_play(int argc, char **argv);
void cmd_motor(int argc, char **argv);
void cmd_mem(int argc, char **argv);
void cmd_list(int argc, char **argv);
void cmd_store(int argc, char **argv);
void cmd_format(int argc, char **argv);
void cmd_mon(int argc, char **argv);
void cmd_beep(int argc, char **argv); 

void cmd_beep(int argc, char **argv)
{
    Message_t msg;
    msg.cmd = CMD_TEST_AUDIO;
    safeQueueSend(_queueAudio, &msg);
    logPrintln("Beep test queued");
}

/* ===================== COMMAND TABLE ===================== */
cli_command_t cli_table[] =
    {
        {"help", cmd_help, "Show help"},
        {"led", cmd_led, "led on/off/toggle"},
        {"play", cmd_play, "play <file>"},
        {"motor", cmd_motor, "motor <1-8> on/off"},
        {"mem", cmd_mem, "show memory"},
        {"list", cmd_list, "show list file audio in flash"},
        {"store", cmd_store, "store <name> <length> - upload audio file"},
        {"format", cmd_format, "format flash filesystem"},
        {"mon", cmd_mon, "monitor ADC values"},
        {"beep", cmd_beep, "beep [ms] - test 1kHz tone (default 2000ms)"},
    };

#define CLI_CMD_COUNT (sizeof(cli_table) / sizeof(cli_command_t))

void cmd_help(int argc, char **argv)
{
    logPrintln("Commands:");
    for (int i = 0; i < CLI_CMD_COUNT; i++)
    {
        logPrint("  ");
        logPrint(cli_table[i].name);
        logPrint(" - ");
        logPrintln(cli_table[i].help);
    }
}

void cmd_led(int argc, char **argv)
{
    if (argc < 2)
    {
        logPrintln("Usage: led on/off/toggle");
        return;
    }

    Message_t msg;

    if (strcmp(argv[1], "on") == 0)
        msg.cmd = CMD_LED_ON;
    else if (strcmp(argv[1], "off") == 0)
        msg.cmd = CMD_LED_OFF;
    else
        msg.cmd = CMD_LED_TOGGLE;

    safeQueueSend(_queueLed, &msg);
}

void cmd_play(int argc, char **argv)
{
    if (argc < 2)
    {
        logPrintln("Usage: play <file>");
        return;
    }

    Message_t msg;
    msg.cmd = CMD_PLAY_AUDIO;

    strncpy(msg.name, argv[1], sizeof(msg.name));
    msg.name[15] = '\0';

    safeQueueSend(_queueAudio, &msg);
}

void cmd_motor(int argc, char **argv)
{
    if (argc < 3)
    {
        logPrintln("Usage: motor <1-8> on/off");
        return;
    }

    int motor = atoi(argv[1]);
    bool on = (strcmp(argv[2], "on") == 0);

    if (motor < 1 || motor > 8)
    {
        logPrintln("Motor must be 1-8");
        return;
    }

    Message_t msg;
    msg.cmd = CMD_MOTOR_SET;
    msg.param = motor;
    msg.on = on;

    safeQueueSend(_queueMotor, &msg);
}

void cmd_mem(int argc, char **argv)
{
    logPrintf("Heap: %d\n", xPortGetFreeHeapSize());
}

void cmd_list(int argc, char **argv)
{
    logPrintln("Heap: \n");
    flashFsListFiles(_flash);
}

void cmd_store(int argc, char **argv)
{
    if (argc < 3)
    {
        logPrintln("Usage: store <name> <length>");
        return;
    }

    strncpy(uploadName, argv[1], sizeof(uploadName));
    uploadName[15] = '\0';

    uploadLength = atoi(argv[2]);

    if (uploadLength == 0)
    {
        logPrintln("Invalid length");
        return;
    }

    // Disable logging & echo BEFORE outputting anything
    silentMode = true;
    cliEcho = false;

    // Clear UART buffers
    while (Serial1.available())
        Serial1.read();
    
    Serial1.flush();
    vTaskDelay(pdMS_TO_TICKS(10));

    // Direct Serial1 output with explicit newlines
    Serial1.print("OK\r\n");
    Serial1.flush();
    vTaskDelay(pdMS_TO_TICKS(5));
    
    Serial1.print("READY\r\n");
    Serial1.flush();
    vTaskDelay(pdMS_TO_TICKS(5));

    uploadMode = true;
}

void cmd_format(int argc, char **argv)
{
    logPrintln("Formatting flash filesystem...");
    bool success = flashFsFormat(_flash);
    if (success)
    {
        logPrintln("Format successful");
    }
    else
    {
        logPrintln("Format failed");
    }
}

void cmd_mon(int argc, char **argv)
{
    logPrintln("STM8 Battery Monitor (via UART):");
    for (int i = 0; i < 8; i++)
    {
        Stm8Status_t s = stm8GetStatus(i);
        char buf[80];
        if (s.ok)
        {
            float vBat = s.batMv / 1000.0f;
            uint8_t pct = batteryPercent(vBat);
            snprintf(buf, sizeof(buf),
                     "CH%d %.3fV %3u%% pwKey=%s locked=%s",
                     i, vBat, pct,
                     s.pwKeyOn  ? "ON"  : "OFF",
                     s.pwLocked ? "YES" : "NO");
            logPrintln(buf);
            if (vBat < BAT_UNDERVOLTAGE)
                logPrintln("  WARNING: UNDER-VOLTAGE");
            else if (vBat > BAT_OVERVOLTAGE)
                logPrintln("  WARNING: OVER-VOLTAGE");
        }
        else
        {
            snprintf(buf, sizeof(buf), "CH%d NO RESPONSE (err=0x%02X)", i, s.errCode);
            logPrintln(buf);
        }
    }
}

void cli_execute(char *line)
{
    char *argv[CLI_MAX_ARGS];
    int argc = cli_tokenize(line, argv);

    if (argc == 0)
        return;

    for (int i = 0; i < CLI_CMD_COUNT; i++)
    {
        if (strcmp(argv[0], cli_table[i].name) == 0)
        {
            cli_table[i].func(argc, argv);
            return;
        }
    }

    logPrintln("Command not found");
}

void commTask(void *pvParameters)
{
    char buffer[CLI_BUFFER_SIZE];
    uint8_t index = 0;

    logPrintln("commTask started");
    logPrint("> ");

    while (1)
    {
        /* ================= UPLOAD MODE ================= */
        if (uploadMode)
        {
            bool ok = flashFsWriteFileFromSerial_PRO(
                _flash,
                uploadName,
                uploadLength);

            uploadMode = false;
            cliEcho = true;
            silentMode = false;  // Re-enable logging

            if (ok)
                logPrintln("DONE");
            else
                logPrintln("ERR");

            logPrint("> ");
            continue;
        }

        /* ================= CLI MODE ================= */
        while (Serial1.available())
        {
            char c = Serial1.read();

            if (c == '\r' || c == '\n')
            {
                buffer[index] = '\0';

                if (index > 0)
                {
                    silentMode = (strcmp(buffer, "store") == 0) || // Detect "store" command early
                                  strncmp(buffer, "store ", 6) == 0;
                    
                    if (!silentMode)
                        logPrintln("");
                    
                    cli_execute(buffer);
                    index = 0;

                    if (uploadMode)
                        break;  // Exit CLI loop immediately - don't read binary data as commands
                }

                if (!silentMode)
                    logPrint("> ");
            }
            else if (c == 0x08 || c == 0x7F)
            {
                if (index > 0)
                {
                    index--;
                    Serial1.print("\b \b");
                }
            }
            else if (c >= 32 && c <= 126)
            {
                if (index < CLI_BUFFER_SIZE - 1)
                {
                    buffer[index++] = c;

                    if (cliEcho)
                        Serial1.print(c);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void cli_set_command(QueueHandle_t n_queueLed,
                     QueueHandle_t n_queueAudio,
                     QueueHandle_t n_queueMotor, SPIFlash *n_flash)
{
    _queueLed = n_queueLed;
    _queueAudio = n_queueAudio;
    _queueMotor = n_queueMotor;
    _flash = n_flash;
}