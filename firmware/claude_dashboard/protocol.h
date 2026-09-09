/* Zeilenprotokoll zwischen Bridge (Mac) und Geraet.
 *
 * Ein JSON-Objekt pro Zeile, in beide Richtungen. JSON und nicht etwas
 * Einfacheres, weil hier echte Shell-Befehle durchlaufen: die enthalten
 * Anfuehrungszeichen, Tabs und Zeilenumbrueche, und genau dafuer ist das
 * Escaping von JSON da.
 *
 * Bridge -> Geraet
 *   {"t":"req","id":..,"agent":..,"init":..,"tool":..,"l1":..,"l2":..,
 *    "meta":..,"full":..,"cwd":..,"note":..,"warn":..,
 *    "qpos":1,"qtot":1,"next":"","ttl":30}
 *   {"t":"idle","hdr":..,"clock":..,"rows":[{"n":..,"a":..,"on":true},..]}
 *   {"t":"cancel","id":..}      Anfrage ist anderweitig beantwortet
 *   {"t":"link","up":false}     Bridge meldet sich ab
 *   {"t":"ping"}
 *
 * Geraet -> Bridge
 *   {"t":"hello"}               nach dem Start
 *   {"t":"decision","id":..,"v":"allow"|"deny"}
 *   {"t":"pong"}
 */
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
