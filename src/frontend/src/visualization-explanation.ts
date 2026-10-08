import { type LlmImage } from "./llm";

export interface VisualizationDraft {id:string;title:string;dataset:string;image:LlmImage;prompt:string;width:number;height:number;}
export interface VisualizationTarget {element:HTMLElement;title:string;}
export function visualizationPrompt(title:string,dataset:string):string {
  return "Explain this process-mining visualization: "+title+".\nDataset: "+dataset+".\nDescribe the structure, displayed metrics and any visible patterns. Distinguish what the image shows from conclusions that need more data.";
}
export function visualizationSize(width:number,height:number):{width:number;height:number;scale:number} {
  if(!Number.isFinite(width)||!Number.isFinite(height)||width<=0||height<=0)throw new Error("The selected view has no visible size.");
  const scale=Math.min(1,1600/Math.max(width,height),Math.sqrt(4000000/(width*height)));
  return {width:Math.max(1,Math.round(width*scale)),height:Math.max(1,Math.round(height*scale)),scale};
}
export function visualizationTargets(root:HTMLElement):VisualizationTarget[] {
  const candidates=Array.from(root.querySelectorAll<HTMLElement>(".panel")).filter(element=>{
    const box=element.getBoundingClientRect();return box.width>0&&box.height>0&&!!element.querySelector("svg,canvas,table")&&!element.closest(".ask-model-panel");
  });
  return candidates.filter(element=>!candidates.some(child=>child!==element&&element.contains(child))).map(element=>({element,title:element.querySelector("h2,h3")?.textContent?.trim()||"Process visualization"}));
}
export async function captureVisualization(target:VisualizationTarget,dataset:string,signal?:AbortSignal):Promise<VisualizationDraft> {
  const check=()=>{if(signal?.aborted)throw new DOMException("Capture cancelled","AbortError");if(!target.element.isConnected)throw new Error("The visualization changed; select the current view again.");};
  check();await document.fonts.ready;check();
  await Promise.all(target.element.getAnimations({subtree:true}).filter(animation=>Number.isFinite(animation.effect?.getComputedTiming().endTime)).map(animation=>animation.finished.catch(()=>{})));check();
  const elements=[target.element,...target.element.querySelectorAll("*")];
  const styles=elements.map(element=>{const style=getComputedStyle(element);return {opacity:style.opacity,transform:style.transform,svg:element instanceof SVGElement?Array.from(style).map(name=>[name,style.getPropertyValue(name)] as const):null};});
  const box=target.element.getBoundingClientRect(),size=visualizationSize(box.width,box.height);
  const {default:render}=await import("html2canvas");check();
  const canvas=await render(target.element,{backgroundColor:"#f4f1e9",scale:size.scale,logging:false,imageTimeout:10000,allowTaint:false,
    onclone:(_document,element)=>{
      [element,...element.querySelectorAll("*")].forEach((node,index)=>{
        const captured=styles[index];if(!captured||!("style" in node))return;
        const styled=node as HTMLElement|SVGElement;
        styled.style.animation="none";styled.style.transition="none";styled.style.opacity=captured.opacity;styled.style.transform=captured.transform;
        if(captured.svg)for(const [name,value] of captured.svg)styled.style.setProperty(name,value.replace(/url\(["']?[^#)"']*#([^)"']+)["']?\)/g,"url(#$1)"));
      });
    }});
  check();
  const pixels=canvas.getContext("2d")?.getImageData(0,0,canvas.width,canvas.height).data;
  if(!pixels||!pixels.some((value,index)=>value!==pixels[index%4]))throw new Error("The selected view rendered blank. Wait for its data and capture it again.");
  const url=canvas.toDataURL("image/png"),data=url.split(",",2)[1];
  if(!data||data.length>2097152)throw new Error("The rendered image exceeds 1.5 MiB. Select a smaller view or reduce visible detail.");
  const name=target.title.replace(/[<>:"/\\|?*\u0000-\u001f]/g,"-")+".png";
  return {id:crypto.randomUUID(),title:target.title,dataset,image:{name,media_type:"image/png",data},prompt:visualizationPrompt(target.title,dataset),width:canvas.width,height:canvas.height};
}
