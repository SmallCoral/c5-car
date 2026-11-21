"""在 M2.8 连接合同上应用已核对的模块移除及网络合并，作为独立于现行网表的检查基准。"""
from m28_contract import intended as m28_intended

REMOVED = set('''U13 Q4 U14 Q5 J27 R68 R69 R70 R71 R76 R77 R78 R80
 C79 C73 C77 U15 Q6 Q7 Q8 Q9 D5 F2 R81 R82 R83 R84 R85 R86 R87 R88
 R89 R90 C80 Q3 R74 R75 C76'''.split())

MERGES = [
    ('U10:4', 'U10:3'),                 # ENUV 连到该芯片的 VIN。
    ('U2:9', 'U10:8'),                  # 闲置的 OVP 接地以禁用此功能。
    ('U10:5', 'J19:2'),                 # 取消独立的理想二极管级。
    ('J14:B34', 'J19:6'),               # Core 的 USART6 发送端接 Maix 接收端。
    ('J14:B35', 'J19:5'),               # Maix 发送端接 Core 的 USART6 接收端。
    ('U2:11', 'J23:1'),                 # 步进驱动取开关后的主电源 VM。
    ('J14:A30', 'J23:7'),               # USART1 发送端接驱动接收端。
    ('J14:A29', 'J23:8'),               # 驱动发送端接 USART1 接收端。
    ('U2:9', 'U11:1'),                  # 两路电平转换器的使能保持低电平。
]

# 以已保存的电路为基础，按批准的删除与合并规则推导各网络端点集合。
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
