# Verifiche — 28 settembre 2026

Ambiente: macOS Apple Silicon, Xcode 26.3 (17C529), JUCE locale 9.0.2.

- Generazione del progetto Xcode completata con CMake.
- Compilazione Debug arm64 riuscita: AU, VST3, Standalone e SpectralStripTests.
- La compilazione finale non ha riportato warning o errori.
- CTest: 1 suite eseguita, 1 superata, 0 fallimenti.
- Ricostruzione neutra stereo e latenza dry verificate a 44,1 / 48 / 96 / 192 kHz.
  Errore massimo assoluto della ricostruzione: 1,19209e-7.
- Stress numerico degli effetti a tutti e quattro i sample rate: nessun campione non finito.
- Freeze mono: il tono catturato continua dopo la cessazione dell'ingresso.
- Gate normale: attenua un tono sotto soglia; Gate invertito: conserva il tono.
- Delay: un burst emerge con il ritardo atteso e conserva la forma d'onda nella regione stabile.
- Standalone avviato; layout controllato visivamente, comprese le etichette della scala logaritmica.

Log riproducibili: `Builds/build-final.log` e `Builds/Testing/Temporary/LastTest.log`.

Non eseguiti: ascolto critico con materiale musicale, validazione auval/pluginval,
caricamento in DAW, test Intel, build Release e firma/notarizzazione per distribuzione.
Il test di stabilità numerica non sostituisce queste verifiche.

## Aggiornamento FFT Size

- Selettore 512 / 1024 / 2048 / 4096 / 8192 aggiunto e interfaccia controllata visivamente.
- AU, VST3 e Standalone ricompilati: `Builds/build-fft-size.log`.
- Test ampliati ricompilati: `Builds/build-fft-tests.log`.
- CTest superato (3,86 s): ricostruzione, latenza dry, isolamento stereo e invalidazione
  della memoria Delay a tutte le dimensioni e a quattro sample rate.
- Test del processor superati: cambi FFT con blocchi da 257 campioni, recupero del segnale
  dopo la dissolvenza, aggiornamento della latenza dichiarata, richiamo dello stato,
  sessioni precedenti prive del parametro e cambi rapidi durante bypass host.
- Rimane da verificare il comportamento della compensazione dinamica nella DAW scelta.

## Correzione click all'avvio — 0.1.1

- Gestiti avvio trasporto, discontinuità della timeline e reset host.
- Raccordo smoothstep di 5 ms sull'ingresso e sul residuo d'uscita; nessuna nuova latenza.
- Compilati AU, VST3, Standalone e test: `Builds/build-click.log`.
- CTest superato in 4,68 s, inclusa regressione con ingresso costante non nullo,
  tutte le dimensioni FFT, wet/dry, riavvio Play, seek e reset host.
- Audio Unit installata aggiornata a 0.1.1; hash identico al prodotto compilato e firma valida.
- Backup precedente: `Backups/20260928-235235/Spectral Strip.component`.
- auval passa, ma il registro Apple restituisce ancora i metadati della versione 0.1.0:
  `Builds/auval-click.log`. La rilettura della nuova versione da parte di Logic
  richiede la chiusura e riapertura dell'host. Nessuna sessione Logic è stata chiusa.
- Il click segnalato nella sessione reale non è stato ascoltato direttamente:
  resta da confermare la correzione sul materiale dell'utente dopo il riavvio.

## Filtro LOW/HIGH — 0.1.2

- Causa del mancato funzionamento in Logic: era installata la Audio Unit 0.1.1, priva del filtro.
- AU, VST3, Standalone e test compilati senza warning: `Builds/build-filter.log`.
- Nuovo test sul processor: LOW e HIGH, tramite i parametri, attenuano oltre 30 dB
  fuori banda con effetti spenti; banda passante e FILTER disattivato intatti.
- CTest superato (5,56 s): `Builds/build-filter-tests.log`.
- Audio Unit 0.1.2 installata; auval riuscito con versione 0.1.2: `Builds/auval-filter.log`.
- Backup precedente: `Backups/20260929-000829/Spectral Strip.component`.
- Da confermare all'ascolto in Logic.

## Miglioramenti effetti — 0.2.0

