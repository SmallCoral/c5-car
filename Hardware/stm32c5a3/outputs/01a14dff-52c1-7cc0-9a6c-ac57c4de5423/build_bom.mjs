import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {FileBlob, SpreadsheetFile} from '@oai/artifact-tool';
process.on('uncaughtException',error=>{console.error(error.message);process.exit(1)});
const outputDir = path.dirname(fileURLToPath(import.meta.url));
const template = 'C:/Users/sc/Downloads/立创商城BOM模板.xlsx';
const workbook = await SpreadsheetFile.importXlsx(await FileBlob.load(template));
if(process.argv.includes('--api-help')){
  console.log(workbook.help('worksheet.name',{include:'index,examples,notes',maxChars:2400}).ndjson);
  console.log(JSON.stringify(workbook.worksheets.getItem('Sheet1').tables.items.map(t=>({name:t.name}))));
  process.exit(0);
}
const parts=JSON.parse(await fs.readFile(path.join(outputDir,'selected-parts.json'),'utf8'));
const source=JSON.parse(await fs.readFile(path.join(outputDir,'source-components.json'),'utf8'));
const excluded=new Map([['D3','按要求排除 LED0603 本体'],['D4','按要求排除 LED0603 本体'],['SW1','按要求排除按钮'],['SW2','按要求排除按钮'],['SW5','按要求排除按钮'],['U1','按要求排除 STM32 芯片'],['J2','按确认，仅排除 USB 连接器'],['J13','PCB 自带 98P 金手指，无需另购器件']]);
const refs=parts.flatMap(p=>p.refs);
if(source.length!==43||parts.length!==18||refs.length!==35||new Set(refs).size!==35)throw new Error('BOM counts mismatch');
for(const s of source)if(!refs.includes(s.ref)&&!excluded.has(s.ref))throw new Error(`Unaccounted ${s.ref}`);
for(const ref of refs)if(!source.some(s=>s.ref===ref)||excluded.has(ref))throw new Error(`Invalid selected ${ref}`);

const bom=workbook.worksheets.getItem('Sheet1');
const buying=workbook.worksheets.getItem('Sheet2');
const scope=workbook.worksheets.getItem('Sheet3');
for(const table of bom.tables.items)table.delete();
bom.unmergeCells('A7:J7');bom.getRange('A2:J7').clear({applyTo:'contents'});
for(let r=3;r<=19;r++)bom.getRange(`A${r}:J${r}`).copyFrom(bom.getRange('A2:J2'),'all');
bom.getRange('A2:J19').values=parts.map((p,i)=>[i+1,p.model,p.pkg,p.spec,p.refs.length,p.brand,p.category,p.code,null,p.refs.join(',')]);
bom.getRange('A1:J19').format.font={name:'Microsoft YaHei',size:11,color:'#142B3E'};
bom.getRange('A1:J1').format.font={name:'Microsoft YaHei',size:11,bold:true,color:'#142B3E'};
bom.getRange('A1:J19').format.wrapText=true;bom.getRange('A1:J19').format.verticalAlignment='center';
bom.getRange('A1:J1').format.rowHeight=32;bom.getRange('A2:J19').format.rowHeight=44;
bom.getRange('A2:A19').setNumberFormat('0');bom.getRange('E2:E19').setNumberFormat('0');
bom.getRange('C2:C14').setNumberFormat('0000');
[6,30,18,48,11,23,24,15,15,39].forEach((w,i)=>bom.getRangeByIndexes(0,i,19,1).format.columnWidth=w);
bom.freezePanes.freezeRows(1);bom.showGridLines=false;bom.tabColor='#2877A0';

