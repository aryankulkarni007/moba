# moba - design

`README.md` and `TODO.md` say what is built and how it is tested. This says
what is being built, and why each decision constrains the engine.

Written 2026-09-09. Every statement here is a decision unless it appears under
**Open questions**. Where something was proposed rather than stated, it says
so. Nothing here is inferred from genre comparison -- the previous cost of
doing that is recorded at the foot of this file.

Scope: **design facts that change a technical decision.** Champion kits, lore,
art direction and tuning numbers are deliberately absent. They belong in a
different document and would take this one from three pages to fifty.

---

## 1. What this is

A 5v5 MOBA -- lanes, towers, jungle camps, minions, and the enemy nexus as the
objective -- whose combat is a fighting game.

The README's shorthand, now unpacked: _closer to League than Dota_ is the
map, the control scheme and the champion-ability count. _Closer to Tekken than
Street Fighter_ means four specific things, all of which are in:

|                                                   |                                                                                                                                                                                                                                                                       |
| ------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Frame data is the language**                    | Every action has startup, active and recovery frames. The tick is the design unit, not a duration in seconds.                                                                                                                                                         |
| **Moves have properties, and movesets transform** | Abilities carry authored properties -- launchers, unblockables, feintable startups. A champion may also have a moveset that _changes_: using one ability rewrites the other three, so strings are formed from a small slot set rather than a long move list. See 3.3. |
| **Geometry is literal**                           | Hitboxes sweep along the actual arc. Hurtboxes are bodies, not circles. Position is the truth -- there is no targeting that overrides a miss.                                                                                                                         |
| **Hit levels**                                    | High, mid and low. A low sweep passes under an airborne character.                                                                                                                                                                                                    |

What is _not_ imported from Tekken: frame-precise movement. Movement is
point-and-click, so it operates at pathing granularity. All frame data lives on
abilities and on the defensive action, never on walking.

---

## 2. Control

**Point and click to move.** Right-click issues a move order; the character
paths there.

**The cursor is a single intent vector.** It carries move orders, ability aim,
and dodge direction. These are time-sliced rather than simultaneous -- the
cursor is free between clicks -- but the consequence is permanent and was
chosen knowingly:

> You cannot express _"keep going there, but dodge this way."_ Retreating means
> the cursor is behind you, so the dodge is backwards.

This was accepted rather than patched with a second aim source. One input, one
intent, and the dodge inherits your intention. Adding a second aim axis later
is a control-scheme rewrite, not a feature.

**High APM, constant movement.** This is not a game of standing still between
cooldowns.

**Inputs are edge-triggered orders, not held state.** A click is an event;
between clicks the character continues an order it already has. This has a
large netcode consequence -- see section 6.

**Aim is analog and must be quantised at capture.** Cursor-directed dodging
means an angle enters the simulation from a mouse position. Un-quantised aim
is the classic rollback desync. This is what `fx/angle.hpp` exists for, and it
is why that file is no longer a side branch.

---

## 3. Combat

### 3.1 Frame data

Every action is `startup -> active -> recovery`. Advantage is measured against
the defender's **dodge recovery**: the punish is landing on someone who rolled
early.

**The simulation runs at 60 Hz.** This is independent of render rate, which can
be as high as the machine allows. 60 rather than 120 because **rollback cost is
quadratic in tick rate**: the same wall-clock RTT means twice as many frames to
re-simulate, in half the per-frame budget -- 4x the pressure for the same
latency. 60 Hz is also already at the edge of human input precision; a two-frame
window is 33 ms.

#### Commitment has two currencies

> Commitment is paid in **time** or in **space**, and both are punishable.

A greatsword windup commits you in _time_: you are locked in frames and the
counterplay is reacting during them. A near-instant skillshot commits you in
_space_: you had to be in range, you are now standing somewhere known, and the
counterplay is punishing the position.

This is why MOBA abilities can keep near-zero cast times without losing the
design. The cost moves from the clock to the map. It is also why **only
time-commitment can be feinted** -- a four-frame cast offers nothing to bait.

### 3.2 The defensive action

**One slot, one button, one timeline shape -- different states on the
timeline.**

```
                startup      active                recovery
  dodge     f1-3  ___     f4-11  ####  invuln    f12-18 ______   also moves you
  parry     f1-2  __      f3-5   ##    parry     f6-24  ________ brutal on whiff
  block     f1-2  __      f3-N   ####  held      f+6    ___      cheap on whiff
```

