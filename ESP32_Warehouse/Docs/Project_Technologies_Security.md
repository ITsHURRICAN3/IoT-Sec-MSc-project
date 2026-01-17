# Tecnologie e Logiche di Sicurezza del Progetto ESP32 Warehouse

Di seguito un'analisi delle tecnologie e delle teorie crittografiche implementate nel sistema, con le motivazioni, sicurezza e scelte progettuali.

## 1. Crittografia e Libreria Core
*   **Tecnologia**: **LibSodium** (porting per Arduino/ESP32).
*   **Utilizzo**: Gestione di tutte le primitive crittografiche (scambio chiavi, cifratura autenticata, firme digitali, hashing).
*   **Perché**: È una libreria standard industriale moderna, progettata per essere "sicura di default" (riduce il rischio di errori di implementazione come il riutilizzo dei nonce o attacchi *timing*). È molto più sicura rispetto all'uso di primitive "grezze" come l'AES puro.

## 2. Scambio Chiavi (Key Exchange) & Forward Secrecy
*   **Teoria**: **Elliptic Curve Diffie-Hellman (ECDH)** su curva **X25519**.
*   **Funzioni**: `crypto_kx_keypair`, `crypto_kx_client_session_keys`, `crypto_kx_server_session_keys`.
*   **Perché**:
    *   Permette a Client e Server di concordare una chiave di sessione segreta su un canale insicuro (WiFi) senza mai trasmetterla.
    *   **Forward Secrecy**: Le chiavi sono *effimere* (generate nuove ad ogni connessione). Se un attaccante compromette la chiave di una sessione passata, non può decifrare le sessioni future o precedenti.
    *   X25519 è estremamente veloce su microcontrollori e offre un margine di sicurezza molto alto (128-bit security level).

## 3. Cifratura del Canale (Transport Layer)
*   **Algoritmo**: **ChaCha20-Poly1305** (Authenticated Encryption with Associated Data - AEAD).
*   **Utilizzo**: Cifratura di tutti i pacchetti dati scambiati dopo l'handshake.
*   **Perché**:
    *   **Velocità**: ChaCha20 è un cifrario a flusso molto veloce via software, ideale per ESP32 che potrebbe non avere sempre accelerazione hardware AES disponibile o configurata.
    *   **Integrità (Poly1305)**: Oltre alla confidenzialità (leggibilità), garantisce l'autenticità. Se un attaccante modifica anche solo un bit del messaggio cifrato (bit-flipping attack), la verifica del tag Poly1305 fallisce e il pacchetto viene scartato. Protegge da attacchi *Man-in-the-Middle* attivi.

## 4. Autenticazione Utente (Challenge-Response)
*   **Teoria**: **Firma Digitale Ed25519** (Variante di Schnorr signature su curva Ed25519).
*   **Meccanismo**:
    1.  Il Server invia una **Challenge** (un numero casuale/Nonce di 32 byte).
    2.  Il Client firma la challenge con la sua Chiave Privata utente.
    3.  Il Server verifica la firma con la Chiave Pubblica registrata dell'utente.
*   **Perché**:
    *   **Zero-Knowledge Property**: La password o la chiave privata non vengono MAI inviate in rete. Il server non le conosce e non le salva.
    *   **Protezione Replay Attack**: Poiché la "Challenge" è casuale e diversa ogni volta, un attaccante non può registrare una vecchia risposta di login e riusarla (la firma non sarebbe valida per la nuova challenge).

## 5. Protezione Dati a Riposo (Data at Rest)
*   **Logica**: Cifratura del Database su SD Card.
*   **Algoritmo**: ChaCha20-Poly1305 con una chiave di storage statica ("Secure Element" simulato).
*   **Perché**:
    *   Se un attaccante ruba fisicamente la scheda SD, non può leggere i dati (inventario e lista utenti) né modificarli (causa check integrità Fallito) senza possedere la chiave di storage hardcodata nell'ESP32 Server.

## 6. Generazione Credenziali (Key Derivation)
*   **Algoritmo**: **BLAKE2b** (Generic Hash).
*   **Utilizzo**: `deriveIdentity(UID + Password)`.
*   **Perché**:
    *   Permette di trasformare input a bassa entropia (password umana) o hardware (RFID UID) in una chiave crittografica (Seed) in modo **deterministico**.
    *   L'utente non deve memorizzare un file di "chiave privata", essa viene rigenerata al volo quando presenta il Tag RFID e la password corretta.

## 7. Input Validation (Sicurezza Software)
*   **Logica**: **Whitelisting** (Lista bianca).
*   **Funzione**: `isValidName()`, `isValidNumber()`.
*   **Perché**:
    *   Accetta solo caratteri A-Z, 0-9.
    *   Blocca alla radice attacchi di **Command Injection** e **Delimiter Injection** (es. inserire `;` per corrompere il file database CSV-like). È l'approccio difensivo più robusto rispetto al "Blacklisting" (cercare di bloccare caratteri cattivi specifici).

## 8. Hardware Security (Fisica)
*   **Componente**: **RFID (MFRC522)**.
*   **Utilizzo**: Fattore di possesso ("Qualcosa che hai").
*   **Perché**: Aggiunge un secondo fattore di autenticazione (2FA implicito: Tag Fisico + Password). Senza il tag fisico corretto, anche conoscendo la password, non è possibile generare la chiave privata corretta (poiché l'UID del tag è parte del seed).

## 9. Sicurezza di Rete (Link Layer)
*   **Tecnologia**: **WPA2** (Wi-Fi Protected Access 2).
*   **Utilizzo**: Protezione della rete SoftAP generata dal Server.
*   **Perché**:
    *   Fornisce il primo livello di difesa (**Defense in Depth**) cifrando il traffico a livello radio (AES-CCMP).
    *   Impedisce l'accesso alla rete (e quindi il tentativo di connessione alla porta 80) a chi non possiede la password del WiFi, riducendo la superficie di attacco esposta.
