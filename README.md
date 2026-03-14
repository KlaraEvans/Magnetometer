# Magnetometer 
DE:
Software für das selbstgebaute Magnetometer mit dem selbstgebauten Fluxgate-Sensor. Befindet sich noch in Bearbeitung.
### Anleitung für Flashen der Programme:
Für ESP32: Ordner "cos-generator" aus "esp32" herunterladen und als Projekt in Arduino IDE öffnen

Für Pico:
1) Ordner "magnetometer" erstellen
2) Alle Dateien aus dem Ordner Pico kopieren
3) VS Code und Pico SDK Extension installieren
4) Pico SDK Extension: Import Project -> Ordner "magnetometer" finden -> Import
5) RPi-Pico Bootsell drücken und Pico einstecken
6) Build
### Änderung der Frequenz
Gerade ist die Frequenz von ESP32 auf 4100 Hz eingestellt, und so ist die Anzahl N0 und N in Pico Programm. 
Um die Frequenz zu ändern, stelle sie in ESP32 Programm um, dann in CMakeFiles.txt bei "add executable" magnetometer.cpp kommentieren: "#magnetometer.cpp" und "#test.cpp" auskommentieren. Dann mit Anpassen verschiedener Werte von N in test.cpp finde den Wert N, für den das Bildschirm ungefähr 1 ausgibt. Das ist dann N0 in der Datei "magnetometer.cpp". N ist dann in der Regel 2*N0 zu einstellen.
