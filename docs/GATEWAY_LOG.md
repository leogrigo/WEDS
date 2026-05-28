# WEDS Gateway Log

Questo documento descrive lo stato aggiornato del gateway WEDS nel progetto:
firmware embedded, bus MQTT e dashboard FastAPI.

Ultimo aggiornamento: 2026-05-27

## Architettura generale

Il gateway WEDS e' composto da tre parti:

- firmware ESP32 `gateway`: riceve pacchetti LoRa dai nodi, mantiene il
  `WedsGatewayRegistry`, pubblica stato su MQTT e gestisce i comandi interni
  verso i nodi;
- firmware ESP32 `gateway_self_test`: variante senza LoRa reale che genera dati
  demo per verificare registry, MQTT, eventi, trend e pagina admin;
- app Python `dashboard`: servizio FastAPI esterno che usa MQTT come bus,
  persiste lo stato in SQLite e offre dashboard/admin web.

Il firmware embedded resta il punto di raccolta LoRa. La dashboard FastAPI non
parla direttamente con i nodi: riceve stato e pubblica comandi tramite MQTT.

La separazione tra firmware PlatformIO e' fatta con:

```ini
build_src_filter = +<${PIOENV}/>
```

Quindi ogni environment compila solo la cartella `src/<nome_env>/`.

## Struttura dashboard

La dashboard esterna e' in:

```text
dashboard/
```

File principali:

- `dashboard/app/main.py`: app FastAPI, lifecycle, route HTTP e mount statici.
- `dashboard/app/mqtt_bus.py`: client MQTT e dispatch dei messaggi.
- `dashboard/app/db.py`: store SQLite thread-safe e aggregazione eventi/trend.
- `dashboard/app/config.py`: configurazione da variabili ambiente.
- `dashboard/templates/`: pagine `index.html` e `admin.html`.
- `dashboard/static/`: JavaScript e CSS della dashboard.
- `dashboard/requirements.txt`: dipendenze Python.
- `dashboard/.gitignore`: esclude virtualenv, cache e database runtime.

Dipendenze dichiarate:

- `fastapi==0.115.6`;
- `uvicorn[standard]==0.34.0`;
- `paho-mqtt==2.1.0`;
- `jinja2==3.1.5`.

## Librerie gateway embedded

Le librerie gateway stanno sotto `code/lib/` e hanno responsabilita' separate.

- `weds_gateway_config`: configurazione centralizzata del gateway embedded.
- `weds_protocol`: protocollo binario comune, CRC16, encode/decode pacchetti.
- `weds_gateway_comm`: comunicazione LoRa lato gateway.
- `weds_gateway_registry`: stato runtime dei dispositivi remoti, posizioni e
  comandi LoRa pendenti.
- `weds_gateway_mqtt`: bridge tra registry embedded e broker MQTT locale.

Regola importante: `heltec_unofficial.h` deve restare solo nei `.cpp` che usano
direttamente radio, `heltec_setup()` o `heltec_loop()`. Gli header pubblici non
devono includerlo.

## Configurazione embedded

La maggior parte dei valori operativi del gateway reale e' in:

```text
code/lib/weds_gateway_config/WedsGatewayConfig.h
```

Categorie principali:

- profilo LoRa: frequenza, bandwidth, spreading factor, coding rate, sync word,
  potenza TX e preambolo;
- affidabilita' radio: timeout ACK, numero retry e backoff;
- dimensionamento registry: massimo dispositivi remoti;
- logica alert mode: durata comando e frequenza di campionamento richiesta ai
  dispositivi vicini;
