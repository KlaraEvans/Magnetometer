# Magnetometer 
DE:
Software für das selbstgebaute Magnetometer mit dem selbstgebauten Fluxgate-Sensor. Dieses Readme und das Programm befinden sich noch in Bearbeitung.

Gerade verfügt das Magnetometer über ein Bildschirm mit einer SD Karte (ST7735 1,8 Zoll) und über eine RTC.
RTC gibt die Zeit (und kann die Temperatur ausgeben falls DS3231). Auf der SD Karte wird der mit einer der 3 Methoden berechnete Wert der Spannung sowie die Zeit, der Tag und die Temperatur gespeichert.
In der Zukunft ist es geplannt, noch Eingabetasten und die Kommunikation zwischen dem Pico und dem ESP32 einzuführen.
### Anleitung für Flashen der Programme:
Für ESP32: Ordner "cos-generator" aus "esp32" herunterladen und als Projekt in Arduino IDE öffnen

Für Pico:
1) VS Code und Pico SDK Extension installieren
2) Pico SDK Extension: Import Project -> Ordner "pico" finden -> Import
3) ggf. RPi-Pico Bootsell drücken und Pico einstecken
4) Build
Auf Linux muss in der Datei .vscode/tasks.json an der Stelle "Run Project" folgendes:
{
            "label": "Run Project",
            "type": "process",
            "command": "sudo",
            "args": [
               "${env:HOME}/.pico-sdk/picotool/2.2.0-a4/picotool/picotool",
                "load",
                "${command:raspberry-pi-pico.launchTargetPath}",
                "-fx"
            ],....
}
Also nur Zeilen 23 und 24 ergänzen. 