- Nuovi controlli: SHIMMER (Freeze), ATTACK, RELEASE, TILT (Gate), DAMPING (Delay).
- DISPERSION ora logaritmica; interpolazione del delay su ampiezza e fase.
- Interfaccia portata a 1040×800, valori con decimali adeguati: `Builds/ui-0.2.0.png`.
- AU, VST3, Standalone e test compilati senza warning: `Builds/build-effects.log`.
- CTest superato (7,6 s). Nuovi test: spaziatura per ottave di DISPERSION; ritardo di mezzo frame
  su un tono a 1 kHz senza perdita di livello (con il vecchio metodo circa -6 dB);
  DAMPING che accorcia le ripetizioni a 8 kHz e lascia intatte quelle a 150 Hz;
  ATTACK e RELEASE indipendenti; TILT che conserva un tono debole a 8 kHz;
  SHIMMER che modifica il freeze mantenendone il livello (rapporto di energia 0,99);
  sessioni precedenti riaperte con i valori che ne preservano il suono. Stress numerico esteso ai nuovi controlli.
- Audio Unit 0.2.0 installata; auval riuscito con versione 0.2.0: `Builds/auval-effects.log`.
- Backup precedente (0.1.2): `Backups/20260929-002055/Spectral Strip.component`.
- Da valutare all'ascolto: carattere di SHIMMER, range di DAMPING e default del Gate su materiale reale.

## Freeze più fluido — 0.2.1 (30 settembre 2026)

- SHIMMER usa offset di fase limitati, con obiettivi casuali ogni 150 ms e raccordo di 80 ms, al posto della precedente passeggiata casuale di fase per frame.
- Rimossa la compensazione fissa di guadagno della vecchia modulazione; ampiezza modulata più delicatamente.
- Test Freeze esteso a tutte le cinque FFT: energia relativa sul tono a 440 Hz tra 0,812 e 1,219 (circa ±1 dB). La soglia minima di differenza dal Freeze statico passa da 0,1 a 0,02 per riflettere il movimento intenzionalmente più delicato.
- Suite CTest superata in 8,08 s; build AU/VST3/Standalone e test senza warning/errori nei log build-freeze-motion.log e build-freeze-tests.log.
- AU installata: versione nel plist 0.2.1 e hash identico al compilato; firma valida. auval passa, ma restituisce ancora versione 0.2.0 dalla cache: Builds/auval-freeze-motion.log.
- Backup AU precedente: Backups/20260930-124001/Spectral Strip.component.
- Rimane da confermare all’ascolto in Logic il miglioramento timbrico su materiale musicale. Le sessioni con SHIMMER maggiore di zero cambiano carattere.

## Ridimensionamento — 0.2.2

- Maniglia JUCE in basso a destra; proporzioni 1040:800, limiti 780×600–1560×1200.
- Grafica, controlli e regioni di repaint scalano insieme; maniglia indipendente dalla scala.
- Provati riduzione al minimo e successivo ingrandimento nello Standalone: controlli e testi visibili, nessun taglio del layout.
- Compilati AU, VST3, Standalone e test; CTest superato in 8,02 s.
- AU 0.2.2 installata con firma valida e binario identico alla build. auval passa con metadati 0.2.1 ancora in cache; verifica del trascinamento dentro Logic da effettuare dopo riapertura.
- Backup: Backups/20260930-124432/Spectral Strip.component. Log: Builds/build-resize.log, Builds/auval-resize.log.

## Strip modulare — 0.3.0 (30 settembre 2026)

- Freeze/Smear, Gate, Spectral Delay e Filter inseribili/rimovibili e riordinabili dalla testata; un’istanza per tipo.
- Routing spettrale in ordine di strip, usando una sola FFT: Freeze cattura l’ingresso della propria posizione e Gate rileva il segnale che riceve dal modulo precedente.
- La catena è pubblicata atomicamente al processore audio e applicata al confine di un blocco con fade-out, reset delle memorie e warmup; nessuna allocazione o lock aggiunti nella callback. Il riordino azzera le code.
- Ordine e presenza salvati nello stato; fallback per sessioni precedenti: Freeze → Gate → Delay → Filter. Parametri e ID automazione conservati. La catena originale mantiene la precedente miscela ai bordi della banda. Modifiche strutturali notificate alla DAW.
- Rimuovendo Filter la banda comune si riapre; valori LOW/HIGH conservati per il reinserimento.
- Test: ordine Gate/Delay cambia l’energia della coda (10,3449 contro 0,00000191738); strip vuota trasparente anche con effetti abilitati; richiamo della catena; sanitizzazione di ID/duplicati; migrazione legacy; cambi di topologia durante playback con recupero di livello. Tutte le regressioni precedenti passano. CTest: 8,04 s.
- AU/VST3/Standalone e test compilati senza warning/errori: Builds/build-modular.log. Test: Builds/test-modular.log.
- Interfaccia verificata nello Standalone: rimozione Filter, reinserimento dal menu e trascinamento Filter al primo posto. Ridimensionamento proporzionale mantenuto.
- AU installata 0.3.0: plist verificato, binario identico alla build, firma valida. auval passa ma riporta metadati 0.2.2 ancora in cache: Builds/auval-modular.log.
- Backup precedente: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-131333/Spectral Strip.component.
- Da confermare in Logic dopo riapertura: interazione della strip, salvataggio nel progetto e ascolto dei cambi di catena.

