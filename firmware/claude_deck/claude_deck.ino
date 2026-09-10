// Absichtlich leer.
//
// Arduino braucht eine .ino-Datei mit dem Namen des Sketch-Ordners, aber genau
// diese Datei laeuft durch den Arduino-Praeprozessor, der Funktionsprototypen
// per ctags einfuegt. Auf Apple Silicon ist das mitgelieferte ctags ein
// x86-Binary; der Ersatz (universal-ctags) meldet Zeilennummern um eins
// versetzt und ein anderes typeref-Format, wodurch die Prototypen kaputt
// eingefuegt werden.
//
// Deshalb steht hier nichts. setup() und loop() liegen in main.cpp und werden
// als normales C++ uebersetzt, ganz ohne Praeprozessor-Magie.
