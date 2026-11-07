"""Independent M2.8 connectivity intent applied to the saved M2.7 live netlist.

The lists below come from the handoff requirements and manufacturer pinouts.
They deliberately do not read the current schematic or its exported netlist.
"""

PIN_MAP = {'U1:1':'U1:3','U1:2':'U1:5','U1:3':'U1:7','U1:6':'U1:8',
           'U1:7':'U1:1','U1:8':'U1:2','U1:9':'U1:9','U1:10':'U1:4'}
REMOVED_PINS = {'U1:4','U1:5'}  # old unused PG/SS; no corresponding AP64500 pins
DETACH = {'C63:1','C63:2','J14:B26','J14:B27'}
B_LOADS = {'C72:1','C78:1','C81:1','C82:1','J27:1','R70:1','U10:4',
           'U16:2','U16:3','U16:4','U16:5','U16:6','U16:7','U16:8'}
ADDED_PHYSICAL = set('C74 C75 C84 C85 C86 C87 C88 C89 C90 C91 C92 C93 C94 J18 J25 L2 Q3 R61 R62 R63 R64 R65 R66 R67 R73 R74 R75 R79 R93 U6 U8 U11 U12'.split())

def intended(before):
    parts=set()
    old_pins={p for group in before for p in group}
    if not REMOVED_PINS|DETACH <= old_pins:
        raise ValueError('M2.7 input is missing a required source pin')
    for group in before:
        retained=frozenset(PIN_MAP.get(p,p) for p in group if p not in REMOVED_PINS|DETACH)
        if retained:parts.add(retained)
    def take(anchor):
        matches=[g for g in parts if anchor in g]
        if len(matches)!=1:raise ValueError('Missing/duplicate intent anchor '+anchor)
        parts.remove(matches[0]);return set(matches[0])
    def augment(anchor,add):parts.add(frozenset(take(anchor)|set(add.split())))
    # Split existing raw 5V: retain Lichee/Core/gray/LDO on A; move only the
    # reviewed Maix and Action4 input nodes to the new B converter.
    rail_a=take('L1:2')
    if not B_LOADS <= rail_a:raise ValueError('B consumers do not originate on old raw 5V')
    rail_a-=B_LOADS
    rail_a.update('C92:1 C93:1 U8:2 U8:3 U8:4 U8:5 U8:6 U8:7 U8:8'.split())
    parts.add(frozenset(rail_a))
    parts.add(frozenset(B_LOADS|set('C88:1 L2:2 R64:1'.split())))
    augment('U2:11','C84:1 C85:1 C86:1 R61:1 U6:2')
    augment('J14:A47','C74:1 C75:1 R73:1 R74:1 R79:1 U11:5 U12:5')
    augment('U2:9','''C63:2 C74:2 C75:2 C84:2 C85:2 C86:2 C88:2 C89:2
        C90:2 C91:2 C92:2 C93:2 C94:2 J18:3 J18:4 J25:A12 J25:B12 J25:SH
        Q3:2 R62:2 R63:2 R65:2 R75:2 U6:7 U6:9 U8:12 U8:21 U11:3 U12:3''')
    new_nets=[
        'U1:6 R67:1 C63:1','R67:2 C91:1',
        'U6:3 R61:2 R62:1','U6:4 R63:1','U6:1 C87:1',
        'U6:8 C87:2 L2:1','U6:5 R64:2 R65:1',
        'U6:6 R66:1 C89:1','R66:2 C90:1',
        'U8:10 R93:1','U8:9 R93:2',
        'U8:14 U8:15 J25:A9 J25:B9 C94:1',
        'U8:11 J25:A5','U8:13 J25:B5',
        'J14:B26 R73:2 U12:2','J14:B27 R79:2 U11:4',
        'J18:11 U12:4','J18:12 U11:2',
        'J18:2 Q3:1 R75:1','Q3:3 R74:2 U11:1 U12:1',
    ]
    new_nets += ['U8:'+p for p in ['1','16','17','18','19','20']]
    new_nets += ['J18:'+p for p in ['1','5','6','7','8','9','10']]
    parts.update(frozenset(n.split()) for n in new_nets)
    flat=[p for group in parts for p in group]
    if len(flat)!=len(set(flat)):raise ValueError('Intent assigns one pin to multiple nets')
    return parts

PRIMARY_PINOUTS={
    'AP64500':{'1':'BST','2':'VIN','3':'EN','4':'RT_CLK','5':'FB','6':'COMP','7':'GND','8':'SW','9':'EP_GND'},
    'SN74LV1T125':{'1':'nOE','2':'A','3':'GND','4':'Y','5':'VCC'},
    'SN74LVC1G125':{'1':'nOE','2':'A','3':'GND','4':'Y','5':'VCC'},
    'TPS25810':{'1':'nFAULT','2':'IN1','3':'IN1','4':'IN2','5':'AUX','6':'EN','7':'CHG','8':'CHG_HI','9':'REF_RTN','10':'REF','11':'CC1','12':'GND','13':'CC2','14':'OUT','15':'OUT','16':'nDEBUG','17':'nAUDIO','18':'nPOL','19':'nUFP','20':'nLD_DET','21':'EP_GND'},
    'Lichee_header':{'1':'5V_NC','2':'3V3_SENSE','3':'GND','4':'GND','5':'SPI_CLK_NC','6':'SPI_SI_NC','7':'SPI_CS_NC','8':'SPI_SO_NC','9':'UART0_RX_NC','10':'UART0_TX_NC','11':'UART1_RX_3V3','12':'UART1_TX_1V8'},
}
