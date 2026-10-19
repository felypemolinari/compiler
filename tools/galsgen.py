#!/usr/bin/env python3
"""Regenerates the SLR parser tables of src/gals/Constants.{h,cpp}.

GALS itself is a desktop tool, so this script stands in for it when only the
#Grammar section changed -- in particular when semantic actions (#N) are added
or moved.  It emits exactly the tables the generated src/gals/Sintatico.cpp
expects, and it leaves the scanner tables in Constants.cpp untouched.

Run it from the repository root after editing grammar/compiler.gals:

    python3 tools/galsgen.py

Use --check to verify the tables on disk match the grammar without writing.

Table layout expected by Sintatico.cpp
--------------------------------------
Symbols are numbered 1..N: 1 is $, then the tokens in #Tokens order, then the
non terminals in #NonTerminals order.  A symbol s lives in column s-1, so the
real symbols occupy columns 0..N-1.  Semantic actions are different: the parser
reads action k at `FIRST_SEMANTIC_ACTION + k - 1`, used directly as a column.
With FIRST_SEMANTIC_ACTION = N + 1 that puts action k in column N + k, leaving
column N-1... in fact leaving one unused column between the two groups -- the
same hole GALS leaves, which is why a grammar with no actions still declares
99 columns for 98 symbols.
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GRAMMAR = ROOT / "grammar" / "compiler.gals"
CONSTANTS_H = ROOT / "src" / "gals" / "Constants.h"
CONSTANTS_CPP = ROOT / "src" / "gals" / "Constants.cpp"

SHIFT, REDUCE, ACTION, ACCEPT, GO_TO, ERROR = range(6)
CMD_NAMES = {SHIFT: "SHIFT ", REDUCE: "REDUCE", ACTION: "ACTION",
             ACCEPT: "ACCEPT", GO_TO: "GO_TO ", ERROR: "ERROR "}


# ---------------------------------------------------------------------------
# Grammar file
# ---------------------------------------------------------------------------

def read_sections(text):
    """Splits a .gals file into {section name: list of non empty lines}."""
    sections, current = {}, None
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("#"):
            current = line[1:].strip()
            sections[current] = []
        elif line and current is not None:
            sections[current].append(line)
    return sections


def parse_tokens(lines):
    """Token names, in declaration order -- that is the order GALS numbers them.

    Accepts both forms used in #Tokens: `name : regexp` and `name = base :
    "lexeme"`.  Lines starting with ':' define tokens that are discarded
    (comments, blanks) and get no number.
    """
    names = []
    for line in lines:
        if line.startswith(":"):
            continue
        name = re.split(r"[:=]", line, maxsplit=1)[0].strip()
        if name and name not in names:
            names.append(name)
    return names


def parse_productions(lines, nonterminals):
    """Returns [(lhs, [rhs symbols])], actions as the string '#N'.

    Productions span several lines and are separated by ';'.
    """
    text = " ".join(lines)
    productions = []
    for rule in text.split(";"):
        rule = rule.strip()
        if not rule:
            continue
        lhs, rhs = rule.split("::=", 1)
        lhs = lhs.strip()
        if lhs not in nonterminals:
            raise SystemExit(f"unknown non terminal on the left of ::= : {lhs}")
        for alternative in rhs.split("|"):
            symbols = alternative.split()
            productions.append((lhs, symbols))
    return productions


class Grammar:
    def __init__(self, path):
        sections = read_sections(path.read_text(encoding="utf-8"))
        self.tokens = parse_tokens(sections["Tokens"])
        self.nonterminals = [n.strip() for n in sections["NonTerminals"]]
        self.productions = parse_productions(sections["Grammar"], set(self.nonterminals))

        # 1 is $, tokens follow, then non terminals.
        self.number = {"$": 1}
        for i, name in enumerate(self.tokens):
            self.number[name] = 2 + i
        for i, name in enumerate(self.nonterminals):
            self.number[name] = 2 + len(self.tokens) + i
        self.symbol_count = 1 + len(self.tokens) + len(self.nonterminals)

        self.start = self.productions[0][0]
        self.actions = sorted({int(s[1:]) for _, rhs in self.productions
                               for s in rhs if s.startswith("#")})
        self._validate()
        self._compute_first()
        self._compute_follow()

    # -- helpers ------------------------------------------------------------

    def is_action(self, symbol):
        return symbol.startswith("#")

    def is_terminal(self, symbol):
        return symbol in self.number and symbol not in self.by_lhs

    def _validate(self):
        self.by_lhs = {}
        for index, (lhs, rhs) in enumerate(self.productions):
            self.by_lhs.setdefault(lhs, []).append(index)
            for symbol in rhs:
                if not self.is_action(symbol) and symbol not in self.number:
                    raise SystemExit(f"undefined symbol '{symbol}' in rule for <{lhs}>")
        missing = [n for n in self.nonterminals if n not in self.by_lhs]
        if missing:
            raise SystemExit(f"non terminals with no production: {missing}")
        if self.actions and self.actions != list(range(1, len(self.actions) + 1)):
            raise SystemExit(f"semantic actions must be numbered 1..N, got {self.actions}")

    # -- FIRST / FOLLOW -----------------------------------------------------
    #
    # Semantic actions derive nothing, so both computations skip them.

    def first_of_sequence(self, symbols):
        result, nullable = set(), True
        for symbol in symbols:
            if self.is_action(symbol):
                continue
            if symbol not in self.by_lhs:          # terminal
                result.add(symbol)
                nullable = False
                break
            result |= self.first[symbol] - {None}
            if None not in self.first[symbol]:
                nullable = False
                break
        if nullable:
            result.add(None)
        return result

    def _compute_first(self):
        self.first = {n: set() for n in self.nonterminals}
        changed = True
        while changed:
            changed = False
            for lhs, rhs in self.productions:
                before = len(self.first[lhs])
                self.first[lhs] |= self.first_of_sequence(rhs)
                changed |= len(self.first[lhs]) != before

    def _compute_follow(self):
        self.follow = {n: set() for n in self.nonterminals}
        self.follow[self.start].add("$")
        changed = True
        while changed:
            changed = False
            for lhs, rhs in self.productions:
                for i, symbol in enumerate(rhs):
                    if symbol not in self.by_lhs:
                        continue
                    tail = self.first_of_sequence(rhs[i + 1:])
                    extra = tail - {None}
                    if None in tail:
                        extra |= self.follow[lhs]
                    before = len(self.follow[symbol])
                    self.follow[symbol] |= extra
                    changed |= len(self.follow[symbol]) != before


# ---------------------------------------------------------------------------
# LR(0) automaton
# ---------------------------------------------------------------------------
#
# An item is (production index, dot) with -1 standing for the augmented
# production S' ::= <start>.  States keep their items in insertion order
# because that order decides the numbering GALS gives the states: a state's
# successors are created in the order their symbols are first seen while
# walking the item list.

def closure(grammar, kernel):
    items = list(kernel)
    seen = set(kernel)
    i = 0
    while i < len(items):
        production, dot = items[i]
        i += 1
        rhs = [grammar.start] if production == -1 else grammar.productions[production][1]
        if dot >= len(rhs):
            continue
        symbol = rhs[dot]
        if symbol in grammar.by_lhs:
            for index in grammar.by_lhs[symbol]:
                item = (index, 0)
                if item not in seen:
                    seen.add(item)
                    items.append(item)
    return items


def transitions(grammar, items):
    """Symbols leaving a state, in the order the items mention them."""
    order, targets = [], {}
    for production, dot in items:
        rhs = [grammar.start] if production == -1 else grammar.productions[production][1]
        if dot >= len(rhs):
            continue
        symbol = rhs[dot]
        if symbol not in targets:
            order.append(symbol)
            targets[symbol] = []
        targets[symbol].append((production, dot + 1))
    return [(symbol, targets[symbol]) for symbol in order]


def build_automaton(grammar):
    start = closure(grammar, [(-1, 0)])
    states = [start]
    index_of = {tuple(sorted(start)): 0}
    gotos = []
    i = 0
    while i < len(states):
        state_gotos = {}
        for symbol, kernel in transitions(grammar, states[i]):
            target = closure(grammar, kernel)
            key = tuple(sorted(target))
            if key not in index_of:
                index_of[key] = len(states)
                states.append(target)
            state_gotos[symbol] = index_of[key]
        gotos.append(state_gotos)
        i += 1
    return states, gotos


def action_lookaheads(grammar, items, action_symbol):
    """Tokens that may follow `action_symbol` where this state can run it.

    The action fires with the next input token already read, so the ACTION
    entry has to sit in that token's column -- which is exactly the SLR
    lookahead set of the items whose dot is on the action.
    """
    lookaheads = set()
    for production, dot in items:
        if production == -1:
            continue
        lhs, rhs = grammar.productions[production]
        if dot >= len(rhs) or rhs[dot] != action_symbol:
            continue
        tail = grammar.first_of_sequence(rhs[dot + 1:])
        lookaheads |= tail - {None}
        if None in tail:
            lookaheads |= grammar.follow[lhs]
    return lookaheads


def build_table(grammar):
    states, gotos = build_automaton(grammar)
    width = grammar.symbol_count + 1 + len(grammar.actions)
    table = [[[ERROR, 0] for _ in range(width)] for _ in states]
    conflicts = []

    def place(state, column, entry, what):
        current = table[state][column]
        if current[0] != ERROR and current != entry:
            conflicts.append(f"state {state}, column {column}: "
                             f"{CMD_NAMES[current[0]].strip()} {current[1]} vs {what}")
            return
        table[state][column] = entry

    for state, items in enumerate(states):
        for symbol, target in gotos[state].items():
            if grammar.is_action(symbol):
                number = int(symbol[1:])
                # Goto on the action symbol, read straight from its column.
                table[state][grammar.symbol_count + number] = [GO_TO, target]
                for token in action_lookaheads(grammar, items, symbol):
                    place(state, grammar.number[token] - 1, [ACTION, number],
                          f"ACTION {number}")
            elif symbol in grammar.by_lhs:
                table[state][grammar.number[symbol] - 1] = [GO_TO, target]
            else:
                place(state, grammar.number[symbol] - 1, [SHIFT, target],
                      f"SHIFT {target}")

        for production, dot in items:
            if production == -1:
                if dot == 1:
                    table[state][0] = [ACCEPT, 0]
                continue
            lhs, rhs = grammar.productions[production]
            if dot == len(rhs):
                for token in grammar.follow[lhs]:
                    place(state, grammar.number[token] - 1, [REDUCE, production],
                          f"REDUCE {production}")

    if conflicts:
        raise SystemExit("grammar is not SLR(1):\n  " + "\n  ".join(conflicts))

    return table, width


# ---------------------------------------------------------------------------
# Emitting Constants.h / Constants.cpp
# ---------------------------------------------------------------------------

def render_table(table, width):
    lines = [f"const int PARSER_TABLE[{len(table)}][{width}][2] =", "{"]
    for i, row in enumerate(table):
        cells = ", ".join(f"{{{CMD_NAMES[cmd]}, {value:3d}}}" for cmd, value in row)
        comma = "" if i == len(table) - 1 else ","
        lines.append(f"    {{ {cells} }}{comma}")
    lines.append("};")
    return "\n".join(lines)


def render_productions(grammar):
    lines = [f"const int PRODUCTIONS[{len(grammar.productions)}][2] =", "{"]
    for i, (lhs, rhs) in enumerate(grammar.productions):
        comma = "" if i == len(grammar.productions) - 1 else ","
        # The length counts semantic actions: the parser pushes a state for
        # each one, so the reduce has to pop them back off.
        lines.append(f"    {{ {grammar.number[lhs]}, {len(rhs)} }}{comma}")
    lines.append("};")
    return "\n".join(lines)


def render_errors(count):
    lines = [f"const char *PARSER_ERROR[{count}] =", "{"]
    for i in range(count):
        comma = "" if i == count - 1 else ","
        lines.append(f'    "Erro estado {i}"{comma}')
    lines.append("};")
    return "\n".join(lines)


def patch(text, marker, replacement):
    """Replaces the `const ... marker[...] = { ... };` block."""
    start = text.index(marker)
    end = text.index("\n};", start) + len("\n};")
    return text[:start] + replacement + text[end:]


def generate(grammar):
    table, width = build_table(grammar)

    cpp = CONSTANTS_CPP.read_text(encoding="utf-8")
    cpp = patch(cpp, "const int PARSER_TABLE", render_table(table, width))
    cpp = patch(cpp, "const int PRODUCTIONS", render_productions(grammar))
    cpp = patch(cpp, "const char *PARSER_ERROR", render_errors(len(table)))

    header = CONSTANTS_H.read_text(encoding="utf-8")
    header = re.sub(r"const int FIRST_SEMANTIC_ACTION = \d+;",
                    f"const int FIRST_SEMANTIC_ACTION = {grammar.symbol_count + 1};",
                    header)
    header = re.sub(r"extern const int PARSER_TABLE\[\d+\]\[\d+\]\[2\];",
                    f"extern const int PARSER_TABLE[{len(table)}][{width}][2];",
                    header)
    header = re.sub(r"extern const int PRODUCTIONS\[\d+\]\[2\];",
                    f"extern const int PRODUCTIONS[{len(grammar.productions)}][2];",
                    header)
    header = re.sub(r"extern const char \*PARSER_ERROR\[\d+\];",
                    f"extern const char *PARSER_ERROR[{len(table)}];",
                    header)
    return header, cpp, len(table)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="compare with the files on disk instead of writing")
    args = parser.parse_args()

    grammar = Grammar(GRAMMAR)
    header, cpp, states = generate(grammar)

    if args.check:
        same = (header == CONSTANTS_H.read_text(encoding="utf-8")
                and cpp == CONSTANTS_CPP.read_text(encoding="utf-8"))
        print("up to date" if same else "OUT OF DATE: run tools/galsgen.py")
        return 0 if same else 1

    CONSTANTS_H.write_text(header, encoding="utf-8")
    CONSTANTS_CPP.write_text(cpp, encoding="utf-8")
    print(f"{len(grammar.productions)} productions, {states} states, "
          f"{len(grammar.actions)} semantic actions")
    return 0


if __name__ == "__main__":
    sys.exit(main())
