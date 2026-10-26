import { useEffect, useRef, useState } from "react";
import type { MineraExport } from "../types";
import { requestLogQuerySchema, downloadQueryBlob } from "../log-query";
import { diagnosticCsv } from "../model-diagnostics";
import { nativeJson } from "../native-json";
import { nativeResponseRequest } from "../native-response";
import { DiagnosticTableView } from "./ModelDiagnosticsInspection";
import {
  ORGANIZATIONAL_CONTROLS, ORGANIZATIONAL_FAMILIES, ORGANIZATIONAL_TITLES, OrganizationalRequestGate, OrganizationalSourceChangedError,
  canHandoffOrganizationalRoles, defaultOrganizationalWorkbenchForm, organizationalAllTablesCsv,
  organizationalResultSummary, organizationalRoleGroups, organizationalSourceFromSchema,
  organizationalWorkbenchReport, runOrganizationalWorkbench, sameOrganizationalSource,
  type OrganizationalControl, type OrganizationalFamily, type OrganizationalSource,
  type OrganizationalWorkbenchForm, type OrganizationalWorkbenchRun,
} from "../organizational-workbench";

const familyMap = <T,>(create: () => T): Record<OrganizationalFamily, T> => ({roles: create(), profiles: create(), diagnostics: create(), network: create()});
interface ScopedRun {epoch: number; run: OrganizationalWorkbenchRun;}
interface ScopedSource {epoch: number; value: OrganizationalSource;}
export function OrganizationalWorkspace({data, available, mining}: {data: MineraExport; available: boolean; mining: boolean}) {
  const [forms, setForms] = useState(() => familyMap(defaultOrganizationalWorkbenchForm));
  const [runs, setRuns] = useState(() => familyMap<ScopedRun | null>(() => null));
  const [pending, setPending] = useState(() => familyMap(() => false));
  const [errors, setErrors] = useState(() => familyMap<string | null>(() => null));
  const [source, setSource] = useState<ScopedSource | null>(null), [sourceError, setSourceError] = useState<string | null>(null), [resolving, setResolving] = useState(false);
  const controllers = useRef(familyMap<AbortController | null>(() => null));
  const gates = useRef(familyMap(() => new OrganizationalRequestGate()));
  const schemaController = useRef<AbortController | null>(null);
  // Update the epoch during render, before effects or old promises can publish.
  const host = useRef({data, available, mining, epoch: 0});
  if (host.current.data !== data || host.current.available !== available || host.current.mining !== mining) host.current = {data, available, mining, epoch: host.current.epoch + 1};
  const epoch = host.current.epoch;
  const currentSource = source?.epoch === epoch ? source.value : null;
  const currentRun = (family: OrganizationalFamily) => runs[family]?.epoch === epoch ? runs[family]!.run : null;
  const cancelFamily = (family: OrganizationalFamily) => {
    gates.current[family].invalidate(); controllers.current[family]?.abort(); controllers.current[family] = null;
    setPending(current => ({...current, [family]: false}));
  };
  const cancelAll = () => ORGANIZATIONAL_FAMILIES.forEach(cancelFamily);
  const resolveSource = async () => {
    if (!available || mining) return;
    schemaController.current?.abort(); const controller = new AbortController(); schemaController.current = controller;
    setResolving(true); setSourceError(null);
    try {
      const resolved = organizationalSourceFromSchema(await requestLogQuerySchema(controller.signal), data);
      if (host.current.epoch !== epoch || controller.signal.aborted || schemaController.current !== controller) return;
      if (currentSource && !sameOrganizationalSource(currentSource, resolved)) {cancelAll(); setRuns(familyMap(() => null)); setErrors(familyMap(() => null));}
      setSource({epoch, value: resolved});
    } catch (cause) {
      if (host.current.epoch === epoch && !controller.signal.aborted && schemaController.current === controller) {
        if (cause instanceof OrganizationalSourceChangedError) {cancelAll(); setRuns(familyMap(() => null)); setSource(null);}
        setSourceError(cause instanceof Error ? cause.message : String(cause));
      }
    } finally {
      if (schemaController.current === controller) {schemaController.current = null; setResolving(false);}
    }
  };
  useEffect(() => {
    cancelAll(); schemaController.current?.abort(); setSource(null); setSourceError(null); setRuns(familyMap(() => null)); setErrors(familyMap(() => null));
    setResolving(false); if (available && !mining) void resolveSource();
    return () => {
      schemaController.current?.abort(); schemaController.current = null;
      ORGANIZATIONAL_FAMILIES.forEach(family => {gates.current[family].invalidate(); controllers.current[family]?.abort(); controllers.current[family] = null;});
    };
    // Host identity changes invalidate every completed result and in-flight run.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [data, available, mining]);
  const edit = (family: OrganizationalFamily, key: string, value: string) => setForms(current => ({...current, [family]: {...current[family], fields: {...current[family].fields, [key]: value}}}));
  const execute = async (family: OrganizationalFamily) => {
    if (!currentSource || !available || mining || resolving) return;
    cancelFamily(family); const controller = new AbortController(), ticket = gates.current[family].issue(); controllers.current[family] = controller;
    const snapshot = structuredClone(forms[family]);
    setPending(current => ({...current, [family]: true})); setErrors(current => ({...current, [family]: null}));
    const accepts = () => host.current.epoch === epoch && gates.current[family].accepts(ticket, controller.signal);
    try {
      // Recheck the cached source immediately before each POST as other workspaces can replace it.
      const resolved = organizationalSourceFromSchema(await requestLogQuerySchema(controller.signal), data);
      if (!accepts()) return;
      if (!sameOrganizationalSource(currentSource, resolved)) {
        cancelAll(); setRuns(familyMap(() => null)); setSource({epoch, value: resolved});
        setSourceError("The cached source changed. Previous results were cleared; review the source and run again."); return;
      }
      const result = await runOrganizationalWorkbench(family, resolved, snapshot, controller.signal);
      if (accepts()) setRuns(current => ({...current, [family]: {epoch, run: result}}));
    } catch (cause) {
      if (accepts()) {
        const message = cause instanceof Error ? cause.message : String(cause);
        if (cause instanceof OrganizationalSourceChangedError || /source log changed|source changed/i.test(message)) {
          cancelAll(); setRuns(familyMap(() => null)); setSource(null); setSourceError(message);
        } else setErrors(current => ({...current, [family]: message}));
      }
    } finally {
      if (controllers.current[family] === controller) {controllers.current[family] = null; setPending(current => ({...current, [family]: false}));}
    }
  };
  const roles = currentRun("roles");
  const handoff = () => {
    if (!roles || !currentSource) return;
    const generated = organizationalRoleGroups(roles, currentSource);
    setForms(current => ({...current, diagnostics: {...generated, fields: {...current.diagnostics.fields, ...generated.fields}}}));
  };
  return <>
    <section className="panel" aria-label="Organizational analysis source">
      <div className="panel-head"><h2>Organizational analysis workbench</h2><span className="panel-meta">{data.dataset}</span></div>
      <p>Discover roles, profile resources, inspect group diagnostics and analyze an attribute network. Each analysis keeps its completed native result and submitted settings independently.</p>
      <p>{currentSource ? `Source ${currentSource.source_id} · ${currentSource.case_count} cases · ${currentSource.event_count} events` : mining ? "The log is being mined. Organizational analysis resumes when the new source is ready." : available ? "Resolve the current cached log before running analysis." : "Start the native backend and mine a log to use organizational analysis."}</p>
      <button className="btn-ghost" disabled={!available || mining || resolving} onClick={() => void resolveSource()}>Reload source</button>
      {resolving && <p role="status">Resolving organizational source…</p>}
      {sourceError && <p className="error-banner" role="alert">{sourceError}</p>}
    </section>
    {ORGANIZATIONAL_FAMILIES.map(family => <section className="panel" aria-label={ORGANIZATIONAL_TITLES[family] + " workbench"} key={family}>
      <div className="panel-head"><h2>{ORGANIZATIONAL_TITLES[family]}</h2><span className="panel-meta">Native organizational analysis</span></div>
      <FamilyGuide family={family} form={forms[family]} />
      <fieldset className="query-fields" disabled={!currentSource || !available || mining || resolving || pending[family]}>
        <div className="query-options">{ORGANIZATIONAL_CONTROLS[family].map(control => <OrganizationalField key={control.key} control={control} value={forms[family].fields[control.key] ?? ""} onChange={value => edit(family, control.key, value)} />)}</div>
        {family === "diagnostics" && <>
          <button className="btn-ghost" disabled={!canHandoffOrganizationalRoles(roles, currentSource)} onClick={handoff}>Use completed roles as diagnostics groups</button>
          {forms.diagnostics.roles_source_id && <p>Generated role groups are bound to source {forms.diagnostics.roles_source_id} and use that role run’s activity and resource selectors. Edit the JSON to review membership; every resource label remains exact. <button className="btn-ghost" onClick={() => setForms(current => ({...current, diagnostics: {fields: current.diagnostics.fields}}))}>Remove role-source binding</button></p>}
        </>}
        <button className="btn-open" onClick={() => void execute(family)}>Run {ORGANIZATIONAL_TITLES[family].toLowerCase()}</button>
      </fieldset>
      {pending[family] && <p role="status">Running {ORGANIZATIONAL_TITLES[family].toLowerCase()}… <button className="btn-ghost" onClick={() => cancelFamily(family)}>Cancel {ORGANIZATIONAL_TITLES[family].toLowerCase()} request</button></p>}
      {errors[family] && <p className="error-banner" role="alert">{errors[family]}</p>}
      {currentRun(family) && <OrganizationalResult key={epoch + ":" + currentRun(family)!.raw} run={currentRun(family)!} />}
    </section>)}
  </>;
}
function OrganizationalField({control, value, onChange}: {control: OrganizationalControl; value: string; onChange: (value: string) => void}) {
  return <label className="filters-field"><span>{control.label}</span>
    {control.kind === "enum" || control.kind === "boolean" ? <select value={value} onChange={event => onChange(event.target.value)}>
      <option value="">Native default</option>{(control.kind === "boolean" ? ["true", "false"] : control.choices ?? []).map(option => <option key={option} value={option}>{control.kind === "boolean" ? option === "true" ? "On" : "Off" : option}</option>)}
    </select> : control.kind === "json" ? <textarea rows={5} value={value} placeholder={'Native group attribute, or {"Team": ["resource"]}'} onChange={event => onChange(event.target.value)} /> : <input value={value} placeholder="Native default" onChange={event => onChange(event.target.value)} />}
  </label>;
}
function FamilyGuide({family, form}: {family: OrganizationalFamily; form: OrganizationalWorkbenchForm}) {
  if (family === "roles") return <p>LOG and PANDAS use the native selected attributes and merge roles when similarity is strictly greater than the threshold. Resource counts describe native role membership; merged counts can differ from event frequency. Blank settings use native defaults.</p>;
  if (family === "network") return <p>Choose the attributes that identify source nodes, target nodes, relations and edge labels. Performance contains every returned link duration in seconds; an empty list is retained. Weekly business-hour slots use seconds from the start of the week, for example 28800:43200,46800:61200. Blank settings use native defaults.</p>;
  if (family === "diagnostics") return <p>Use a group attribute, or enter exact resource membership as a JSON object. Groups may overlap. Blank JSON uses the native group-attribute mode; an explicit empty object or empty member array is submitted as entered. Focus, stake, coverage and member contributions remain separate native maps.</p>;
  const metric = form.fields.resource_profiles_metric || "distinct_activities";
  const guidance: Record<string, string> = {
    activity_frequency: "This metric requires an activity label.", average_duration_activity: "This metric requires an activity label. Durations use the selected timestamps.",
    average_workload: "This metric requires t2 and evaluates intervals active at t2.", multitasking: "This metric requires t1 and t2 and evaluates intervals wholly inside the slot; touching endpoints count as concurrent.",
    interaction_two_resources: "This metric requires resource and second resource labels.", activity_completions: "This metric counts timestamped activity events, including lifecycle markers.",
    case_completions: "The last selected case timestamp determines the time slot; every resource in the case receives credit.", fraction_case_completions: "The last selected case timestamp determines the time slot; every resource in the case receives credit.",
    average_case_duration: "A case with any selected timestamp in the time slot contributes its full duration.",
  };
  return <p>All eleven native metrics are available. Blank resource and activity labels select all values where the metric supports it; blank time bounds are unbounded. Custom case selectors resolve a trace key or its first event and retain native trace boundaries. {guidance[metric]}</p>;
}
function OrganizationalResult({run}: {run: OrganizationalWorkbenchRun}) {
  const [tableId, setTableId] = useState(run.value.family === "ROLE_DISCOVERY" ? "role_resources" : run.value.family === "RESOURCE_PROFILES" ? "/rows" : run.value.family === "LOCAL_DIAGNOSTICS" ? "group_metrics" : "/edges");
  const table = run.tables.find(item => item.id === tableId) ?? run.tables[0];
  const name = ORGANIZATIONAL_TITLES[run.family].toLowerCase(), stem = "minera-organizational-" + run.family;
  const request = nativeResponseRequest(run.value) ?? {fields: run.fields};
  const download = (text: string, suffix: string, type = "application/json") => downloadQueryBlob(new Blob([text], {type}), `${stem}-${suffix}`);
  return <div aria-label={`Complete ${name} inspection`}>
    <h3>Completed {name} result</h3>
    <p>{run.source.dataset} · source {run.source.source_id}. This result retains the options of its completed run. {organizationalResultSummary(run)}</p>
    <details><summary>Submitted {name} request and source</summary><pre>{nativeJson({source: run.source, request}, 2)}</pre></details>
    <div className="query-actions">
      <button className="btn-ghost" onClick={() => download(run.raw, "native.json")}>Download complete {name} native JSON</button>
      <button className="btn-ghost" onClick={() => download(organizationalWorkbenchReport(run), "report.json")}>Download {name} report</button>
      <button className="btn-ghost" onClick={() => download(nativeJson(request, 2), "request.json")}>Download {name} request</button>
      <button className="btn-ghost" onClick={() => download(diagnosticCsv(table), `${encodeURIComponent(table.id)}.csv`, "text/csv;charset=utf-8")}>Download complete {name} table CSV</button>
      <button className="btn-ghost" onClick={() => download(organizationalAllTablesCsv(run), "all-tables.csv", "text/csv;charset=utf-8")}>Download all {name} tables CSV</button>
    </div>
    <label className="filters-field"><span>{ORGANIZATIONAL_TITLES[run.family]} result table</span><select value={table.id} onChange={event => setTableId(event.target.value)}>
      {run.tables.map(item => <option key={item.id} value={item.id}>{item.title} · {item.rows.length} rows</option>)}
    </select></label>
    <DiagnosticTableView key={table.id} table={table} noun={name} />
    <details><summary>Complete {name} native result JSON</summary><pre>{run.raw}</pre></details>
  </div>;
}
