#!/usr/bin/env bash
#
# Builds round five of the Anti language additions, generics and collections,
# one fresh headless session per step.
#
#   phase 0  main tree: apply the addendum
#   phase 1  main tree, one after another: generics, optional values, own and
#            lent, walking, hashing, direct imports, and the shared collection core
#   phase 2  six lanes in worktrees, at most MAX_PARALLEL at once: sequences,
#            hashed maps and sets, sorted, handles, Shared<T>, memory checks
#   phase 3  the driver brings each finished lane onto main, then a reconcile
#   phase 4  two lanes: the synchronized collections and the concurrent ones
#   phase 5  main tree: merge, final reconcile, and the syntax overview
#
# Worktrees: Eddie's global rule is to work without git worktrees, with one
# exception he approved: a driver's parallel phase may create one worktree per
# headless session, managed by the driver. The lanes are that case, and each
# lane's prompt says so at the very top. No session creates, removes or switches
# a worktree or branch. Each worktree's build/deps is a link to the main
# checkout's, so nothing is downloaded again.
#
# Restartable: completed step ids are recorded in build/drive/add5-completed. A
# lane that stopped keeps its worktree; a rerun continues it there.
#
#   ./drive-additions-5.sh              run, or resume after a stop
#   ./drive-additions-5.sh --list       list steps and their state
#   ./drive-additions-5.sh --redo <id>  run one step again
#
# Needs round four finished, a clean tree, and docs/anti-language-additions-5.md
# committed. Set once in your shell:
#   export MAX_TURNS=600
#   export MAX_PARALLEL=3        (optional; the default is 3)
# Written for the bash 3.2 that macOS ships.

set -u

ROOT="$(pwd)"
STATE_DIR="$ROOT/build/drive"
STATE_FILE="$STATE_DIR/add5-completed"
LOG_DIR="$STATE_DIR/logs"
STATUS_DIR="$STATE_DIR/add5-status"
WT_DIR="$ROOT/build/worktrees"
MAX_TURNS="${MAX_TURNS:-600}"
MAX_PARALLEL="${MAX_PARALLEL:-3}"
POLL="${POLL:-20}"
mkdir -p "$STATE_DIR" "$LOG_DIR" "$STATUS_DIR" "$WT_DIR"
touch "$STATE_FILE"

# --- gate blocks ---------------------------------------------------------------

read -r -d '' GATES_COMMON <<'EOF'

--- How to work ---

You are a headless session with no one to answer questions. Do the work above,
then check it yourself against docs/decisions.md, docs/anti-object-model.md and
docs/anti-language-additions.md. Those documents and CLAUDE.md are the authority.
Round five, generics and collections, is part of docs/anti-language-additions.md;
read the sections your step names in full before you start.

Do not spawn a background subagent or any background task. Do all work and any
review inline. Do not create, remove or switch git worktrees or branches. Do not
touch build/drive/. Follow the repository layout in CLAUDE.md: a step that needs a
new directory directly under src/, tests/ or docs/ stops and reports BLOCKED.

Commit as you go. When a coherent part is finished and the build compiles, commit
it with a message naming the part, even if the suite is not green yet. Only the
final commit of the step must pass every gate.

Logging: send every command's output to a file under build/drive/logs/, never
into your context. Grep the logs for errors and failures, and read a full log
only after a grep shows one.

Gates, all must pass before the final commit:
- the build has zero warnings (warnings are errors)
- the full suite passes on the host
- both sanitizer suites pass
- the docs-style checker reports nothing on every file you touched

If a gate fails, fix it in line with the authority documents and rerun the gates.
Try a given failure at most three times. Do not invent a design decision the
documents do not contain: if the work needs one, stop and report BLOCKED with the
question. Never weaken a test or a gate to pass.

Write docs/reports/<date>-<this step's id>.md, short, referencing logs by path.
List what you did, what failed and how you fixed it, and every [provisional]
decision you made.
EOF