const navy='#173D59',pale='#E7F1F7',light='#F5F9FC',text='#17384F',amber='#FFF0CA';
function base(sheet,range){sheet.showGridLines=false;sheet.getRange(range).format.font={name:'Microsoft YaHei',size:11,color:text};sheet.getRange(range).format.verticalAlignment='center';sheet.getRange(range).format.wrapText=true;}
function title(sheet,range,label){sheet.mergeCells(range);sheet.getRange(range).values=[[label]];sheet.getRange(range).format.fill=navy;sheet.getRange(range).format.font={name:'Microsoft YaHei',size:17,bold:true,color:'#FFFFFF'};sheet.getRange(range).format.rowHeight=36;}
function band(sheet,range,label){sheet.mergeCells(range);sheet.getRange(range).values=[[label]];sheet.getRange(range).format.fill=pale;sheet.getRange(range).format.font={name:'Microsoft YaHei',size:11,bold:true,color:navy};sheet.getRange(range).format.rowHeight=32;}
function header(sheet,range){sheet.getRange(range).format.fill=navy;sheet.getRange(range).format.font={name:'Microsoft YaHei',size:11,bold:true,color:'#FFFFFF'};sheet.getRange(range).format.rowHeight=36;}
function note(sheet,range,value,height=34){sheet.mergeCells(range);sheet.getRange(range).values=[[value]];sheet.getRange(range).format.rowHeight=height;}

base(buying,'A1:M32');title(buying,'A1:M1','CoralC5-Core｜国内渠道采购核验');
note(buying,'A2:M2','核验日期：2026-10-29（北京时间）｜人民币/个｜按 1 块板、无现有库存估算；库存与价格以下单页面为准。',30);
note(buying,'A3:M3','“近期成交”为商城页面展示的订单数，不代表全市场用量排名。优先规格适配、国内现货、常用型号及较低起售量。',30);
note(buying,'A4:M4','注意 U2：自营缺货，下表采用国内合作渠道报价，详情及直达链接见第 28–31 行。注意 C7：已选 10V，ESR 仍需核实。',32);buying.getRange('A4:M4').format.fill=amber;
buying.getRange('A6:M6').values=[['序号','商品编号','位号','单板用量','起售数量','1板采购量','国内库存','起售单价/元','采购小计/元','近期成交','渠道 / 交期','选型与采购说明','商品参数与自营页']];header(buying,'A6:M6');
buying.getRange('A7:M24').values=parts.map((p,i)=>[i+1,p.code,p.refs.join(','),p.refs.length,p.moq,null,p.stock,p.price,null,p.recent,p.channel||'国内自营现货\n最快 4 小时发货',p.note,p.url]);
buying.getRange('F7:F24').formulas=parts.map((_,i)=>[`=MAX(D${i+7},E${i+7})`]);
buying.getRange('I7:I24').formulas=parts.map((_,i)=>[`=F${i+7}*H${i+7}`]);
buying.getRange('A7:M24').format.rowHeight=72;
for(let r=7;r<=24;r++)if(r%2===0)buying.getRange(`A${r}:M${r}`).format.fill=light;
buying.getRange('A7:M24').format.borders={insideHorizontal:{style:'thin',color:'#D6E4EC'}};
buying.getRange('D7:G24').setNumberFormat('#,##0');buying.getRange('H7:H24').setNumberFormat('0.000000');buying.getRange('I7:I25').setNumberFormat('0.00');
buying.getRange('F7:F24').format.fill='#EDF7F2';buying.getRange('I7:I24').format.fill='#EDF7F2';
buying.getRange('M7:M24').format.font={name:'Microsoft YaHei',size:10,color:'#087A56'};
buying.getRange('A9:M9').format.fill=amber;buying.getRange('A22:M22').format.fill=amber;
buying.getRange('A25:C25').merge();buying.getRange('A25').values=[['合计（不含运费、优惠券及焊接）']];
buying.getRange('D25').formulas=[['=SUM(D7:D24)']];buying.getRange('F25').formulas=[['=SUM(F7:F24)']];buying.getRange('I25').formulas=[['=SUM(I7:I24)']];
buying.getRange('A25:M25').format.fill=pale;buying.getRange('A25:M25').format.font={name:'Microsoft YaHei',size:11,bold:true,color:navy};buying.getRange('A25:M25').format.rowHeight=40;
band(buying,'A27:M27','U2｜AP63203WU-7 国内合作库存（与自营条目分开下单）');
note(buying,'A28:M28','自营 C780769：现货 0；页面标价 1 个起售、7.48 元/个。上表采用合作库存 118 个、1 个起售、递增量 1、10.3224 元/个、1–3 个工作日；批次为 2 年内。',42);
note(buying,'A29:M29','合作库存直达链接（下单时确认库存和交期；导入主 BOM 的 C780769 不会自动选中此合作条目）：',28);
note(buying,'A30:M30','https://item.szlcsc.com/details/RzE0MTYz/AP63203WU-7Diodes(%25E7%25BE%258E%25E5%258F%25B0)2%25E5%25B9%25B4%25E5%2586%2585.html',44);buying.getRange('A30:M30').format.font={name:'Microsoft YaHei',size:10,color:'#087A56'};
note(buying,'A31:M31','单价取核验时公开页面第一价格阶梯；R50/R51、D2、Y1 含页面折扣。未叠加满减券、会员价、运费或采购晶。未下单采购。',30);
note(buying,'A32:M32','1 板采购量 = MAX（单板用量，起售数量），用于一块板的采购估算；主 BOM 的单套用量保持实际设计数量。',30);
[6,15,35,11,11,13,16,16,16,13,25,73,48].forEach((w,i)=>buying.getRangeByIndexes(0,i,32,1).format.columnWidth=w);
buying.freezePanes.freezeRows(6);buying.tabColor='#20785C';

