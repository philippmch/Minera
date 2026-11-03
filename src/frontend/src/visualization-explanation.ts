import { type LlmImage } from "./llm";

export interface VisualizationDraft {id:string;title:string;dataset:string;image:LlmImage;prompt:string;width:number;height:number;}
export interface VisualizationTarget {element:HTMLElement;title:string;svgCount?:number;}
export function visualizationPrompt(title:string,dataset:string):string {
  return "Explain this process-mining visualization: "+title+".\nDataset: "+dataset+".\nDescribe the structure, displayed metrics and any visible patterns. Distinguish what the image shows from conclusions that need more data.";
}
export function visualizationSize(width:number,height:number):{width:number;height:number;scale:number} {
  if(!Number.isFinite(width)||!Number.isFinite(height)||width<=0||height<=0)throw new Error("The selected view has no visible size.");
  const scale=Math.min(1,1600/Math.max(width,height),Math.sqrt(4000000/(width*height)));
  return {width:Math.max(1,Math.round(width*scale)),height:Math.max(1,Math.round(height*scale)),scale};
}
export function visualizationRasterDimensions(width:number,height:number):{width:number;height:number;scale:number}{
  if(!Number.isFinite(width)||!Number.isFinite(height)||width<=0||height<=0)throw new Error("The selected view has no visible size.");
  const w=Math.ceil(width),h=Math.ceil(height);
  if(w>32767||h>32767||w*h>32000000)throw new Error("This view is too large for a reliable local image export (32 million pixels or 32767 pixels on one side). Reduce the displayed view or export its data instead.");
  return{width:w,height:h,scale:1};
}
export function visualizationTargets(root:HTMLElement):VisualizationTarget[] {
  const candidates=Array.from(root.querySelectorAll<HTMLElement>(".panel")).filter(element=>{
    const box=element.getBoundingClientRect();return box.width>0&&box.height>0&&!!element.querySelector("svg,canvas,table")&&!element.closest(".ask-model-panel");
  });
  return candidates.filter(element=>!candidates.some(child=>child!==element&&element.contains(child))).map(element=>({element,title:element.querySelector("h2,h3")?.textContent?.trim()||"Process visualization",svgCount:element.querySelectorAll("svg").length}));
}
/** Keep discovery attached when navigation replaces main; style changes refresh once. */
export function watchVisualizationTargets(onChange:(targets:VisualizationTarget[])=>void,onRootChange:()=>void):()=>void{
  let root:HTMLElement|null=null,attached=false;
  const refresh=()=>onChange(root?.isConnected?visualizationTargets(root):[]),observer=new MutationObserver(refresh);
  const attach=()=>{const next=document.querySelector<HTMLElement>("main");if(attached&&next===root)return;attached=true;observer.disconnect();onRootChange();root=next;if(root)observer.observe(root,{childList:true,subtree:true,attributes:true,attributeFilter:["class","style","hidden"]});refresh();};
  const navigation=new MutationObserver(attach);navigation.observe(document.body,{childList:true,subtree:true});attach();window.addEventListener("resize",refresh);
  return()=>{navigation.disconnect();observer.disconnect();window.removeEventListener("resize",refresh);};
}
/** Controls hidden from captured images (they keep their space so the layout does not shift). */
export const VISUALIZATION_CHROME=".map-hint, [data-export-ignore], .map-controls button:not(.active):not([aria-pressed=\"true\"]), .map-controls input, .map-controls select";
/** The LLM preview has a size budget; local downloads keep the displayed CSS-pixel size. */
export async function renderVisualizationCanvas(target:VisualizationTarget,signal?:AbortSignal,forLlm=true):Promise<HTMLCanvasElement> {
  const check=()=>{if(signal?.aborted)throw new DOMException("Capture cancelled","AbortError");if(!target.element.isConnected)throw new Error("The visualization changed; select the current view again.");};
  check();await document.fonts.ready;check();
  await Promise.all(target.element.getAnimations({subtree:true}).filter(animation=>Number.isFinite(animation.effect?.getComputedTiming().endTime)).map(animation=>animation.finished.catch(()=>{})));check();
  const elements=[target.element,...target.element.querySelectorAll("*")];
  const styles=elements.map(element=>{const style=getComputedStyle(element);return {opacity:style.opacity,transform:style.transform,svg:element instanceof SVGElement?Array.from(style).map(name=>[name,style.getPropertyValue(name)] as const):null};});
  const box=target.element.getBoundingClientRect();if(!Number.isFinite(box.width)||!Number.isFinite(box.height)||box.width<=0||box.height<=0)throw new Error("The selected view has no visible size.");
  const size=forLlm?visualizationSize(box.width,box.height):visualizationRasterDimensions(box.width,box.height);
  const {default:render}=await import("html2canvas");check();
  const canvas=await render(target.element,{backgroundColor:"#f4f1e9",scale:size.scale,...(!forLlm?{width:size.width,height:size.height}:{}),logging:false,imageTimeout:10000,allowTaint:false,
    onclone:(_document,element)=>{
      [element,...element.querySelectorAll("*")].forEach((node,index)=>{
        const captured=styles[index];if(!captured||!("style" in node))return;
        const styled=node as HTMLElement|SVGElement;
        // html2canvas paints box-shadow as a translucent fill over part of the box, so shadows are dropped from captures.
        styled.style.animation="none";styled.style.transition="none";styled.style.boxShadow="none";styled.style.opacity=captured.opacity;styled.style.transform=captured.transform;
        if(captured.svg)for(const [name,value] of captured.svg)styled.style.setProperty(name,value.replace(/url\(["']?[^#)"']*#([^)"']+)["']?\)/g,"url(#$1)"));
      });
      // Interactive chrome (zoom hints, sliders, mode buttons) is not part of the picture.
      element.querySelectorAll<HTMLElement>(VISUALIZATION_CHROME).forEach(node=>{node.style.visibility="hidden";});
    }});
  check();
  if(!forLlm&&(canvas.width!==size.width||canvas.height!==size.height))throw new Error("The browser could not render the complete view at its requested size. Reduce the displayed view and try again.");
  const pixels=canvas.getContext("2d")?.getImageData(0,0,canvas.width,canvas.height).data;
  if(!pixels||!pixels.some((value,index)=>value!==pixels[index%4]))throw new Error("The selected view rendered blank. Wait for its data and capture it again.");
  return canvas;
}
export async function captureVisualization(target:VisualizationTarget,dataset:string,signal?:AbortSignal):Promise<VisualizationDraft> {
  const canvas=await renderVisualizationCanvas(target,signal,true);
  const url=canvas.toDataURL("image/png"),data=url.split(",",2)[1];
  if(!data||data.length>2097152)throw new Error("The rendered image exceeds 1.5 MiB. Select a smaller view or reduce visible detail.");
  const name=target.title.replace(/[<>:"/\\|?*\u0000-\u001f]/g,"-")+".png";
  return {id:crypto.randomUUID(),title:target.title,dataset,image:{name,media_type:"image/png",data},prompt:visualizationPrompt(target.title,dataset),width:canvas.width,height:canvas.height};
}
