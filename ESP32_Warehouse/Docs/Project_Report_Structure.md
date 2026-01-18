# Struttura Documentazione Progetto ESP32 Warehouse

Basato sui 9 punti richiesti, ecco una proposta dettagliata di cosa inserire in ciascuna sezione, facendo riferimento diretto al codice di `Server.ino` e `Client.ino`.

## 1. Introduzione
*   **Contesto**: Descrizione di un sistema IoT per la gestione remota di un magazzino merci.
*   **Obiettivo**: Realizzare un prototipo sicuro (Security-by-Design) utilizzando microcontroller a basso costo (ESP32).
*   **Componenti**:
    *   **Server**: Agisce come Access Point e Database centrale (su SD Card).
    *   **Client**: Interfaccia utente dotata di lettore RFID per l'autenticazione.
*   **Tecnologia Chiave**: Utilizzo della libreria crittografica `LibSodium` per garantire sicurezza allo stato dell'arte (non la classica crittografia 'fatta in casa').

## 2. Caso di studio
*   **Scenario**: Un magazzino fisico dove l'accesso fisico al terminale (Client) è possibile, e la comunicazione avviene su un canale wireless potenzialmente intercettabile (WiFi).
*   **Problema**: I sistemi IoT classici spesso trascurano la sicurezza (trasmissione in chiaro, nessuna autenticazione forte).
*   **Soluzione Proposta**: Un'architettura che combina possesso fisico (RFID), conoscenza (Password) e crittografia end-to-end per proteggere i dati aziendali (inventario).

## 3. Requisiti
*   **Funzionali**:
    *   Registrazione nuovi utenti tramite RFID.
    *   Login sicuro con verifica a due fattori.
    *   Operazioni CRUD (Create, Read, Update, Delete) sulle merci.
*   **Non Funzionali (Security)**:
    *   **Confidenzialità**: I dati del magazzino non devono viaggiare in chiaro.
    *   **Integrità**: I comandi non devono essere modificabili in transito.
    *   **Autenticazione**: Solo utenti autorizzati poss ono modificare l'inventario.
    *   **Disponibilità**: Il sistema deve gestire errori di connessione base.

## 4. Caso d'uso (Riferimento a `Docs/Use_Cases.md`)
*   Descrivere brevemente i flussi principali implementati nel codice:
    *   **Registrazione**: Flusso `REG` (Derivazione chiave da UID+Password -> Invio Public Key).
    *   **Login**: Flusso `LOG` -> Challenge del Server -> Firma del Client -> Verifica Server.
    *   **Gestione Inventario**: Comandi `NEW ENTRY`, `READ`, `ADD`, `SUB`, `DELETE`.

## 5. Errori e condizioni anomale
*   **Input Validation**:
    *   Funzione `isValidName()`: Rifiuta nomi con caratteri speciali (prevenzione injection).
    *   Funzione `isValidNumber()`: Rifiuta lettere dove sono attesi numeri.
    *   Controlli Logici: Tentativo di sottrarre più merce di quella presente (`qty < 0`), overflow (`> MAX_QTY`).
*   **Network Errors**:
    *   Timeout durante l'handshake (5 secondi impostati nel codice).
    *   Disconnessione improvvisa del WiFi.
*   **Crypto Errors**:
    *   Fallimento decifratura (Tag Poly1305 non valido) -> Pacchetto scartato.
    *   Firma digitale non valida durante il login.

## 6. Vulnerabilità (Analisi critica)
*   **Single Point of Failure**: Tutto risiede su un singolo ESP32 Server.
*   **Denial of Service (DoS)**: Il server è single-threaded; un loop di handshake maligno può bloccarlo (Vedi `Docs/Misuse_Cases.md`, punto 1).
*   **Trust on First Use (TOFU)**: Il Client si fida ciecamente del Server alla prima connessione (manca una CA che certifica il Server), aprendo a teorici MITM attivi.
*   **Sicurezza Fisica**: Se la SD card viene rubata, la chiave di cifratura è hardcodata nel firmware (`AcademicSecureKey...`). Se si estrae il firmware, si decifrano i dati.

## 7. Attacchi e soluzioni (Analysis & Mitigation)
| Attacco | Soluzione Implementata (nel Codice) |
| :--- | :--- |
| **Eavesdropping (Sniffing)** | Cifratura **ChaCha20-Poly1305** su tutto il traffico post-handshake. |
| **Replay Attack** (Login) | Uso di **Challenge-Response** con Nonce casuale (il server invia 32 byte random, il client firma quelli). Una vecchia firma non è valida per una nuova challenge. |
| **Man-in-the-Middle** (Passivo) | **Key Exchange X25519** (Effimero). Le chiavi di sessione non viaggiano mai sulla rete. |
| **Clonazione RFID** | **2FA**: Il solo UID del tag non basta; serve anche la password per generare la chiave privata corretta (funzione `deriveIdentity`). |
| **Tampering (Modifica Dati)** | **Poly1305 (MAC)**: Se un byte viene modificato, la verifica fallisce e il server ignora il comando. |

## 8. Scelte architetturali
*   **Perché LibSodium?** Libreria moderna, facile da usare ("Hard to misuse"), performante su ARM/ESP32.
*   **Perché ChaCha20?** Più veloce di AES sui microcontrollori senza accelerazione hardware dedicata, senza compromettere la sicurezza.
*   **Perché X25519 + Ed25519?** Curve ellittiche ad alte prestazioni (Curve25519), standard moderno (usato in TLS 1.3, WireGuard, SSH). Chiavi piccole (32 byte) ideali per pacchetti ridotti IoT.
*   **Storage Sicuro**: Scelta di cifrare i file su SD (`encryptLine`) invece di database complessi, per mantenere semplicità e portabilità, simulando un "Secure Element" via software.

## 9. Misuse Diagram (Riferimento a `Docs/Misuse_Cases.md`)
*   Sezione dedicata ai diagrammi "negativi" (cosa può andare storto).
*   Includere gli scenari:
    *   **DoS Attack**: Attaccante blocca il canale.
    *   **Physical Theft**: Furto SD Card.
    *   **User Enumeration**: Tentativi di indovinare username validi.
