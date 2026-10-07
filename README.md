# Spectral Strip — JUCE / Xcode

Versione 0.7.4 di un multieffetto spettrale per macOS. Strip riordinabile con copie di
**Freeze/Smear, Gate, Spectral Delay, Blur, Tremolo e Shimmer Reverb**, applicate alla banda LOW–HIGH.

## Aprire e compilare

Il progetto già generato è `Builds/SpectralStrip.xcodeproj`.
Aprilo in Xcode e scegli lo schema `SpectralStrip_Standalone`, quindi Run (⌘R).
Per i plugin scegli `SpectralStrip_AU` o `SpectralStrip_VST3`, quindi Build (⌘B).
`SpectralStrip_All` compila tutti i formati. Non è necessario un account Apple per l'uso locale.

Lo Standalone parte con ingresso audio silenziato per evitare feedback: configura
interfaccia e ingressi dal pulsante **Settings…**, usando cuffie quando attivi il monitoring.

Per rigenerare il progetto fai doppio clic su `Generate Xcode.command`.
Richiede CMake e JUCE, rilevati rispettivamente in `/opt/homebrew/bin/cmake` e `/Applications/JUCE`.
Il progetto usa questa installazione locale: non scarica né include una copia di JUCE.
Su un altro computer imposta `JUCE_PATH` al percorso corretto.
Modifica `CMakeLists.txt` per aggiungere sorgenti e rigenera; i file Xcode sono generati.

I prodotti si trovano in `Builds/SpectralStrip_artefacts/Debug/` (oppure `Release/`):
- `Standalone/Spectral Strip.app`
- `AU/Spectral Strip.component`
- `VST3/Spectral Strip.vst3`

Per usarli in una DAW copia manualmente il `.component` in `~/Library/Audio/Plug-Ins/Components/`
e/o il `.vst3` in `~/Library/Audio/Plug-Ins/VST3/`, quindi esegui una nuova scansione nella DAW.
Il progetto non installa automaticamente plugin nel sistema.

## Controlli

- **FFT SIZE**: 512, 1024, 2048 (default), 4096 o 8192 campioni. Valori grandi migliorano la risoluzione in frequenza e aumentano la latenza. La scelta è salvata nella sessione. Il cambio introduce una breve dissolvenza e una pausa per riempire la nuova finestra; azzera le memorie Freeze/Delay. Se FREEZE resta attivo, cattura una nuova finestra completa.
- **Freeze / Enabled**: abilita il modulo. **FREEZE** cattura lo spettro corrente; premilo mentre passa audio. La fase continua a evolvere per sostenere il suono.
- **SMEAR**: media temporale delle ampiezze spettrali, con costante di tempo da 0 a 1,5 secondi (non una durata massima della coda). Ammorbidisce gli attacchi e prolunga il decadimento sul segnale in ingresso. Agisce prima della cattura: dopo aver attivato FREEZE, muoverlo non modifica la fotografia già congelata. Non è riverbero.
- **Gate / Enabled**: abilita il gate per bin, con ginocchio morbido di 12 dB. **THRESHOLD** è una soglia spettrale indicativa, non un misuratore dBFS calibrato.
- **TILT**: inclina la soglia in dB per ottava attorno a 1 kHz: più bassa sugli acuti, più alta sui bassi. 3 dB/oct (default) compensa l'andamento naturale di musica e rumore rosa; 0 applica la stessa soglia a tutte le frequenze.
- **ATTACK / RELEASE**: tempi di apertura e chiusura del gate per bin (default 10 ms e 100 ms). Valori sotto la durata di un hop (circa 11 ms con FFT 2048 a 48 kHz) agiscono al frame successivo.
- **Invert gate**: conserva maggiormente le componenti sotto soglia.
- **Delay / Enabled**: abilita ritardi spettrali. **TIME** imposta il tempo centrale; **DISPERSION** distribuisce i ritardi tra bassi e alti in modo uniforme per ottave all'interno della banda LOW–HIGH. **FEEDBACK** controlla la rigenerazione; **DELAY MIX** miscela il modulo.
- **DAMPING**: a ogni ripetizione le frequenze sopra 200 Hz perdono progressivamente energia, fino al 90% a 20 kHz con DAMPING 1. Rende le code più scure e naturali; 0 lascia le ripetizioni invariate.
- **LOW / HIGH**: banda comune agli effetti, con bordi sfumati. Se i valori si incrociano, viene usato l'intervallo tra i due.
- **FILTER**: con FILTER attivo (default) LOW e HIGH filtrano anche il segnale processato fuori banda, pure con tutti gli effetti spenti. LOW a 20 Hz e HIGH a 20 kHz lasciano la banda completamente aperta. Il filtro agisce sul wet: con DRY / WET sotto il 100% il dry resta a banda piena.
- **DRY / WET**: miscela globale con dry compensato per la latenza.
- **OUTPUT**: guadagno finale. Il feedback è limitato internamente, ma l'uscita non ha un brickwall limiter.