(frame counts illustrative, not proposals)

Not every champion has a dodge. Some block, some parry, some have hyper-armour,
some have just-frame windows for perfect follow-ups. What is shared is the
_shape_: startup, an active window, recovery. The player learns that once and it
transfers to the whole roster; per champion they then learn two facts -- when
the window is, and what it does.

This is what makes frame data learnable at 5v5. Thirty bespoke defensive
mechanics would not be.

Engine consequence: one hit-resolution function reading the defender's current
state, and champions are a table of `(frame -> state)`.

### 3.3 Commitment and cancellation

**The governing rule: nothing cancels unless someone authored it.**

League's animation cancelling -- interrupting the remainder of an animation
once damage has registered -- does not port here, because it erases recovery,
and recovery is where the punish game lives. The general principle:

> A mechanic that emerges from implementation accident is a bug, even when
> players like it. Orb-walking, auto-cancels and the fast-combo tech that made
> certain League champions famous were nobody's design. They are gaps in state
> locking that the community learned to exploit.

The engineering expression of that principle is **default-deny**:

> Every frame, an entity is in exactly one authored state, and the legal exits
> from that state are authored data. There is no `default: allow`.

League's failure mode is the inverse -- an action is permitted unless something
explicitly locks it out, so every gap in the locking is a cancel. Invert it and
a cancel exists only where it was written down.

This is exhaustively testable, which matters given how the rest of this repo is
verified. The transition table is **finite**: for every state, every frame of
that state, and every input, the legal successor set is enumerable. It can be
swept rather than sampled, the same way the fx paths were.

**Cancel access is per champion, and it is a balance lever.** A character with
generous cancel windows is safe, and pays for it somewhere else.

#### The basic attack

**Basic attacks are kit-specific.** Each champion's default action is their own
move with its own frame data, rather than a shared auto-attack differing only
in range and speed. Tempo becomes an identity axis, the way jab speed does in
Tekken.

Two consequences, and the first is a straight win:

- **There is no auto-attack subsystem.** The basic attack is the move bound to
  the default state -- the same transition table as everything else. League's
  auto-attack system sits _beside_ its ability system with its own cancel
  rules, and that seam is precisely where orb-walking and animation-cancelling
  live. Delete the special case and the exploit surface goes with it.
- **Farming is asymmetric**, and the compensation must be _lateral_ rather than
  _temporal_. A champion who last-hits badly cannot be repaid by scaling into a
  monster -- 3.6 forbids that -- so they are repaid with a different way to
  clear, a different place to be, or a map-based advantage.

This is deliberate asymmetry, and the line it must not cross:

> **Asymmetry is fine. Illegibility is not.**

Fighting games are wildly asymmetric and are the most skill-expressive
competitive games there are, because the asymmetry is learnable: their jab is
10 frames, yours is 13, and you play around it. What 3.6 rejects is not
asymmetry but uncounterability -- power that nothing you learn can answer.

#### Just-frame follow-ups

The pattern that makes cancellation a mechanic rather than an exploit.

An auto attack is `windup -> active -> recovery -> idle`. Attack speed governs
the _cycle_, not the recovery; the recovery is a real, separate window in which
the character returns to idle.

A champion may have a follow-up that must be input inside a narrow window at
the frame recovery begins -- not necessarily one frame. Window width is the
difficulty dial: fewer frames, harder input, larger payoff. Hit it and the follow-up consumes the recovery and comes
out safe. Miss it and you get a slower version carrying a recovery of its own,
which is punishable.

That is a cancel which had to be earned, authored, and paid for. It is the
opposite of a cancel that exists because nothing stopped it.

#### State-dependent movesets

A champion's four slots are not four fixed abilities. Using one may **rewrite
the other three**, so a small slot set generates strings: dodge a long-recovery
ability, land W into R, R is a launcher, the launcher opens follow-ups that
only exist while the target is airborne.

The point is Invoker-like depth from a MOBA-sized input surface -- learnable
because the slots are few, deep because their meaning is contextual.

Architecturally this costs nothing new. **It is the same table as default-deny.**
Cancels, just-frames, string branches and moveset transformation are all
entries in one relation:

```
(state, frame, input) -> (next state, move)
```

Which is why that table has to be authored data and exhaustively swept, not
scattered across ability scripts.

#### Feints