- WiFi/MQTT/NTP: broker MQTT, timeout connessione, modem sleep, server NTP;
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
WedsGatewayMqtt gatewayMqtt;
```

Il registry e' l'unica sorgente di verita' dello stato gateway embedded. La
radio e il bridge MQTT non tengono copie parallele dei dispositivi remoti.

### Setup

Il setup segue questo ordine:

1. avvia `Serial`;
2. crea il mutex FreeRTOS del registry;
3. inizializza `WedsGatewayRegistry` e monta LittleFS;
4. carica `/weds_config.json`, se presente;
5. avvia WiFi e sincronizza NTP;
6. inizializza Heltec, radio LoRa e bridge MQTT;
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

`GatewayMqttTask`:

- prende il mutex del registry;
- chiama `gatewayMqtt.loop()`;
- rilascia il mutex;
- dorme per `WEDS_GATEWAY_MQTT_TASK_DELAY_MS`.

La radio ha priorita' maggiore del bridge MQTT. Il mutex protegge il registry
condiviso da accessi concorrenti.

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

Flusso RX principale:

1. `poll()` riceve bytes dalla radio;
2. `weds_deserialize_packet()` valida pacchetto e CRC;
3. se `dst_node_id != WEDS_GATEWAY_ID`, il pacchetto viene ignorato;
4. per `NODE_STATUS` / `NODE_ALERT`, il payload viene decodificato;
5. i duplicati affidabili non vengono riprocessati, ma ricevono di nuovo ACK;
6. i pacchetti nuovi aggiornano il registry;
7. gli alert di anomaly generano comandi pendenti per i dispositivi vicini
   localizzati; gli alert solo risk non propagano `ALERT_MODE_ENABLE`.

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
- comando LoRa `ALERT_MODE_ENABLE` pendente.

Lo storico eventi e trend non vive piu' nel firmware embedded: viene ricostruito
e persistito dalla dashboard FastAPI in SQLite.

### Persistenza embedded

Il file persistente sul gateway e':

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
il comando MQTT `setLocation` viene gestito dal gateway e chiama anche
`savePersistentConfig()`.

### Neighbor detection

Quando un dispositivo remoto entra in anomaly alert, il registry cerca
dispositivi vicini entro:

```cpp
WEDS_NEIGHBOR_RADIUS_M
```

La distanza e' calcolata con Haversine. Se un vicino viene trovato, il gateway
salva per quel dispositivo un comando `ALERT_MODE_ENABLE` pendente. Gli alert
solo risk non attivano i vicini.

## Credenziali embedded

Le credenziali WiFi e MQTT reali stanno in:

```text
code/include/secrets.h
```

Questo file e' locale e ignorato da `.gitignore`. Il template versionabile e':

```text
code/include/secrets.example.h
```

Il firmware gateway non espone piu' dashboard o API HTTP embedded: la UI e le
route HTTP sono servite solo dall'app FastAPI esterna.

## MQTT

Il bus MQTT collega gateway e dashboard FastAPI.

Implementazione dashboard:

```text
dashboard/app/mqtt_bus.py
```

Topic usati:

- sottoscrizione dashboard: `weds/#`;
- stato nodi: `weds/nodes/<node_id>/state`;
- comandi verso gateway: `weds/gateway/commands`;
- risposte ai comandi: `weds/gateway/command_responses`;
- stato gateway: `weds/gateway/status`.

La dashboard usa `paho-mqtt` con callback API v2. Il client id e' composto da
`WEDS_MQTT_CLIENT_ID` piu' un suffisso UUID di 8 caratteri. La connessione usa
`connect_async(MQTT_HOST, MQTT_PORT, keepalive=30)` e il loop gira con
`loop_start()`.

### Configurazione MQTT/dashboard

Valori in `dashboard/app/config.py`:

- `WEDS_MQTT_HOST`, default `localhost`;
- `WEDS_MQTT_PORT`, default `1883`;
- `WEDS_MQTT_CLIENT_ID`, default `weds-dashboard`;
- `WEDS_DB_PATH`, default `dashboard/weds_dashboard.sqlite3`;
- `ALERT_STATE = 1`;
- `TREND_DEFAULT_LIMIT = 60`.

### Flusso messaggi

Alla partenza della FastAPI app:

1. viene creato `DashboardStore(DB_PATH)`;
2. viene creato `MqttBus(store)`;
3. nel lifespan FastAPI viene chiamato `mqtt_bus.start()`;
4. il client MQTT si connette al broker e sottoscrive `weds/#`;
5. alla chiusura dell'app viene chiamato `mqtt_bus.stop()`.

Gestione messaggi ricevuti:

- `weds/nodes/<node_id>/state`: payload JSON passato a
  `store.upsert_node_state()`;
- `weds/gateway/command_responses`: payload JSON passato a
  `store.apply_command_response()`;
- `weds/gateway/status`: payload JSON passato a `store.apply_gateway_status()`;
- payload non UTF-8 o non JSON aggiornano `last_error` e vengono ignorati.

Formato base dei comandi pubblicati su `weds/gateway/commands`:

```json
{
  "id": "<uuid>",
  "method": "<method>",
  "params": {},
  "node_id": 1
}
```

`node_id` viene incluso solo per comandi destinati a un nodo specifico. Ogni
comando viene prima salvato in SQLite con `store.log_command()` e poi pubblicato
con QoS 0 e `retain=false`.

Comandi esposti dalla dashboard:

- `setLocation`, con `node_id`, `latitude`, `longitude`;
- `clearConfig`, senza nodo specifico.

`ALERT_MODE_ENABLE` non e' un comando MQTT. E' un comando interno del protocollo
LoRa gateway->nodo, generato dal gateway quando riceve un `NODE_ALERT` e trova
nodi vicini localizzati.

## App FastAPI

File principale:

```text
dashboard/app/main.py
```

La app:

- crea `DashboardStore` e `MqttBus`;
- monta `/static` da `dashboard/static`;
- serve template da `dashboard/templates`;
- avvia e ferma MQTT tramite `lifespan`;
- espone API JSON per stato nodi, MQTT, gateway, eventi, trend, comandi ed
  export snapshot.

### Route FastAPI disponibili

