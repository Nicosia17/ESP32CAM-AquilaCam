# 🐾 AquilaCam – ESP32-CAM per il Controllo Remoto del Gatto

**AquilaCam** è un progetto IoT sviluppato con PlatformIO e una scheda **ESP32-CAM**, progettato per monitorare e interagire a distanza con il mio gatto, **Aquila**, tramite comandi Telegram e streaming video accessibile anche da fuori casa.

---

## 🎯 Obiettivi del progetto

- Osservare il gatto in tempo reale tramite **streaming video live**
- Ricevere **foto istantanee via Telegram**
- Accendere/spegnere il **flash LED** per migliorare la visibilità
- Accedere alla telecamera da qualunque luogo tramite **Dynamic DNS**

---

## 🌍 Accesso remoto

Lo stream è visibile da browser, anche da fuori casa, all’indirizzo:

http://NOMESERVIZIO.zapto.org:8080/stream

(L’indirizzo è configurato con DDNS e port forwarding nel router domestico.)

---

## 🤖 Comandi Telegram disponibili

- `/start` – Mostra i comandi disponibili con tastiera interattiva
- `/photo` – Scatta e invia una foto dal vivo
- `/stream` – Fornisce il link per visualizzare lo streaming
- `🔦 Toggle Flash` – Accende o spegne il flash LED della ESP32-CAM

Solo gli ID Telegram autorizzati nel codice possono accedere.

---

## 🧠 Funzionalità implementate

- Streaming MJPEG live con pagina web HTML minimal
- Bot Telegram per interazione testuale e con pulsanti
- Invio foto su richiesta con accensione automatica del flash
- Spegnimento automatico del flash dopo 5 minuti per sicurezza
- Log su serial monitor per debugging

---

## ⚙️ Requisiti

- Scheda ESP32-CAM AI Thinker
- PlatformIO installato
- Bot Telegram con token attivo
- Wi-Fi con IP pubblico o DDNS configurato
- Porte del router aperte (es. 8080)

---

## 📁 Struttura del progetto

ESP32CAM-AquilaCam/
├── src/
│ └── main.cpp ← codice principale
├── platformio.ini ← configurazione PlatformIO
├── .gitignore ← file per escludere file temporanei
└── README.md ← questo file

---

## 🚀 Come caricare il progetto

1. Clona il repository o scaricalo
2. Aprilo con VSCode + PlatformIO
3. Configura `main.cpp` con:
   - Nome Wi-Fi e password
   - Token Telegram
   - ID utente autorizzati
4. Collega la scheda e carica il firmware:
   ```bash
   pio run --target upload

🐱 Perché questo progetto?

Volevo un sistema semplice e smart per tenere d’occhio il mio gatto Aquila quando non sono in casa. 
Ora posso vedere cosa fa, ricevere aggiornamenti e controllare la stanza in qualsiasi momento, direttamente dallo smartphone.

Made with ❤️ and 🐾 by Nicosia17