A feint is a cancelled startup, used to bait a defensive commitment and punish
the recovery of whichever option the opponent chose.

**Feints belong only to champions whose startup forces a decision.** A champion
who mostly throws skillshots has no business feinting: a skillshot's counterplay
is dodging the projectile after release, so the opponent commits nothing during
the cast and there is nothing to bait. Feints attach to close-range,
high-commitment threats -- launchers, unblockables -- where the opponent must
answer during startup or eat it.

Feints also impose a frame budget that constrains startup lengths globally:

1. The opponent must be able to react to the startup at all, so startup has to
   exceed human reaction time -- roughly 12-15 frames at 60 Hz.
2. The feint has to cancel _after_ they have committed, or there is nothing to
   punish.
3. They must not be able to react to the cancel itself.

A game of six-frame startups cannot have meaningful feints, because nobody
reacts to 100 ms. Any champion meant to feint needs startups long enough to be
read.

### 3.4 Archetype priors

Sword characters tend to parry. Assassins tend to have i-frame dodges. Tanks
tend to have hyper-armour or a block-overload mechanic.

This is not a convention, it is a **prior**: silhouette tells you the hit band
_and_ the defensive class before you have ever fought that champion. It is
therefore a hard constraint on art direction, not a style note.

The related constraint: **wind-up length must visibly correlate with
commitment, and the hit band must be legible from the animation's shape at MOBA
camera distance.** A mechanic the player cannot see is not a mechanic.

### 3.5 Resources

**No mana.** Mana exists in League because League's abilities are cheap to
throw -- near-instant, and you keep kiting through them, so something external
has to meter the spam. Here every action has startup and recovery, so throwing
an ability _is_ standing exposed. **Commitment is already the cost.** That is
why Tekken has no mana and never needed one.

Supporting observation: the champions cited as good design -- Yasuo, Riven,
Katarina -- are the manaless ones. Removing mana removes the excuse; the only
limits left are cooldowns and execution.

Frame data meters _fights_. A resource meters _attrition_. Both layers exist
here, so:

| Meter      | Timescale | Job                                                 |
| ---------- | --------- | --------------------------------------------------- |
| Frame data | frames    | paces an engagement from the inside                 |
| Stamina    | seconds   | paces a fight; gates repeated defensive actions     |
| Health     | minutes   | the lane currency. Trading costs HP, not a blue bar |

The known cost of this: everyone manaless means poke has no price, so lane
cooldowns have to carry that weight alone.

### 3.6 Progression

**Scaling raises the ceiling, never the floor.**

A stat-check is power that wins the interaction without requiring you to win
the interaction -- power as a function of the clock rather than of play.
Rejected.

What makes scaling safe here is that **commitment does not scale.** A fed
carry's combo still has the same startup and the same recovery. Fed means "you
die faster when you are wrong," not "you stop being punishable." The
dodge-and-punish loop is intact at every gold lead.

Therefore: scale damage and options freely. **Never scale safety** -- no item
that shortens recovery, no level that grants armour frames.

**No ability derives offensive output from a defensive stat.** This is the
specific mechanism behind champions who one-shot you while building tanky: one
stat purchase buying both halves of the interaction. A power budget is spent,
not doubled.

**Attack speed compresses startup, never recovery.** There is no idle gap
between attacks to shorten -- the frame data _is_ the limiter, and League has a
gap only because its autos carry a separate cooldown on top of the animation.
So attack speed scales the windup and the swing, down to a floor where they
cannot compress further. That floor is the attack speed cap, and it is a
property of the move rather than a global number.

Two constraints on it:

- **Recovery is untouched.** Shrinking it is safety scaling, which this section
  forbids. You attack faster; you remain exactly as punishable each time.
- **It must not scale a committal move into unreactability.** A six-frame basic
  was never reactable and losing two frames changes nothing. A greatsword
  windup exists to be read, and compressing it destroys the mechanic. Attack
  speed therefore belongs to champions whose basics are already fast.

This produces a useful self-limit for free: with recovery fixed and startup
shrinking, **the faster a champion attacks, the larger the proportion of their
time spent punishable.** Attack speed has automatically diminishing safety.

Marksmen therefore exist, and are defined by short recoveries rather than by a
separate auto-attack system.

**Items grant options, not stat sticks** -- abilities, passives, gameplay
modifiers. With one trap: _an item that grants defensive frames is a safety
scale in disguise_. Offensive and utility options scale freely; i-frames,
cancels and armour do not, or the rule above is violated through the side door.