read -r -d '' GATES_MAIN <<'EOF'

One more gate: the syntax overview and both specifications agree with what you
built, including the "Built / Not built yet" status lines.

Record decisions in docs/decisions.md, marked [provisional] where you chose where
the documents are silent. When green: push.

Prove completion. The report and your reply must include the literal output of
'git log --oneline -3', 'git status --short', the result of
'git rev-parse HEAD origin/main' showing both equal, and the three suite pass
counts.

The last line of your reply must be exactly DONE if everything is green and
pushed, or BLOCKED: <one sentence> if you stopped. Nothing after it.
EOF

read -r -d '' WORKTREE_NOTICE <<'EOF'
Read this first. You are running in a git worktree, on a branch of its own, and
that is deliberate. Eddie's standing rule is to work without git worktrees, with
one exception he approved: a driver's parallel phase may create one worktree per
headless session, managed by the driver. This is that case. Other sessions are
building other parts of round five at the same time in theirs, which is why you
cannot share the main checkout. So do not stop, block or move your work because of
the worktree rule. The driver created this worktree and will bring your branch
onto main. Do not create, remove or switch any worktree or branch yourself, and do
not touch the main working tree. This worktree's build/deps is the main
checkout's pinned downloads; configure as usual.

Your task follows.

EOF

read -r -d '' GATES_LANE <<'EOF'

Stay inside your fence. Change only the paths the step names, tests under tests/
for them, the identity manifests where your change adds or moves generated
output, docs/decisions-LANE.md and your report under docs/reports/. Do not edit
CLAUDE.md, docs/decisions.md, the specifications or the syntax overview: record
your decisions in docs/decisions-LANE.md, marked [provisional] where you chose,
and a later step folds them in and updates the status lines. When you add a test
to tests/CMakeLists.txt, add it next to the other tests of the same area. A change
the step needs outside the fence: stop and report BLOCKED with the path and the
reason.

When green: commit, and do NOT push; the driver brings your branch onto main.
Prove completion. Your reply must include the literal output of
'git log --oneline -3', 'git status --short' and the three suite pass counts.

The last line of your reply must be exactly DONE if everything is green and
committed, or BLOCKED: <one sentence> if you stopped. Nothing after it.
EOF

# --- the steps ---------------------------------------------------------------------

# step_prompt <id> prints the task of a step
step_prompt() {
	case "$1" in
	apply-5) cat <<'EOF'
Documents only. Apply docs/anti-language-additions-5.md, round five of the
additions, as its first section says: fold every section into
docs/anti-language-additions.md, make the changes its "What to change" section
lists, and add the new sections of the syntax overview marked "Not built yet".
Delete the addendum. Build nothing.
EOF
	;;
	generics-syntax) cat <<'EOF'
Build the syntax of generics, the "Syntax" section of round five: type parameters
in <> on functions, structs, classes, variants and interfaces, type arguments in
type positions, the C# rule in expressions with its refusal and fix for a name
that is not generic, >> closing two lists, constraint declarations, and type
aliases. Inference of type arguments from the arguments of a call. Parse and check
the declarations; compiling the copies is a later step. Tests for each form and
each refusal.
EOF
	;;
	generics-constraints) cat <<'EOF'
Build "Constraints" of round five: hooks and interfaces as constraints, combined
with +, named sets with constraint, Number shipped in anti.lang, the check of a
generic body against its constraints where it is written, the check of every use
where it is used, and what an unconstrained parameter allows, with the round's
messages. Tests for each.
EOF
	;;
	generics-copies) cat <<'EOF'
Build "Compilation" of round five: one compiled copy per use with concrete
arguments, one type for the same arguments across modules, the merge of identical
copies by the whole-program pass, the dev-build cache of copies with one copy kept
at the link, size_of(T) per copy, and a descriptor per copy of a generic class.
Tests that two modules share one List-like copy, that copies over two pointer
types share code, and that dev and release builds give the same results.
EOF
	;;
	generics-libraries) cat <<'EOF'
