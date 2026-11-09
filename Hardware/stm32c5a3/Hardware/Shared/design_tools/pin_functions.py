"""Extract function names and AF column numbers from DS15137 Rev3 Table 14."""
import re

def parse_alternate_functions(source):
 table=source.split('4.3                     Alternate functions',1)[1].split('===== PDF PAGE 62 =====',1)[0]
 functions={};centers={};port=None
 for line in table.splitlines():
  heads=list(re.finditer(r'\bAF(\d+)\b',line))
  if len(heads)==16:
   centers={int(m.group(1)):(m.start()+m.end())/2 for m in heads};port=None;continue
  m=re.match(r'^\s*(P[A-H]\d+)\s',line)
  if m:port=m.group(1)
  elif not line.strip():port=None
  if not port or not centers:continue
  for token in re.finditer(r'\b(?:ETH1_[A-Z0-9_]+|SPI[123]_[A-Z0-9_]+|(?:USART[1236]|UART[457]|LPUART1)_[A-Z0-9_]+|TIM\d+_CH[1-4]N?)\b',line):
   mid=(token.start()+token.end())/2
   af=min(centers,key=lambda n:abs(centers[n]-mid))
   functions[(port,token.group(0))]=af
 return functions
