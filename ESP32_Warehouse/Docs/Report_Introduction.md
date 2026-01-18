# Introduzione al Progetto ESP32 Warehouse

## Contesto e Motivazioni
Con la rapida espansione dell'Internet of Things (IoT) in ambito industriale, la gestione remota dei magazzini permette un'efficienza senza precedenti. Tuttavia, i dispositivi IoT "low-cost" sono spesso afflitti da gravi lacune di sicurezza, come trasmissioni in chiaro e meccanismi di autenticazione deboli, che li rendono bersagli facili per attacchi informatici.

## Obiettivo del Progetto
Questo progetto mira a realizzare un sistema **Secure-by-Design** per la gestione di un magazzino merci distribuito. L'obiettivo principale è dimostrare come sia possibile implementare standard di sicurezza elevati (crittografia end-to-end, autenticazione forte, integrità dei dati) anche su hardware con risorse limitate come il microcontrollore **ESP32**, senza comprometterne le prestazioni.

## Soluzione Proposta
Il sistema "ESP32 Warehouse" è composto da un Server centrale (che custodisce il database cifrato su SD Card) e da Client mobili per gli operatori.
A differenza delle implementazioni standard, questo progetto rifiuta l'uso di connessioni non protette. Abbiamo integrato la libreria crittografica **LibSodium** per garantire:
1.  **Confidenzialità Totale**: Tutto il traffico WiFi è cifrato con l'algoritmo **ChaCha20-Poly1305**.
2.  **Autenticazione a Due Fattori (2FA)**: L'accesso richiede sia il possesso fisico di un **Tag RFID** autorizzato, sia la conoscenza di una **Password**.
3.  **Protezione Fisica**: I dati salvati sulla memoria SD sono cifrati "a riposo" (Data at Rest) per prevenire furti di informazioni in caso di sottrazione fisica del dispositivo.

Questa architettura assicura che solo gli operatori legittimi possano visualizzare o modificare l'inventario, proteggendo l'azienda da sabotaggi, furti di dati e accessi non autorizzati.