**Movement speed is kit-derived, not a purchasable stat.** It is obtainable and
it is not free; some champions use it to gap close. Treating it as an option a
kit grants rather than a number anyone buys keeps it out of the safety-scaling
problem -- spacing is defence, so a universally purchasable movement stat would
be buying safety through the side door.

Health scales as well as damage. Note the arithmetic: if they scale _together_,
time-to-kill is flat and there is no lethality arc at all. One has to outpace
the other for late fights to feel decisive.

#### The match arc

Matches run 30-60 minutes. Nothing in the arc below is a function of the clock:
the map changed because players cut it, and options accumulated because players
bought them.

|       | Map                                 | Options         | Fights                                  |
| ----- | ----------------------------------- | --------------- | --------------------------------------- |
| 0-10  | dense forest, cover everywhere      | 0-1 items       | short-range, ambush-driven, ganks work  |
| 10-25 | paths cut, sightlines opening       | 2-3 items each  | combinatorial depth rises               |
| 25-60 | substantially cleared, little cover | full option set | open-field, committal, decided by reads |

**Forest regrowth rate is the pacing dial.** Never regrow and minute 60 is a
parking lot. Regrow fast and the map never opens. The rate sets the equilibrium
between how much cover exists and how hard a team must work to remove it.

### 3.7 Duels and teamfights

The game oscillates between 1v1 and 5v5 with no boundary, and combat mechanics
do not price the same at both.

What breaks is **windows**, not depth. A whiffed attack, a launcher or a
knockup opens the target for a duration. In a duel that duration is priced
against one follow-up; at 5v5 the same duration is worth five times as much,
because five people collect on it. **CC value scales superlinearly with the
number of attackers. Damage does not.** That is why a juggle is a punishment in
a duel and absurd in a team fight.

> **Combat mechanics create windows sized for one attacker. Team-scale lockdown
> is an ability with a cooldown, not something the combat system hands out on a
> read.**

A launcher gives roughly one follow-up -- long enough for your own combo, too
short for four allies to rotate onto it. Anything that wants to be a two-second
lockdown is an ultimate, costed as team CC.

With that rule in place, **teamfights do not need a separate, coarser design.**
A 5v5 is a set of overlapping local engagements: you answer the one or two
threats actually on you rather than reading five startups. What decides the
fight is picks, cooldown management and positioning -- the layer MOBAs already
run on. What decides _your_ survival is your kit.

Being caught is more punishing here, and also more escapable, because everyone
has real defensive options. Kit identity determines what "caught" means: a
greatsword bruiser whose outplay lives on a parry has to stand and fight; an
assassin dodges and leaves.

#### Simultaneous commitment

The anti-gank mechanic falls out of frame data rather than needing a bandaid on
top of it.

In League a gank is safe because abilities have negligible recovery. Here every
attacker pays commitment, so **a gank is the moment at which the largest number
of players are simultaneously in recovery.** One well-timed parry or i-frame
dodge against overlapping commitments whiff-punishes several people at once.

That is the opposite of For Honor's Revenge, which fills a meter automatically
for being outnumbered and is widely read as rewarding having been outplayed.
This has to be executed, and it exists only because everyone committed.

Obligation that follows: **the outnumbered defensive option must have a path to
victory, not merely a longer loss.** If standing and fighting can never win,
kit identity is cosmetic -- you die, stylishly.

### 3.8 Roles

Lanes generate roles whether they are designed or not. League's positional
roles are emergent from map asymmetry, not authored. Given a lane structure,
players derive roles by week two; the only choice is designed or emergent.

_Proposed, not yet ratified:_ make the role about **what space you take, not
what stats you have.** With body-blocking, literal geometry and hit bands, a
frontliner is not someone with a large health number -- that is the stat-check
the design rejects. A frontliner is someone whose body denies a choke and whose
hyper-armour means they cannot be interrupted out of holding it. Same role,
expressed geometrically, and it is a skill-check rather than a number.

---

## 4. Geometry and hit resolution

### 4.1 The rule

**Team filters the candidate set. Geometry resolves the hit.**

The filter runs before the overlap test, not instead of it. Geometry decides
_whether_ you are hit; teams decide _who can be_.