Lo spettro mostra il canale sinistro dopo gli effetti spettrali e prima di mix/output.
Tutti i controlli sono automatizzabili; i parametri si salvano nella sessione della DAW.
Il contenuto audio catturato da Freeze e la memoria dei delay non vengono serializzati.

## Tre punti di partenza

1. **Drone vocale**: attiva FREEZE durante una vocale, SMEAR 0,3; Gate e Delay disattivati.
2. **Pioggia spettrale**: Delay attivo, TIME 180 ms, DISPERSION 0,8, FEEDBACK 0,6, DELAY MIX 0,7.
3. **Dettagli nascosti**: Gate attivo e invertito, THRESHOLD -45 dB; regola la soglia in funzione del materiale.

## Motore e limiti della prima versione

FFT selezionabile da 512 a 8192, hop pari a FFT Size / 4, finestre sqrt-Hann, overlap-add normalizzato. Mono e stereo,
nessun ingresso MIDI o sidechain. Latenza dichiarata alla DAW: FFT Size campioni, aggiornata al cambio
(da 10,67 a 170,67 ms a 48 kHz; 42,67 ms con il valore iniziale 2048). Non è una modalità a bassa latenza per monitoring dal vivo.
I ritardi operano su frame spettrali; i tempi intermedi interpolano ampiezza e fase separatamente, evitando le cancellazioni dell'interpolazione complessa.
Le sessioni salvate prima della 0.2.0 si riaprono con il suono originale: SHIMMER 0, TILT 0, ATTACK/RELEASE 35 ms, DAMPING 0. Cambiano invece, per tutte le sessioni, la distribuzione logaritmica di DISPERSION e la nuova interpolazione del delay.
La selezione di banda è globale. Ordine moduli e overlap sono fissi; la finestra segue FFT Size.
Non sono ancora implementati pad XY, morphing, preset browser, shift/stretch o spettrogramma storico.
Il freeze può sostenersi indefinitamente; il tail dichiarato all'host è 120 secondi.

Nessuna allocazione esplicita o lock nel percorso di elaborazione. Memorie delay allocate
in prepareToPlay; piani FFT e finestre precalcolati. Il cambio dimensione riutilizza
la memoria disponibile e invalida logicamente la storia dei delay. GUI a 30 Hz con trasferimento tramite atomiche. I due canali hanno
memoria spettrale indipendente. Il salvataggio usa AudioProcessorValueTreeState.

## Test

Lo schema `SpectralStripTests` verifica ricostruzione neutra stereo, latenza del dry,
stabilità numerica con gli effetti a più sample rate, sustain Freeze mono,
attenuazione Gate normale/invertito e temporizzazione del Delay. Verifica anche tutte
le dimensioni FFT, cambi ripetuti, compensazione dry, aggiornamento latenza host,
salvataggio/ripristino della scelta e compatibilità con gli stati precedenti (default 2048).

Da Terminale, nella cartella del progetto:

```sh
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
cmake --build Builds --config Debug --target SpectralStrip_All SpectralStripTests
ctest --test-dir Builds -C Debug --output-on-failure
```

