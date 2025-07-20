## Build 

0. predelavka struktury na gui a engine
   1. zprovoznit vsechno
   2. Nota bude mit referenci na track, track bude mit referenci na sequence !!!!! predelat vsechny metody !!!!!!!!!!!!!
   3. zprovoznit direktivu enginu na komplet deaktivaci Qt
   4. zdrojovy kod enginu presunout do samostatne slozky + samostatny cmake

1. refactoring celeho kodu.. 
   1. std::stared_ptr + not null kontrola
   2. vstupde pouzivat optional kde to je mozne
   3. nazvy netridnitch metod: snake_case, tridnich: camelCase
   4. odstranit nadbitence pradne sloty volajici 1 metodu
   5. vsechny signaly zapis timto stylem: visibilityChangedSignal(int, bool);

2. opravit chyb v UI (aby to vypadalo stejne jak predtim)
   1. custom tile bar pro QDock widgety..
   2. Dock Panel Nefunguje jak by mel: reset layout, nachyta se presne na dane pozice jak by mel

----
NOVE FUNKCE:

bug: lepsi detekce bich nastoroju (pres instanci mixer)

pouziti u routing entry = indikacni led diada (signalizace aktivniho routingu)

tlacitka u track listu:
   -> add, remove, remove all, record

tlacitka u mixeru:
   -> set all to min volume / max volume
   -> set output for all (replace ouput)
   -> automacaly select channels 

midi editor zoom bug (kdyz se zoomuje tak ujizdi s akutalni pozice)

midi editor optimalizace
   -> moznost zmenit vysku radku
   -> moznost upravy sledovani aktualni pozice (komplexni moznosti)

midi control bar: impelemnatce celkoveho nahledu tracku (nahradi time label)

velocity editor, ...

nastaveni editor + gui

implementace editor modu:
   -> select tool
   -> manaipulation tool: move, delete, copy, shift, move to track, ...
   -> verist vecit ohledne ppq (velikost nejmensiho bloku/noty)
   -> add new note, note lenght editing
   -> velocity editing







---------------------
GUI


2:
midi editor (sequence):
   -> pri zmene aktivni midi sekvence (je automaticky volano i pri nacteni projektu)


-> routing stack bude pak vazany primo na midi sequcence ** 

4:
vyuzivat zmenu current tict primo jako singal z projektu -> editor, tick ruler (primo interne uz resit)