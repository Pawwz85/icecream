# Icecream

Icecream is a C++ chess engine for [Spell Chess](https://www.chess.com/terms/spell-chess), the variant created through Chess.com's 2023 collaboration with Supercell. Spell Chess originates on Chttps://external-content.duckduckgo.com/iu/?u=https%3A%2F%2Ftse4.mm.bing.net%2Fth%2Fid%2FOIP.wXhVzcdrvMLxJz6Yl2VU3wHaHa%3Fr%3D0%26pid%3DApi&f=1&ipt=de809b7008d063e741dbe3dcd42a195095c44aec5687b6430df43960a6c1b034&ipo=imageshess.com and is available to play on the [Chess.com variant server](https://www.chess.com/variants/spell-chess). It combines normal chess with two tactical spells: **Freeze** and **Jump**.

## Highlights

- Standard chess move generation, search, evaluation, and transposition-table support
- Spell-aware positions, legal moves, and UCI move formatting
- Freeze and Jump metadata in an engine-specific extended FEN format
- A command-line UCI interface suitable for scripts, chess tooling, and compatible clients
- Standard UCI communication for regular chess applications and GUIs

Icecream is a functional standard chess engine as well as a Spell Chess engine: ordinary chess positions and moves can be used through `startpos` and standard FEN. It is an **engine**, not a graphical chess application. It does not include a board, mouse controls, game window, or bundled GUI. It is compliant with the standard UCI protocol, so it can be integrated with regular UCI chess software such as [Cute Chess](https://cutechess.com/) and other chess applications; its spell moves and extended positions are additional Icecream-specific conventions for clients that support this variant. It reads commands from standard input and writes responses to standard output.

## What is Spell Chess?

The rules in this section follow the [Spell Chess rules reference at spellchess.win](https://spellchess.win/). They describe the variant independently of Icecream's UCI notation.

Normal chess movement rules apply. Before making the normal chess move, a player may optionally use one spell: **Freeze** or **Jump**, but never both. Each player starts with five Freeze spells and two Jump spells.

### Freeze

Freeze targets a square and prevents pieces in the $3 \times 3$ area centered on that square from moving or giving check. The freeze applies during both the caster's turn and the opponent's turn, and expires at the beginning of the caster's next turn.

### Jump

Jump makes one piece transparent, allowing sliding pieces to move or give check through it. The effect applies during both the caster's turn and the opponent's turn, and expires at the beginning of the caster's next turn. Therefore, the opponent can move through a piece just jumped by the other player without spending one of their own Jump spells. This can produce a double-jump when the opponent has already jumped one of the pieces you want to move through.

Moving onto, rather than through, the square jumped by the opponent cancels that jump immediately, before the end of the moving player's turn.

### Shared rules

- Every turn must contain a normal chess move. If all of a player's pieces are frozen or otherwise immobile, that player cannot complete a legal move even if they could freeze an opposing piece; the position is then checkmate or stalemate depending on whether that player is in check.
- A player must not be in check at the end of their turn unless they have captured the opposing king. Capturing the opposing king wins immediately, even if the capturing player would otherwise be checkmated.
- Attacking a king through a piece that has not been jumped does not count as check. It is therefore legal to leave your king exposed to an attack that could be enabled by a future Jump. It is also legal to leave your king exposed to an attacker that is frozen but will unfreeze after your turn.
- Kings may never touch, even if one of them is frozen.
- Each spell may be used at most once every three turns. After using a Freeze, for example, its owner must go two turns without using Freeze before it recharges. Freeze and Jump have independent cooldowns, so Jump may be used on the turn immediately after Freeze.

The rules above describe the game. The protocol notation and starting resources described below are implementation details of Icecream.

## Build and launch on Windows

The project requires CMake 3.10 or newer and a C++20 compiler. From a PowerShell or Developer Command Prompt, run these commands from the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

CMake generates the executable named `icecream`. With a single-configuration generator it is normally at `build/icecream.exe`; with a Visual Studio multi-configuration generator it is normally at `build/Release/icecream.exe`.

Launch it directly:

```powershell
.\build\Release\icecream.exe
```

For a single-configuration generator, use `.\build\icecream.exe` instead. The executable is a UCI process: send commands through standard input and read replies from standard output. It is therefore possible to connect it to a compatible UCI client or GUI, but Icecream does not provide that client or GUI itself.

## UCI protocol

Icecream identifies itself as `Icecream` by `Pawwz85` after `uci`. The implemented command paths are:

| Command | Support and notes |
| --- | --- |
| `uci` | Reports the engine identity and `uciok`. |
| `isready` | Replies with `readyok`. |
| `setoption name <id> [value <value>]` | Standard UCI option command is routed by the client. No stable engine-specific options are documented here. |
| `position` | Loads `startpos`, `spellstartpos`, standard FEN, or extended spell FEN, optionally followed by legal moves. See the protocol reference below. |
| `go` | Parses supported search limits such as `depth`, `nodes`, `movetime`, `wtime`, `btime`, increments, `movestogo`, `mate`, `ponder`, and `infinite`. |
| `stop` | Requests that the search stop. |
| `quit` | Ends the engine process. |

`debug` is parsed but its value is not forwarded to the engine. `ponderhit` is routed but not implemented. Registration is a no-op. `searchmoves` is not supported, and unrecognized commands or unsupported `go` parameters are reported as UCI `string` messages. The engine expects a `position` command before `go`; otherwise it reports an error. Some UCI output paths, including general `info` and complete option validation, remain partial.

## Spell-chess UCI notation

### Positions and moves

The position command supports these starting positions:

```text
position startpos
position spellstartpos
```

`startpos` uses the ordinary chess starting state with no available spells. `spellstartpos` uses the ordinary board with the engine's spell starting metadata. Moves can be appended after `moves` and are validated as they are applied:

```text
position spellstartpos moves e2e4 e7e5
```

Ordinary moves use coordinate notation, for example `e2e4`. Promotion uses a lowercase suffix: `q`, `r`, `b`, or `n`.

A spell is prefixed to the base move with the target square and `&`:

```text
freeze@e7&e2e4
jump@a5&a1a8
freeze@g2&e8h8q
```

The final example shows a promotion suffix on the base move; spell moves use the same suffix rules as ordinary moves. A search result is printed as `bestmove` followed by the same formatter, so a spell move appears, for example, as:

```text
bestmove freeze@e7&e2e4
```

Castling is represented through the engine's normal coordinate move handling; clients should prefer legal moves returned or accepted by the engine rather than constructing special castling spell strings.

### Extended FEN

The engine accepts standard six-field FEN after the `fen` keyword:

```text
position fen <piece-placement> <side-to-move> <castling> <en-passant> <halfmove> <fullmove>
```

It also accepts a nine-component spell extension:

```text
position fen <piece-placement> <side-to-move> <castling> <en-passant> <halfmove> <fullmove> <freeze-square> <jump-square> <spell-info>
```

The standard fields are:

1. Piece placement, such as `rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR`
2. Side to move: `w` or `b`
3. Castling rights, such as `KQkq` or `-`
4. En-passant field
5. Halfmove clock
6. Fullmove number

The optional spell fields are:

- `freeze-square`: the active freeze target square, or `-`
- `jump-square`: the active jump-through square, or `-`
- `spell-info`: four slash-separated records in the form `F00/J00/f00/j00`

Each spell record is exactly one letter plus four digits: `[FfJj][0-9][0-9][0-9][0-9]`. `F` and `J` describe White's Freeze and Jump state; lowercase `f` and `j` describe Black's state. In each record, the first two digits are spells remaining and the last two digits are cooldown half-moves stored by the engine. For example, `F50/J20/f50/j20` is the spell metadata used by the built-in spell starting position.

This extended FEN grammar is an **engine-specific UCI extension**, not standard FEN and not part of the ordinary UCI specification. A six-field FEN supplied to the engine is supplemented internally with `- - F00/J00/f00/j00`.

## Example command sessions

Start the engine and ask it to identify itself:

```text
uci
```

Check readiness:

```text
isready
```

Load a spell-chess starting position:

```text
position spellstartpos
```

Apply ordinary moves:

```text
position spellstartpos moves e2e4 e7e5
```

Start a bounded search at depth 6:

```text
go depth 6
```

A typical completed search ends with a `bestmove` line. Finish a session with:

```text
quit
```

## Project status