## Filtro fisso e istanze multiple — 0.3.1 (30 settembre 2026)

- LOW/HIGH/FILTER ripristinati nella fascia fissa inferiore; filtro wet a valle della catena e attivo anche con strip vuota. Nessuna voce Filter nel selettore.
- Fino a otto istanze totali di Freeze/Smear, Gate e Delay, liberamente ripetibili. Scorrimento orizzontale con quattro schede visibili e scorrimento ai bordi durante il trascinamento.
- Identità di slot stabile indipendente dalla posizione; parametri APVTS distinti, Freeze/phase/magnitude, Gate envelope e storia Delay separati per slot e canale. FFT e latenza restano condivise.
- Storie Delay preallocate per tutti gli slot in prepareToPlay; parametri delle istanze letti tramite puntatori atomici precalcolati, senza costruzione di ID/allocazioni nella callback.
- Salvataggio catena e valori indipendenti; migrazione stati precedenti e 0.3.0 rimuovendo Filter dalla catena e conservando l’ordine degli effetti. ID originali Freeze/Gate/Delay mantenuti sui tre slot iniziali.
- CTest superato in 10,35 s: duplicati Gate con soglie opposte indipendenti; due Delay in serie sommano i rispettivi tempi e mantengono la forma d’onda; filtro fisso con strip vuota; otto Delay inseribili; valori 120/430 ms conservati dopo inversione della catena e richiamo; migrazione 0.3.0; regressioni precedenti.
- Interfaccia verificata: aggiunte due copie di Delay (cinque moduli totali), nuova copia portata a 430 ms e trascinata davanti al Gate mantenendo il valore, altre copie a 250 ms; scorrimento della strip al primo modulo e filtro sempre visibile.
- AU/VST3/Standalone e test compilati senza warning/errori: Builds/build-instances.log. Test: Builds/test-instances.log.
- AU installata 0.3.1: plist verificato, binario identico alla build, firma valida. auval passa con metadati 0.3.0 ancora in cache: Builds/auval-instances.log.
- Backup precedente: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-133313/Spectral Strip.component.
- Restano da valutare in Logic ascolto e carico CPU con molte istanze attive; i cambi strutturali mantengono il reset delle code e la breve dissolvenza.

## Spectral Blur — 0.4.0 (30 settembre 2026)

- Nuovo modulo Blur ripetibile e riordinabile: AMOUNT 0–1, WIDTH 20–4000 Hz, MIX 0–1, Enabled. Default 0,60 / 400 Hz / 1,00. Parametri distinti per slot, salvati e richiamati; parametri nuovi aggiunti dopo quelli esistenti per conservarne l’ordine. Migrazione del vecchio Filter mantenuta.
- Elaborazione per moduli sulla FFT condivisa: tre medie mobili sulla potenza spettrale, normalizzazione della banda LOW–HIGH e banco di fasi continuo per i bin diffusi. Complessità lineare nel numero di bin; nessuna allocazione o lock aggiunti nel render. Controlli raccordati, nessuna latenza supplementare.
- Test su tono a 1500 Hz: deviazione spettrale 62,4699 Hz con WIDTH 100 Hz, 857,457 Hz con WIDTH 1800 Hz; due copie da 100 Hz producono 86,8637 Hz. Rapporto di potenza del Blur largo 0,806679 rispetto al neutro: la normalizzazione spettrale non garantisce identico livello dopo la ricostruzione.
- Verificati Amount/Mix zero e bypass trasparenti, copie cumulative, valori finiti alla larghezza massima su tutte le cinque FFT, routing audio del processore e richiamo di due copie indipendenti dopo riordino. Regressioni Freeze/Gate/Delay/filtro e stati precedenti superate. CTest: 10,88 s, nessun fallimento.
- AU/VST3/Standalone e test compilati; log Builds/build-blur.log e Builds/test-blur.log. Nessun warning/errore nei log di compilazione verificati.
- Anteprima dell’editor verificata: due Blur, modifica indipendente di Width a 1200 Hz e Amount a 0,35; prima copia trascinata dopo Freeze conservando Width 1200 Hz. Filtro fisso e barra della strip visibili. Target opzionale SpectralStripPreview, senza apertura di hardware audio.
- Lo Standalone ha atteso in CoreAudio durante l’apertura del dispositivo, prima della creazione dell’editor; stack salvato in Builds/standalone-blur-sample.txt. Il processo di verifica è stato chiuso; l’anteprima ha permesso il controllo UI. Avvio Standalone e ascolto musicale nella DAW restano da confermare.
- AU 0.4.0 installata: versione plist verificata, binario identico alla build e firma valida. auval passa, ma riporta ancora metadati 0.3.1 dalla cache: Builds/auval-blur.log; questo non conferma la ricarica della 0.4.0 nell’host.
- Backup precedente: Backups/20260930-142443/Spectral Strip.component.