base(scope,'A1:F33');title(scope,'A1:F1','CoralC5-Core｜范围与选型说明');
note(scope,'A2:F2','从当前 KiCad 原理图的原生网表提取 43 个元件，排除 8 个位号，保留 35 个器件，归并为 18 条采购物料。数量均为每块板。',40);
note(scope,'A3:F3','来源：CoralC5-Core.kicad_sch 及其 3 张子原理图；商城核验日期为 2026-10-29。工程源文件未修改。',32);
band(scope,'A5:F5','本次排除');scope.getRange('A6:F6').values=[['位号','原图器件','封装 / 形态','数量','处理','原因']];header(scope,'A6:F6');
scope.getRange('A7:F14').values=[...excluded].map(([ref,reason])=>{const s=source.find(s=>s.ref===ref);return [ref,s.value,s.footprint.split(':').at(-1),1,'排除',reason]});
scope.getRange('A7:F14').format.rowHeight=50;for(let r=7;r<=14;r++)if(r%2===0)scope.getRange(`A${r}:F${r}`).format.fill=light;
band(scope,'A16:F16','规格归并与采购差异');scope.getRange('A17:F17').values=[['涉及位号','原图规格','本表采购规格','用量','处理','说明']];header(scope,'A17:F17');
scope.getRange('A18:F24').values=[
 ['C1,C3,C8–C12,C15,C16','100nF；16V/50V；0603','100nF 50V X7R；0603',9,'归并','统一耐压到 50V；容量和焊盘尺寸保持。'],
 ['C6,C14','1µF；16V/10V；0603','1µF 50V X5R；0603',2,'归并','原图未限定介质，选择常用 X5R；耐压提高。'],
 ['C2','10µF 16V X7R；1206','10µF 25V X7R；1206',1,'提高耐压','保持 1206 及 X7R，选择有现货、20 个起售的型号。'],
 ['C4,C5','22µF 10V X7R；1206','22µF 16V X7R；1206',2,'提高耐压','保持 1206 及 X7R；国产华瓷现货、5 个起售。'],
 ['C7','2.2µF 6.3V X7R；0603','2.2µF 10V X7R；0603',1,'待核实 ESR','ST 要求 VCAP 额定耐压≥10V，ESR<20mΩ@3MHz；本表满足名义容量、耐压、介质、封装，ESR 曲线或实测仍待核实。'],
 ['R3,R4,R5','0R；0R；0/VREF LINK','同一颗 0603 0Ω 跳线',3,'归并','采用当前原理图，不沿用旧 BOM 中 R3/R4 的历史数值。'],
 ['Y1,C17,C18','24MHz CL18pF；27pF C0G','24MHz CL18pF；27pF C0G',3,'保留','未选负载为 20pF 的热销晶振。符合原图名义规格；本任务不包含整机起振测试。']
];
scope.getRange('A18:F24').format.rowHeight=68;scope.getRange('A22:F22').format.rowHeight=104;scope.getRange('A22:F22').format.fill=amber;
scope.getRange('A18:F24').format.borders={insideHorizontal:{style:'thin',color:'#D6E4EC'}};
band(scope,'A26:F26','边界与来源');
note(scope,'A27:F27','USB 排除范围：仅 J2 连接器。保留 U3 USBLC6-2SC6、R1/R2 CC 下拉、R3/R4 数据线跳线及相关电源阻容。按钮与 LED 的关联阻容也保留。',44);
note(scope,'A28:F28','“优先国内渠道”按立创商城国内站自营现货执行；U2 自营缺货时采用国内合作库存。品牌不限国产，型号适配优先于成交热度与起售量。',44);
note(scope,'A29:F29','C7 依据：ST STM32C5A3xG 数据手册 DS15137 Rev 3，外部电容 COUT / VCAP 参数表（PDF 第 63 页）。',32);
note(scope,'A30:F30','https://www.st.com/resource/en/datasheet/stm32c5a3vg.pdf',30);scope.getRange('A30:F30').format.font={name:'Microsoft YaHei',size:11,color:'#087A56'};
note(scope,'A31:F31','电感保留原型号；官方资料：',28);
note(scope,'A32:F32','https://www.bourns.com/docs/Product-Datasheets/SRN6045TA.pdf',30);scope.getRange('A32:F32').format.font={name:'Microsoft YaHei',size:11,color:'#087A56'};
note(scope,'A33:F33','“物料编码”为用户自定义字段，工程未提供编码规则，留空；“商品编号”已填立创 C 码。主 BOM 保留模板原十列表头。',40);
[29,40,48,8,16,80].forEach((w,i)=>scope.getRangeByIndexes(0,i,33,1).format.columnWidth=w);scope.freezePanes.freezeRows(6);scope.tabColor='#B98925';

