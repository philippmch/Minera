import {renderVisualizationCanvas,type VisualizationTarget} from "./visualization-explanation";

export type VisualizationDownloadFormat="png"|"svg"|"pdf";
export interface VisualizationDownload {blob:Blob;name:string;width:number;height:number;}
export interface StyledSvgNode {tag:string;attributes:Array<[string,string]>;styles:Array<[string,string]>;children:Array<StyledSvgNode|string>;}
const encoder=new TextEncoder();
export function visualizationDownloadName(title:string,format:VisualizationDownloadFormat):string{return(title.replace(/[<>:"/\\|?*\u0000-\u001f]/g,"-").replace(/[. ]+$/g,"")||"visualization")+"."+format;}
const xml=(value:string)=>{for(const c of value){const n=c.codePointAt(0)!;if(!(n===9||n===10||n===13||n>=32&&n<=0xd7ff||n>=0xe000&&n<=0xfffd||n>=0x10000&&n<=0x10ffff))throw new Error("This SVG contains text that XML cannot represent. Download PNG or PDF instead.");}return value.replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&apos;"})[c]!);};
function reference(value:string,base:string,ids:Set<string>):string{
 if(/^data:image\/(?:png|jpeg|gif|webp);base64,[a-z0-9+/=\s]+$/i.test(value))return value;
 const url=new URL(value,base),page=new URL(base);page.hash="";const fragment=url.hash;url.hash="";
 if(!fragment||url.href!==page.href||!ids.has(decodeURIComponent(fragment.slice(1))))throw new Error("This SVG references an external or missing resource. Download PNG or PDF instead.");
 return fragment;
}
function localSvgUrls(value:string,base:string,ids:Set<string>):string{return value.replace(/url\(\s*(?:"([^"]*)"|'([^']*)'|([^)]*?))\s*\)/gi,(_match,double:string|undefined,single:string|undefined,plain:string|undefined)=>`url("${reference((double??single??plain??"").trim(),base,ids)}")`);}
/** Export presentation data only, with styles inline and local marker/filter links. */
export function serializeStyledSvg(root:StyledSvgNode,width:number,height:number,background:string,title:string,base:string):string{
 if(root.tag!=="svg"||!Number.isFinite(width)||!Number.isFinite(height)||width<=0||height<=0)throw new Error("The selected SVG has no visible size.");
 const ids=new Set<string>(),nodes=[root];while(nodes.length){const n=nodes.pop()!;const id=n.attributes.find(([k])=>k==="id")?.[1];if(id)ids.add(id);for(const child of n.children)if(typeof child!=="string")nodes.push(child);}
 const attrs=new Map(root.attributes),viewBox=attrs.get("viewBox")??`0 0 ${width} ${height}`,box=viewBox.trim().split(/[\s,]+/).map(Number);if(box.length!==4||box.some(n=>!Number.isFinite(n))||box[2]<=0||box[3]<=0)throw new Error("The SVG viewBox is invalid.");
 attrs.set("xmlns","http://www.w3.org/2000/svg");attrs.set("xmlns:xlink","http://www.w3.org/1999/xlink");attrs.set("width",String(width));attrs.set("height",String(height));attrs.set("viewBox",viewBox);
 const pending:Array<{node:StyledSvgNode|string;close?:boolean;isRoot?:boolean}>=[{node:{...root,attributes:[...attrs]},isRoot:true}],parts=['<?xml version="1.0" encoding="UTF-8"?>\n'];
 while(pending.length){const entry=pending.pop()!,n=entry.node;if(typeof n==="string"){parts.push(xml(n));continue;}if(entry.close){parts.push(`</${n.tag}>`);continue;}
  if(n.tag==="style")continue;
  if(!/^[a-zA-Z][\w:.-]*$/.test(n.tag)||["script","foreignObject","animate","animateTransform","animateMotion","set"].includes(n.tag))throw new Error("This SVG contains content that cannot be exported as a standalone diagram. Download PNG or PDF instead.");
  const localUrls=(value:string)=>localSvgUrls(value,base,ids);
  const a=new Map(n.attributes.filter(([name])=>!/^on/i.test(name)&&name!=="style"));for(const [name,value]of a){if(!/^[a-zA-Z_][\w:.-]*$/.test(name))throw new Error("Invalid SVG attribute.");a.set(name,name==="href"||name==="xlink:href"?reference(value,base,ids):localUrls(value));}
  const styles=n.styles.filter(([k])=>!k.startsWith("--")&&!/[;{}]/.test(k)).map(([name,value])=>[name,localUrls(value)] as const);
  if(entry.isRoot){for(let i=styles.length-1;i>=0;i--)if(["width","height","max-width","max-height","min-width","min-height"].includes(styles[i][0]))styles.splice(i,1);styles.push(["width",width+"px"],["height",height+"px"]);}
  if(entry.isRoot)styles.push(["animation","none"],["transition","none"]);if(styles.length)a.set("style",styles.map(([k,v])=>`${k}:${v}`).join(";"));parts.push(`<${n.tag}${[...a].map(([k,v])=>` ${k}="${xml(v)}"`).join("")}>`);pending.push({node:n,close:true});
  for(let i=n.children.length-1;i>=0;i--)pending.push({node:n.children[i]});if(entry.isRoot)parts.push(`<title>${xml(title)}</title><desc>Displayed diagram with inline styles. Fonts are not embedded; PNG and PDF retain the rendered font appearance.</desc><rect x="${box[0]}" y="${box[1]}" width="${box[2]}" height="${box[3]}" fill="${xml(background)}"/>`);
 }
 return parts.join("");
}
/** Inherited presentation properties: a child repeats one only where its value differs from its parent's. */
export const SVG_INHERITED_STYLES:readonly string[]=["fill","fill-opacity","fill-rule","stroke","stroke-width","stroke-opacity","stroke-dasharray","stroke-dashoffset","stroke-linecap","stroke-linejoin","stroke-miterlimit","marker-start","marker-mid","marker-end","paint-order","clip-rule","color","color-interpolation","color-interpolation-filters","visibility","font-family","font-size","font-style","font-weight","font-variant","font-stretch","font-kerning","font-feature-settings","font-variation-settings","letter-spacing","word-spacing","text-anchor","dominant-baseline","direction","writing-mode","white-space","shape-rendering","text-rendering","image-rendering"];
/** Non-inherited presentation properties: kept only where they differ from the element's default. */
export const SVG_OWN_STYLES:readonly string[]=["display","opacity","transform","transform-origin","transform-box","clip-path","mask","filter","mix-blend-mode","isolation","overflow","stop-color","stop-opacity","flood-color","flood-opacity","lighting-color","vector-effect","baseline-shift","alignment-baseline","text-decoration-line","text-decoration-color","text-decoration-style","unicode-bidi"];
/** The root's own box: its border and padding are part of the displayed size the export reproduces. */
export const SVG_ROOT_BOX_STYLES:readonly string[]=["box-sizing","padding-top","padding-right","padding-bottom","padding-left","border-top-width","border-right-width","border-bottom-width","border-left-width","border-top-style","border-right-style","border-bottom-style","border-left-style","border-top-color","border-right-color","border-bottom-color","border-left-color","border-top-left-radius","border-top-right-radius","border-bottom-right-radius","border-bottom-left-radius","background-color"];
/** The presentation styles one exported element needs: the root carries every inherited property (the file has no
 * page around it); a child repeats an inherited property only where it differs from its parent and a non-inherited one
 * only where it differs from a default element of its tag. Layout, animation and interaction properties are left out. */
export function svgExportStyles(computed:ReadonlyMap<string,string>,parent:ReadonlyMap<string,string>|null,defaults:ReadonlyMap<string,string>):Array<[string,string]>{
 const styles:Array<[string,string]>=[];
 for(const name of SVG_INHERITED_STYLES){const value=computed.get(name);if(value!==undefined&&value!==""&&(parent===null||parent.get(name)!==value))styles.push([name,value]);}
 for(const name of parent===null?[...SVG_OWN_STYLES,...SVG_ROOT_BOX_STYLES]:SVG_OWN_STYLES){const value=computed.get(name);if(value!==undefined&&value!==""&&defaults.get(name)!==value)styles.push([name,value]);}
 return styles;
}
function snapshotSvg(svg:SVGSVGElement):StyledSvgNode{
 const read=(element:Element)=>{const computed=getComputedStyle(element),values=new Map<string,string>();for(const name of SVG_INHERITED_STYLES)values.set(name,computed.getPropertyValue(name));for(const name of SVG_OWN_STYLES)values.set(name,computed.getPropertyValue(name));if(element.localName==="svg")for(const name of SVG_ROOT_BOX_STYLES)values.set(name,computed.getPropertyValue(name));return values;};
 // Defaults come from bare elements of each tag in an empty same-origin frame, so the page's own rules (for example a
 // global box-sizing or a tag selector) cannot hide a value that the standalone file would otherwise lose.
 const frame=document.createElement("iframe");frame.setAttribute("aria-hidden","true");frame.tabIndex=-1;frame.style.cssText="position:absolute;width:0;height:0;border:0;left:-10000px;top:0;visibility:hidden";(document.body??document.documentElement).appendChild(frame);
 const blank=frame.contentDocument!,sandbox=blank.createElementNS("http://www.w3.org/2000/svg","svg");(blank.body??blank.documentElement).appendChild(sandbox);
 const defaults=new Map<string,Map<string,string>>(),defaultsFor=(tag:string)=>{let value=defaults.get(tag);if(!value){const probe=blank.createElementNS("http://www.w3.org/2000/svg",tag);sandbox.appendChild(probe);const computed=frame.contentWindow!.getComputedStyle(probe);value=new Map([...SVG_OWN_STYLES,...SVG_ROOT_BOX_STYLES].map(name=>[name,computed.getPropertyValue(name)]));probe.remove();defaults.set(tag,value);}return value;};
 try{
  const make=(element:Element,parent:ReadonlyMap<string,string>|null)=>{const values=read(element);return{values,node:{tag:element.localName,attributes:Array.from(element.attributes,a=>[a.name,a.value]),styles:svgExportStyles(values,parent,defaultsFor(element.localName)),children:[]} as StyledSvgNode};};
  const top=make(svg,null),pending=[{element:svg as Element,...top}];
  while(pending.length){const{element,node,values}=pending.pop()!;for(const child of element.childNodes){if(child.nodeType===3)node.children.push(child.textContent??"");else if(child.nodeType===1){const e=child as Element,made=make(e,values);node.children.push(made.node);pending.push({element:e,...made});}}}
  return top.node;
 }finally{frame.remove();}
}
function jpegDimensions(bytes:Uint8Array):{width:number;height:number;components:number}{
 if(bytes.length<4||bytes[0]!==255||bytes[1]!==216||bytes[bytes.length-2]!==255||bytes[bytes.length-1]!==217)throw new Error("The canvas did not produce a complete JPEG image.");let p=2;
 while(p<bytes.length){if(bytes[p++]!==255)throw new Error("Malformed JPEG marker.");while(bytes[p]===255)p++;const marker=bytes[p++];if(marker===217||marker===218)break;if(marker===1||marker>=208&&marker<=215)continue;if(p+2>bytes.length)break;const length=bytes[p]*256+bytes[p+1];if(length<2||p+length>bytes.length)break;if([192,193,194].includes(marker)){if(length<8||bytes[p+2]!==8)break;return{height:bytes[p+3]*256+bytes[p+4],width:bytes[p+5]*256+bytes[p+6],components:bytes[p+7]};}p+=length;
 }throw new Error("The JPEG has no supported frame dimensions.");
}
/** One-page raster PDF. All offsets and stream lengths count physical bytes. */
export function visualizationPdfPageSize(width:number,height:number):{width:number;height:number}{
 if(!Number.isSafeInteger(width)||!Number.isSafeInteger(height)||width<=0||height<=0)throw new Error("PDF dimensions must be positive pixels.");const scale=Math.min(1,14400/(Math.max(width,height)*0.75));return{width:width*0.75*scale,height:height*0.75*scale};
}
export function visualizationPdf(jpeg:Uint8Array,width:number,height:number):Uint8Array<ArrayBuffer>{
 const actual=jpegDimensions(jpeg);if(actual.width!==width||actual.height!==height||![1,3].includes(actual.components)||!Number.isSafeInteger(width)||!Number.isSafeInteger(height)||width<=0||height<=0)throw new Error("PDF JPEG dimensions or color format do not match the captured view.");
 const {width:w,height:h}=visualizationPdfPageSize(width,height),chunks:Uint8Array[]=[encoder.encode("%PDF-1.4\n%"),new Uint8Array([226,227,207,211,10])],offsets=[0];let length=chunks.reduce((n,c)=>n+c.length,0);
 const append=(value:string|Uint8Array)=>{const b=typeof value==="string"?encoder.encode(value):value;chunks.push(b);length+=b.length;};
 const object=(id:number,body:string|Uint8Array[])=>{offsets[id]=length;append(`${id} 0 obj\n`);if(typeof body==="string")append(body);else for(const part of body)append(part);append("\nendobj\n");};
 object(1,"<< /Type /Catalog /Pages 2 0 R >>");object(2,"<< /Type /Pages /Kids [3 0 R] /Count 1 >>");object(3,`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${w} ${h}] /Resources << /XObject << /Image 4 0 R >> >> /Contents 5 0 R >>`);
 object(4,[encoder.encode(`<< /Type /XObject /Subtype /Image /Width ${width} /Height ${height} /ColorSpace /${actual.components===1?"DeviceGray":"DeviceRGB"} /BitsPerComponent 8 /Filter /DCTDecode /Length ${jpeg.length} >>\nstream\n`),jpeg,encoder.encode("\nendstream")]);const content=`q\n${w} 0 0 ${h} 0 0 cm\n/Image Do\nQ\n`;object(5,`<< /Length ${encoder.encode(content).length} >>\nstream\n${content}endstream`);
 const xref=length;append("xref\n0 6\n0000000000 65535 f \n");for(let i=1;i<=5;i++)append(String(offsets[i]).padStart(10,"0")+" 00000 n \n");append(`trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`);const result=new Uint8Array(length);let at=0;for(const chunk of chunks){result.set(chunk,at);at+=chunk.length;}return result;
}
function canvasBlob(canvas:HTMLCanvasElement,type:string):Promise<Blob>{return new Promise((resolve,reject)=>canvas.toBlob(blob=>blob?resolve(blob):reject(new Error("The browser could not encode this view. Try a smaller displayed view.")),type,0.95));}
export async function prepareVisualizationDownload(target:VisualizationTarget,format:VisualizationDownloadFormat,signal?:AbortSignal):Promise<VisualizationDownload>{
 const check=()=>{signal?.throwIfAborted();if(!target.element.isConnected)throw new Error("The visualization changed; select the current view again.");};check();const name=visualizationDownloadName(target.title,format);
 if(format==="svg"){await document.fonts.ready;check();await Promise.all(target.element.getAnimations({subtree:true}).filter(a=>Number.isFinite(a.effect?.getComputedTiming().endTime)).map(a=>a.finished.catch(()=>{})));check();const diagrams=target.element.querySelectorAll<SVGSVGElement>("svg");if(diagrams.length!==1)throw new Error("Select a view containing one SVG diagram. PNG and PDF also support charts and tables.");const svg=diagrams[0],box=svg.getBoundingClientRect();let background="#f4f1e9";for(let e:Element|null=svg;e;e=e.parentElement){const color=getComputedStyle(e).backgroundColor;if(color&&color!=="transparent"&&!/^rgba\([^)]*,\s*0\s*\)$/.test(color)){background=color;break;}}const raw=serializeStyledSvg(snapshotSvg(svg),box.width,box.height,background,target.title,document.baseURI);check();return{blob:new Blob([raw],{type:"image/svg+xml;charset=utf-8"}),name,width:box.width,height:box.height};}
 const canvas=await renderVisualizationCanvas(target,signal,false);check();const image=await canvasBlob(canvas,format==="pdf"?"image/jpeg":"image/png");check();const blob=format==="pdf"?new Blob([visualizationPdf(new Uint8Array(await image.arrayBuffer()),canvas.width,canvas.height)],{type:"application/pdf"}):image;check();return{blob,name,width:canvas.width,height:canvas.height};
}