## Valori senza box e knob Delay — 0.4.1

- Rimossi sfondo e bordo dai valori di tutti i knob, compreso l’editor del valore; digitazione conservata.
- Ingranditi i cinque knob Delay riducendo il margine grafico e aumentando l’altezza delle righe; layout verificato nell’anteprima con Delay/Blur/Freeze/Gate.
- AU, VST3, Standalone e anteprima compilati: Builds/build-knobs.log. Modifica esclusivamente grafica.
- AU 0.4.1 installata, versione e identità del binario verificate. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-151855/Spectral Strip.component.

## Spazio indicatori Delay e intensità Blur — 0.4.2

- Margine dei knob Delay aumentato per contenere il thumb a mezzogiorno; verificato visivamente nell’anteprima, incluso Delay Mix 0,50.
- Rimossa la doppia attenuazione di AMOUNT: interpolazione unica verso lo spettro completamente diffuso. Zero e massimo conservano il comportamento precedente; valori intermedi più incisivi, anche negli stati esistenti.
- Nuovo test su differenza audio a AMOUNT 0,60 rispetto alla trasformazione massima, con tolleranza 0,55–0,65. Tutte le regressioni passano, CTest 11,25 s: Builds/test-blur-strength.log. Build: Builds/build-blur-strength.log.
- AU 0.4.2 installata, binario identico alla build e firma verificata. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-152409/Spectral Strip.component. Valutazione musicale del nuovo Amount da effettuare nella DAW.

## Chiusura del filtro — 0.4.3

- HIGH al minimo, LOW al massimo e limiti LOW/HIGH incrociati chiudono il wet completamente dopo il raccordo. Corretti il residuo nel bin DC e il riordino dei limiti che riapriva una banda.
- Test con componente continua e toni a 40/440 Hz, tutte le cinque FFT: chiusura superiore a 100 dB di attenuazione e successiva riapertura. Tutte le regressioni superate: CTest 11,48 s, Builds/test-filter-endpoints.log.
- AU/VST3/Standalone compilati senza warning/errori: Builds/build-filter-endpoints.log. AU 0.4.3 installata, versione, identità del binario e firma verificate. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-180752/Spectral Strip.component.
- La quota dry rimane volutamente non filtrata; verifica della chiusura con FILTER attivo e DRY/WET 1,00.

## Tremolo — 0.5.0

- Modulo tipo 5: RATE 0,10–20 Hz, DEPTH 0–1, SHAPE 0–1 da seno a impulsi arrotondati; Enabled raccordato. Default 4 Hz / 0,75 / 0. Parametri nuovi in coda, vecchi ID e migrazione conservati. Istanze duplicabili e riordinabili, richiamo indipendente.
- Modulazione per campione nel frame alla posizione del modulo (IFFT, inviluppo, FFT), banda comune e filtro finale conservati. Stereo in fase, memoria preallocata, nessun lock o latenza aggiuntiva. Due trasformazioni aggiuntive per Tremolo attivo/canale: costo CPU da valutare nella DAW con molte copie. Rate libero, senza BPM sync.
- Test: sinusoide ricostruita contro formula analitica, Depth zero, bypass, due copie in serie, coerenza stereo e impulsi a 20 Hz su tutte le cinque FFT. Richiamo di due Rate indipendenti 2/7 Hz e Shape 0,8, parametri collegati all’audio del processore. Regressioni complete superate: CTest 13,17 s.
- AU/VST3/Standalone, test e anteprima compilati senza warning/errori: Builds/build-tremolo.log e Builds/test-tremolo.log. UI verificata nell’anteprima: Tremolo visibile, Rate 6,25 Hz e Shape 0,65 impostati correttamente, valori senza box.
- AU installata 0.5.0, plist e identità binario verificati, firma valida. Log validazione host: Builds/auval-tremolo.log. Backup precedente: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-181712/Spectral Strip.component.
- auval superato, con versione 0.4.3 ancora nei metadati in cache; la ricarica della 0.5.0 nell’host va confermata dopo riapertura. Ascolto musicale da effettuare nella DAW.