workbook.recalculate();
console.log((await workbook.inspect({kind:'region',sheetId:buying.name,range:'D25:I25',maxChars:1800,tableMaxRows:2,tableMaxCols:6})).ndjson);
const errors=[];
for(const [sheet,range] of [[bom,'A1:J19'],[buying,'A1:M32'],[scope,'A1:F33']]){
 for(const row of sheet.getRange(range).values)for(const cell of row)if(typeof cell==='string'&&/^#(?:REF!|DIV\/0!|VALUE!|NAME\?|N\/A|NUM!|SPILL!)/.test(cell))errors.push({sheet:sheet.name,cell});
}
if(errors.length)throw new Error(JSON.stringify(errors));
console.log(JSON.stringify({formulaErrors:errors.length,total:buying.getRange('I25').values,purchaseQuantity:buying.getRange('F25').values}));
console.log((await workbook.inspect({kind:'sheet',include:'id,name',maxChars:1200})).ndjson);
for(const [sheetName,range,file] of [[bom.name,'A1:J19','bom-preview.png'],[buying.name,'A1:M32','buying-preview.png'],[scope.name,'A1:F33','scope-preview.png']]){
 const preview=await workbook.render({sheetName,range,scale:1.1,format:'png'});
 await fs.writeFile(path.join(outputDir,file),new Uint8Array(await preview.arrayBuffer()));
}
const result=await SpreadsheetFile.exportXlsx(workbook);
const file=path.join(outputDir,'CoralC5-Core_立创BOM_国内渠道_2026-10-29.xlsx');await result.save(file);
console.log(JSON.stringify({file,groups:parts.length,quantity:refs.length,excluded:excluded.size}));
