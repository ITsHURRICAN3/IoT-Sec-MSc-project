# Analisi degli Use Cases (Casi d'Uso) - ESP32 Warehouse

Questo documento descrive le funzionalità legittime del sistema, identificando gli attori e i flussi di interazione principali tra Client e Server.

## Attori del Sistema
1.  **Magazziniere (User)**: Utente umano dotato di Tag RFID e Password.
2.  **Client ESP32**: Dispositivo di interfaccia che legge RFID e comunica con il Server.
3.  **Server ESP32**: Dispositivo centrale che gestisce il database su SD Card e la crittografia.

---

## 1. Registrazione Utente (User Registration)
Permette a un nuovo operatore di registrarsi nel sistema per abilitare l'accesso futuro.

*   **Attori**: Magazziniere, Client, Server.
*   **Precondizioni**: Il sistema è acceso e connesso alla rete WiFi.
*   **Flusso Principale**:
    1.  Il Magazziniere seleziona l'opzione di registrazione sul Client o via Serial Monitor.
    2.  Il Client richiede: Username e Password.
    3.  Il Client richiede la scansione del Tag RFID personale.
    4.  Il Client deriva una coppia di chiavi (Ed25519) usando `Hash(UID + Password)` come seed.
    5.  Il Client invia al Server il comando `REG <username> <public_key_hex>` attraverso il canale cifrato.
    6.  Il Server verifica che l'utente non esista già.
    7.  Il Server salva l'utente e la chiave pubblica nel file cifrato `users.txt`.
    8.  Il Server conferma l'avvenuta registrazione.

## 2. Autenticazione (User Login)
Permette all'operatore di accedere alle funzionalità di gestione del magazzino.

*   **Attori**: Magazziniere, Client, Server.
*   **Precondizioni**: L'utente è registrato.
*   **Flusso Principale**:
    1.  Il Magazziniere richiede il Login e fornisce Username e Password.
    2.  Il Magazziniere scansiona il proprio Tag RFID.
    3.  Il Client rigenera le chiavi crittografiche (Private Key) al volo.
    4.  Il Client invia `LOG <username>` al Server.
    5.  Il Server risponde con una **Challenge** (32 byte random).
    6.  Il Client firma digitalmente la Challenge con la Private Key derivata.
    7.  Il Client invia la firma al Server.
    8.  Il Server verifica la firma usando la Public Key salvata su SD.
    9.  Se valida, il Server concede l'accesso e invia il Menu Principale.

## 3. Consultazione Magazzino (Read Inventory)
Visualizza l'elenco completo degli articoli e le relative quantità.

*   **Attori**: Magazziniere Autenticato, Server.
*   **Flusso Principale**:
    1.  L'utente invia il comando `READ`.
    2.  Il Server legge il file `warehouse.txt`.
    3.  Il Server decifra ogni riga usando la chiave di storage hardware.
    4.  Il Server formatta la lista (Item -> Qty).
    5.  Il Server invia la lista cifrata al Client per la visualizzazione.

## 4. Inserimento Nuovo Articolo (New Entry)
Aggiunge un nuovo tipo di prodotto al magazzino.

*   **Attori**: Magazziniere Autenticato.
*   **Flusso Principale**:
    1.  L'utente invia `NEW ENTRY <item_name> <qty>`.
    2.  Il Server verifica che il nome sia alfanumerico (5-50 caratteri) e la quantità valida.
    3.  Il Server controlla che l'articolo non esista già.
    4.  Il Server cifra la nuova entry e la appende al file `warehouse.txt`.
    5.  Il Server conferma l'operazione.

## 5. Aggiornamento Quantità (Update/Add/Sub)
Modifica la giacenza di un articolo esistente. Include tre varianti:
*   **UPDATE**: Imposta una quantità specifica.
*   **ADD**: Aggiunge alla quantità esistente.
*   **SUB**: Sottrae dalla quantità esistente.

*   **Flusso Principale (es. ADD)**:
    1.  L'utente invia `ADD <item_name> <qty>`.
    2.  Il Server cerca l'articolo nel database.
    3.  Il Server calcola la nuova quantità (verificando overflow o valori negativi).
    4.  Il Server riscrive il database aggiornando la riga cifrata dell'articolo.
    5.  Il Server notifica il nuovo saldo.

## 6. Rimozione Articolo (Delete Item)
Rimuove definitivamente un articolo dal magazzino.

*   **Attori**: Magazziniere Autenticato.
*   **Flusso Principale**:
    1.  L'utente invia `DELETE <item_name>`.
    2.  Il Server verifica l'esistenza dell'articolo.
    3.  Il Server rigenera il file `warehouse.txt` omettendo l'articolo specificato.
    4.  Il Server conferma l'eliminazione.
