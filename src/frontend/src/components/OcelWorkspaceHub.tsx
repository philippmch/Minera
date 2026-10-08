import { useEffect,useState } from "react";
import { type OcelDiscoveryResult } from "../types";
import { type OcelDiscoveryOptions } from "../api";
import { OcelExplorer } from "./OcelExplorer";
import { OcelAnalysisWorkspace } from "./OcelAnalysisWorkspace";
import { OcelPreparationWorkspace } from "./OcelPreparationWorkspace";
import { OcelExecutionWorkspace } from "./OcelExecutionWorkspace";
import { OcelIoWorkspace } from "./OcelIoWorkspace";
export function OcelWorkspaceHub({result,file,busy,onDiscover,onOpen}:{result:OcelDiscoveryResult;file:File|null;busy:boolean;onDiscover:(options:OcelDiscoveryOptions)=>void;onOpen?:(file:File)=>void}) {
  const [group,setGroup]=useState<"models"|"analysis"|"preparation"|"execution"|"io">("models");useEffect(()=>setGroup("models"),[file]);return <><section className="panel ocel-workspace-hub"><div className="query-actions"><button className={group==="models"?"btn-open":"btn-ghost"} onClick={()=>setGroup("models")}>Object-centric process models</button><button className={group==="analysis"?"btn-open":"btn-ghost"} onClick={()=>setGroup("analysis")}>Analyze objects & events</button><button className={group==="preparation"?"btn-open":"btn-ghost"} onClick={()=>setGroup("preparation")}>Prepare object-centric data</button><button className={group==="execution"?"btn-open":"btn-ghost"} onClick={()=>setGroup("execution")}>Compare & simulate OCEL</button><button className={group==="io"?"btn-open":"btn-ghost"} onClick={()=>setGroup("io")}>Import & export OCEL</button></div></section>{group==="models"?<OcelExplorer result={result} busy={busy} onDiscover={onDiscover}/>:group==="analysis"?<OcelAnalysisWorkspace file={file} busy={busy}/>:group==="preparation"?<OcelPreparationWorkspace file={file} busy={busy} onOpen={onOpen??(()=>{})}/>:group==="execution"?<OcelExecutionWorkspace file={file} busy={busy} onOpen={onOpen??(()=>{})}/>:<OcelIoWorkspace file={file} busy={busy} initialMode="export" onOpen={onOpen??(()=>{})}/>}</>;
}