- **No friendly fire.** For Honor is the counter-example: dying to an ally's
  hitbox for minimal damage, and being interrupted by their swings, is
  annoyance rather than depth. This is a player-experience decision that
  knowingly overrides realism.
- **Ally bodies do block movement.** Being _interrupted_ by a teammate is
  noise; being _blocked_ by one is spacing. Body-blocking a choke is skill
  expression, and it is the entire reason frontliners exist.
- **No targeting override.** If the shape does not overlap, it misses. An
  auto-attack is not guaranteed -- the target may dodge, or accelerate out of
  the arc. Conversely, standing beside a greatsword user gets you hit by the
  blade even though you were not the target.

### 4.2 Hit bands

A body occupies a **set of bands**. An attack is authored to a band. A hit
requires 2D overlap **and** band intersection.

An airborne character does not occupy the low band, so a low sweep passes under
them. This is Tekken's high/mid/low system, and it is the reason the mechanic
does not require a 3D simulation.

Bands are **discrete and few**, not a continuous height:

1. A fixed-angle top-down camera cannot convey continuous height. Collision
   more precise than the camera can show reads to the player as randomness.
2. **Height is never a player input, always an ability outcome.** The cursor is
   on the ground plane; you never ask to be airborne, an ability puts you
   there. So the set of height states is small, authored and enumerable.

### 4.3 2.5D, not 3D

**Shapes are 2D plus a height interval.** An overlap is
`overlaps_2d(a, b) && bands_intersect(a, b)` -- one extra comparison pair on
top of the predicates `shapes.hpp` already has.

Full 3D was rejected, and the cost is documented in this repo already:
`TODO.md` records deleting the segment-segment interior-point solve because
_only 3D needs it -- skew segments can be closest at interior points of both_ --
along with the fourth-power term that overflowed at 215 units. Going 3D
reintroduces exactly that, plus 3D orientation, which is where determinism gets
genuinely unpleasant.

What makes 2.5D sufficient is the baking decision below: **a 3D motion is a
sequence of 2D snapshots when the atomic unit is a frame.** A greatsword
overhead does not need a swept volume, it needs enough frames.

```
one slam = 5 baked frames, each a 2D shape + a band

   f4    f6    f8    f10   f12
   _     -     =     #     #      2D sector, widening
  HIGH  HIGH   MID   LOW   LOW    band it occupies

a leaping character does not occupy LOW: it eats f4-f6, and f10-f12 pass under
```

### 4.4 The simulation is authoritative over animation

The sim owns position, facing, state and frame counter. Hitboxes are **data**:
shapes in local space, per frame, transformed by position and facing. The
renderer is a slave -- it reads sim state, plays whatever matches, and if it
drifts nothing breaks, because nothing reads it back.

Animation-authoritative was rejected: it puts the animation system _inside_ the
determinism boundary. No float blending, no engine anim graph, fixed-point bone
transforms, and every re-export becomes a candidate desync. Root motion would
become sim state, so an animator lengthening a step would change an attack's
reach.

**Hitboxes are baked offline.** The artist animates; a build step samples the
skeleton per frame and emits fixed-point shapes; the sim loads a table. Limb-
accurate geometry that visually matches, with the animation system entirely
outside the determinism boundary.

---

## 5. The world

Everything on the map is collidable: champions, minions, jungle camps,
structures, persistent projectiles, and terrain.

### 5.1 Terrain

**A dense destructible forest that behaves as solid wall.** League's terrain
model -- solid, blocking movement and vision -- but destructible. Not Dota's
walkable-between trees.

- **Not enterable by default.** To go into the forest you destroy a path
  through it.
- Some champions can perch on trees and navigate the forest without cutting it.
- The reason is vision: if the forest is walk-through, vision control degrades
  into peekaboo instead of being a mechanic.

Engine consequence: terrain is an immutable baked collision structure plus a
**bitset of destroyed cells**. Rewinding terrain is restoring a bitset, not
re-simulating a forest. The same treatment applies to structures and camps --
anything that changes only on an event is state to _snapshot_, not state to
_step_.

### 5.2 Movement and pathing

Solid walls create concave pockets, so local steering alone is insufficient and
a real pathfinder is required. Grid-based, A\* over passable cells, plus the
destroyed-cells bitset.

Two things that keep this affordable under rollback:

- **Pathfinding is an event, not a per-frame cost.** It runs when a move order
  is issued. The resulting path is a handful of waypoints -- a dozen `fx` values
  per entity -- so it is snapshotted rather than recomputed.
