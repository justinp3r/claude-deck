/* Zeilenprotokoll zur Bridge: ein JSON-Objekt pro Zeile, in beide Richtungen.
 * Welche Nachrichten es gibt, steht in protocol.cpp und in bridge/bridge.mjs. */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>

void protocol_begin(void);

/* Aus loop() aufrufen. Liest Serial und wendet Kommandos an. */
void protocol_poll(void);

/* Eine Sekunde ist vergangen: Countdown der laufenden Anfrage fortschreiben. */
void protocol_tick_second(void);

/* Hat sich die Bridge schon einmal gemeldet? Solange nicht, laeuft die Demo. */
bool protocol_host_seen(void);

#endif
