# 📝 Report Tecnico: Emulazione Bluetooth Toolchain su Linux
```bash
[LedSaber-BLE]> info
Device 10:52:1C:75:58:56 (public)
	Name: LedSaber-BLE
	Alias: LedSaber-BLE
	Class: 0x0002c110 (180496)
	Icon: computer
	Paired: yes
	Bonded: yes
	Trusted: yes
	Blocked: no
	Connected: yes
	LegacyPairing: no
	CablePairing: no
	UUID: Serial Port               (00001101-0000-1000-8000-00805f9b34fb)
	UUID: Generic Access Profile    (00001800-0000-1000-8000-00805f9b34fb)
	UUID: Generic Attribute Profile (00001801-0000-1000-8000-00805f9b34fb)
	UUID: Vendor specific           (4fafc201-1fb5-459e-8fcc-c5c9c331914b)
	AdvertisingFlags:
  06                                               .               
[LedSaber-BLE]> 
```
## 🎯 Obiettivo

Sostituire le applicazioni Android "Serial Bluetooth Terminal" (per la comunicazione SPP/RFCOMM) e "nRF Connect" (per il controllo BLE/GATT) utilizzando strumenti nativi Linux (stack BlueZ) da riga di comando per maggiore potenza e controllo.

---

## 1. ⚙️ Soluzione per la Porta Seriale (SPP/RFCOMM)

Per comunicare con moduli Bluetooth classici (come HC-05 o ESP32 in modalità SPP) che emulano una porta seriale, è necessario creare un device file virtuale (TTY) nel filesystem Linux.

### A. Tool: `rfcomm` (Parte di `bluez-deprecated-tools`)

*   **Funzione**: Implementa il protocollo RFCOMM (Radio Frequency Communication), che emula la comunicazione RS-232 (porta seriale) sul Bluetooth Classico.
*   **Best Practice/Note**: Il comando è stato rimosso dal pacchetto standard su Arch/CachyOS ed è stato necessario installare `bluez-deprecated-tools` per renderlo disponibile.

| Fase          | Comando                               | Spiegazione                                                                                             |
|---------------|---------------------------------------|---------------------------------------------------------------------------------------------------------|
| **Installazione** | `sudo pacman -S bluez-deprecated-tools` | Installazione del pacchetto che contiene il tool `rfcomm`.                                              |
| **Bind**        | `sudo rfcomm bind 0 <MAC_ADDRESS>`    | Associa il MAC address del dispositivo alla porta seriale virtuale `/dev/rfcomm0`. Questa porta rimane "creata" finché non viene rilasciata. |
| **Release**     | `sudo rfcomm release 0`               | Rilascia la connessione e rimuove il device file `/dev/rfcomm0`.                                        |

### B. Tool: `screen` o `CuteCom`

*   **Funzione**: Agiscono come terminali per leggere e scrivere dati sul device file seriale creato da `rfcomm`.
*   **Best Practice**: `screen` è il più leggero e veloce in ambiente CLI.

| Tool          | Comando                                       | Spiegazione                                                                                             |
|---------------|-----------------------------------------------|---------------------------------------------------------------------------------------------------------|
| **Screen (CLI)**  | `screen /dev/rfcomm0 115200`                  | Avvia una sessione terminale su `/dev/rfcomm0` al baud rate di 115200 (sostituire con il baud rate corretto). |
| **CuteCom (GUI)** | (Avviare l'app e selezionare `/dev/rfcomm0`) | Fornisce un'interfaccia grafica per visualizzare i log.                                                 |

---

## 2. 🛡️ Soluzione per il Controllo BLE (GATT)

Per la scansione, la connessione e la scrittura di dati su dispositivi Bluetooth Low Energy (BLE), come nel caso del controller LED, si utilizza lo strumento ufficiale dello stack BlueZ.

### Tool: `bluetoothctl` (Parte di `bluez-utils`)

*   **Funzione**: Interfaccia da riga di comando per lo stack BlueZ. Permette di accoppiare, connettersi e, tramite il sottomenu GATT, interagire con i servizi e le caratteristiche BLE.

| Fase            | Comando                                | Spiegazione                                                                                             |
|-----------------|----------------------------------------|---------------------------------------------------------------------------------------------------------|
| **Avvio**         | `bluetoothctl`                         | Avvia l'interfaccia interattiva di BlueZ.                                                               |
| **Connessione**   | `connect <MAC_ADDRESS>`                | Stabilisce la connessione BLE con il dispositivo Peripheral.                                            |
| **Esplorazione**  | `list-attributes`                      | Elenca tutti i Servizi (GATT Service) e le Caratteristiche (GATT Characteristic) disponibili sul dispositivo connesso. |
| **Selezione**     | `select-attribute <UUID_CARATTERISTICA>` | Seleziona l'attributo specifico (la "variabile") su cui agire (es. il canale di comando LED).            |
| **Scrittura**     | `write 0x01`                           | Invia il comando (valore esadecimale) all'attributo selezionato. Esempio: `0x01` per accendere, `0x00` per spegnere il LED. |
| **Uscita**        | `quit`                                 | Esce dall'interfaccia `bluetoothctl`.                                                                   |

---

## 3. ⭐ Best Practice e Leggibilità

### Assegnazione dei Nomi (Naming Conventions)

Come emerso, l'uso di UUID generici come `beb5483e-36e1-4688-b7f5-ea07361b26a8` rende il debug estremamente difficile.

**Suggerimento**: È fondamentale rinominare le tue caratteristiche BLE nel firmware del dispositivo.

| Oggetto             | Metodo Consigliato                                                                                             | Vantaggio                                                                                                  |
|---------------------|----------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|
| **Caratteristica GATT** | Aggiungere un *Characteristic User Description Descriptor* (UUID standard: `0x2901`) nel codice del microcontrollore. | Permette agli strumenti come nRF Connect (e ad altri tool basati su D-Bus) di mostrare nomi leggibili come "LED Command" invece di un UUID. |
| **Comandi (Valori)**  | Standardizzare i valori che invii. Ad esempio, usa sempre `0x01` per ON, `0x00` per OFF e documenta i formati (es. `RRGGBB`). | Riduce la confusione e semplifica la scrittura di script automatizzati.                                    |

### Scripting (Automazione CLI)

La potenza degli strumenti CLI su Linux sta nella loro automazione. Una volta individuati gli UUID corretti, puoi scrivere script bash per eseguire comandi complessi senza dover avviare interfacce grafiche o interattive.

```bash
# Esempio concettuale per accendere il LED da uno script esterno.
# (Richiede un wrapper D-Bus o uno script 'expect' per automatizzare l'input a bluetoothctl)

MAC_ADDRESS="10:52:1C:75:58:56"
ATTRIBUTE_UUID="beb5483e-36e1-4688-b7f5-ea07361b26a8"

bluetoothctl << EOF
connect $MAC_ADDRESS
select-attribute $ATTRIBUTE_UUID
write "0x01"
disconnect
quit
EOF
```

---

C'è qualche sezione specifica di questo report che vorresti approfondire, ad esempio l'automazione dei comandi BLE?