Le verifiche eseguite sono riportate in `VALIDATION.md`. La prova musicale nella DAW
rimane importante per valutare artefatti, preset e comportamento sotto automazione.
Prima di distribuire il prodotto verifica le condizioni della tua licenza JUCE e prepara
firma/notarizzazione appropriate. Nessun marchio o codice GRM è incluso nel progetto.

## Correzione avvio riproduzione (0.1.1)

Play, salti della testina e reset host avviano un nuovo segmento del motore FFT,
svuotando le memorie di Freeze/Delay. Un raccordo di 5 ms riduce le discontinuità;
la dissolvenza iniziale viene applicata prima della FFT e al dry compensato.
Questo attenua leggermente i primissimi 5 ms del nuovo segmento. Il normale playback
continuo non riattiva la dissolvenza. Anche il ritorno di un loop è un nuovo segmento.

Ridimensionamento: trascina la maniglia in basso a destra. Interfaccia scalata proporzionalmente da 780×600 a 1560×1200 (dimensione iniziale 1040×800).

## Strip modulare — 0.3.1

Il segnale attraversa gli effetti da sinistra a destra. Trascina la testata per cambiarne l’ordine; usa **+ ADD MODULE** per aggiungere Freeze/Smear, Gate, Spectral Delay, Blur o Tremolo e **x** per rimuovere un’istanza. Ogni effetto può comparire più volte, fino a otto moduli complessivi. Oltre quattro moduli usa la barra orizzontale sotto la strip; trascinando una testata oltre il bordo la strip scorre verso gli altri moduli.

Ogni copia ha parametri, cattura Freeze, envelope Gate e memoria Delay propri. Il numero **#** identifica l’istanza e resta invariato quando la sposti: anche automazioni e impostazioni seguono quell’istanza. La posizione nella catena è il numero davanti al nome. Rimuovere un’istanza libera lo slot; reinserire un effetto nello stesso slot riutilizza i relativi valori conservati.

LOW/HIGH e FILTER restano fissi nella fascia inferiore, insieme a DRY/WET e OUTPUT. Il filtro agisce sul wet dopo l’intera catena, anche quando la strip è vuota. LOW/HIGH definiscono anche la banda comune degli effetti e l’intervallo di DISPERSION. Analizzatore e FFT Size sono globali; tutti gli effetti condividono una sola analisi/sintesi FFT, perciò aggiungere copie non moltiplica la latenza.

La sessione salva istanze, ordine e parametri. Le sessioni precedenti alla 0.3.0 si aprono con Freeze → Gate → Delay e il filtro fisso; quelle della 0.3.0 mantengono l’ordine degli effetti e trasferiscono Filter nella fascia fissa. Gli ID di automazione dei tre effetti originali sono conservati.

I cambi di struttura usano una dissolvenza e azzerano le memorie Freeze/Delay prima di riempire nuovamente la FFT: può essere udibile una breve interruzione. La finestra resta ridimensionabile dall’angolo in basso a destra.

## Spectral Blur — 0.4.0

**Blur** diffonde l’energia tra frequenze vicine. **AMOUNT** regola l’intensità della trasformazione; **WIDTH** la larghezza della diffusione (20–4000 Hz); **MIX** miscela il risultato con l’ingresso del modulo. Default: AMOUNT 0,60, WIDTH 400 Hz, MIX 1,00. **Enabled** consente il bypass. AMOUNT o MIX a zero lasciano passare il segnale; controlli raccordati in circa 35 ms.

Il modulo lavora nella banda comune LOW–HIGH e non aggiunge latenza. Puoi inserirne più copie, ognuna con controlli indipendenti. Prima del Freeze catturi uno spettro già diffuso; dopo il Freeze trasformi il suono congelato; dopo il Delay diffondi anche le ripetizioni. La diffusione non è un riverbero e non genera una coda propria.

La potenza spettrale è normalizzata durante la diffusione; il livello percepito può comunque cambiare per il nuovo timbro e la ricostruzione FFT. Le sessioni precedenti mantengono la loro catena, senza aggiungere automaticamente Blur.

## Regolazioni — 0.4.2

