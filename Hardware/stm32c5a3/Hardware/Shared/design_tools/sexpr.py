"""Small KiCad s-expression reader/writer. Does not access board files."""
import re
import json

class Atom(str):
    pass

def parse(text):
    tokens = re.findall(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+', text)
    root, stack = [], []
    cur = root
    for token in tokens:
        if token == '(':
            child = []
            cur.append(child)
            stack.append(cur)
            cur = child
        elif token == ')':
            cur = stack.pop()
        elif token.startswith('"'):
            cur.append(json.loads(token))
        else:
            cur.append(Atom(token))
    assert not stack
    return root[0]

def dump(obj, level=0):
    if not isinstance(obj, list):
        return str(obj) if isinstance(obj, Atom) else json.dumps(obj, ensure_ascii=False)
    if all(not isinstance(x, list) for x in obj):
        return '(' + ' '.join(dump(x) for x in obj) + ')'
    parts=[]
    for x in obj:
        if isinstance(x, list):
            parts.append('\n' + '\t'*(level+1) + dump(x, level+1))
        else:
            parts.append((' ' if parts else '') + dump(x))
    return '(' + ''.join(parts) + '\n' + '\t'*level + ')'

def node(obj, key, default=None):
    return next((x for x in obj if isinstance(x,list) and x and x[0]==key),default)

def nodes(obj,key):
    return [x for x in obj if isinstance(x,list) and x and x[0]==key]

def prop(obj,key):
    return next((x for x in nodes(obj,'property') if x[1]==key),None)
