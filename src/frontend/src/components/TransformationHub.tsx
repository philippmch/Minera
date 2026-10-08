import { useState } from "react";
import { type MineraExport } from "../types";
import { TransformationWorkspace } from "./TransformationWorkspace";
import { PreparationWorkspace } from "./PreparationWorkspace";
import { StructuralWorkspace } from "./StructuralWorkspace";
import { RelatedLogsWorkspace } from "./RelatedLogsWorkspace";
import { StreamingWorkspace } from "./StreamingWorkspace";
import { ModelExchangeWorkspace } from "./ModelExchangeWorkspace";
import { ProcessCubeWorkspace } from "./ProcessCubeWorkspace";
const GROUPS = [
  ["transformation", "Features, targets & labels"],
  ["preparation", "Prepare logs & event data"],
  ["structural", "Tries, nets & intervals"],
  ["related", "Relate logs & events"],
  ["streaming", "Replay event streams"],
  ["models", "Inspect & convert model files"],
  ["cube", "Process cubes"],
] as const;
export function TransformationHub({data,available,mining,file=null,onOpenTransformed,onUseModel}:{data:MineraExport;available:boolean;mining:boolean;file?:File|null;onOpenTransformed:(file:File,activityAttribute:string)=>void;onUseModel?:(file:File,sourceId:string,activityKey?:string,inferLegacyBoundaries?:boolean)=>void}) {
  const [group,setGroup]=useState<typeof GROUPS[number][0]>("transformation");
  const [cubeFile,setCubeFile]=useState<File|null>(null);
  return <><section className="panel transformation-hub"><div className="query-actions">
    {GROUPS.map(([id,label])=><button key={id} className={group===id?"btn-open":"btn-ghost"} onClick={()=>setGroup(id)}>{label}</button>)}
  </div></section>
    {group==="transformation"?<TransformationWorkspace data={data} available={available} mining={mining} onOpenTransformed={onOpenTransformed} onUseCube={file=>{setCubeFile(file);setGroup("cube");}}/>
    :group==="preparation"?<PreparationWorkspace data={data} available={available} mining={mining} onOpenPrepared={onOpenTransformed}/>
    :group==="structural"?<StructuralWorkspace data={data} available={available} mining={mining} onUseModel={onUseModel}/>
    :group==="related"?<RelatedLogsWorkspace file={file} available={available} mining={mining} dataset={data.dataset} onOpen={onOpenTransformed}/>
    :group==="streaming"?<StreamingWorkspace file={file} busy={mining}/>
    :group==="cube"?<ProcessCubeWorkspace initialFile={cubeFile} busy={mining}/>
    :<ModelExchangeWorkspace available={available} busy={mining} onUseModel={onUseModel}/>}
  </>;
}
