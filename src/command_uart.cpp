#include "bat_man.h"
#include "flash_files.h"

#define CLI_MAX_ARGS 8
#define CLI_BUFFER_SIZE 64

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

/* ===================== COMMAND TABLE ===================== */
cli_command_t cli_table[] =
    {
        {"help", cmd_help, "Show help"},
        {"led", cmd_led, "led on/off/toggle"},
        {"play", cmd_play, "play <file>"},
        {"motor", cmd_motor, "motor <1-4> on/off"},
        {"mem", cmd_mem, "show memory"},
        {"list", cmd_list, "show list file audio in flash"},
};

#define CLI_CMD_COUNT (sizeof(cli_table) / sizeof(cli_command_t))

void cmd_help(int argc, char **argv)
{
    Serial1.println("Commands:");
    for (int i = 0; i < CLI_CMD_COUNT; i++)
    {
        Serial1.print("  ");
        Serial1.print(cli_table[i].name);
        Serial1.print(" - ");
        Serial1.println(cli_table[i].help);
    }
}

void cmd_led(int argc, char **argv)
{
    if (argc < 2)
    {
        Serial1.println("Usage: led on/off/toggle");
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
        Serial1.println("Usage: play <file>");
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
        Serial1.println("Usage: motor <1-4> on/off");
        return;
    }

    int motor = atoi(argv[1]);
    bool on = (strcmp(argv[2], "on") == 0);

    if (motor < 1 || motor > 4)
    {
        Serial1.println("Motor must be 1-4");
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
    Serial1.print("Heap: ");
    Serial1.println(xPortGetFreeHeapSize());
}

void cmd_list(int argc, char **argv)
{
    Serial1.println("Heap: \n");
    flashFsListFiles(_flash);
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

    Serial1.println("Command not found");
}

void commTask(void *pvParameters)
{
    char buffer[CLI_BUFFER_SIZE];
    uint8_t index = 0;

    while (1)
    {
        while (Serial1.available())
        {
            char c = Serial1.read();

            /* ===== ENTER ===== */
            if (c == '\r' || c == '\n')
            {
                Serial1.println(); // enter

                buffer[index] = '\0';

                if (index > 0)
                {
                    cli_execute(buffer);
                    index = 0;
                }

                Serial1.print("> ");
            }

            /* ===== BACKSPACE ===== */
            else if (c == 0x08 || c == 0x7F) // BS or DEL
            {
                if (index > 0)
                {
                    index--;

                    // clr terminal
                    Serial1.print("\b \b");
                }
            }

            /* ===== NORMAL CHAR ===== */
            else if (c >= 32 && c <= 126) // printable ASCII
            {
                if (index < CLI_BUFFER_SIZE - 1)
                {
                    buffer[index++] = c;

                    // echo lại ký tự
                    Serial1.print(c);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
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