# Pravidlá pre spoluprácu a kód

Tento dokument definuje interné pravidlá pre náš tím pri vývoji projektu IFJ26.

## 1. Hlavičky súborov a komentáre
Každý zdrojový a hlavičkový súbor (`.c` / `.h`) musí obsahovať štandardnú hlavičku a dokumentáciu funkcií:

### Hlavička súboru
```c
/*
 * Project: IFJ26
 * Authors: Meno (xlogin)
 * Description: Stručný popis účelu modulu.
 */
```

### Popis funkcií
Každá funkcia musí obsahovať stručný popis rozhrania a významu:

```C
/*
 * @brief Stručný popis toho, čo funkcia robí a na čo slúži.
 * @param param1 Popis vstupného parametra
 * @return Čo funkcia vracia
 */
```

## 2. Git Workflow a Conventional Commits
Pri verzovaní kódu dodržiavajte tieto zásady:

- Pracujte na samostatných vetvách a vytvárajte Pull Requesty (PR).
- Každý PR musí pred merge do vetvy main prejsť code review a schválením od iného člena tímu.
- Commity robte menšie, pričom kód musí byť po každom commite preložiteľný.

### Formát správ commitov (Conventional Commits)
Správy commitov píšte v angličtine v nasledujúcom formáte:

```plaintext
<type>(<scope>): <short description>
```
Povolené typy:

- `feat(part)`: Nová funkcionalita (napr. feat(parser): add recursive descent for rules).
- `fix(part)`: Oprava chyby (napr. fix(scanner): fix handling of string literals).
- `docs`: Úpravy dokumentácie (dokumentace.tex).
- `style(part)`: Formátovanie kódu pomocou clang-format, úprava bielych znakov.
- `refactor(part)`: Úprava kódu, ktorá nemení logiku ani nepridáva funkciu.
- `test(part)`: Pridanie alebo úprava testov.
- `chore(part)`: Úpravy buildu (Makefile), Nix flake alebo konfigurácie projektu.
