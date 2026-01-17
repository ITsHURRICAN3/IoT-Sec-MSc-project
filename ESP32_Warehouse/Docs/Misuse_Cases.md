# Analisi dei Misuse Cases (Casi di Abuso) - ESP32 Warehouse

Questo documento descrive i possibili scenari di attacco (Misuse Cases) identificati analizzando l'architettura e il codice sorgente del progetto. Questi casi possono essere formalizzati in "Misuse Case Diagrams" (estensione dei diagrammi Use Case UML).

## 1. Denial of Service (DoS) del Server
Questo scenario descrive come un attaccante può rendere il magazzino inaccessibile agli utenti legittimi.

*   **Attore (Threat Agent)**: Utente Maligno (nella rete WiFi o Jammer).
*   **Obiettivo**: Rendere il servizio irraggiungibile.
*   **Vulnerabilità Sfruttata**: Il Server ESP32 è "Single-threaded" e le operazioni di rete sono bloccanti (es. handshake timeout di 5 secondi).
*   **Scenario Gradi (Steps)**:
    1.  L'attaccante si connette alla rete WiFi `ESP_Server_AP`.
    2.  L'attaccante apre una connessione TCP verso la porta 80.
    3.  L'attaccante *non* invia alcun dato (o invia dati molto lentamente - "Slowloris").
    4.  Il Server attende i dati per il Key Exchange bloccando il loop principale per 5 secondi (`millis() - start < 5000`).
    5.  Durante questo tempo, nessun altro Client può connettersi o operare.
    6.  L'attaccante ripete la connessione immediatamente dopo il timeout.
*   **Diagramma suggerito**:
    *   **Misuse Case**: "Bloccare Accesso al Server".
    *   **Relazione "Threatens"**: Minaccia lo Use Case "Login" o "Operazioni Magazzino".

## 2. Man-in-the-Middle (MITM) Relay Attack
Poiché il Client non verifica l'identità del Server (non c'è una "Certificate Authority" o Pinning della chiave pubblica del Server), un attaccante può interporsi.

*   **Attore**: Attaccante di Rete (con hardware WiFi, es. un altro ESP32 o Laptop).
*   **Obiettivo**: Intercettare dati o impartire comandi fraudolenti.
*   **Scenario**:
    1.  L'attaccante crea un Access Point con lo stesso SSID (`ESP_Server_AP`) e Password del vero Server, ma con segnale più forte.
    2.  L'attaccante si assegna l'IP `192.168.4.1` (hardcodato nel Client).
    3.  Il Client legittimo si connette all'Attaccante credendo sia il Server.
    4.  L'Attaccante si connette al vero Server impersonando il Client.
    5.  L'Attaccante fa da "ponte" (Relay) per l'Handshake crittografico.
    6.  **Risultato**: Il canale è cifrato, ma l'attaccante controlla il flusso e può tentare di alterare messaggi o semplicemente negare il servizio (Blackhole).
*   **Nota**: L'autenticazione utente (Challenge-Response) mitiga l'iniezione di comandi *se* l'attaccante non possiede la chiave privata utente, ma il MITM rimane tecnicamente fattibile a livello di trasporto.

## 3. Furto Fisico e Dump del Firmware (Physical Attack)
Scenario in cui la sicurezza fisica del magazzino viene violata.

*   **Attore**: Ladro / Insider.
*   **Obiettivo**: Decifrare il database del magazzino e degli utenti.
*   **Scenario**:
    1.  L'attaccante ruba fisicamente la scheda ESP32 Server e la SD Card.
    2.  L'attaccante legge la SD Card ma trova file cifrati (ChaCha20-Poly1305).
    3.  L'attaccante effettua il dump del firmware dall'ESP32 (via USB/JTAG se non disabilitati).
    4.  L'attaccante estrae la stringa `storage_key` hardcodata nel codice (`"AcademicSecureKey..."`).
    5.  L'attaccante usa la chiave per decifrare `warehouse.txt` e `users.txt`.
*   **Diagramma suggerito**:
    *   **Misuse Case**: "Estrarre Credenziali da Hardware".
    *   **Relazione "Mitigates"**: Lo Use Case "Secure Boot / Flash Encryption" (non implementato nel codice base) dovrebbe mitigare questo rischio.

## 4. Enumerazione Utenti (User Enumeration)
Il sistema rivela se un utente esiste o no durante il tentativo di login.

*   **Attore**: Spia / Utente non autorizzato.
*   **Obiettivo**: Scoprire quali utenti sono registrati nel sistema.
*   **Scenario**:
    1.  L'attaccante si connette al server (anche scrivendo un client custom in Python).
    2.  Invia comandi `LOG <nome_tentativo>`.
    3.  Osserva la risposta:
        *   Se riceve `Error: User not found.`, l'utente non esiste.
        *   Se riceve una Challenge (stringa hex 64 byte), l'utente esiste.
    4.  L'attaccante costruisce una lista di utenti validi per futuri attacchi (es. ingegneria sociale).

## 5. Clonazione Identità (Identity Theft)
Compromissione dei fattori di autenticazione lato Client.

*   **Attore**: Collega malevolo / Ladro.
*   **Scenario**:
    1.  **Fattore 1 (Possesso)**: L'attaccante usa un lettore RFID portatile (es. Flipper Zero) per clonare il Tag UID del Client mentre è incustodito o vicino (Skimming).
    2.  **Fattore 2 (Conoscenza)**: L'attaccante osserva il Client digitare la password su Serial Monitor (Shoulder Surfing) o installa uno sniffer USB.
    3.  L'attaccante usa un proprio ESP32 Client con il Tag clonato e la password rubata per autenticarsi come l'admin e svuotare il magazzino (`DELETE` items).
