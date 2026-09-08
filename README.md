Softverska (kernel) implementacija RISC-V procesora

Ovaj deo projekta realizuje kompletan procesor u kernel prostoru. Hardver
(FPGA, AXI, BRAM kontroleri) ne ucestvuje ni u jednom koraku: instrukcijska
memorija, memorija podataka, registarski fajl, ALU i upravljacka jedinica su
realizovani kao strukture podataka i funkcije u kernel modulu.

Aplikacija u korisnickom prostoru salje drajveru isti niz instrukcija koji je
koriscen za demonstraciju hardverske verzije, a drajver po zavrsenom izvrsavanju
vraca stanje svih 32 registra** (i, dodatno, sadrzaj memorije podataka).



1. Sadrzaj foldera

 Fajl | Opis 

 `rv32_core.h`: Model procesora: dekodiranje, ALU, memorije, registri. Prevodi se i u kernel i u korisnickom prostoru. |
 `cpu_sw_driver.c`: Kernel modul (char drajver) — kreira `/dev/cpu_sw` i `/dev/cpu_sw_mem`. |
 `app_sw.c`: Korisnicka aplikacija — reset, prenos programa, pokretanje, ispis registara. |
 `program.h`: Demonstracioni program (isti niz instrukcija kao u hardverskoj verziji). |
 `program_zbb.h`: Dodatni test program: pokriva Zbb, M ekstenziju, grananja, skokove i pristup memoriji. |
 `test_core.c`: Provera ISA jezgra bez kernela (`./test_core`) — korisno pri razvoju i za odbranu. |
 `Makefile`:  Prevodjenje modula, aplikacije i testa. |



2. Arhitektura resenja

'''
  korisnicki prostor            kernel prostor
 ┌───────────────────┐        ┌──────────────────────────────────────┐
 │     app_sw        │  write │  cpu_sw_driver.c                     │
 │  "s"  reset       │───────>│   ┌────────────────────────────────┐ │
 │  "i addr word"    │        │   │  struct rv32_cpu               │ │
 │  "r"  run         │        │   │   x[32]  pc  steps  stop_flag  │ │
 │                   │        │   │   imem[256]   dmem[16384]      │ │
 │                   │  read  │   └────────────────────────────────┘ │
 │  ispis registara  │<───────│   rv32_step()  - fetch/decode/exec   │
 └───────────────────┘        └──────────────────────────────────────┘
'''

Zasto ovakva podela: `rv32_core.h` ne zavisi ni od jednog kernel API-ja osim
`linux/types.h` i `string.h`, pa se isti kod prevodi i kao obican C program.
Time se semantika instrukcija testira brzo, bez `insmod`/`dmesg` ciklusa, a u
kernelu se nalazi samo "lijepak" (char uredjaj, parsiranje komandi, sinhronizacija).



Hardverska verzija ima petostepeni protok (IF/ID/EX/MEM/WB) sa jedinicama za
prosledjivanje i detekciju hazarda. Posto te jedinice u hardveru upravo
obezbjedjuju da rezultat bude isti kao kod sekvencijalnog izvrsavanja,
softverski model izvrsava instrukcije jednu po jednu (arhitekturni model), sto
daje identicno stanje registara i memorije po zavrsetku programa.

3. Podrzane instrukcije

Skup je preslikan iz RTL opisa (`control_decoder.v`, `alu_decoder.v`, `alu.v`,
`immediate.v`, `branch_module.v`):

Grupa:  Instrukcije 

 RV32I aritmeticko-logicke : `add, sub, sll, slt, sltu, xor, srl, sra, or, and` 
 RV32I sa neposrednom vrednoscu: `addi, slti, sltiu, xori, ori, andi, slli, srli, srai` 
 Gornji imedijati/skokovi: `lui, auipc, jal, jalr` 
 Grananja: `beq, bne, blt, bge, bltu, bgeu` 
 Memorija: `lb, lh, lw, lbu, lhu, sb, sh, sw` 
 RV32M:  `mul, mulh, mulhsu, mulhu, div, divu, rem, remu` 
 Zbb logicke: `andn, orn, xnor` 
 Zbb rotacije: `rol, ror, rori` 
 Zbb min/max: `min, minu, max, maxu` 
 Zbb brojanje bitova: `clz, ctz, cpop` 
 Zbb prosirenja i bajtovi: `sext.b, sext.h, zext.h, rev8, orc.b` 
 Sistemske: `ecall, ebreak` (postavljaju stop flag) 