I valori dei knob restano senza riquadri. I knob Delay hanno un margine superiore che lascia spazio all’indicatore anche quando punta a mezzogiorno.

Il Blur applica AMOUNT una sola volta alla trasformazione completa: i valori intermedi producono una diffusione più evidente rispetto alla 0.4.0/0.4.1. A zero rimane trasparente e a uno mantiene la diffusione massima precedente. I progetti esistenti conservano i valori, ma il timbro con AMOUNT intermedio cambia.

## Chiusura del filtro — 0.4.3

Con FILTER attivo, HIGH a 20 Hz, LOW a 20 kHz o LOW maggiore/uguale a HIGH chiudono completamente il segnale wet, dopo il raccordo del filtro e la latenza FFT. I limiti del filtro non vengono scambiati quando si incrociano. L’intervallo comune degli effetti continua a usare i due estremi ordinati.

Il filtro agisce sul wet: per verificarne la chiusura imposta DRY/WET a 1,00. Con una quota dry il segnale originale rimane udibile.

## Tremolo — 0.5.0

Da **+ ADD MODULE → Tremolo**. **RATE** regola la velocità da 0,10 a 20 Hz; **DEPTH** la profondità (zero è trasparente); **SHAPE** passa dalla sinusoide (0) a un’onda quadra arrotondata (0,5), poi a impulsi brevi (1: durata al 50% pari al 12,5% del ciclo). **Enabled** applica un bypass raccordato. Default: 4 Hz, Depth 0,75, Shape 0. La velocità è libera in Hz, senza sincronizzazione BPM.

Ogni istanza ha parametri e fase indipendenti; i canali sinistro/destro condividono la stessa pulsazione. Il Tremolo segue l’ordine della strip e la banda LOW–HIGH, con filtro fisso a valle. Le sessioni precedenti mantengono la catena senza aggiungerlo automaticamente. Il reset del trasporto o una modifica strutturale riavvia la fase, come le memorie degli altri moduli.

La modulazione è calcolata campione per campione dentro il frame corrente, con una trasformazione inversa e una diretta per Tremolo attivo e canale: nessuna latenza aggiuntiva, ma carico CPU maggiore con molte copie. La FFT e il raccordo dei controlli possono ammorbidire le variazioni rapide dei parametri.

## Verifica generale — 0.5.1

Il filtro parte subito dalla risposta impostata: con HIGH al minimo non lascia più passare un breve residuo all’avvio o al reset. Il richiamo di uno stato azzera ora le memorie audio anche quando ordine dei moduli e FFT restano uguali, con dissolvenza e riempimento della FFT; vecchie catture Freeze e code Delay non vengono ereditate dal nuovo stato. Le catture audio non sono salvate nei preset.

Per l’uso nella DAW viene installata la compilazione Release ottimizzata. Il ritardo minimo del Delay spettrale è un hop FFT: circa 2,67 ms con FFT 512 e 42,67 ms con FFT 8192 a 48 kHz, anche se TIME richiede un valore inferiore. Il tooltip TIME ricorda questo limite; FILTER specifica che agisce sulla quota wet.

## Shimmer con ottava acuta — 0.6.0 (storico, rimosso nella 0.7.0)

**SHIMMER** miscela una voce trasposta di **+12 semitoni** con il suono congelato. Richiede **Enabled** e **FREEZE** accesi: cattura mentre passa audio, poi alza Shimmer. A zero resta il Freeze originale; al massimo le due voci hanno uguale coefficiente nel mix a potenza costante. La sonorità e il livello effettivi dipendono dal contenuto catturato. Le componenti trasposte oltre Nyquist vengono escluse. Il filtro HIGH può attenuare l’ottava aggiunta.

Il knob sostituisce il precedente movimento delicato di fase/volume: le sessioni con Shimmer maggiore di zero suoneranno diversamente. ID del parametro, automazioni, copie e salvataggio restano gli stessi. Backup della precedente AU conservato.

## Shimmer Reverb — 0.7.1

