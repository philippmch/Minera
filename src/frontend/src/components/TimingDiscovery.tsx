import { useEffect,useRef,useState } from "react";
import type { MineraExport } from "../types";
import { TIMING_VARIANTS,defaultTimingForm,runTimingDiscovery,type TimingFamily,type TimingForm,type TimingRun } from "../timing-discovery";
import { requestLogQuerySchema,downloadQueryBlob,type LogQuerySchema } from "../log-query";
import { nativeJson } from "../native-json";
import { diagnosticCsv } from "../model-diagnostics";
import { DiagnosticTableView } from "./ModelDiagnosticsInspection";
import { PerformanceSpectrumView } from "./PerformanceSpectrum";
import { Batches } from "./Batches";
export function TimingDiscovery({data,available,mining}:{data:MineraExport;available:boolean;mining:boolean}) {
  const [family,setFamily]=useState<TimingFamily>('spectrum'),[form,setForm]=useState(()=>defaultTimingForm('spectrum'));
  const [source,setSource]=useState<LogQuerySchema|null>(null),[run,setRun]=useState<TimingRun|null>(null),[pending,setPending]=useState(false),[error,setError]=useState<string|null>(null);
  const request=useRef<AbortController|null>(null),schemaRequest=useRef<AbortController|null>(null),sequence=useRef(0);
  const cancel=()=>{request.current?.abort();request.current=null;setPending(false);};
  const refresh=async()=>{cancel();schemaRequest.current?.abort();setSource(null);setRun(null);setError(null);if(!available||mining)return;const c=new AbortController();schemaRequest.current=c;try{const next=await requestLogQuerySchema(c.signal);if(schemaRequest.current===c&&!c.signal.aborted)setSource(next);}catch(cause){if(schemaRequest.current===c&&!c.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}};
  useEffect(()=>{void refresh();return()=>{request.current?.abort();schemaRequest.current?.abort();};},[available,mining]);
  const change=<K extends keyof TimingForm>(key:K,value:TimingForm[K])=>setForm(old=>({...old,[key]:value}));
  const discover=async()=>{if(!source||pending||mining)return;const c=new AbortController();request.current=c;setPending(true);setError(null);try{const next=await runTimingDiscovery(family,form,source,c.signal);if(request.current===c&&!c.signal.aborted){sequence.current++;setRun(next);}}catch(cause){if(request.current===c&&!c.signal.aborted)setError(cause instanceof Error?cause.message:String(cause));}finally{if(request.current===c){request.current=null;setPending(false);}}};
  const fields: Array<[keyof TimingForm,string,string]>=family==='spectrum'?[['pattern','Spectrum activity pattern','A,B,D (comma separated)'],['sample','Spectrum sample size','10000'],['activity','Spectrum activity field','activity'],['timestamp','Spectrum timestamp field','timestamp'],['caseId','Spectrum case-id field','case_id']]:[['merge','Batch merge distance (seconds)','900'],['minimum','Batch minimum size','2'],['activity','Batch activity field','concept:name'],['resource','Batch resource field','org:resource'],['timestamp','Batch completion-time field','time:timestamp'],['caseId','Batch case-id field','case:concept:name'],['start','Batch start-time field','blank uses completion time'],['eventId','Batch event-id field','table adapters only']];
  return <main className="layout-log">
    <section className="panel" aria-label="Configured timing discovery"><div className="panel-head"><h2>Timing discovery</h2><span className="panel-meta">{data.dataset}</span></div><p>Discover a performance spectrum or batches from the current source. Pattern, selectors and every complete result come from the selected native adapter.</p>
      <button className="btn-ghost" disabled={!available||mining||pending} onClick={()=>void refresh()}>Refresh timing source</button>{source&&<p>Source {source.source_id} · {source.case_count} cases · {source.event_count} events</p>}{!available&&<p>Open a CSV or XES source to run native timing discovery.</p>}
      <fieldset disabled={!source||mining||pending}><div className="align-settings-grid"><label>Timing discovery family<select value={family} onChange={event=>{const value=event.target.value as TimingFamily;setFamily(value);setForm(defaultTimingForm(value));}}><option value="spectrum">Performance spectrum</option><option value="batches">Batch detection</option></select></label><label>Timing discovery variant<select value={form.variant} onChange={event=>change('variant',event.target.value)}>{TIMING_VARIANTS[family].map(value=><option key={value}>{value}</option>)}</select></label>
        {fields.map(([key,label,placeholder])=><label key={key}>{label}<input value={String(form[key])} disabled={key==='start'&&!form.overrideStart} placeholder={placeholder} onChange={event=>change(key,event.target.value)}/></label>)}
        {family==='spectrum'?<label><input type="checkbox" checked={form.sort} onChange={event=>change('sort',event.target.checked)}/> Sort native spectrum events</label>:<label><input type="checkbox" checked={form.overrideStart} onChange={event=>change('overrideStart',event.target.checked)}/> Override batch start-time field</label>}
      </div><p>Blank fields retain native defaults. An enabled blank start-time override uses completion times for starts. Event identities require a table adapter.</p><button className="btn-open" onClick={()=>void discover()}>Run native timing discovery</button></fieldset>
      {pending&&<p role="status">Discovering timing… <button className="btn-ghost" onClick={cancel}>Cancel timing discovery</button></p>}{error&&<div className="error-banner" role="alert">{error}</div>}
    </section>
    {run?<TimingInspection key={sequence.current} run={run}/>:<p>Choose a native adapter and run discovery. Existing dotted-chart and saved spectrum views remain available separately.</p>}
  </main>;
}
function TimingInspection({run}:{run:TimingRun}) {
  const value=run.value,[selected,setSelected]=useState(value.method==='spectrum'?'/result/performance_spectrum/observations':'/result/batch_discovery/groups');const table=run.tables.find(t=>t.id===selected)??run.tables[0];
  const download=(raw:string,name:string,type='application/json')=>downloadQueryBlob(new Blob([raw],{type}),name);
  return <>
    {value.method==='spectrum'?<PerformanceSpectrumView data={value.result} nativeOnly/>:<Batches data={value.result}/>}
    <section className="panel" aria-label="Complete timing discovery inspection"><div className="panel-head"><h2>Complete timing results</h2><span className="panel-meta">{value.method} · {value.variant} · source {value.source_id}</span></div><p>Every native observation, pattern offset, batch interval, category, event reference and typed returned value remains available beyond visualization and table paging.</p>
      <div className="query-actions"><button className="btn-ghost" onClick={()=>download(run.raw,'minera-timing-discovery.json')}>Download complete timing JSON</button><button className="btn-ghost" onClick={()=>download(run.report,'minera-timing-discovery-report.json')}>Download timing discovery report</button><button className="btn-ghost" onClick={()=>download(nativeJson(value.result),'minera-discovered-timing-analysis.json')}>Download timing analysis JSON</button></div>
      <label>Timing result table<select value={table.id} onChange={event=>setSelected(event.target.value)}>{run.tables.map(t=><option key={t.id} value={t.id}>{t.title} · {t.rows.length} rows</option>)}</select></label><button className="btn-ghost" onClick={()=>download(diagnosticCsv(table),'minera-timing-'+encodeURIComponent(table.id)+'.csv','text/csv;charset=utf-8')}>Download complete timing table CSV</button><DiagnosticTableView key={table.id} table={table} noun="timing"/>
      <details><summary>Complete native timing response</summary><pre>{run.raw}</pre></details>
    </section>
  </>;
}
