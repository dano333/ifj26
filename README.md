# IFJ26 - Prekladač jazyka IFJ26

Tento projekt predstavuje implementáciu prekladača imperatívneho jazyka IFJ26 do cieľového mezikódu IFJcode26, vyvíjaný v rámci predmetov IFJ a IAL na FIT VUT v Brne.

## Hlavné súčasti
* **Lexikálna analýza:** Konečný automat pre rozpoznávanie tokenov.
* **Syntaktická analýza:** Rekurzívny sestup a LL(1) gramatika pre štruktúru, doplnená o precedenčnú analýzu výrazov.
* **Sémantická analýza a TS:** Kontrola typov a správa tabuľky symbolů.
* **Generovanie kódu:** Prekladač zdrojového kódu do formátu IFJcode26.

## TODO
- [ ] Pridat testy na generator + CI spustenie testov
- [ ] Dokoncit dokumentaciu ku generatoru