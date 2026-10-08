import { useState } from "react";
import { type MineraExport } from "../types";
import { TransformationWorkspace } from "./TransformationWorkspace";
import { PreparationWorkspace } from "./PreparationWorkspace";
import { StructuralWorkspace } from "./StructuralWorkspace";
import { RelatedLogsWorkspace } from "./RelatedLogsWorkspace";
export function TransformationHub({data,available,mining,file=null,onOpenTransformed,onUseModel}:{data:MineraExport;available:boolean;mining:boolean;file?:File|null;onOpenTransformed:(file:File,activityAttribute:string)=>void;onUseModel?:(file:File,sourceId:string,activityKey?:string)=>void}) {
  const [group,setGroup]=useState<"transformation"|"preparation"|"structural"|"related">("transformation");
  return <><section className="panel transformation-hub"><div className="query-actions"><button className={group==="transformation"?"btn-open":"btn-ghost"} onClick={()=>setGroup("transformation")}>Features, targets & labels</button><button className={group==="preparation"?"btn-open":"btn-ghost"} onClick={()=>setGroup("preparation")}>Prepare logs & event data</button><button className={group==="structural"?"btn-open":"btn-ghost"} onClick={()=>setGroup("structural")}>Tries, nets & intervals</button><button className={group==="related"?"btn-open":"btn-ghost"} onClick={()=>setGroup("related")}>Relate logs & events</button></div></section>{group==="transformation"?<TransformationWorkspace data={data} available={available} mining={mining} onOpenTransformed={onOpenTransformed}/>:group==="preparation"?<PreparationWorkspace data={data} available={available} mining={mining} onOpenPrepared={onOpenTransformed}/>:group==="structural"?<StructuralWorkspace data={data} available={available} mining={mining} onUseModel={onUseModel}/>:<RelatedLogsWorkspace file={file} available={available} mining={mining} dataset={data.dataset} onOpen={onOpenTransformed}/>}</>;
}
