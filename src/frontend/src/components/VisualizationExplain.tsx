import { useEffect,useRef,useState } from "react";
import { captureVisualization,watchVisualizationTargets,type VisualizationDraft,type VisualizationTarget } from "../visualization-explanation";
import {prepareVisualizationDownload,type VisualizationDownloadFormat} from "../visualization-download";
import {downloadQueryBlob} from "../log-query";

export function VisualizationExplain({dataset,busy,onUse}:{dataset:string;busy:boolean;onUse?:(draft:VisualizationDraft)=>void}) {
  const [targets,setTargets]=useState<VisualizationTarget[]>([]);const [selected,setSelected]=useState(0);const [pending,setPending]=useState<VisualizationDownloadFormat|"explain"|null>(null);const [error,setError]=useState<string|null>(null);const request=useRef<AbortController|null>(null);
  useEffect(()=>{
    const stop=watchVisualizationTargets(next=>setTargets(old=>old.length===next.length&&old.every((item,i)=>item.element===next[i].element&&item.title===next[i].title&&item.svgCount===next[i].svgCount)?old:next),()=>{request.current?.abort();request.current=null;setPending(null);setSelected(0);});
    return()=>{stop();request.current?.abort();request.current=null;};
  },[dataset]);
  useEffect(()=>{if(busy){request.current?.abort();request.current=null;setPending(null);}},[busy]);
  const capture=async(action:VisualizationDownloadFormat|"explain")=>{
    const target=targets[Math.min(selected,Math.max(0,targets.length-1))];if(!target||busy||pending)return;
    const controller=new AbortController();request.current=controller;setPending(action);setError(null);
    try{if(action==="explain"){const draft=await captureVisualization(target,dataset,controller.signal);if(request.current===controller&&!controller.signal.aborted)onUse?.(draft);}else{const result=await prepareVisualizationDownload(target,action,controller.signal);if(request.current===controller&&!controller.signal.aborted)downloadQueryBlob(result.blob,result.name);}}
    catch(cause){if(request.current===controller&&!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(null);}}
  };
  const disabled=busy||pending!==null||!targets.length,svgAvailable=targets[Math.min(selected,Math.max(0,targets.length-1))]?.svgCount===1;
  return <section className="visualization-explain" aria-label="Visualization tools"><div className="visualization-explain-controls"><label className="filters-field"><span>Visualization to export or explain</span><select value={Math.min(selected,Math.max(0,targets.length-1))} disabled={disabled} onChange={event=>{request.current?.abort();request.current=null;setPending(null);setSelected(Number(event.target.value));setError(null);}}>{targets.map((target,index)=><option key={index} value={index}>{target.title}</option>)}{!targets.length&&<option value={0}>Open a diagram, chart or table</option>}</select></label><button className="btn-ghost" disabled={disabled} onClick={()=>void capture("png")}>Download PNG</button><button className="btn-ghost" disabled={disabled||!svgAvailable} title="Standalone vector export requires one SVG diagram in the selected view" onClick={()=>void capture("svg")}>Download SVG</button><button className="btn-ghost" disabled={disabled} onClick={()=>void capture("pdf")}>Download PDF</button>{onUse&&<button className="btn-ghost" disabled={disabled} onClick={()=>void capture("explain")}>Prepare visualization explanation</button>}{pending&&<><span role="status">{pending==="explain"?"Rendering image…":"Preparing "+pending.toUpperCase()+"…"}</span><button className="btn-ghost" onClick={()=>{request.current?.abort();request.current=null;setPending(null);}}>Cancel rendering</button></>}</div><p>PNG and PDF capture the displayed view, including current filters and zoom, at its CSS-pixel size. PDF contains a raster image. SVG retains vector shapes and inline styles for a single diagram; fonts are not embedded. Full data remains in the table and model downloads.{onUse&&" Prepare an explanation to review the image and prompt before sending it to a model."}</p>{error&&<div className="error-banner" role="alert">{error}</div>}</section>;
}