- `GET /`: dashboard esterna, template `index.html`.
- `GET /admin`: pagina admin esterna, template `admin.html`.
- `GET /api/state/all`: lista nodi persistiti in SQLite.
- `GET /api/mqtt/status`: stato connessione MQTT, ultimo topic e ultimo errore.
- `GET /api/gateway/status`: stato MQTT piu' ultimo stato gateway persistito.
- `GET /api/state?node_id=...`: stato singolo nodo, 404 se assente.
- `GET /api/nodes/unlocated`: nodi senza posizione.
- `GET /api/commands?limit=...`: ultimi comandi, limite 1..100, default 20.
- `GET /api/node/events?node_id=...`: eventi aggregati del nodo.
- `GET /api/node/trend?node_id=...&limit=...`: trend del nodo.
- `POST /api/admin/setlocation`: valida coordinate e pubblica `setLocation`.
- `POST /api/admin/clearconfig`: pubblica `clearConfig`.
- `POST /api/admin/cleardb`: svuota il database locale dashboard.
- `GET /api/export`: esporta snapshot JSON come `weds_snapshot.json`.

### Validazioni FastAPI

`POST /api/admin/setlocation`:

- `node_id` deve essere maggiore di zero;
- `latitude` e `longitude` sono obbligatorie;
- `latitude` deve essere tra -90 e 90;
- `longitude` deve essere tra -180 e 180;
- se valido, accoda un comando MQTT `setLocation`.

`GET /api/state`:

- richiede `node_id > 0`;
- restituisce 404 con `node_id was not found` se il nodo non esiste.

## Persistenza SQLite dashboard

Database locale:

```text
dashboard/weds_dashboard.sqlite3
```

La connessione usa:

- `sqlite3.Row`;
- `check_same_thread=False`;
- `PRAGMA journal_mode=WAL`;
- `PRAGMA foreign_keys=ON`;
- `threading.RLock` nello store applicativo.

Tabelle create da `DashboardStore.init_schema()`:

- `nodes`: ultimo stato noto per nodo, letture sensori, coordinate,
  `pending_alert_mode`, contatori trend/eventi e timestamp di aggiornamento;
- `telemetry`: campioni temporali dei nodi, usati per il trend;
- `events`: eventi aggregati con apertura/chiusura, picchi e conteggio campioni;
- `command_log`: comandi MQTT pubblicati e relative risposte;
- `gateway_status`: ultimo stato gateway pubblicato via MQTT.

Indici:

- `idx_telemetry_node_time` su `telemetry(node_id, timestamp_s, id)`;
- `idx_events_node_open` su `events(node_id, still_open, event_id)`.

### Logica eventi dashboard

Ogni stato nodo ricevuto via MQTT viene confrontato con la soglia applicativa:

```text
ALERT_STATE = 1
```

Se `anomaly_state` o `risk_state` sono in alert, lo store apre o aggiorna un
evento. Il tipo evento puo' essere:

- `ANOMALY_ALERT`;
- `RISK_ALERT`;
- `BOTH_ALERT`;
- `NONE` solo come fallback interno.

Quando lo stato torna normale, l'evento aperto viene chiuso impostando
`still_open = 0`.

### Logica trend dashboard

Ogni stato nodo non duplicato viene inserito in `telemetry`. Un campione e'
considerato duplicato se ha lo stesso `last_sequence_id` e `last_msg_type`
dell'ultimo stato noto del nodo.

`GET /api/node/trend` legge da `telemetry`, ordina i campioni per arrivo e
restituisce gli ultimi punti richiesti. Il limite default e' configurato da
`TREND_DEFAULT_LIMIT`.

## Firmware `gateway_self_test`

File principale:

```text
code/src/gateway_self_test/main.cpp
```

Questo firmware non inizializza `WedsGatewayComm` e non usa LoRa reale.

Serve per verificare:

- WiFi/MQTT;
- dashboard FastAPI;
- pagina admin FastAPI;
- dispositivi localizzati e non localizzati;
- eventi alert;
- trend;
- pending command per vicini.

I dispositivi demo e le coordinate stanno in `WedsGatewayConfig.h` dentro
`WEDS_SELF_TEST_NODES`.

Il self-test crea due task:

- task MQTT, uguale al gateway reale;
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

## Avvio dashboard FastAPI

Dalla cartella `dashboard`:

```powershell
.\.venv\Scripts\uvicorn.exe app.main:app --reload
```

Oppure con broker MQTT esplicito:

```powershell
$env:WEDS_MQTT_HOST="localhost"
$env:WEDS_MQTT_PORT="1883"
.\.venv\Scripts\uvicorn.exe app.main:app --reload
```

Se si ricrea l'ambiente:

```powershell
python -m venv .venv
.\.venv\Scripts\pip.exe install -r requirements.txt
```

## Note operative

Il gateway embedded usa modem sleep WiFi configurabile:

```cpp
WEDS_WIFI_MODEM_SLEEP_ENABLED
```

Se la connessione WiFi/MQTT risultasse lenta o instabile su una rete specifica,
disabilitare questa costante e ricompilare.

Il database `dashboard/weds_dashboard.sqlite3`, la virtualenv `.venv/` e le
cache Python sono runtime locali e sono esclusi da `dashboard/.gitignore`.