Build "Libraries and C" of round five: a generic stored in a library file as IR
with its parameters open and its constraints in the public interface, used from
another module with its own arguments; export type naming a copy for the C header;
a generic export fn refused; anti doc showing generics with their parameters and
constraints. Tests with a generic in a library used by a program, and a named copy
called from C.
EOF
	;;
	generics-features) cat <<'EOF'
Build the rest of "What can be generic" and "Other features with generics" of
round five: integer constant parameters, methods with their own type parameters
kept out of the table and refused as abstract or replaced, nested types of a
generic class, generic synchronized and concurrent classes, generic workers checked
per copy, language hooks in generic types, generic may-fail functions, function
types as type arguments, and generic variants. Tests for each.
EOF
	;;
	optional-values) cat <<'EOF'
Build "Optional values" of round five: ?T for any type, with the rules of ?*T, the
value and one flag byte, ?*T unchanged, the match result as a ?Match, and the C
header's struct. Tests for each rule, including a ?T of a struct, of a class value
and of a generic parameter.
EOF
	;;
	own-and-lent) cat <<'EOF'
Build "Ownership at a call" and "Lending" of round five: own parameters that move
their argument with use after the move refused, and lent pointer parameters valid
only for the call, with the checker refusing to store, return or capture one or
pass it to a keep or own place. Tests for each rule and each refusal.
EOF
	;;
	walking) cat <<'EOF'
Build the language part of "Walking a collection" of round five: for x in c gives
a read-only copy, with the refusal and its message when the loop changes it; for x
in &c gives a lent pointer for one turn; both for slices and for any type with the
iter hook. The change count and its dev-build trap are part of the collections, but
build here what the compiler must give them: the iterator's view of the count and
the trap's message naming both places. Tests for each.
EOF
	;;
	hash-ordered) cat <<'EOF'
Build "Hashing and order" of round five: the hash hook in the table of language
hooks, hash for every built-in type, the default hash of a struct or class over its
fields in order, the Ordered constraint in anti.lang, and the random seed chosen at
program start for hashing collections, with a way for tests to fix it. Tests for
each, including that eq values hash alike under the default.
EOF
	;;
	direct-imports) cat <<'EOF'
Build "Direct imports" of round five: import path.{Name, Name}; with the listed
names visible unqualified, the refusal of a name that clashes, the module still
reachable by its name, and anti fmt keeping the list sorted. Tests for each.
EOF
	;;
	collection-core) cat <<'EOF'
Build the module anti.collection of round five: Iterable<T> and Iterator<T>, and
the parts every collection shares, as "Principles" and "Operations every
collection has" describe: the allocator at creation, the initial capacity with
reserve and shrink, the change count and its dev-build trap, the find, update and
remove forms with _all, _first and _one and the failure of _one with its count,
and eq, to_text and serialize over elements. Build them over a small internal
collection in the tests, so each part is proven before the real collections exist
in their own modules, which later steps build in parallel.
EOF
	;;
	lane-seq) cat <<'EOF'
Build the sequences of round five, each in its own module of "Module layout of the
collections": List<T> in anti.collection.list, Deque<T> in anti.collection.deque,
Ring<T, N> in anti.collection.ring and Grid<T> in anti.collection.grid, with every
operation the round lists and the shared parts of anti.collection. Ring allocates
nothing after it is made; test that under the allocation count. Tests for every
operation, a list of a class type whose destruct runs, a list of pointers that
owns only the pointers, and the change-count trap.
EOF
	;;
	lane-hashed) cat <<'EOF'
