## Build 

1. opravit chyb v UI (aby to vypadalo stejne jak predtim)
   1. Vylepsi zvyrazeni tlacitek (toogle play/mute, visible, ...) najak zvyraznit kdyz jsou neaktivni
   3. Dock Panel Nefunguje jak by mel: reset layout, ...

2. refactoring celeho kodu.. 
   1. vsude pouzivat misto * std::stared_ptr, vsude kde nacitam pointer MUSI BYT OVERENI ZDA NENI NULL
   2. vstupde pouzivat optional kde to je mozne
   3. prejmenovat sloty (na konci nazvu nebude _slot + odstrani zbytecne metody duplicitny metody pro slot)
   4. nazvy netridnitch metod: snake_case, tridnich: camelCase

BUG: kdyz vypinam a zapinam play/mute tak to nekdy nevypne hrajici zvuk + id je shiftnute + 1
   -> napriklad v tracku 5 se prehravaji noty jak kdyby byli v tracku 6



----
NOVE FUNKCE:

nacitani primo vystupnich kanalu ulozenych v midi

implementace solo play ( u tracku )

indikacni led diada (signalizace aktivniho routingu)

tlacitka u track listu:
   -> add, remove, remove all, record

tlacitka u mixeru:
   -> set all to min volume / max volume
   -> set output for all (replace ouput)
   -> automacaly select channels 

midi editor zoom bug (kdyz se zoomuje tak ujizdi s akutalni pozice)

