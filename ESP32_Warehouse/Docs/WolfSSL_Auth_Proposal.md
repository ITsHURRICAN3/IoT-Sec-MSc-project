# Proposta di Autenticazione Server tramite WolfSSL

## 1. Il Problema Attuale
Attualmente, il sistema utilizza uno scambio chiavi **Diffie-Hellman anonimo** (via LibSodium).
*   Il protocollo garantisce la *Confidenzialità* (nessuno legge) e l'*Integrità* (nessuno modifica).
*   **NON** garantisce l'*Autenticità* del Server: Il Client non ha modo di sapere se sta parlando con il "Vero Server del Magazzino" o con un attaccante che ha clonato l'Access Point (Evil Twin).

## 2. La Soluzione: TLS 1.3 con WolfSSL
La soluzione standard industriale per questo problema è l'uso di **TLS (Transport Layer Security)**. Poiché l'ESP32 è un sistema embedded, **WolfSSL** è la libreria ideale (leggera, compatibile con Arduino/ESP-IDF) per implementare uno stack TLS completo.

L'idea è sostituire l'attuale handshake manuale con un handshake TLS standard.

---

## 3. Architettura della Soluzione (Design)

Per "autenticare" il server senza usare internet o CA pubbliche (come Let's Encrypt), dobbiamo implementare una **Private PKI (Public Key Infrastructure)** minima.

### A. Componenti Crittografici
1.  **Root CA (Certificate Authority) Offline**:
    *   Creiamo (sul PC dello sviluppatore) una coppia di chiavi e un certificato "Root" autofirmato (`ca.key`, `ca.crt`).
    *   Questa è la fonte della fiducia (Trust Anchor).

2.  **Certificato Server**:
    *   Generiamo una chiave privata per l'ESP32 Server (`server.key`).
    *   Creiamo una richiesta di firma (CSR) e la firmiamo con la `ca.key`.
    *   Otteniamo il `server.crt` firmato dalla CA.

3.  **Trust Anchor (Client)**:
    *   Prendiamo *solo* il certificato pubblico della CA (`ca.crt`) e lo inseriamo nel firmware del Client.

### B. Flusso di Connessione (Handshake)

1.  **Client Hello**: Il Client si connette e chiede di avviare TLS.
2.  **Server Hello + Certificate**: Il Server risponde e invia il suo `server.crt` (che contiene la sua chiave pubblica ed è firmato dalla CA).
3.  **Verifica (Il passo cruciale)**:
    *   Il Client riceve il certificato.
    *   Il Client controlla la **Firma Digitale** del certificato usando il `ca.crt` che ha in pancia.
4.  **Esito**:
    *   Poiché solo noi (sviluppatori) abbiamo la `ca.key`, solo noi possiamo emettere certificati validi.
    *   Un attaccante (MitM) può intercettare la connessione, ma **non** può presentare un certificato valido firmato dalla nostra CA (non ha la chiave privata della CA).
    *   Il Client rileva l'errore ("Certificate Verification Failed") e tronca la connessione.

---

## 4. Vantaggi e Svantaggi

### Vantaggi
*   **Standardizzazione**: Non "inventiamo" crittografia (Don't Roll Your Own Crypto). Usiamo protocolli testati da decenni.
*   **Sicurezza Completa**: Risolve definitivamente il problema Man-in-the-Middle dell'autenticazione server.
*   **Estendibilità**: Se un domani volessimo aggiungere un secondo server, basterebbe firmare un nuovo certificato, senza aggiornare i client.

### Svantaggi (Costi)
*   **Overhead Memoria**: WolfSSL occupa più Flash e RAM rispetto a LibSodium (che serve comunque per altre funzioni).
*   **Complessità**: Bisogna gestire il ciclo di vita dei certificati (scadenza, generazione).
*   **Prestazioni**: L'handshake TLS completo richiede più pacchetti e calcoli rispetto a un semplice ECDH.

---

## 5. Implementazione Pratica (Senza Codice)

Se dovessimo implementarlo, ecco i passi operativi:

1.  **Installazione**: Aggiungere `wolfssl` come libreria in Arduino IDE.
2.  **Generazione Chiavi (OpenSSL su PC)**:
    ```bash
    # Genera CA
    openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.crt -days 3650
    # Genera Server Key/Cert
    openssl req -newkey rsa:2048 -nodes -keyout server.key -out server.csr
    openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -set_serial 01 -out server.crt
    ```
3.  **Conversione**: Convertire `server.key`, `server.crt` e `ca.crt` in array C (`unsigned char[]`) o file su SPIFFS.
4.  **Codice**:
    *   Nel `Server`: `wolfSSL_CTX_use_certificate_buffer()` e `wolfSSL_CTX_use_PrivateKey_buffer()`.
    *   Nel `Client`: `wolfSSL_CTX_load_verify_buffer(ca_crt)`.

Questa soluzione garantisce matematicamente che il Client stia parlando con il server autorizzato.