## Controllo generale — 0.5.1

- Ispezionati motore DSP, processore, gestione stato e controlli UI. Due regressioni riprodotte prima della correzione: Builds/test-audit-repro.log.
- Filtro chiuso all’avvio/reset: il gain iniziale a uno produceva un picco 0,257972 con ingresso 0,3. Il primo frame adotta ora subito la risposta richiesta; picco corretto zero su tutte le FFT. Le variazioni durante playback mantengono il raccordo.
- Richiamo stato con identica catena/FFT: Freeze continuava a riprodurre una vecchia cattura. Una revisione atomica richiede ora il reset sul thread audio dopo fade-out, anche senza variazioni topologiche; la misura di uscita successiva al richiamo passa da 8,14413 a zero nel test. Memorie Delay e fase Tremolo vengono anch’esse reimpostate, come negli altri reset.
- Nuova prova di otto moduli misti, automazioni alternate ogni 1024 campioni, feedback 0,92, estremi Blur/Tremolo e LOW/HIGH, 44,1/96 kHz e FFT 512/8192: tutti i campioni finiti, nessun segnale nel canale destro silenzioso, picchi 0,041–0,089 con sorgente rumore ±0,06. Questo test verifica stabilità e isolamento, non l’assenza percettiva di ogni artefatto sotto automazione.
- Tempi del renderer offline nei quattro casi: 24–49% della durata audio; comprendono generazione della sorgente e controlli del test. Non sono una misura del carico CPU nella DAW né una garanzia sulle scadenze di ogni buffer.
- Regressioni complete superate: 16,68 s, Builds/test-audit.log e Builds/Testing/Temporary/LastTest.log. Ricostruzione neutra, bypass, avvio/seek/reset, filtro, ritardi frazionari, Freeze, Gate, Blur, Tremolo, copie e migrazioni.
- Compilati AU/VST3/Standalone, test e anteprima in Release, senza warning/errori: Builds/build-audit.log. Aggiunti tooltip per il filtro sulla quota wet e il minimo Delay di un hop FFT.
- Installata AU Release 0.5.1: versione plist, firma e identità del binario verificate. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-184833/Spectral Strip.component. Log auval: Builds/auval-audit.log.
- Limiti residui: il timbro su materiale musicale e il carico con molte istanze in Logic richiedono ascolto/prova nella DAW; i cambi strutturali e il richiamo stato azzerano le code con una breve dissolvenza.
- auval superato; i metadati host riportano ancora 0.5.0 in cache. Questo esito non conferma la ricarica della 0.5.1 nella DAW, da riaprire.

## Ricontrollo funzionale — 1 ottobre 2026, versione 0.5.1

- Ricompilazione Release AU/VST3/Standalone e suite completa riuscita senza warning/errori (Builds/build-functional-check.log). Nessuna modifica al DSP o alla versione installata.
- Aggiunto test del parametro Shimmer nel processore completo, slot originale 1 e copia slot 4: con FREEZE spento differenza audio zero; con FREEZE acceso differenza relativa di energia 0,164058 e 0,0987597. Verifica della variazione durante playback, oltre ai precedenti test del solo motore.
- Suite ampliata superata in 17,54 s, zero fallimenti: Builds/test-functional-check.log. Include filtro, Freeze/Smear, Gate, Delay, Blur, Tremolo, copie, bypass, transizioni, richiamo stato e stress di otto moduli.
- AU installata identica alla build Release (SHA-256 76427434e1210a2769532585f5dceb2c689d61565a7542a80bc55fb1cfb6ddd7), firma valida. auval riconosce ora esplicitamente 0.5.1 e supera la validazione: Builds/auval-functional-check.log. Il primo tentativo nel sandbox non vedeva il registro AU; verifica riuscita con accesso al registro host.
- Nessun nuovo difetto riprodotto nei casi verificati. Questo controllo è numerico e tramite validatore AU: non sostituisce ascolto musicale e prova di un progetto reale nella DAW.

## Shimmer +12 semitoni — 0.6.0 (1 ottobre 2026)

