# Diagramma di Sequenza: Registrazione Utente

Questo diagramma illustra il flusso di registrazione aggiornato con il controllo di unicità del Tag RFID.

```mermaid
sequenceDiagram
    participant U as User (Magazziniere)
    participant C as Client ESP32
    participant S as Server ESP32
    participant SD as Server SD Card

    Note over U, C: Fase 1: Input Credenziali
    U->>C: Input Username ("Mario")
    U->>C: Input Password ("1234")
    U->>C: Scansione RFID Tag

    Note over C: Fase 2: Derivazione Identità
    C->>C: UID = Leggi UID Tag
    C->>C: Seed = Hash(UID + Password)
    C->>C: Genera KeyPair (Pub, Priv) da Seed
    C->>C: TagHash = Hash(UID)

    Note over C, S: Fase 3: Registrazione Sicura
    C->>S: REG "Mario" <PubKey> <TagHash>
    
    activate S
    S->>S: Decifra comando
    
    Note over S, SD: Fase 4: Verifiche
    S->>SD: Leggi users.txt
    SD-->>S: Lista utenti
    
    S->>S: Check 1: Username esiste?
    alt Username Occupato
        S-->>C: Errore: User exists
        C->>U: Mostra Errore
    else Username Libero
        S->>S: Check 2: TagHash esiste?
        alt Tag Già Registrato
            S-->>C: Errore: Tag already registered!
            C->>U: Mostra Errore
        else Tag Nuovo
            Note over S, SD: Fase 5: Salvataggio
            S->>SD: Append "Mario;PubKey;TagHash" (Cifrato)
            SD-->>S: OK
            S-->>C: REG SUCCESS
            C->>U: "Registrazione Completata"
        end
    end
    deactivate S
```
