"""Reproduce M2.8 first-order sizing; does not certify loop stability or thermals."""
from pathlib import Path
import json,math,cmath,itertools
DOC=Path(__file__).resolve().parents[1]/'docs'
def main():
 v=.8*(1+53.6/10);fs=500e3;rc=15800;cc=10e-9;ch=470e-12
 cases=[]
 # Lumped C/ESR envelope is an explicit sizing assumption, not a measured model.
 for cap,esr,load in itertools.product([130e-6,180e-6,220e-6],[.001,.03],[.5,4,4.2]):
  def loop(f):
   s=2j*math.pi*f;zc=1/(1/(rc+1/(s*cc))+s*ch)
   zo=1/(load/v+1/(esr+1/(s*cap)))
   return .00015*zc/.089*zo*.8/v
  crossings=[];previous=(10.,loop(10.))
  for i in range(1,1201):
   f=10*10**(i*5/1200);z=loop(f)
   if abs(previous[1])>=1 and abs(z)<1:
    crossings.append({'hz':f,'phase_margin_deg':180+cmath.phase(z)*180/math.pi})
   previous=(f,z)
  cases.append({'effective_C_uF':cap*1e6,'assumed_ESR_ohm':esr,'load_A':load,'eq17_fc_Hz':rc/(4.67e3*v*cap),'eq18_Cseries_nF':v*cap/(load*rc)*1e9,'eq19_Chf_pF':max(esr*cap/rc,1/(math.pi*fs*rc))*1e12,'first_order_unity_crossings':crossings})
 di=v*(1-v/12)/(3.3e-6*fs);iin=7+v*8.2/(12*.85)
 result={'date':'2026-11-07','measurement':False,'datasheet':'https://www.diodes.com/datasheet/download/AP64500.pdf','datasheet_revision':'DS41979 Rev5-2 December2024; equations17-19','polymer_source':'https://industrial.panasonic.com/ww/products/pt/os-con/models/16SVP150M','polymer_ESR_ohm_at100kHz':.03,'nominal_output_V':v,'reference_tolerance':.01,'divider_tolerance_each':.001,'output_tolerance_V':[.8*.99*(1+53.6*.999/(10*1.001)),.8*1.01*(1+53.6*1.001/(10*.999))],'rt_ohm':200000,'fsw_target_Hz':fs,'inductor_H':3.3e-6,'nominal_ripple_A':di,'peak_A_4p2_load':4.2+di/2,'hs_peak_limit_min_A':6.8,'compensation':{'R_ohm':rc,'Cseries_F':cc,'Chf_F':ch,'Chf_min_5pct_pF':ch*.95*1e12,'previous_Chf_pF':39,'reason':'Include the retained 150uF polymer ESR in Eq19; 39pF did not cover that term.'},'cases':cases,'rail_loads_A':{'Lichee':3,'Core':.75,'gray':.3,'TFT_BT_LDO':.15,'Action4':3,'Maix':1},'input_budget':{'adapter_V':12,'adapter_A':12.5,'assumed_efficiency':.85,'wheel_A':4,'stepper_A':3,'calculated_A':iin,'adapter_headroom_A':12.5-iin,'F1_rating_A':15,'F1_75pct_A':11.25,'fuse_derated_headroom_A':11.25-iin},'mosfet_conduction_only_W_25C':{str(i):i*i*(.045*v/12+.020*(1-v/12)) for i in [4,4.2]},'limits':['Ceff130-220uF and ESR1-30mOhm are sizing envelopes, not measured converter output impedance. Unknown module/cable capacitors are excluded.','The simple transconductance/current-source model omits inner current-loop poles, sampling, DCM/PFM, delay, distributed ESR/ESL and parasitics; its apparent phase margins are not a stability certificate.','10nF series compensation and 470pF high-frequency pole are prototype starting values. Measure loop gain/load-step response at cable/load/temperature corners before release.','5A is the IC rating, not proven board capability. Peak current uses nominal L/frequency only; verify tolerance, inductor saturation and thermal rise.','Conduction numbers omit switching, quiescent, inductor, diode, copper and connector losses. No junction temperature is certified.','Input startup/stall/inrush and fuse temperature derating are unmeasured; 0.16A assumed continuous fuse headroom is small. Stage module startup and test, do not silently raise F1.','5V Type-C is 3A current advertisement, no PD/PPS negotiation. 150uF bulk remains at TPS25810 IN and 10uF at OUT per TI8.2.1.1.1.']}
 result['eq19_envelope_satisfied']=all(c['eq19_Chf_pF']<=ch*.95*1e12 for c in cases)
 (DOC/'m28_power_analysis.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(json.dumps({k:result[k] for k in ['nominal_output_V','output_tolerance_V','nominal_ripple_A','peak_A_4p2_load','input_budget','eq19_envelope_satisfied']},indent=2))
 return 0 if result['eq19_envelope_satisfied'] else 1
if __name__=='__main__':raise SystemExit(main())
