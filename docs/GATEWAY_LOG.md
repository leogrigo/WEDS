# WEDS Gateway Firmware Log

Questo documento descrive come e' organizzato il gateway WEDS nel progetto
PlatformIO principale.

Ultimo aggiornamento: 2026-05-14

## Architettura generale

Il gateway e' disponibile in due firmware separati:

- `gateway`: firmware reale, con LoRa, registry, API HTTP, dashboard e FreeRTOS.
- `gateway_self_test`: firmware di test, senza LoRa reale, che genera dati finti
  per verificare dashboard, API, eventi, trend e pagina admin.

La separazione tra firmware e' fatta con:

```ini
build_src_filter = +<${PIOENV}/>
```

Quindi ogni environment compila solo la cartella `src/<nome_env>/`.

## Librerie gateway

Le librerie gateway stanno sotto `code/lib/` e hanno responsabilita' separate.

- `weds_gateway_config`: configurazione centralizzata del gateway.
- `weds_protocol`: protocollo binario comune, CRC16, encode/decode pacchetti.
- `weds_gateway_comm`: comunicazione LoRa lato gateway.
- `weds_gateway_registry`: stato centrale dei dispositivi remoti, eventi,
  trend, posizioni e comandi pendenti.
- `weds_gateway_api`: WiFi, WebServer, API JSON, NTP e route HTTP.
- `weds_gateway_web`: HTML/CSS/JS statici per dashboard e pagina admin.

Regola importante: `heltec_unofficial.h` deve restare solo nei `.cpp` che usano
direttamente radio, `heltec_setup()` o `heltec_loop()`. Gli header pubblici non
devono includerlo.

## Configurazione centralizzata

La maggior parte dei valori operativi del gateway reale e' in:

```text
code/lib/weds_gateway_config/WedsGatewayConfig.h
```

Categorie principali:

- profilo LoRa: frequenza, bandwidth, spreading factor, coding rate, sync word,
  potenza TX e preambolo;
- affidabilita' radio: timeout ACK, numero retry e backoff;
- dimensionamento registry: massimo dispositivi remoti, massimo eventi per
  dispositivo, punti trend;
- logica alert mode: durata comando e frequenza di campionamento richiesta ai
  dispositivi vicini;
- WiFi/API/NTP: porta HTTP, timeout connessione, modem sleep, server NTP;
- FreeRTOS: delay task, stack, priorita' e core;
- self-test: dispositivi demo, coordinate demo e periodo di aggiornamento.

Valori da trattare come protocollo stabile, non come tuning normale:

- `WEDS_MAGIC`;
- `WEDS_GATEWAY_ID`;
- tipi messaggio;
- dimensioni header/payload pacchetto;
- layout delle struct payload.

Questi stanno in `weds_protocol` e cambiarli richiede aggiornare tutti i
firmware che parlano con il gateway.

## Firmware `gateway`

File principale:

```text
code/src/gateway/main.cpp
```

Oggetti globali:

```cpp
WedsGatewayRegistry registry;
WedsGatewayComm gatewayComm;
WedsGatewayApi gatewayApi;
```

Il registry e' l'unica sorgente di verita' dello stato gateway. La radio e le
API non tengono copie parallele dei dispositivi remoti.

### Setup

Il setup segue questo ordine:

1. avvia `Serial`;
2. crea il mutex FreeRTOS del registry;
3. inizializza `WedsGatewayRegistry` e monta LittleFS;
4. carica `/weds_config.json`, se presente;
5. avvia WiFi, NTP, WebServer e route HTTP;
6. inizializza Heltec e radio LoRa;
7. crea i task FreeRTOS.

Se un'inizializzazione critica fallisce, il firmware entra in `fatalError()`:
stampa l'errore e resta fermo con delay periodico. Questo evita di avere un
gateway parzialmente avviato.

### Task FreeRTOS

Il gateway reale usa due task principali.

`GatewayRadioTask`:

- chiama `gatewayComm.loop()`;
- prende il mutex del registry;
- chiama `gatewayComm.poll()`;
- rilascia il mutex;
- dorme per `WEDS_GATEWAY_RADIO_TASK_DELAY_MS`.

`GatewayApiTask`:

- prende il mutex del registry;
- chiama `gatewayApi.handleClient()`;
- rilascia il mutex;
- dorme per `WEDS_GATEWAY_API_TASK_DELAY_MS`.

La radio ha priorita' maggiore dell'API. L'API e la radio sono pinnate su core
diversi. Il mutex protegge il registry condiviso da accessi concorrenti.

Nota: attualmente il lock copre l'intera gestione di una richiesta API o di un
poll radio. Se in futuro la dashboard diventasse pesante, si potra' ottimizzare
copiando lo stato sotto lock e serializzando JSON fuori dal lock.

## Comunicazione LoRa gateway

Libreria:

```text
code/lib/weds_gateway_comm
```

Responsabilita':

- inizializzare Heltec e radio LoRa;
- ricevere pacchetti binari WEDS;
- deserializzare e validare CRC/header;
- ignorare pacchetti non destinati al gateway;
- aggiornare il registry su `NODE_STATUS` e `NODE_ALERT`;
- inviare ACK quando richiesto;
- creare e consegnare `ALERT_MODE_ENABLE` ai dispositivi vicini quando
  opportuno.

