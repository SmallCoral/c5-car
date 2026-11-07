"""Normalize native-save ordering and explicit defaults; retain electrical data."""
def canonical(value):
    if isinstance(value,dict):return {k:canonical(v) for k,v in value.items()}
    if not isinstance(value,list):return value
    if not value:return []
    if value in [['duplicate_pin_numbers_are_jumpers','no'],['embedded_fonts','no'],['exclude_from_sim','no'],['in_pos_files','yes'],['show_name','no'],['do_not_autoplace','no']]:return None
    if value[:3]==['property','Description','']:return None
    result=[v for x in value if (v:=canonical(x)) is not None]
    if value[0]=='property':
        hidden=['hide','yes'] in result
        for item in result[3:]:
            if isinstance(item,list) and item and item[0]=='effects' and ['hide','yes'] in item:
                item.remove(['hide','yes']);hidden=True
        result=[x for x in result if x!=['hide','yes']]
        if hidden:result.append(['hide','yes'])
        return result[:3]+sorted(result[3:],key=repr)
    if value[0]=='symbol':return result[:2]+sorted(result[2:],key=repr)
    return result