- Sostituito il movimento casuale del Freeze con voce trasposta all’ottava superiore: inviluppo spettrale interpolato, fasi legate ai picchi locali e avanzamento raddoppiato. Componenti con frequenza trasposta oltre Nyquist escluse. Mix raccordato a 80 ms; al massimo coefficienti 0,707 per originale/ottava. Nessuna latenza aggiuntiva.
- Shimmer zero mantiene il Freeze originale; sul segnale non congelato non interviene. Parametro, copie e salvataggio conservati; gli stati con Shimmer non nullo cambiano timbro intenzionalmente. Tooltip e README aggiornati.
- Test diretto 440 → 880 Hz su tutte le FFT: ampiezza dell’ottava pari a 0,51–0,57 della fondamentale di riferimento; rapporto di energia totale 0,76–0,83 rispetto al Freeze senza Shimmer. Le differenze di ricostruzione impediscono identico livello effettivo delle due voci.
- Suite completa superata in 19,04 s, compresi Shimmer tramite processore e copia slot 4, bypass fuori Freeze, richiamo e stress: Builds/test-octave.log. Compilazione Release AU/VST3/Standalone e test senza warning/errori: Builds/build-octave.log.
- AU Release 0.6.0 installata: plist, identità binario e firma verificati. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20261001-140626/Spectral Strip.component. Da valutare il risultato musicale all’ascolto nella DAW.

## Shimmer Reverb autonomo — 0.7.0 (1 ottobre 2026)

- Nuovo modulo tipo 6, duplicabile e riordinabile: Enabled, Decay 0,5–20 s, Size, Shimmer, Tone e Mix. Default 6 / 0,60 / 0,45 / 0,55 / 0,35. Parametri aggiunti in coda, valori e ordine salvati per istanza.
- Rimosso lo Shimmer dal Freeze e dal suo DSP. I parametri storici restano registrati per non spostare le automazioni, ma sono inerti: verificato su tutte le FFT e sui parametri del processore originale/copia. Nessun inserimento automatico nei progetti vecchi.
- Motore temporale: otto linee FDN con matrice Hadamard normalizzata, quattro diffusori allpass, modulazione lenta, smorzamento, filtro passa-alto nel ritorno trasposto, pitch shifting granulare a due testine +12 semitoni nel feedback e contenimento morbido dei valori interni eccessivi. Due reti con tempi differenti per i canali. Storage preallocato, reset tramite invalidazione delle linee.
- Ponte FFT/tempo: overlap-add dei frame in ingresso, consumo dei soli hop definitivi, rianalisi dell’uscita. Ogni copia aggiunge N−hop campioni di latenza, comunicata all’host; dry compensato anche in bypass e con banda selezionata. Corretto il mix iniziale per evitare residui su Mix zero. Freeze dopo il riverbero attende il riempimento del ponte.
- Test: ricostruzione neutra Mix zero e bypass con una/due copie su tutte le FFT, errore massimo < 1,7e-7; latenza esatta dry/wet. Coda lunga/short con ingresso 440 Hz: energia tardiva 23,323 contro 9,93e-9. Misura componente 880 Hz nella coda: 32,38 con Shimmer contro 0,0696 senza. Valori finiti nel test, coda sostenuta dopo ingresso silenzioso.
- Verificati reset delle memorie, Freeze dopo il ponte, inserimento di copie, richiamo ordine/Decay indipendente, latenza host e recupero dopo rimozione. Suite completa superata in 23,05 s: Builds/test-shimmer-reverb.log.
- Compilazione Release AU/VST3/Standalone, test e anteprima riuscita senza warning/errori nella build finale: Builds/build-shimmer-reverb-final.log. UI verificata: Decay 8 s, Shimmer 0,70, Freeze con solo Smear; footer 74,7 ms a 48 kHz/FFT 2048 e una copia.
- AU 0.7.0 installata, plist e identità binario verificati, firma valida. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20261001-143333/Spectral Strip.component.
- Limiti: Decay nominale dipende anche da Tone/Shimmer; Size in movimento può produrre variazioni di intonazione. Latenza maggiore soprattutto a FFT grandi. La qualità musicale rispetto al riferimento Valhalla richiede ascolto, non è dimostrata dai test funzionali.
- auval superato: Builds/auval-shimmer-reverb.log. I metadati mostrano ancora 0.6.0 in cache; la versione 0.7.0 del bundle è verificata direttamente, la DAW va riaperta per ricaricarla.

## Correzione coda Shimmer — 0.7.1 (1 ottobre 2026)

