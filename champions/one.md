# One

Codename. The first champion, and the vehicle for PoC-0 and PoC-1.

Designed 2026-09-09. Read `../DESIGN.md` first -- every mechanic here is an
instance of a rule in that document, and this file only records what is
specific to this champion.

A sword duelist built on a stance toggle. Unsheathed he is committal and
offensive; sheathed he has no attack at all but gains movement and a low sweep.
The toggle doubles as his recovery cancel, so his defence is paid in tempo
rather than in frames.

---

## Kit

Four buttons, two stances.

| | Unsheathed | Sheathed |
| --- | ---------------------------------------------------------- | ------------------------------------------------------- |
| **Q** | Dash. If it crosses an enemy attack at the right timing, it **deflects**, he vanishes and reappears behind them, backstabbing. No i-frames, and it does not deflect everything. | Short back dash with i-frames, along his facing. |
| **W** | **Unblockable** mid slash. | **Low sweep.** On hit, refreshes the unsheathe cooldown. |
| **E** | Sheathe. | Unsheathe -- releases a fast 360 flurry around him. |
| **R** | Hyper-armour domain expansion. Charging narrows the slash and lengthens its range. Release dashes forward. Catching someone in recovery cuts them critically -- heavy bleed. | Not castable. But unsheathing and inputting R inside a just-frame window yields a **fully charged R immediately**. |

## Rules

- **No basic attack while sheathed.** Sheathed is a movement and setup stance,
  not an offensive one.
- **Sheathing cancels recovery.** Input it quickly after an ability or basic and
  the remaining recovery is replaced by the sheathe's own.
- **The low sweep refreshes the unsheathe cooldown only** -- never ability
  cooldowns. Landing it continues pressure; it does not restart the mixup.
- **No minion reset on Q.** Considered and cut: a dash that resets on low
  minions would make the deflect permanently available in lane, which removes
  the cost of the read.

## Gameplan

Dash in, deflect something, appear behind them, W into sheathe-cancel, then the
sheathed mixup -- back dash out, or low sweep to continue. Unsheathe or R to
punish and re-engage. The just-frame unsheathe-into-R is the maximum punish on a
recovery window, and it requires perfect input rather than only recognition.

His power is bimodal on purpose. Land the read and everything follows; miss it
and he has very little. That is Fiora, and it is the intent.

## Counterplay

- **Bait the dash.** Q has no i-frames and does not deflect everything, so
  making him spend it on nothing is the primary answer.
- **Dodge the low sweep.** Sheathed with the sweep whiffed, he has no offence
  at all and it is your turn.
- **Interrupt the setup.** Sheathe after Q, then a sweep interrupted in startup,
  leaves him with cooldowns down and no way to deal damage. Bad sequencing
  punishes itself.

## Why this champion works

Each mechanic is an instance of a system rather than a bespoke rule:

- The stance toggle is the transition table from `DESIGN.md` 3.3, not a special
  case.
- The sheathe cancel passes the cancel test: **failing the input costs more than
  not attempting it**, because you end up in a stance with no attack.
- Cancelling escapes the *frame* punish and pays in *tempo*, which is the two
  currencies of 3.1 -- there is no auto-block in this game, so standing in
  sheathed stance is genuinely exposed.
- The critical cut on a target in recovery is counter-hit -- a universal rule
  materialised as this champion's ultimate.
- **The stance is legible in silhouette.** Sword drawn or sheathed is the most
  readable state indicator a character can have, which directly answers the
  legibility risk in `DESIGN.md` 3.4. Preserve this property in the art.

## Open

- **Name.** "One" is a codename.
- **What the deflect catches.** Autos only, or skillshots too? Directional or
  omnidirectional? Deliberately unanswered: no MOBA has a deflect mechanic, so
  there is no reference to reason from. PoC decides it.
- **Q shares one cooldown across both stances.** *Proposed, not stated.* If the
  dash and the back dash are on separate cooldowns he has two escapes, and
  "bait the dash" stops being counterplay. Shared is what makes him catchable.
- **Actionable frames inside a chain.** Cooldowns bound how often the mixup
  restarts; they say nothing about whether the defender can act *between* links.
  `DESIGN.md` 3.7's rule applies -- every link leaves an actionable frame, and
  the skill is that the window is tight rather than absent. A numbers question.

## PoC staging

Build him in stages so PoC-0 still isolates its variable.

| Stage | Add | Question it answers |
| ----- | -------------------------------- | -------------------------------------------- |
| PoC-0 | basic attack + Q dash, no deflect, no reset | does cursor-directed dodge-and-punish feel good |
| | W unblockable slash | does commitment read at MOBA camera distance |
| | E stance toggle + sheathed W | does the cancel cost enough to be fair |
| | Q deflect | what should a deflect catch |
| PoC-1 | R, and the just-frame charge | does the maximum-punish input feel earned |