Build the hashed maps and sets of round five: Map<K, V> and HashMap<K, V> in
anti.collection.map, and Set<T>, HashSet<T> and BitSet in anti.collection.set,
with every operation the round lists: Map in insertion order with a hash table of
positions, HashMap with its entries in the table and a walking order randomised on
every walk, serialize of a HashMap sorted by key where the key has lt. Tests for
every operation, insertion order after removals, and heavy churn under the leak
check.
EOF
	;;
	lane-sorted) cat <<'EOF'
Build SortedMap<K, V> and SortedSet<T> of round five in anti.collection.sorted as
B-trees, with every operation the round lists, including range, floor, ceiling,
first, last, pop_first and pop_last. Tests for each, and for order after many
insertions and removals.
EOF
	;;
	lane-handles) cat <<'EOF'
Build the collections on handles and the heap of round five: Pool<T> and Handle<T>
in anti.collection.pool, Tree<T> in anti.collection.tree and PriorityQueue<T> in
anti.collection.queue, with every operation the round lists: blocks that double,
elements that never move, generations that make a stale handle find nothing even
after its slot is reused, get_versioned and set if_version, removal of a whole
subtree, and a binary heap in one array. Tests for each.
EOF
	;;
	lane-shared) cat <<'EOF'
Build Shared<T> of round five in anti.mem: new, share with an atomic count, read
and modify by lending, and destruction through destruct when the last handle is
freed. Tests with handles in a list-like collection of the tests, handles shared
between threads, and the documented leak of two shared objects that hold each
other.
EOF
	;;
	lane-memcheck) cat <<'EOF'
Build "Memory checks" of round five: --memory-checks for antic and for anti build,
anti test and anti run. The back end emits AddressSanitizer's checks around every
load and store, and the build links the AddressSanitizer runtime of the pinned
clang, with the names and lines of -g in its reports. Refused with its message on
windows-arm64. Off by default. Tests that it catches a use after free, a double
free, a heap overflow and a leak.
EOF
	;;
	reconcile) cat <<'EOF'
Bring the merged lanes together. Fold every docs/decisions-*.md into
docs/decisions.md, each entry into the section it belongs to, keeping its wording
and tag, and delete those files. Update the specifications' and the syntax
overview's "Built / Not built yet" lines to what is now built, and the "State"
section of CLAUDE.md with the current test counts and format version. Regenerate
the identity manifests on this Mac if the merged changes moved generated output,
and check that every changed line is explained. With --memory-checks built, add a
test that it catches a pointer into a List kept across a growing push. Run every
gate on the merged tree.
EOF
	;;
	lane-sync) cat <<'EOF'
Build the synchronized collections of "Thread-safe collections" in round five, in
anti.collection.sync: SyncList<T>, SyncMap<K, V>, SyncSet<T> and SyncPool<T>, with
the operations of their plain collections less every operation by position and
lend_slice, versioned set on all of them, and read, modify and the functions of
update and remove running inside the lock. Tests with threads for each, including
a versioned set that fails after another thread's change.
EOF
	;;
	lane-concurrent) cat <<'EOF'
Build the concurrent collections of "Thread-safe collections" in round five, in
anti.collection.concurrent: ConcurrentMap<K, V>, split into parts locked apart,
and SpscRing<T, N>, lock-free with one atomic counter per side and marked
unchecked with its reason. Tests with threads: many threads on different keys of a
ConcurrentMap, and an SpscRing passing a long run of values from one thread to
another at full speed with nothing lost or reordered, on every host.
EOF
	;;
	overview-5) cat <<'EOF'
Bring docs/anti-syntax-overview.md fully in line with round five as built: every
section, example and status line of generics, optional values, ownership at a
call, lending, walking, hashing, direct imports and the collections. The
overview_examples test compiles every example; fix every example it finds broken.
EOF
	;;
	esac
}

# lanes: the steps each runs in order, and the extended regex of its fence
LANES1="seq hashed sorted handles shared memcheck"
LANES2="sync concurrent"

lane_steps() { echo "lane-$1"; }

