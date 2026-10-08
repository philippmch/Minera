import { useState } from "react";
import { type MineraExport } from "../types";
import { TransformationWorkspace } from "./TransformationWorkspace";
import { PreparationWorkspace } from "./PreparationWorkspace";
import { StructuralWorkspace } from "./StructuralWorkspace";
export function TransformationHub({data,available,mining,onOpenTransformed,onUseModel}:{data:MineraExport;available:boolean;mining:boolean;onOpenTransformed:(file:File,activityAttribute:string)=>void;onUseModel?:(file:File,sourceId:string,activityKey?:string)=>void}) {
  const [group,setGroup]=useState<"transformation"|"preparation"|"structural">("transformation");
  return <><section className="panel transformation-hub"><div className="query-actions"><button className={group==="transformation"?"btn-open":"btn-ghost"} onClick={()=>setGroup("transformation")}>Features, targets & labels</button><button className={group==="preparation"?"btn-open":"btn-ghost"} onClick={()=>setGroup("preparation")}>Prepare logs & event data</button><button className={group==="structural"?"btn-open":"btn-ghost"} onClick={()=>setGroup("structural")}>Tries, nets & intervals</button></div></section>{group==="transformation"?<TransformationWorkspace data={data} available={available} mining={mining} onOpenTransformed={onOpenTransformed}/>:group==="preparation"?<PreparationWorkspace data={data} available={available} mining={mining} onOpenPrepared={onOpenTransformed}/>:<StructuralWorkspace data={data} available={available} mining={mining} onUseModel={onUseModel}/>}</>;
}
