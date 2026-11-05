const fs=require('fs'),path=require('path');
const {pathToFileURL}=require('url');
const {chromium}=require('C:/Users/sc/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const base=path.resolve(__dirname,'..');
const expected=JSON.parse(fs.readFileSync(path.join(base,'preview','assembly-meshes.json'),'utf8')).parts.length;
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:'C:/Program Files/Google/Chrome/Application/chrome.exe'});
 try{
  const page=await browser.newPage({viewport:{width:1700,height:1100},deviceScaleFactor:1});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(pathToFileURL(path.join(base,'preview','查看模型.html')).href);
  await page.waitForFunction(()=>window.modelReady===true,{timeout:60000});
  await page.waitForTimeout(800);
  await page.locator('#viewport').screenshot({path:path.join(base,'preview','preview-isometric.png')});
  await page.screenshot({path:path.join(base,'preview','preview-interface.png')});
  await page.getByRole('button',{name:'俯视',exact:true}).click();
  await page.waitForTimeout(250);
  await page.locator('#viewport').screenshot({path:path.join(base,'preview','preview-top.png')});
  await page.getByRole('button',{name:'侧视',exact:true}).click();
  await page.waitForTimeout(250);
  await page.locator('#viewport').screenshot({path:path.join(base,'preview','preview-side.png')});
  const poses=[];
  for(const angle of [-6,0,6]){
   await page.locator('#angle').fill(String(angle));
   const state=await page.evaluate(()=>{
    const eye=meshes.find(m=>m.userData.name==='RodEye60');
    const pin=meshes.find(m=>m.userData.name==='BeamLinkPin');
    const center=m=>{m.geometry.computeBoundingBox();return m.geometry.boundingBox.getCenter(new THREE.Vector3()).applyMatrix4(m.matrix)};
    const rodEnd=center(eye),attach=center(pin);
    attach.y+=(B.link_plane_y+B.rod_lateral_offset)-(-41+16/2);
    return {angle:window.currentBeamAngle,closure:rodEnd.distanceTo(attach)};
   });
   poses.push({requested:angle,displayed:state.angle,rod_closure_error_mm:state.closure,
    pass:Math.abs(state.angle-angle)<1e-6&&state.closure<0.02});
  }
  await page.locator('input[data-group="electronics"]').uncheck();
  const toggleOff=await page.evaluate(()=>meshes.filter(m=>m.userData.family==='electronics'&&m.visible).length===0);
  await page.locator('input[data-group="electronics"]').check();
  await page.getByRole('button',{name:'运动预览',exact:true}).click();await page.waitForTimeout(100);
  const animatedStart=await page.evaluate(()=>window.currentBeamAngle);
  await page.waitForTimeout(400);
  const animatedAngle=await page.evaluate(()=>window.currentBeamAngle);
  await page.getByRole('button',{name:'暂停',exact:true}).click();
  await page.getByRole('button',{name:'复位',exact:true}).click();
  await page.locator('#explode').check();await page.waitForTimeout(250);
  await page.locator('#viewport').screenshot({path:path.join(base,'preview','preview-exploded.png')});
  await page.getByRole('button',{name:'复位',exact:true}).click();
  const count=await page.evaluate(()=>window.modelPartCount);
  const animationPass=Number.isFinite(animatedAngle)&&Math.abs(animatedAngle-animatedStart)>1e-5;
  const report={date:'2026-11-05',browser_version:browser.version(),node_version:process.version,
   parts:count,poses,toggle_pass:toggleOff,animation_moves:animationPass,javascript_errors:errors,
   screenshots:['preview-isometric.png','preview-interface.png','preview-top.png','preview-side.png','preview-exploded.png'],
   pass:count===expected&&poses.every(p=>p.pass)&&toggleOff&&animationPass&&errors.length===0};
  fs.writeFileSync(path.join(base,'validation','viewer-check.json'),JSON.stringify(report,null,2)+'\n');
  console.log(JSON.stringify(report));if(!report.pass)process.exitCode=1;
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exitCode=1});
