"""读写 KiCad 的 S 表达式，调用者负责提供需要处理的文件。"""
import re
import json

# 区分 KiCad 未加引号的原子与普通文本。
class Atom(str):
    pass

# 将括号表达式解析为嵌套节点。
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

# 把嵌套节点按 S 表达式格式序列化。
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

# 查找指定标签的首个子节点。
def node(obj, key, default=None):
    return next((x for x in obj if isinstance(x,list) and x and x[0]==key),default)

# 返回全部同名子节点。
def nodes(obj,key):
    return [x for x in obj if isinstance(x,list) and x and x[0]==key]

# 按名称查找器件属性字段。
def prop(obj,key):
    return next((x for x in nodes(obj,'property') if x[1]==key),None)
