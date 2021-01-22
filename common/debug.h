
//
// If this is defined we output debug-messages over the serial
// console.
//
#define DEBUG 1

//
// Last 20 debug messages.
//
#define DEBUG_MAX 20
String debug_logs[DEBUG_MAX];
String debug_prefix;

//
// Record a debug-message, only if `DEBUG` is defined.
//
void DEBUG_LOG(const char *format, ...)
{
#ifdef DEBUG
    static int last_debug = 0;
    char buff[1024] = {'\0'};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buff, sizeof(buff), format, arguments);
    Serial.print(debug_prefix);
    Serial.print(buff);

    debug_logs[last_debug] = String(debug_prefix) + String(buff);
    last_debug += 1;

    if (last_debug >= DEBUG_MAX)
    {
        last_debug = 0;
    }

    va_end(arguments);
#endif
}