lane_fence() {
	local common='^(tests/|docs/reports/|docs/decisions-'"$1"'\.md$'
	case "$1" in
	seq)        echo "$common|src/std/anti/collection/(list|deque|ring|grid)\.)" ;;
	hashed)     echo "$common|src/std/anti/collection/(map|set)\.)" ;;
	sorted)     echo "$common|src/std/anti/collection/sorted\.)" ;;
	handles)    echo "$common|src/std/anti/collection/(pool|tree|queue)\.)" ;;
	shared)     echo "$common|src/std/anti/mem)" ;;
	memcheck)   echo "$common|src/antic/|src/anti/|src/rt/|tools/|CMakeLists\.txt$)" ;;
	sync)       echo "$common|src/std/anti/collection/sync\.)" ;;
	concurrent) echo "$common|src/std/anti/collection/concurrent\.)" ;;
	esac
}

SEQ0="apply-5"
SEQ1="generics-syntax generics-constraints generics-copies generics-libraries generics-features optional-values own-and-lent walking hash-ordered direct-imports collection-core"
SEQ3="reconcile"
SEQ5="reconcile-final overview-5"

# --- state -----------------------------------------------------------------------

is_done() { grep -qxF "$1" "$STATE_FILE"; }
mark_done() { echo "$1" >> "$STATE_FILE"; }

list_steps() {
	local id l
	for id in $SEQ0 $SEQ1; do show "$id"; done
	for l in $LANES1; do for id in $(lane_steps "$l"); do show "$id" "$l"; done; done
	show "merge-1"
	for id in $SEQ3; do show "$id"; done
	for l in $LANES2; do for id in $(lane_steps "$l"); do show "$id" "$l"; done; done
	show "merge-2"
	for id in $SEQ5; do show "$id"; done
}

show() {
	local tag="${2:+ ($2)}"
	if is_done "$1"; then printf "  [done]    %s%s\n" "$1" "$tag"
	else printf "  [pending] %s%s\n" "$1" "$tag"; fi
}

# --- running one session ---------------------------------------------------

run_claude() {
	local id="$1" dir="$2" prompt="$3"
	local json="$LOG_DIR/add5-$id.out.json"
	local out="$LOG_DIR/add5-$id.out"
	echo "=== $id : starting $(date -u +%FT%TZ) ==="
	( cd "$dir" && printf '%s\n' "$prompt" | \
		claude -p \
			--permission-mode auto \
			--max-turns "$MAX_TURNS" \
			--output-format json ) > "$json" 2>&1
	jq -r '.result // empty' "$json" > "$out" 2>/dev/null || cp "$json" "$out"

	local turns cost
	turns="$(jq -r '.num_turns // "?"' "$json" 2>/dev/null)"
	cost="$(jq -r '.total_cost_usd // "?"' "$json" 2>/dev/null)"
	echo "--- $id : $turns turns, \$$cost"

	if [ "$(jq -r '.subtype // empty' "$json" 2>/dev/null)" = "error_max_turns" ]; then
		echo "=== $id : hit the turn cap at $turns turns; its committed parts are kept ==="
		return 1
	fi
	local last
	last="$(grep -v '^[[:space:]]*$' "$out" | tail -n 1 | tr -d '\r')"
	echo "--- $id : last line: $last"
	[ "$last" = "DONE" ]
}

run_main_step() {
	local id="$1"
	if is_done "$id"; then
		echo "=== $id : already done, skipping ==="
		return 0
	fi
	local task
	if [ "$id" = "reconcile-final" ]; then task="$(step_prompt reconcile)"
	else task="$(step_prompt "$id")"; fi
	if ! run_claude "$id" "$ROOT" "$task
$GATES_COMMON
$GATES_MAIN"; then
		echo "=== $id : stopped. See $LOG_DIR/add5-$id.out and docs/reports/. ==="
		return 1
	fi
	if [ -n "$(git -C "$ROOT" status --porcelain)" ]; then
		echo "=== $id : left the main tree dirty; stopping so you can look ==="
		return 1
	fi
	mark_done "$id"
	echo "=== $id : done ==="
}

