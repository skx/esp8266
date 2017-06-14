
//
// If this is defined we output debug-messages over the serial
// console.
//
#define DEBUG 1

//
// Record a debug-message, only if `DEBUG` is defined.
//
void DEBUG_LOG(const char *format, ...)
{
#ifdef DEBUG
    char buff[1024] = {'\0'};
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buff, sizeof(buff), format, arguments);
    Serial.print(buff);
    va_end(arguments);
#endif
}