Parametri radio e affidabilita' sono in `WedsGatewayConfig.h`.

Flusso RX principale:

1. `poll()` riceve bytes dalla radio;
2. `weds_deserialize_packet()` valida pacchetto e CRC;
3. se `dst_node_id != WEDS_GATEWAY_ID`, il pacchetto viene ignorato;
4. per `NODE_STATUS` / `NODE_ALERT`, il payload viene decodificato;
5. i duplicati affidabili non vengono riprocessati, ma ricevono di nuovo ACK;
6. i pacchetti nuovi aggiornano il registry;
7. gli alert generano comandi pendenti per i dispositivi vicini localizzati.

## Registry gateway

Libreria:

```text
code/lib/weds_gateway_registry
```

Il registry mantiene in RAM:

- ultimo stato noto di ogni dispositivo remoto;
- timestamp ultimo contatto;
- posizione del dispositivo, se nota;
- ultimo pacchetto affidabile visto, per rilevare duplicati;
- comando `ALERT_MODE_ENABLE` pendente;
- eventi alert recenti;
- trend downsampled.

Il numero massimo di dispositivi remoti, eventi e punti trend e' fisso e
configurato in `WedsGatewayConfig.h`. Non vengono usati `std::vector` o
`std::map`, per tenere il comportamento prevedibile su embedded.

### Persistenza

Il file persistente e':

```text
/weds_config.json
```

Vengono salvati solo dati stabili:

- `node_id`;
- `location_known`;
- `latitude`;
- `longitude`.

Non vengono salvati:

- letture sensori;
- eventi;
- trend;
- comandi pendenti;
- timestamp ultimo contatto;
- sequenze pacchetti.

`setNodeLocation()` aggiorna la RAM. Il salvataggio persistente avviene quando
l'API admin chiama anche `savePersistentConfig()`.

### Neighbor detection

Quando un dispositivo remoto entra in alert, il registry cerca dispositivi
vicini entro:

```cpp
WEDS_NEIGHBOR_RADIUS_M
```

La distanza e' calcolata con Haversine. Se un vicino viene trovato, il gateway
salva per quel dispositivo un comando `ALERT_MODE_ENABLE` pendente.

## API HTTP e dashboard

Libreria:

```text
code/lib/weds_gateway_api
```

Responsabilita':

- connettere il gateway alla WiFi;
- abilitare modem sleep WiFi se configurato;
- sincronizzare l'orologio via NTP;
- avviare `WebServer`;
- servire dashboard, pagina admin e API JSON.

Le credenziali WiFi reali stanno in:

```text
code/include/secrets.h
```

Questo file e' locale e ignorato da `.gitignore`. Il template versionabile e':

```text
code/include/secrets.example.h
```

### Route disponibili

- `GET /`: dashboard locale.
- `GET /admin`: pagina admin per posizioni e config.
- `GET /api`: info base gateway.
- `GET /api/state/all`: tutti i dispositivi conosciuti.
- `GET /api/state?node_id=...`: stato singolo dispositivo.
- `GET /api/nodes/unlocated`: dispositivi senza posizione.
- `GET /api/node/events?node_id=...`: eventi recenti del dispositivo.
- `GET /api/node/trend?node_id=...`: trend del dispositivo.
- `POST /api/admin/setlocation`: imposta e salva posizione dispositivo.
- `POST /api/admin/clearconfig`: cancella config persistente.

## Firmware `gateway_self_test`

File principale:

```text
code/src/gateway_self_test/main.cpp
```

Questo firmware non inizializza `WedsGatewayComm` e non usa LoRa reale.

Serve per verificare:

- WiFi/API;
- dashboard;
- pagina admin;
- dispositivi localizzati e non localizzati;
- eventi alert;
- trend;
- pending command per vicini.

I dispositivi demo e le coordinate stanno in `WedsGatewayConfig.h` dentro
`WEDS_SELF_TEST_NODES`.

Il self-test crea due task:

- task API, uguale al gateway reale;
- task dati finti, che aggiorna periodicamente il registry.

Il primo dispositivo demo entra in alert in modo ciclico. Gli altri dispositivi
servono per testare dashboard, vicini e pagina admin.

## PlatformIO

Environment gateway principali:

```text
gateway
gateway_self_test
```

Build tipiche:

```powershell
platformio run -e gateway
platformio run -e gateway_self_test
```

Flag ancora usati per il gateway:

- `ARDUINO_LOOP_STACK_SIZE=24576`: aumenta lo stack del task Arduino principale
  per gateway e self-test.
- `WEDS_TREND_SAMPLE_INTERVAL_SEC_OVERRIDE=10`: solo self-test, accelera il
  trend per vedere grafici senza aspettare un minuto per punto.

## Note operative

Il gateway reale usa modem sleep WiFi configurabile:

```cpp
WEDS_WIFI_MODEM_SLEEP_ENABLED
```

Se la dashboard risultasse lenta o instabile su una rete specifica, disabilitare
questa costante e ricompilare.