# --- lanes -------------------------------------------------------------------------

lane_done() {
	local l="$1" id
	for id in $(lane_steps "$l"); do is_done "$id" || return 1; done
	return 0
}

prepare_lane() {
	local l="$1" base="$2"
	local wt="$WT_DIR/add5-$l" br="add5/$l"
	if [ -d "$wt" ] && git -C "$wt" rev-parse --abbrev-ref HEAD 2>/dev/null | grep -qx "$br"; then
		:
	else
		git worktree prune
		git branch -D "$br" >/dev/null 2>&1 || true
		git worktree add -q -b "$br" "$wt" "$base"
		echo "$base" > "$STATUS_DIR/base-$l"
	fi
	# the worktree's build/deps is the main checkout's, so nothing is fetched
	# again and every tool that looks in build/deps finds the pinned ones
	mkdir -p "$wt/build"
	[ -e "$wt/build/deps" ] || ln -s "$ROOT/build/deps" "$wt/build/deps"
}

# run_lane <lane> : its steps in order, in its worktree; writes DONE or the
# reason to the lane's status file
run_lane() {
	local l="$1"
	local wt="$WT_DIR/add5-$l"
	local base fence id notice gates
	base="$(cat "$STATUS_DIR/base-$l")"
	fence="$(lane_fence "$l")"
	notice="${WORKTREE_NOTICE//MAIN_DEPS/$ROOT/build/deps}"
	gates="${GATES_LANE//LANE/$l}"
	for id in $(lane_steps "$l"); do
		if is_done "$id"; then
			echo "=== $id : already done, skipping ==="
			continue
		fi
		if ! run_claude "$id" "$wt" "$notice
$(step_prompt "$id")
$GATES_COMMON
$gates"; then
			echo "STOPPED at $id" > "$STATUS_DIR/lane-$l"
			return 1
		fi
		if [ -n "$(git -C "$wt" status --porcelain)" ]; then
			echo "DIRTY after $id" > "$STATUS_DIR/lane-$l"
			return 1
		fi
		local outside
		outside="$(git -C "$wt" diff --name-only "$base" HEAD | grep -v -E "$fence" || true)"
		if [ -n "$outside" ]; then
			echo "$outside" > "$STATUS_DIR/outside-$l"
			echo "OUTSIDE after $id; see $STATUS_DIR/outside-$l" > "$STATUS_DIR/lane-$l"
			return 1
		fi
		mark_done "$id"
		echo "=== $id : done in lane $l ==="
	done
	echo "DONE" > "$STATUS_DIR/lane-$l"
}

