# IMS projekt - Příprava a rozvoz jídel společností Siesta pizza

### Adam Grünwald (xgrunw00), Dominik Nejedlý (xnejed09), 2020

### Popis

Simulace pizzerie zaměřená na sledování objednávek a přípravy jídel od jejich přijetí až po doručení.

### Překlad

Vytvoření spustitelného programu obstarává přítomný `Makefile`.

Příkazy:

- `make` - vytvoří spustitelný soubor
- `make run` - spustí simulaci (s výchozími vstupními hodnotami)
- `make clean` - smaže spustitelný soubor

### Spuštění

Program se spouští pomocí příkazu:

	./ims_proj [-r runs] [-m meals] [-e employees] [-f furnace_places] [-c cars] [-s shift_len] [-a acceptance_time]

Vstupní parametry:

- `-r runs` - počet simulačních běhů (výchozí - 1000)
- `-m meals` - průměrný počet jídel za směnu (výchozí - 400)
- `-e employees` - počet zaměstnanců (výchozí - 3)
- `-f furnace_places` - počet míst v peci (výchozí - 8)
- `-c cars` - počet aut (výchozí - 2)
- `-s shift_len` - délka směny v minutách (výchozí - 720)
- `-a acceptance_time` - doba, po kterou jsou přijímány objednávky, v minutách (výchozí - 660)

Pro výpis nápovědy je možné použít parametr `-h`.