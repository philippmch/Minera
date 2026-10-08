import { useEffect,useRef,useState } from "react";
import { captureVisualization,visualizationTargets,type VisualizationDraft,type VisualizationTarget } from "../visualization-explanation";

export function VisualizationExplain({dataset,busy,onUse}:{dataset:string;busy:boolean;onUse:(draft:VisualizationDraft)=>void}) {
  const [targets,setTargets]=useState<VisualizationTarget[]>([]);const [selected,setSelected]=useState(0);const [pending,setPending]=useState(false);const [error,setError]=useState<string|null>(null);const request=useRef<AbortController|null>(null);
  useEffect(()=>{
    const root=document.querySelector<HTMLElement>("main");if(!root)return;
    const refresh=()=>{const next=visualizationTargets(root);setTargets(old=>old.length===next.length&&old.every((item,i)=>item.element===next[i].element&&item.title===next[i].title)?old:next);};
    refresh();const observer=new MutationObserver(refresh);observer.observe(root,{childList:true,subtree:true});return()=>{observer.disconnect();request.current?.abort();};
  },[]);
  useEffect(()=>{if(busy){request.current?.abort();request.current=null;setPending(false);}},[busy]);
  const capture=async()=>{
    const target=targets[Math.min(selected,Math.max(0,targets.length-1))];if(!target||busy||pending)return;
    const controller=new AbortController();request.current=controller;setPending(true);setError(null);
    try{const draft=await captureVisualization(target,dataset,controller.signal);if(request.current===controller&&!controller.signal.aborted)onUse(draft);}
    catch(cause){if(!controller.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}
    finally{if(request.current===controller){request.current=null;setPending(false);}}
  };
  return <section className="visualization-explain" aria-label="Explain a visualization"><div className="visualization-explain-controls"><label className="filters-field"><span>Visualization to explain</span><select value={Math.min(selected,Math.max(0,targets.length-1))} disabled={busy||pending||!targets.length} onChange={event=>{setSelected(Number(event.target.value));setError(null);}}>{targets.map((target,index)=><option key={index} value={index}>{target.title}</option>)}{!targets.length&&<option value={0}>Open a diagram, chart or table</option>}</select></label><button className="btn-ghost" disabled={busy||pending||!targets.length} onClick={()=>void capture()}>{pending?"Rendering image…":"Prepare visualization explanation"}</button>{pending&&<button className="btn-ghost" onClick={()=>{request.current?.abort();request.current=null;setPending(false);}}>Cancel rendering</button>}</div><p>Render the selected view as an image, then review its prompt and attachment in Ask a model.</p>{error&&<div className="error-banner" role="alert">{error}</div>}</section>;
}
