#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

void wifi_init(void);

bool wifi_is_connected(void);

void wifi_check_connection(void);

#endif