- **Minions do not pathfind.** They follow fixed lane routes and steer around
  what is in the way. Precomputed flow fields per fixed destination cost
  N entries per destination for a handful of destinations.

An all-pairs path lookup table was considered and rejected: the table is
quadratic in tile count and tile count is quadratic in linear resolution, so it
is **quartic in map resolution**. A 256x256 grid is 4.3 billion next-hop
entries. Cutting one tree also opens a shortcut that changes optimal routes
globally, so lazy regeneration can invalidate most of the table.

**Rule for any cache: it must be invisible to the simulation result.** Two
machines with differently-populated caches must produce identical answers. The
cached function has to be pure, and the cache stays out of the snapshot.

_This section is a direction, not a ratified plan -- pathfinding technology and
optimisation are explicitly deferred._

---

## 6. Netcode

### 6.1 Why determinism

Frame data is the design language, so **+2 versus -2 must be readable**, which
means input latency is unacceptable. That single requirement eliminates the
alternatives:

| Model                  | Your own input latency                       | Determinism   | Why not                                                        |
| ---------------------- | -------------------------------------------- | ------------- | -------------------------------------------------------------- |
| Deterministic lockstep | the delay buffer, 2-6 frames                 | mandatory     | buys zero resim cost by making _you_ wait. RTS trade, not this |
| Server-authoritative   | ~0 for movement, full RTT for _confirmation_ | not required  | resolves your dodge somewhere other than where you pressed it  |
| **Rollback**           | **zero, always**                             | **mandatory** | **chosen**                                                     |

The mental model:

> Rollback makes your own input authoritative and the world provisional.
> Server-authoritative makes the world authoritative and your input provisional.

Frame-tight defensive timing is the one design that _requires_ your input to be
authoritative. With a server in the decision path, an i-frame window of ~8
frames (133 ms) against 60 ms of one-way latency is displaced by most of a
window -- "I dodged and still got hit," which this game cannot ship with.

Rollback is **not** League's client-side prediction setting. There, prediction
is cosmetic and being wrong means a visual snap. Here prediction lands on the
_simulation_: two machines that ever compute a different result from the same
inputs diverge permanently and silently. That is the entire reason for fixed
point, the pure `step()`, and the golden hashes.

Click-to-move helps here rather than hurting. Because inputs are edge-triggered
orders, "predict no new input" is correct on most frames even at high APM.
Direct WASD control would have been the bad case.

The bill is `depth x world step` -- you pay for a world step _depth_ times per
frame, not once. That is why section 5.1 snapshots terrain instead of stepping
it.

### 6.2 Server-filtered rollback

Pure peer rollback requires every machine to simulate every entity, so every
machine _holds_ every position, and fog of war becomes decoration drawn over
data the client already has. Maphacks are then structural, not a hardening
problem. This is the unsolved condition of deterministic lockstep RTS.

**Chosen: the server is authoritative for information flow; clients run
deterministic rollback over a filtered subset.**

The exact condition for a partial simulation to be correct rather than
approximate:

> A subset **S** can be simulated exactly in isolation **iff nothing outside S
> affects anything inside S** during the window being simulated.

So a client does not simulate what it can see. It simulates the **closure** of
what it can see under interaction:

```
closure radius = vision_radius
               + max_interaction_radius        what can touch what you see
               + max_speed x rollback_depth    what can arrive before you resim
```

The interaction term exists because of body-blocking: an invisible enemy beside
a _visible_ minion deflects that minion's path. Without the buffer the client
walks the minion straight, the server deflects it, and the visible desync --
a minion bumping into nothing -- is itself an information leak.

Long-range abilities do not break this, because **the projectile is its own
entity.** It enters the closure when it crosses the boundary; the caster never
does. You see the dagger, not the thrower.

Net effect: what leaks is a thin annulus just past the vision edge, for roughly
one rollback window, rather than the whole map for the whole match.

Two costs, both accepted:

1. **The desync test changes shape.** Today's acceptance test is that every
   machine folds the same operations into the same hash. Under partial
   simulation no two clients simulate the same set, so it becomes: the server's
   full sim _restricted to subset S_ hashes identically to the client owning S.
   Testable, but it needs designing rather than inheriting.
2. Closure must be computed and maintained per client per frame, and entities
   entering it need an authoritative state handoff.