Deljenje po ivicnim slucajevima je implementirano po RISC-V specifikaciji:
deljenje nulom daje `-1` (`divu`: `0xFFFFFFFF`), ostatak pri deljenju nulom daje
deljenik, a `INT_MIN / -1` daje `INT_MIN` uz ostatak `0`.

4. Interfejs drajvera

Kreiraju se dva uredjaja (analogno hardverskoj verziji, gde je `/dev/cpu` bio
za upravljanje, a `/dev/bram_0` i `/dev/bram_1` za memorije):

 Uredjaj: Namjena 

 `/dev/cpu_sw`:  upravljanje procesorom (`write`) + stanje registara (`read`) 
 `/dev/cpu_sw_mem`: ispis memorije podataka (`read`) 

Komande koje se upisuju u `/dev/cpu_sw`:

 Komanda | Znacenje 

 `s`: reset: brise registre, PC, obe memorije, postavlja stop flag 
 `i <adresa> <rec>`: upis instrukcije na bajt-adresu instrukcijske memorije 
 `d <adresa> <rec>`: upis 32-bitne reci u memoriju podataka 
 `r`:  pokretanje programa od adrese 0 do `ecall`/`ebreak` 
 `n [broj]`: izvrsavanje jednog ili N koraka (single step) 
 `0 <adresa> <rec>` :  kompatibilnost sa formatom iz `bram_driver.c` (uredjaj 0 = instrukcije, 1 = podaci) 

`read` sa `/dev/cpu_sw` vraca status, PC, broj izvrsenih instrukcija i sve
registre (heksadecimalno i dekadno, sa ABI imenima).


5. Prevodjenje i pokretanje

Na razvojnoj masini (x86, lokalni kernel)

```bash
sudo apt install linux-headers-$(uname -r) build-essential   # ako vec nije
make                 # prevodi modul, aplikaciju i test
make load            # insmod + dozvole nad /dev cvorovima
./app_sw             # demonstracioni program
make unload
```

Za ciljnu plocu (isto okruzenje kao za postojece drajvere)

```bash
make KERNELDIR=/put/do/kernel/izvora ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

Zatim se `cpu_sw_driver.ko` i `app_sw` prekopiraju na plocu i pokrenu:

```bash
insmod cpu_sw_driver.ko
./app_sw
```

Brza provera bez kernela

```bash
make test && ./test_core
```


6. Ocekivani rezultat (demonstracioni program)

Program upisuje 5 i 10 u `x1`/`x2`, gradi konstante preko `lui`, radi
`mul/mulh/mulhu/mulhsu`, a zatim upisuje registre `x1..x31` u memoriju podataka
(korak 16 bajtova) i zaustavlja se sa `ecall`.

```
status      : HALTED (ECALL/EBREAK)
PC          : 0x000000a8
instrukcija : 43