# run_lanes "<lanes>" : runs the pending lanes, at most MAX_PARALLEL at once,
# polling, since bash 3.2 has no wait -n
run_lanes() {
	local lanes="$1" l base
	base="$(git rev-parse HEAD)"
	local queue="" running="" pids=""
	for l in $lanes; do
		lane_done "$l" && continue
		[ -f "$STATUS_DIR/base-$l" ] || echo "$base" > "$STATUS_DIR/base-$l"
		prepare_lane "$l" "$(cat "$STATUS_DIR/base-$l")"
		rm -f "$STATUS_DIR/lane-$l"
		queue="$queue $l"
	done
	set -- $queue
	while [ $# -gt 0 ] || [ -n "$running" ]; do
		while [ $# -gt 0 ] && [ "$(echo $running | wc -w)" -lt "$MAX_PARALLEL" ]; do
			l="$1"; shift
			run_lane "$l" > "$LOG_DIR/add5-lane-$l.log" 2>&1 &
			running="$running $l:$!"
			echo "=== lane $l : started; progress in $LOG_DIR/add5-lane-$l.log ==="
		done
		sleep "$POLL"
		local still="" entry lane pid
		for entry in $running; do
			lane="${entry%%:*}"; pid="${entry##*:}"
			if kill -0 "$pid" 2>/dev/null; then
				still="$still $entry"
			else
				wait "$pid" 2>/dev/null
				echo "=== lane $lane : finished: $(cat "$STATUS_DIR/lane-$lane" 2>/dev/null || echo UNKNOWN) ==="
			fi
		done
		running="$still"
	done
}

# merge_lanes "<lanes>" <merge-id> : brings every finished lane onto main in order
merge_lanes() {
	local lanes="$1" mid="$2" l failed=0 merged=0
	if is_done "$mid"; then
		echo "=== $mid : already done, skipping ==="
		return 0
	fi
	for l in $lanes; do
		if ! lane_done "$l"; then
			echo "=== lane $l : not finished ($(cat "$STATUS_DIR/lane-$l" 2>/dev/null || echo UNKNOWN));"
			echo "    its worktree stays at $WT_DIR/add5-$l ==="
			failed=1
			continue
		fi
		[ -d "$WT_DIR/add5-$l" ] || continue
		local base
		base="$(cat "$STATUS_DIR/base-$l")"
		if git cherry-pick "$base..add5/$l" >/dev/null 2>&1; then
			git worktree remove --force "$WT_DIR/add5-$l" >/dev/null 2>&1 || true
			git branch -D "add5/$l" >/dev/null 2>&1 || true
			merged=$((merged + 1))
			echo "=== lane $l : brought onto main ==="
		else
			git cherry-pick --abort >/dev/null 2>&1 || true
			echo "=== lane $l : branch add5/$l does not apply cleanly onto main. It stays, with"
			echo "    its worktree, for a session to bring over by hand; then rerun. ==="
			failed=1
			break
		fi
	done
	if [ "$merged" -gt 0 ] && ! git push -q; then
		echo "=== $mid : the push failed; the lanes are on local main ==="
		return 1
	fi
	[ "$failed" -eq 0 ] || return 1
	mark_done "$mid"
	echo "=== $mid : done ==="
}

# --- argument handling ------------------------------------------------------

if [ "${1-}" = "--list" ]; then
	list_steps
	exit 0
fi

if [ "${1-}" = "--redo" ]; then
	redo="${2-}"
	[ -n "$redo" ] || { echo "usage: $0 --redo <id>"; exit 2; }
	grep -vxF "$redo" "$STATE_FILE" > "$STATE_FILE.tmp" || true
	mv "$STATE_FILE.tmp" "$STATE_FILE"
	echo "cleared $redo; it will run on the next plain invocation"
	exit 0
fi

command -v claude >/dev/null || { echo "claude CLI not on PATH"; exit 2; }
command -v jq >/dev/null || { echo "jq not on PATH (brew install jq)"; exit 2; }
[ -f CLAUDE.md ] || { echo "run from the antic repository root"; exit 2; }
if ! is_done apply-5; then
	git ls-files --error-unmatch docs/anti-language-additions-5.md >/dev/null 2>&1 || {
		echo "docs/anti-language-additions-5.md is not committed; commit it first"; exit 2; }
fi
if [ -n "$(git status --porcelain)" ]; then
	echo "tree is not clean; commit or stash first"; exit 2
fi

stop() { echo; echo "Stopped. Resolve, then run $0 again; finished steps are skipped."; exit 1; }

for id in $SEQ0 $SEQ1; do run_main_step "$id" || stop; done

run_lanes "$LANES1"
merge_lanes "$LANES1" merge-1 || stop

for id in $SEQ3; do run_main_step "$id" || stop; done

run_lanes "$LANES2"
merge_lanes "$LANES2" merge-2 || stop

for id in $SEQ5; do run_main_step "$id" || stop; done

echo
echo "Round five is built. Review its [provisional] entries, then the VM run."