**+ ADD MODULE → Shimmer Reverb** aggiunge un riverbero autonomo, duplicabile e riordinabile. Funziona sull’ingresso senza FREEZE. Nel modulo Freeze rimangono Enabled, FREEZE e SMEAR: il vecchio Shimmer è stato rimosso sia dall’interfaccia sia dal DSP. I vecchi ID di automazione sono conservati, ma non producono più alcun effetto e non viene aggiunto automaticamente un riverbero ai progetti esistenti.

- **Enabled:** bypass raccordato del modulo; la latenza rimane compensata.
- **Decay:** tempo nominale di decadimento, 0,5–20 secondi. Tone e Shimmer influenzano il decadimento effettivo.
- **Size:** lunghezza delle linee del riverbero, da ambiente compatto a coda più ampia; movimenti rapidi possono introdurre variazioni d’intonazione.
- **Shimmer:** quantità di voce +12 semitoni immessa nel riverbero e ricircolata nel feedback. A zero resta il riverbero; aumentando il valore le armoniche acute si accumulano nella coda.
- **Tone:** smorzamento nel feedback, da scuro a brillante.
- **Mix:** miscela dell’ingresso con la coda; a zero il modulo è trasparente ma conserva la propria latenza.

Default: Decay 6 s, Size 0,60, Shimmer 0,45, Tone 0,55, Mix 0,35. Primo ascolto: usa una nota o un accordo breve, poi lascia spazio alla coda. Per texture sostenute posiziona Shimmer Reverb dopo Freeze.

Il motore temporale usa otto linee di feedback con diffusione e modulazione lenta, più trasposizione granulare nel ricircolo. Le due reti dei canali usano tempi diversi. La banda LOW–HIGH delimita il risultato del modulo; FILTER rimane a valle della strip.

Ogni copia aggiunge **3/4 della dimensione FFT** di latenza, comunicata all’host e compensata anche sul dry. A 48 kHz e FFT 2048, una copia aggiunge 32 ms: latenza totale 74,7 ms. A FFT 8192 una copia aggiunge 128 ms. Il footer mostra la latenza totale. Le modifiche della catena e il richiamo dello stato azzerano le code con una dissolvenza.

Nella 0.7.1 il ritorno trasposto viene dosato in funzione di Decay: alzare Shimmer non tronca più la coda come nella 0.7.0. Due voci con grani di diversa durata alimentano il riverbero anche dall’ingresso, rendendo l’ottava più presente e meno dipendente dalla nota suonata. I parametri e la latenza restano invariati.

## Smear: continuità della coda — 0.7.2

La coda dello Smear conserva ora l’avanzamento delle fasi quando l’ingresso tace, evitando che le componenti residue si aggancino alla frequenza dei frame FFT. La stima di intonazione viene stabilizzata e mantenuta attraverso lo spegnimento della nota; Freeze può catturare anche questa coda. Smear a zero resta neutro. Con Freeze già attivo, Smear continua a essere un controllo precedente alla cattura e non modifica lo spettro congelato.

## Tremolo Shape — 0.7.3

Escursione più evidente: la prima metà del knob accorcia le transizioni fra volume alto e basso, la seconda restringe anche gli impulsi. Rate resta invariato; Depth regola sempre la profondità. I bordi sono raccordati, con transizioni di almeno 2 ms alle velocità alte. Gli impulsi stretti abbassano il livello medio perché il segnale resta attenuato più a lungo. I preset con Shape maggiore di zero cambiano carattere intenzionalmente; Shape zero, ID dei parametri e latenza restano invariati.

## Freeze: coerenza del timbro — 0.7.4

Le componenti FFT appartenenti allo stesso picco mantengono ora le relazioni di fase della cattura e condividono l’avanzamento della frequenza del picco. Questo riduce battimenti artificiali e oscillazioni di livello durante il sustain. Una stima della frequenza raccordata a 20 ms riduce le imprecisioni del singolo frame; ampiezze e fasi iniziali rimangono quelle del momento catturato. Compatibile con la cattura della coda Smear. Non vengono introdotti parametri, latenza o riverbero aggiuntivi. Il Freeze rimane una fotografia spettrale: il movimento temporale di una voce o di un attacco non può essere mantenuto integralmente in uno stato fermo.
