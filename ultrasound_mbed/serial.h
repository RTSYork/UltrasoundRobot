#ifndef SERIAL_H
#define SERIAL_H

void serial_init(void);

int serial_read(char *buf, int length);
int serial_write(char *buf, int length);

int serial_printf(const char *format, ...);

#endif
