"""Reviewed simplified circuit intent, composed from the retained M2.8 contract.

Only approved module removals and explicit merges are applied. This module does
not inspect current schematics or current netlists to manufacture expectations.
"""
from m28_contract import intended as m28_intended

REMOVED = set('''U13 Q4 U14 Q5 J27 R68 R69 R70 R71 R76 R77 R78 R80
 C79 C73 C77 U15 Q6 Q7 Q8 Q9 D5 F2 R81 R82 R83 R84 R85 R86 R87 R88
 R89 R90 C80 Q3 R74 R75 C76'''.split())

MERGES = [
    ('U10:4', 'U10:3'),                 # ENUV tied to its VIN
    ('U2:9', 'U10:8'),                  # unused OVP disabled at GND
    ('U10:5', 'J19:2'),                 # no separate ideal-diode stage
    ('J14:B34', 'J19:6'),               # Core USART6 TX -> Maix RX
    ('J14:B35', 'J19:5'),               # Maix TX -> Core USART6 RX
    ('U2:11', 'J23:1'),                 # stepper gets switched main VM
    ('J14:A30', 'J23:7'),               # USART1 TX -> driver RX
    ('J14:A29', 'J23:8'),               # driver TX -> USART1 RX
    ('U2:9', 'U11:1'),                  # both translator enables stay low
]

def intended(before):
    parts = {frozenset(p for p in g if p.split(':', 1)[0] not in REMOVED)
             for g in m28_intended(before)}
    parts.discard(frozenset())
    for a,b in MERGES:
        matches = [g for g in parts if a in g or b in g]
        if len(matches) != 2:
            raise ValueError('Expected two independent nets before merge: '+a+' '+b)
        for g in matches: parts.remove(g)
        parts.add(frozenset().union(*matches))
    flat = [p for g in parts for p in g]
    if len(flat) != len(set(flat)):
        raise ValueError('Simplification intent assigns a pin more than once')
    return parts