No shipped game is known to do fog-filtered deterministic rollback. This is
being built, not copied.

### 6.3 Anti-cheat

Memory protection is mitigation, not a solution -- kernel AC stops other
processes reading yours but not a DMA capture card, and hardware memory
encryption is not deployable to consumer machines. A maphack does not execute
inside the sim, so sandboxing the sim does not touch it.

The primary defence is architectural: closure-filtering means the client does
not hold what it must not see.

The secondary defence is behavioural, and determinism makes it unusually
strong. **Every match is a complete, bit-exact, replayable record at a few
bytes per player per frame.** Every match ever played can be stored for
nearly nothing and re-simulated to any frame, which gives exact ground truth
for what information a player had at an instant versus how they behaved. That
is a far better substrate for automated review than a game that has to
instrument it separately.

---

## 7. Roster

**Humanoid base form.** Humans, hybrids, vampires, gods, demigods, humanoid
aliens, anthropomorphic characters. No animal or monster champions in base
form. Transformations -- a dragon, say -- are welcome as a special case.

This is a technical decision as much as an art one. A humanoid base means
**one skeleton**, therefore one hitbox-baking pipeline, therefore hurtboxes
with comparable proportions across the whole cast. That last part is what makes
the frame-data game fair: if every character's torso sits in roughly the same
place, a _mid_ attack means the same thing everywhere and knowledge transfers
between matchups. A roster of wildly different body plans would make hit bands
character-specific, which is the memorisation explosion the design avoids.

Transformations therefore cost a second rig and a second baked hurtbox set,
and the body's band occupancy becomes mutable state rather than fixed per
champion. Affordable as a special case, expensive as a norm.

---

## 8. Consequences for the code

Each of these follows from a decision above and affects work already tracked in
`TODO.md`.

- **The entity set is a parameter, not an assumption.** Nothing iterates "the
  ten champions." Nothing assumes an entity that existed last frame exists this
  frame. Presence and absence must be representable. This is what the
  generational handles in `core/strong_id.hpp` and the unwritten `slot_map` are
  for. Partial simulation is a property of every loop and every container --
  it cannot be added as a layer later.
- **`fx/angle.hpp` is unblocked and is now load-bearing.** Cursor-directed
  dodging means analog aim enters the sim, and the header's existing
  `[phase 1]` note -- quantise aim at capture, day one -- is now a requirement
  rather than a suggestion.
- **`fx/shapes.hpp` extends, it does not get rewritten.** The 2D predicates
  stay, the segment-segment simplification stays valid, and what is new is an
  interval type plus band occupancy on each body.
- **Terrain, structures and camps are snapshot state, not stepped state.**
- **Caches must not be observable in simulation results**, and stay out of the
  snapshot.
- **The golden-hash story needs extending** to cover subset hashing against a
  server sim once section 6.2 is built.

---

## 9. Open questions

Not deferred by accident -- these are genuinely undecided.

- **Window budget.** Section 3.7 sets the rule -- combat windows are sized for
  one attacker -- but not the number. How long is a launcher's airborne state
  in frames, and does it scale down within a juggle? Needs playtesting, not
  argument.
- **Cancelling out of the defensive action.** Section 3.3 settles cancelling
  _into_ a move -- authored windows, per champion, default-deny. The reverse is
  open: can a dodge or a parry be cancelled into an attack before its recovery
  ends? That is the roll-cancel question, and it decides whether defence is a
  commitment or a free reposition.
- **Dedicated defensive slot, or defence baked into the kit?** 3.2 currently
  assumes a dedicated slot with champion-specific content. The alternative is
  that the defensive option is simply one of the champion's abilities, so some
  spend two slots on defence and some spend none. Baked-in gives far stronger
  identity and is viable only if spacing and geometry are a genuinely
  sufficient universal defence. Leaning baked-in; 3.2 needs revising if that
  holds.

- **Endurance.** The arc in 3.6 answers _escalation_, not _stamina of the
  player_. Fighting-game density sustained at high APM for 30-60 minutes is an
  ergonomics question, not a balance one, and it is unaddressed.
- **The defensive state list.** Deliberately open. i-frames, block, parry,
  hyper-armour and just-frames are known; the full set falls out of designing
  four or five champions, not out of architecture.
- **Stamina specifics.** Pool, regeneration, and crucially whether _offensive_
  actions draw from it. Sharing the pool with offence is the soulslike lever
  and the biggest remaining decision on feel.
