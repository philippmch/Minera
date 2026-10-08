import { useState } from "react";
import { type MineraExport } from "../types";
import { TransformationWorkspace } from "./TransformationWorkspace";
import { PreparationWorkspace } from "./PreparationWorkspace";
export function TransformationHub({data,available,mining,onOpenTransformed}:{data:MineraExport;available:boolean;mining:boolean;onOpenTransformed:(file:File,activityAttribute:string)=>void}) {
  const [group,setGroup]=useState<"transformation"|"preparation">("transformation");
  return <><section className="panel transformation-hub"><div className="query-actions"><button className={group==="transformation"?"btn-open":"btn-ghost"} onClick={()=>setGroup("transformation")}>Features, targets & labels</button><button className={group==="preparation"?"btn-open":"btn-ghost"} onClick={()=>setGroup("preparation")}>Prepare logs & event data</button></div></section>{group==="transformation"?<TransformationWorkspace data={data} available={available} mining={mining} onOpenTransformed={onOpenTransformed}/>:<PreparationWorkspace data={data} available={available} mining={mining} onOpenPrepared={onOpenTransformed}/>}</>;
}
