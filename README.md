# PvZ Bird Mod

An unofficial fan mod that adds slingshot birds to **Plants vs. Zombies: Game of the Year**.
Pick birds alongside your plants, load them into a slingshot at the left edge of the lawn,
drag back to aim, and let go. Pig structures on the lawn shelter the zombies until you smash them.

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

**Title screen and main menu:** a PvZ x Angry Birds logo, built from both games' own art on your PC.

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
| Matilda | 150 | 20 s     | Drops an exploding egg: 1½ squares each way along her lane, plus the squares above and below |

Birds with long recharges start each level half-charged, like Cherry Bomb.
Zombies killed by a bird are removed at once and drop coins like any other kill, and a bird
can take the last zombie of a level.

**Pig structures:** every Adventure level (birds picked or not) gets 1 or 2 Angry Birds structures
(glass, wood and stone blocks with pigs inside) and 1 to 3 pigs standing on their own, placed at
random anywhere from the fourth column to the zombies' end of the lawn, so every level has its
own layout. The three columns
by your house always stay clear. A structure can stretch across two or three lawn squares and
hold a pig in each of its huts. Blocks and pigs are drawn close to the birds' own size, and a
hut is built as big as the pig inside it.
Early worlds build them mostly from glass and wood; later worlds from wood and stone, taller.
- **Physics:** a hit shoves what it doesn't break. Blocks and pigs slide, bump their neighbours,
  tip off the edge when what holds them up is knocked away, and fall. Landings and hard bumps
  hurt both blocks, and a pig under a heavy block pops. Bomb and Matilda's egg throw blocks
  away from the blast, and Bubbles shoves them back. Anything pushed off the lawn is gone.
- **No planting on them:** a lawn square that any part of a structure or pig touches can't be
  planted on. When blocks slide or break, the squares they leave free can be planted again.
- Zombies walk through them, but a zombie behind one is covered: every plant shot hits the
  structure first. Peas, spikes, puffs, fire peas, Starfruit stars and Cattail spikes hit the
  first block in their way, and lobbed shots (cabbages, kernels, butter, melons) come down on the
  structure instead of flying over it.
- **Every attacking plant hurts them:** fume- and gloom-shrooms gas the blocks in their reach,
  Cherry Bomb, Doom-shroom, Potato Mine and the Cob Cannon blow them apart, a Jalapeno burns its
  whole row, a Squash crushes the square it lands on, and a Chomper bites a block or swallows a pig
  (all but the King Pig) in front of it. Spikeweeds and spikerocks hurt a block that slides onto them.
- Shooters, pults, Cactus, fume- and gloom-shrooms and Chomper go for a structure in reach even
  when no zombie is there.
- Birds smash them the Angry Birds way: Blues shatter glass, Chuck splits wood, Bomb cracks
  stone, Terence flattens anything in the square he lands on.
- Chuck and Hal crash into structures like in Angry Birds: every block they break slows them,
  stone stops them dead, and a stopped bird bounces back and tumbles down.
- Blocks fall when what's under them breaks, and a falling block hurts what it lands on.
- Plants only hit the part of a structure in the square their shot reaches: a pea hits the first
  block in its way, and a winter melon freezes and chips only the blocks in the square it lands in.
- Snow peas turn wooden blocks to ice. An Ice-shroom turns every block on the lawn to ice.
- Stella's bubbles lift pigs away (all but the King Pig, who is too heavy).

**The pigs:** open the Almanac and click **View Pigs** to see each one.
| Pig          | Toughness | Sun when popped | Where |
|--------------|-----------|-----------------|-------|
| Small Pig    | 150       | 25  | Every world |
| Medium Pig   | 260       | 25  | Every world |
| Big Pig      | 420       | 50  | From the Day world on, more later |
| Helmet Pig   | 650       | 50  | From the Day world on, more later |
| Mustache Pig | 900       | 75  | From the Pool world on, mostly late |
| King Pig     | 2400      | 200 | Only in the Dr. Zomboss fight, in a stone hall next to the robot |

## Changes
**v2.3**
- Every attacking plant can attack structures: catapult plants, Cactus, Starfruit, Cattail,
  fume- and gloom-shrooms, Chomper, Squash, Cherry Bomb, Doom-shroom, Potato Mine, Jalapeno and
  the Cob Cannon join the peashooters.
- Structures block every shot, lobbed ones included: a pult can't reach zombies behind a
  structure until it's cleared.
- Squares a structure or pig touches can't be planted on.
- Each Adventure level has 1 or 2 structures plus 1 to 3 pigs standing on their own, even
  when you don't pick any birds. Structures now line up with the lawn's squares, and on the
  first levels they stay on the grass.

**v2.2**
- The whole pig family: small, medium and big pigs, Helmet Pig and Mustache Pig turn up as the
  worlds go on, and the King Pig waits in a stone hall in the Dr. Zomboss fight.
- Pig Almanac: a View Pigs panel next to View Birds on the Almanac's index page.
- Blocks and pigs are bigger, close to the birds' own size; huts are built to fit their pig.
- Structures can stretch across two or three lawn squares, with a pig in each hut.
- Physics: blocks and pigs get shoved, slide, tip over edges and fall onto each other.
- Plants only damage the part of a structure in the square they hit (winter melons no longer
  splash the neighbouring squares and rows).

**v2.1.5**
- Structures can turn up anywhere from the fourth column to the far edge of the lawn, in any
  row (more than one to a row when they're spaced apart), so every level has its own layout.
  The three columns closest to the house stay clear.

**v2.1**
- Plants shoot at structures: a straight shooter or winter-melon pult with a block or pig in its
  lane now fires at it even when there are no zombies in that lane, instead of standing idle.
- Chuck and Hal are stopped by structures the Angry Birds way: breaking blocks uses up their
  push, stone stops them dead, and they bounce back off the block that stops them.
- Winter melons act like snow peas on structures: their splash turns wood and stone to ice and
  chips at it.
- Terence only smashes the structure in the square he lands on (he still crushes every zombie
  in his column).

**v2.0**
- Pig structures (above).
- New bird: Matilda drops an exploding egg and flies off.
- The View Birds button no longer shows on the Almanac's Plants and Zombies pages.

**v1.3.2**
- Crazy Dave no longer picks 3 plants for you on later runs through Adventure: every seed slot
  is yours to fill with plants and birds.

**v1.3.1**
- The slingshot works in full screen. On a widescreen monitor PvZ keeps its 4:3 picture with
  black bars at the sides, and the mod used to read the mouse as if the picture filled the whole
  screen, so grabbing the bird missed. The mod now learns where the picture really is from the
  game's own mouse position, and draws the slingshot into the game's picture before it's stretched.

**v1.3**
- A PvZ x Angry Birds logo: the "Angry Birds" title (cut at runtime from your own Angry Birds
  splash screen) with Red perched on it sits under the Plants vs. Zombies logo on the title
  screen, and as a smaller badge above the stone on the main menu.

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