- **Forest regrowth.** Dota's trees respawn. If these do, a regrown tree can
  invalidate a path someone is standing on, which needs a repath event.
- **Vision specifics.** Wards exist; their model, and how vision interacts with
  perching champions, is unspecified.
- **Entity counts.** Minions per wave, camps, projectile lifetimes. Needed to
  budget the world step, which under rollback is paid `depth` times per frame.

---

## 10. Proof of concept and roadmap

### 10.1 The two proofs

**PoC-0 -- one window, one character, one dummy.** The dummy attacks on a loop
with a visible windup. You move, you dodge, you punish the recovery. No
netcode, no map, no minions, no second player.

It answers the question every other decision depends on: **does
cursor-directed, frame-tight defence feel good under a MOBA control scheme?**
Nobody knows. It has never been shipped. If the answer is no, the rest of this
document is moot, and that should be discovered in days rather than months.

**PoC-1 -- two windows.** Two characters with _different_ defensive states --
one dodges, one parries -- on a simplified ARAM-like map, inputs exchanged over
localhost with injectable artificial latency. It answers the second question:
does the read game work human against human, and does rollback hold.

**A frame-data debug overlay is not optional**, and it belongs in PoC-0 rather
than later: current state, frame counter, active hitboxes drawn, advantage on
hit. Fighting games ship training mode for this reason. Frame data that cannot
be seen cannot be tuned.

### 10.2 Phases

| Phase                   | Work                                                                                                                                                                | What it proves                               |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------- |
| **1. Substrate**        | `angle.hpp`; height intervals on `shapes.hpp`; `slot_map`; entity state; fixed-timestep pure `step()`; window and input; debug renderer drawing bodies and hitboxes | nothing yet -- this is the floor             |
| **2. PoC-0**            | one attack with real frame data, one dodge with i-frames, hit bands, scripted dummy, frame-data overlay                                                             | **the feel.** Go / no-go on the whole design |
| **3. PoC-1**            | second champion with a parry, two clients over localhost, rollback, artificial latency                                                                              | the read game, and that rollback holds       |
| **4. MOBA layer**       | terrain and destruction, pathfinding, minions, towers, stamina, items                                                                                               | that it works as a MOBA                      |
| **5. Netcode for real** | server, closure filtering, subset hashing                                                                                                                           | that it scales to ten                        |

Two properties must hold from phase 1 even though they only pay off in phase 3,
because both are rewrites rather than additions if deferred:

- **State is snapshot-able.** POD, no pointers, slot_map indices rather than
  addresses.
- **The entity set is a parameter.** See section 8.

### 10.3 Rendering

_Undecided. Vulkan was recommended by an external advisor; the argument below
is against it for now, and is a recommendation rather than a decision._

Rendering is not the hard part of this project. Forty entities, top-down, no
complex lighting -- the frame is not GPU-bound and will not become so. Vulkan's
real advantages are multithreaded command recording, explicit memory control
and low driver overhead in draw-call-heavy scenes, and none of those bite at
this scale. The costs are roughly a thousand lines before the first triangle,
plus MoltenVK friction, since Vulkan is not native on macOS.

The decisive point is that **the renderer is replaceable by construction.** The
sim is authoritative, POD, and pure; nothing reads back from rendering.
Swapping renderers later costs zero sim changes, so this decision is reversible
and should be optimised for time-to-first-pixel.

Recommended: **SDL3** -- its GPU API wraps Vulkan/Metal/D3D12, and it supplies
windowing and input in the same dependency, both of which are needed anyway.
`sokol_gfx` plus `sokol_app` is the smaller, header-only alternative and is
closer to this repo's existing taste.

Vulkan becomes the right answer if writing a Vulkan renderer is itself a goal
of the project, which is legitimate. It should still come after PoC-0, because
it is weeks of work that answers no design question.

---

## Why this document exists

On 2026-09-08 a session assumed 1v1, built performance arguments on that pair
count, then inferred direct control, analog aim, minion waves and limb hitboxes
from two genre comparisons and wrote those inferences into `README.md` and
`TODO.md` as fact. They were removed before committing.

The root cause was not context loss. It was that the design had never been
written down, so every session re-inferred it and got it wrong differently.

**Do not infer game design from genre references.** If this document does not
answer a question, the answer is unknown -- say so, and add the question to
section 9.