- Riprodotto un difetto nella 0.7.0: Shimmer al massimo sottraeva il 45% del feedback a ogni giro e lo sostituiva con un ritorno a rango ridotto. Con burst 440 Hz, Decay 8 s e Tone 0,8, energia della coda fra 2 e 4 s: 5,913 senza Shimmer, 8,96e-10 con Shimmer massimo. Il precedente test dell’ottava non controllava questo collasso.
- Ritorno trasposto ora dosato rispetto alla perdita prevista da Decay (20% di tale perdita, massimo 0,12), con due direzioni ortogonali. Aggiunte due voci +12 semitoni nell’eccitazione della rete, con grani 43,7/61,3 ms per ridurre le cancellazioni comuni. Shimmer zero conserva il percorso del riverbero originale. Nessuna variazione di parametri, default o latenza.
- Stesso caso dopo la correzione: energia tardiva 3,205, pari al 54,2% del riverbero senza Shimmer; componente a 880 Hz nel test preesistente 690,929 contro 32,381 nella 0.7.0. Sono misure di energia, non dB o giudizi musicali.
- Aggiunte regressioni su 220/440/450 Hz a 44,1/48/96 kHz, entrambe le reti dei canali: energia tardiva con Shimmer massimo pari al 54–77% del riferimento. Verificati ottava e coda attraverso il ponte FFT, automazione Size/Decay, stabilità a 44,1/192 kHz e reset di tutte le memorie del pitch.
- Suite completa superata in 40,19 s, zero fallimenti: Builds/test-shimmer-fix.log, dettagli in Builds/Testing/Temporary/LastTest.log. Compilazione Release AU/VST3/Standalone, test e preview senza warning/errori: Builds/build-shimmer-fix.log.
- AU 0.7.1 installata: versione del bundle e identità binario verificate, firma valida, auval superato (Builds/auval-shimmer-fix.log). Backup AU: Backups/20261001-190836/Spectral Strip.component; backup sorgente e test: Backups/pre-shimmer-tail-fix.
- Questi controlli dimostrano la correzione della coda e la presenza dell’ottava, non equivalenza sonora a Valhalla. Rimane da valutare il risultato su materiale musicale nella DAW dopo averla riaperta per caricare la nuova AU.
- Il registro auval riporta ancora 0.7.0 nei metadati in cache; la versione 0.7.1 è verificata nel plist installato e il binario coincide con la nuova build. La validazione non prova che una DAW già aperta abbia ricaricato la nuova versione.

## Smear: fase della coda — 0.7.2 (1 ottobre 2026)

- Riprodotto difetto della 0.7.1: le ampiezze smussate restavano non nulle sul silenzio ma la fase veniva ricavata da arg(0). La coda diventava periodica rispetto all’hop FFT anziché conservare l’intonazione. Test 440 Hz/48 kHz, una nota di 1 s, analisi della coda fra 1,5 e 2 s: frazione di energia coerente a 440 Hz 0,000108 / 0,000469 / 0,00558 per FFT 512/2048/8192. Regressione fallita prima della correzione: Builds/test-smear-before.log.
- Aggiunta memoria di fase e avanzamento per bin, con stima smussata a 50 ms sui frame di ampiezza stabile e prosecuzione quando l’ingresso è insufficiente. Evita di aggiornare la frequenza con il transitorio di spegnimento. Smear a zero conserva la fase originale. Freeze con Smear cattura anche fase/avanzamento della coda residua, non quelli dell’ingresso silenzioso. Tutte le nuove memorie si azzerano al reset.
- Test mirati superati su tutte le FFT: frazione coerente a 440 Hz nella coda 0,991 / 0,958 / 0,907 / 0,987 / 0,980 per FFT 512/1024/2048/4096/8192. Verificati cattura Freeze durante la coda, reset, controlli reali via processore negli slot 1 e 4, valori 0/0,5/1, bypass e canale destro silenzioso. Cambiare Smear dopo una cattura non modifica il Freeze già sostenuto, come previsto.
- Tooltip e README chiariscono che Smear è una media delle ampiezze precedente al Freeze, con costante di tempo massima 1,5 s (non durata massima della coda). Parametri, default, ordine e latenza invariati. Backup sorgente/test: Backups/pre-smear-phase-fix. Runner test: aggiunta opzione --audit per regressioni mirate.
- Suite completa superata in 43,27 s, nessun fallimento (Builds/test-smear-fix.log). Release AU/VST3/Standalone, test e preview compilati senza warning/errori (Builds/build-smear-fix.log).
- AU 0.7.2 installata: plist e identità binario verificati, firma valida, auval superato (Builds/auval-smear-fix.log). Backup AU: Backups/20261001-202501/Spectral Strip.component. Il validatore può riportare la precedente versione nei metadati in cache: occorre riaprire la DAW e verificare v0.7.2 nel footer per caricare la nuova build.
- Verifiche numeriche e funzionali: non sostituiscono una valutazione musicale su materiale reale.