x1  (ra  ) = 0x00000005            5
x2  (sp  ) = 0x0000000a           10
x3  (gp  ) = 0x00000032           50      <- mul  x3, x1, x2
x4  (tp  ) = 0x00000000            0      <- mulh x4, x1, x2
x5  (t0  ) = 0x00032005       204805      <- add  x5, x1, x8
x6  (t1  ) = 0x000ea00a       958474      <- add  x6, x2, x9
x8  (s0  ) = 0x00032000       204800      <- lui  x8, 0x32
x9  (s1  ) = 0x000ea000       958464      <- lui  x9, 0xea
x10 (a0  ) = 0x0000002d           45      <- mulhu  x10, x5, x6
x11 (a1  ) = 0x0000002d           45      <- mulhsu x11, x5, x6
ostali registri = 0
```

Memorija podataka (`/dev/cpu_sw_mem`) sadrzi iste vrednosti na adresama
`0x00, 0x10, 0x20, 0x40, 0x50, 0x70, 0x80, 0x90, 0xa0`, sto odgovara `sw`
instrukcijama iz demonstracionog programa.

Dodatni test (`./app_sw -t`)

Program `program_zbb.h` proverava Zbb i M instrukcije, petlju sa `bne`, skok
`jal` i pristupe memoriji razlicitih sirina. Ocekivano, izmedju ostalog:
`clz = 4`, `ctz = 2`, `cpop = 13`, `rev8 = 0x34120f0f`, `orc.b = 0xffffffff`,
`div(-16,3) = -5`, `rem(-16,3) = -1`, `divu(0xfffffff0,3) = 0x55555550`,
suma 1..5 = 15 u `x29`.

7. Razlike u odnosu na RTL implementaciju

Tokom preslikavanja RTL-a u softver uocene su sledece stvari; softverska verzija
implementira ispravno ponasanje po RISC-V specifikaciji, a razlike su ovde
dokumentovane:

1. `div`, `divu`, `rem`, `remu` — u `alu.v` su zakomentarisani ("Division not
   supported"), iako ih `alu_decoder.v` dekodira. Softverska verzija ih
   implementira u potpunosti.
2. `bgeu` — u `branch_module.v` koristi `>` umesto `>=`, pa grananje ne bi
   bilo uzeto kada su operandi jednaki. Softverska verzija koristi `>=`.
3. `srai` — `alu_decoder.v` za I-tip ne proverava `funct7 = 0100000`, pa bi
   `srai` bio izvrsen kao `srli`. Softverska verzija razdvaja ta dva slucaja.
4. `auipc` — `immediate.v` generise imedijat samo za opcode `0110111` (LUI),
   dok za `0010111` (AUIPC) vraca nulu, pa bi AUIPC dao samo vrednost PC-a.
   Softverska verzija racuna `pc + imm`.
5. `lui` — u protoku podataka prvi ALU operand je procitani `rs1`, a polje
   `rs1` kod LUI je deo imedijata; rezultat je tacan samo dok je taj registar 0
   (sto u demonstracionom programu jeste slucaj). Softverska verzija upisuje
   samo imedijat.
6. Sirine `load` operacija — RTL uvek cita celu rec (`mem_to_reg`), bez
   obzira na `funct3`. Softverska verzija razlikuje `lb/lh/lw/lbu/lhu`.
7. `rol`/`ror` sa pomerajem 0 — u RTL-u se svodi na pomeranje za 32, sto je
   u Verilogu nedefinisano; softverska verzija maskira pomeraj sa 31.

Uz to, u originalnom `app.c` petlja za prenos programa ide do `i <= 44`, dok niz
`program[]` ima 43 elementa (indeksi 0–42), pa se citaju dve lokacije van niza.
Nova aplikacija koristi `sizeof(program)/sizeof(program[0])`.

8. Napomene o implementaciji u kernelu

* Stanje procesora se alocira sa `vzalloc` (struktura je oko 17 KB zbog dve
  memorije), pa se ne trazi veliki kontinualni blok fizicke memorije.
* Pristup stanju je zasticen mutexom (`cpu_lock`), jer vise procesa moze
  istovremeno otvoriti uredjaj.
* Izvrsavanje ima gornju granicu (`RV_MAX_STEPS = 1 000 000`) i poziva
  `cond_resched()` na svakih 4096 instrukcija, tako da program sa beskonacnom
  petljom ne blokira kernel.
* `read` koristi `simple_read_from_buffer` nad snimkom stanja napravljenim pri
  `open()`, pa `cat` i visestruki pozivi `read()` rade ispravno (bez `endRead`
  globalnih zastavica iz hardverske verzije).
* U jezgru se ne koristi 64-bitno deljenje (samo mnozenje i 32-bitno deljenje),
  cime se izbegava potreba za `do_div()`.
