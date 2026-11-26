# PvZ Bird Mod

An unofficial fan mod that adds slingshot birds to **Plants vs. Zombies: Game of the Year**.
Pick birds alongside your plants, load them into a slingshot at the left edge of the lawn,
drag back to aim, and let go.

Solo play, Adventure mode. Not affiliated with or endorsed by PopCap/EA or Rovio.

## You need both games
- **Plants vs. Zombies GOTY on Steam, version 1.2.0.1096.** Other builds are detected and the mod stays off.
- **Angry Birds for PC**, installed. The mod reads the bird pictures and sounds from your own
  Angry Birds install while the game runs, and the seed-packet background from your own PvZ
  `main.pak`. Nothing from either game is included in this project or copied to disk.

On Melty, press Play: Melty tells the mod where both games are, so there is nothing to set up.

Without Melty, if Angry Birds isn't in `C:\Program Files (x86)\Rovio Entertainment Ltd\Angry Birds`, create
`birdmod.ini` next to `birdmod.dll`:
```ini
[birdmod]
AngryBirdsFolder=D:\Games\Angry Birds
```
or start the launcher with `--angry-birds "D:\Games\Angry Birds" --pvz "<your PvZ folder>"`, which writes that file for you.

## Install and play
1. Download `birdmod.dll` and `birdmod_launcher.exe` from the latest release and keep them in the same folder.
2. Run `birdmod_launcher.exe`. It starts PvZ through Steam, or attaches to it if it's already running.
3. On the **Choose your plants** screen, a **CHOOSE BIRDS** panel sits next to the plant grid.
   Click a bird to put it in your seed bar. Each bird takes one seed slot, in the order you pick,
   just like a plant. Click it in the seed bar to put it back. Let's Rock! turns on once every slot is filled.
4. In the level, click a bird packet to load it into the slingshot. Birds cost sun and recharge like plants.
5. Press on the bird and drag back to aim. A dotted arc and a ring show where it will land.
   Release to fire. Right-click to unload.

**Early levels** (1-1 to 1-9, except 1-5) have no plant select, so Red is added to the seed bar automatically.

**Bird Almanac:** open the Almanac and click **View Birds** to see every bird's cost, recharge,
damage, area and a short description.

**Conveyor levels** (except wall-nut bowling): about one in four new packets on the belt is a bird,
up to two at a time. Belt birds are free, like belt plants.

## The birds
| Bird    | Sun | Recharge | What it does |
|---------|-----|----------|--------------|
| Red     | 100 | 7.5 s    | Heavy hit where he lands |
| Chuck   | 150 | 7.5 s    | Lands, then dashes right along the row, hitting each zombie |
| Bomb    | 175 | 25 s     | 3×3 explosion |
| Blues   |  75 | 7.5 s    | Splits in three: your row and the rows above and below |
| Hal     | 125 | 7.5 s    | Lands, then boomerangs left toward the house along the row |
| Terence | 500 | 50 s     | Crushes the whole column he lands in |
| Bubbles | 100 | 15 s     | Puffs up and shoves zombies in a 3×3 back about 1.5 tiles |
| Stella  | 125 | 20 s     | Traps zombies in a 3×3 in bubbles for 5 seconds |

Birds with long recharges start each level half-charged, like Cherry Bomb.
Zombies killed by a bird are removed at once and drop coins like any other kill, and a bird
can take the last zombie of a level.

## Changes
**v1.2.1**
- The launcher takes `--angry-birds <folder>` and `--pvz <folder>` and saves them for the mod,
  so it never has to guess or ask where either game is installed (Melty passes both on Play).
- The launcher stays open until the game closes.

**v1.2**
- Bird Almanac: a View Birds panel on the Almanac's index page opens a Birds page with
  each bird's stats and a description.
- Bird buttons keep working after switching between windowed and full screen.
- Pop-ups (pause menu, Imitater picker and others) now cover the birds like they cover
  plant cards, and birds can't be clicked through them.
- The bird panel no longer draws over the Almanac when it's opened from plant select.

**v1.1**
- Bird kills are instant: a zombie a bird brings to 0 health is removed straight away
  instead of walking on until something else hits it.
- Bird kills go through the game's own loot drop, so a bird can kill the last zombie
  without the level getting stuck.
- Balance: Terence costs 500 sun (was 300); Bomb recharges in 25 s (was 30 s).

**v1.0**
- First release: eight birds, plant select, conveyor belts, Red in early levels.

## Troubleshooting
The mod writes a log to `%TEMP%\birdmod.log`: the level number, birds picked, and every launch,
landing and hit. If a warning box says the PvZ build isn't supported or Angry Birds wasn't found,
the mod switches itself off and the game plays normally.

## Build from source
Needs a 32-bit Windows C++ compiler, for example [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)
(`i686-w64-mingw32-clang++`). Put it on `PATH` or point `CXX` at it, then run:
```sh
sh build.sh
```
Output goes to `dist/`. `src/pvz.h` lists every PvZ memory address the mod uses; all were found
by scanning the running 1.2.0.1096 game, since this build's layout differs from 1.2.0.1073.

## License
Free to play. Others may remix it with credit.
This project contains no art, sound or code from Plants vs. Zombies or Angry Birds.
