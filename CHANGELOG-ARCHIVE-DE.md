### Syntax

- **Standalone** Änderungen zur Standalone Version unter OS X, Windows und Linux
- **Plugin** Änderungen an der Plugin Variante (Audio Unit, LV2 and VST)
- **Box** Modifikationen der "orangenen" Box (Software)
- **Generell** Allgemeine Veränderungen


### v19.03.1-alpha (04.04.2019)

- **Generell** SVG Fonts entfernt - kleinere Pakete
- **Standalone** OnAir stürzte ab wenn kein anderer Call aktiv war 
- **Standalone** Alte Studio Link Standalone Versionen funktionierten nach dem ausführen nicht mehr 


### v19.03.0-alpha (31.03.2019)

- **Generell** OpenSSL 1.1.1b
- **Generell** Opus 1.3
- **Generell** Baresip 0.6.0
- **Generell** re 0.6.0
- **Generell** rem 0.6.0
- **Misc** #48 - 127.0.0.1 anstatt localhost als URL
- **Misc** #45 - Feste DNS Server in baresip
- **Misc** Verbesserungen an der Benutzeroberfläche (neues Logo etc.)
- **Plugin** Anzahl der Spuren von 8 auf 10 erhöht
- **Standalone** RtAudio hinzugefügt
- **Standalone** Auswahl Audio Interface
- **Standalone** Mehrspurfähig inkl. N-1
- **Standalone** Mehrspuraufnahme - max. Spuren


### v17.03.1-beta (30.03.2017)

- **Standalone** Der Dateiname vom Recording enthält nun eine eindeutige Sitzungskennzeichnung


### v17.03.0-beta (23.03.2017)

- **Generell** OpenSSL 1.1.0e
- **Standalone** Flac 1.3.2
- **Plugin** Auto N-1 Mix Option entfernt (ist nun immer aktiv)
- **onAir** Streaming Bug macOS auval und Logic gefixt
- **Plugin** **onAir** Windows Crash Dumps beim deaktivieren gefixt


### v16.12.1-beta (30.01.2017)

- **Standalone** Record Funktion (FLAC) stabilität verbessert


### v16.12.0-beta (16.12.2016)

- **Generell** Crash bug unter windows 10 gefixt
- **Generell** OpenSSL 1.1.0c
- **Generell** Opus 1.1.3
- **Generell** FLAC Support hinzugefügt
- **Generell** Baresip und re libs aktualisiert
- **Plugin** OPUS Fehler nach mehreren neustarts gefixt
- **onAir** Erstes Release vom neuen Plugin
- **Standalone** Mono Button Funktion hinzugefügt
- **Standalone** Record Funktion (FLAC) eingebaut


### v16.04.1-beta (02.07.2016)

- **Plugin** Anzahl der Tracks erhöht (6->8)
- **Plugin Linux LV2 (Linux)** Fehlerhafte Synchronisierung der Spuren behoben
- **Generell** Einseitiger Ausfall des Audio Signals (meistens nach 30-300 Sekunden) gefixt


### v16.04.0-beta (23.05.2016)

- **Generell** Nummernwahl durch Enter-Taste bestätigen
- **Generell** Webinterface vereinfacht
- **Plugin** Möglichen OSX auval crash Bug gefixt
- **Plugin** Konflikt mit Bluetooth Bibliothek unter Linux behoben #5
- **Standalone** Abtastrate wird nun unter OSX automatisch ausgewählt (48kHz)
- **Standalone** MUTE Taste bei aktiven Gespräch


### v16.02.2-beta (03.03.2016)

- **Plugin** Bug im Audio Routing behoben
- **Plugin** Windows/VST: Versionsinformationen hinzugefügt
- **Generell** OpenSSL 1.0.2g


### v16.02.1-beta (14.02.2016)

- **Plugin** Neue Bypass Funktion (für den Schnitt)
- **Misc** Audio Spuren umbenannt
- **Generell** re 0.4.15


### v16.02.0-beta (07.02.2016)

- **Generell** Geisteraustreibung von nicht sichtbaren Gesprächen
- **Generell** Pegelanzeige etwas besser dem Verhalten von DAWs angepasst (Übergang von Grün zu Gelb -18db)
- **Standalone** Fehlerhafte Mikrofon Erkennung in OS X verbessert
- **Standalone** OS X App Bundle ist nun signiert
- **Plugin** Apple Logic auval Abstürze behoben
- **Plugin** Anzahl der max. Spuren von 4 auf 6 erhöht
- **Plugin** Automatischer N-1 Mix (deaktivierbar)
- **Generell** Anzeige der Studio Link SIP ID auf der Hauptseite
- **Generell** Support von OS X Versionen <10.11
- **Generell** Baresip 0.4.17
- **Generell** rem 0.4.7
- **Generell** Opus 1.1.2
- **Generell** OpenSSL 1.0.2f


### v15.12.2-beta (05.01.2016)

- **Standalone** OSX Version in ein schickes App Bundle verpackt
- **Plugin** VST Crashes unter Windows beim Starten gefixt


### v15.12.1-beta (31.12.2015)

- **Generell** Benachrichtung über Updates
- **Generell** Fehlerhafte Pegelanzeige unter Windows gefixt


### v15.12.0-beta (24.12.2015)

- **Generell** Komplette Neuimplementierung der Webanwendung
- **Generell** Bessere Unterstützung/Anzeige von parallelen Gesprächen
- **Plugin** Erste LV2 und Audio Unit Version
- **Standalone** Erste OSX and Linux Version
- **Standalone** Pegelanzeige
- **Generell** Opus 1.1.1
- **Generell** OpenSSL 1.0.2e (Linux)
- **Generell** Baresip 0.4.16
