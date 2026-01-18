# Diagramma di Sequenza: Login Utente (Challenge-Response)

Questo diagramma illustra il flusso di autenticazione sicura. Il sistema utilizza un approccio **Zero-Knowledge Proof (ZKP)**: il server non conosce la password né la chiave privata dell'utente, ma verifica la sua identità tramite una firma digitale su una sfida casuale (Challenge).

```mermaid
sequenceDiagram
    participant U as User (Magazziniere)
    participant C as Client ESP32
    participant S as Server ESP32
    participant SD as Server SD Card

    Note over U, C: Fase 1: Richiesta Accesso
    U->>C: Input Username ("Mario")
    U->>C: Input Password ("1234")
    U->>C: Scansione RFID Tag

    Note over C: Fase 2: Rigenerazione Chiavi
    C->>C: UID = Leggi UID Tag
    C->>C: Seed = Hash(UID + Password)
    C->>C: Rigenera Private Key da Seed

    Note over C, S: Fase 3: Handshake
    C->>S: LOG "Mario"
    
    activate S
    S->>SD: Get PubKey per "Mario"
    SD-->>S: PubKey (se esiste)
    
    alt Utente Sconosciuto
        S-->>C: Errore: User not found
        C->>U: Mostra Errore
    else Utente Trovato
        Note over S: Fase 4: Sfida (Challenge)
        S->>S: Genera Rnd (32 byte)
        S-->>C: Invia Challenge (Rnd)
        
        Note over C: Fase 5: Firma
        C->>C: Firma Rnd con Private Key
        C-->>S: Invia Firma
        
        Note over S: Fase 6: Verifica
        S->>S: Verifica Firma con PubKey
        
        alt Firma Valida
            S->>S: State = LOGGED_IN
            S-->>C: Login Success + Menu
            C->>U: Mostra Menu
        else Firma Invalida
            S-->>C: Auth Failed
            C->>U: Accesso Negato
        end
    end
    deactivate S
```