## Tremolo Shape più evidente — 0.7.3 (1 ottobre 2026)

- Il parametro era collegato al DSP: la precedente curva passava da sinusoide a tanh, miscelando nuovamente il risultato con la sinusoide. Il duty cycle restava al 50% lungo tutta la corsa. L’intervento risponde alla differenza poco percepibile segnalata dall’utente; non era un comando scollegato.
- Nuova curva a coseno raccordato: Shape 0 è la sinusoide originale; da 0 a 0,5 le transizioni si restringono fino a un’onda quadra arrotondata; da 0,5 a 1 si restringe anche il duty cycle, dal 50% al 12,5%. Edge minimo 2 ms, gain compreso fra 1−Depth e 1, nessuna compensazione artificiale del livello medio. Rate, latenza, parametri e default invariati.
- Aggiunte verifiche dell’inviluppo su Shape 0/0,25/0,5/0,75/1 per tutte le FFT: durata sopra metà ampiezza, tempo nelle transizioni, differenza RMS fra posizioni successive, limiti del guadagno e coerenza stereo. Restano attivi i test preesistenti di sinusoide, copie seriali, bypass, Depth zero e pulsazione a 20 Hz su tutte le FFT.
- Tooltip e README aggiornati. I preset con Shape non nullo cambiano carattere intenzionalmente. Backup sorgenti/test: Backups/pre-tremolo-shape-fix.
- Suite completa superata in 46,88 s senza fallimenti: Builds/test-shape-fix.log. Build Release AU/VST3/Standalone, test e preview senza warning/errori: Builds/build-shape-fix.log. Inviluppi misurati e risultati per FFT in Builds/Testing/Temporary/LastTest.log.
- AU 0.7.3 installata, versione plist e identità binario verificate, firma valida e auval superato: Builds/auval-shape-fix.log. Backup AU: Backups/20261001-203304/Spectral Strip.component. Il registro AU può mostrare la precedente versione in cache: riaprire la DAW e controllare v0.7.3 nel footer. Il gradimento musicale richiede ascolto nella DAW.

## Freeze: fedeltà del timbro — 0.7.4 (2 ottobre 2026)

- Segnalazione: timbro congelato metallico o diverso dall’ingresso. Riprodotti battimenti artificiali su una nota di 220 Hz con quattro armoniche, catturata dopo 1 s. Con FFT 1024 la variazione di energia fra finestre di 50 ms era 65,4% e la frazione coerente sulle armoniche originali 72,1% (Builds/test-freeze-before.log).
- Aggiunto raggruppamento dei bin attorno ai picchi dello spettro catturato: ogni regione conserva le fasi relative iniziali e usa lo stesso avanzamento del picco. Nessuna modifica alle ampiezze catturate. Soglia picchi a −60 dB rispetto al massimo, DC/Nyquist esclusi dal raggruppamento. Eseguito solo al momento della cattura, con storage sullo stack, senza allocazioni.
- Stima di frequenza del Freeze raccordata a 20 ms per ridurre l’errore del singolo frame. Con Smear si usa ancora la stima della coda, ora con la stessa coerenza di fase fra bin vicini. Nessun parametro o ritardo aggiunto; reset azzera la nuova memoria della stima.
- Test mirato a 48 kHz: FFT 1024, frazione coerente 92,0%, variazione d’energia 0,217%, energia media 0,010242 contro riferimento teorico 0,01025. FFT 2048/8192: frazione coerente >99,9%. Queste misure descrivono il segnale sintetico, non equivalenza percettiva su qualsiasi materiale.
- Test esteso a tutte le FFT a 48 kHz e FFT 2048 a 44,1/96 kHz. Regressioni Smear/cattura sulla coda, reset, bypass, stereo e copie incluse. Backup sorgente/test: Backups/pre-freeze-refine.
- Suite completa superata in 48,83 s (Builds/test-freeze-fix.log), build Release AU/VST3/Standalone, test e preview senza warning/errori (Builds/build-freeze-fix.log).
- AU 0.7.4 installata, versione e identità binario verificate, firma valida e auval superato (Builds/auval-freeze-fix.log). Backup AU: Backups/20261002-174031/Spectral Strip.component. Registro AU soggetto a metadati precedenti in cache: riaprire la DAW e verificare v0.7.4 nel footer. Ascolto musicale nella DAW ancora da valutare dall’utente.
