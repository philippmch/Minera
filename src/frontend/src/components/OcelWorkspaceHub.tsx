import { useEffect,useState } from "react";
import { type OcelDiscoveryResult } from "../types";
import { type OcelDiscoveryOptions } from "../api";
import { OcelExplorer } from "./OcelExplorer";
import { OcelAnalysisWorkspace } from "./OcelAnalysisWorkspace";
export function OcelWorkspaceHub({result,file,busy,onDiscover}:{result:OcelDiscoveryResult;file:File|null;busy:boolean;onDiscover:(options:OcelDiscoveryOptions)=>void}) {
  const [group,setGroup]=useState<"models"|"analysis">("models");useEffect(()=>setGroup("models"),[file]);return <><section className="panel ocel-workspace-hub"><div className="query-actions"><button className={group==="models"?"btn-open":"btn-ghost"} onClick={()=>setGroup("models")}>Object-centric process models</button><button className={group==="analysis"?"btn-open":"btn-ghost"} onClick={()=>setGroup("analysis")}>Analyze objects & events</button></div></section>{group==="models"?<OcelExplorer result={result} busy={busy} onDiscover={onDiscover}/>:<OcelAnalysisWorkspace file={file} busy={busy}/>}</>;
}
