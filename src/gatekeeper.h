#ifndef GATEKEEPER_H
#define GATEKEEPER_H

#include <stdbool.h>

void bluetouth_signal_recived();
void bluetouth_release(); // Idel mottas
void timeout_control();

bool bluetouth_in_control();

#endif