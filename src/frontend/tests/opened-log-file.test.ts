import { describe, it, expect, vi, afterEach } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";

const ocelIo = vi.hoisted(() => ({ requested: [] as Array<{ format: string; name: string }> }));
vi.mock("../src/ocel-io", async (original) => {
  const actual = await original<typeof import("../src/ocel-io")>();
  return {
    ...actual,
    requestOcelIo: vi.fn(async (form: { format: string }, file: File) => {
      ocelIo.requested.push({ format: form.format, name: file.name });
      return { mode: "import" } as never;
    }),
    importedOcelFile: vi.fn(() => new File(['{"objectTypes":[],"eventTypes":[],"objects":[],"events":[]}'], "imported.json", { type: "application/json" })),
  };
});

import { gunzipFile, ocelXmlFormat, openableLogFile, openedFileRoute } from "../src/opened-log-file";
import { LOG_FILE_ACCEPT } from "../src/components/ui";
import { OcelIoWorkspace } from "../src/components/OcelIoWorkspace";

async function gzip(text: string): Promise<Blob> {
  return new Response(new Blob([text]).stream().pipeThrough(new CompressionStream("gzip"))).blob();
}

afterEach(() => { ocelIo.requested.length = 0; });

describe("opened log files with pm4py extensions", () => {
  it("routes OCEL and gzip table extensions away from the CSV path", () => {
    expect(openedFileRoute("orders.jsonocel")).toBe("ocel_json");
    expect(openedFileRoute("orders.XMLOCEL")).toBe("ocel_xml");
    expect(openedFileRoute("orders.sqlite")).toBe("ocel_sqlite");
    expect(openedFileRoute("orders.sqlite3")).toBe("ocel_sqlite");
    expect(openedFileRoute("log.csv.gz")).toBe("gzip_table");
    expect(openedFileRoute("log.tsv.GZ")).toBe("gzip_table");
    for (const name of ["log.csv", "log.xes", "log.xes.gz", "export.json", "log.gz"]) expect(openedFileRoute(name)).toBe("as_is");
  });

  it("tells OCEL 2.0 XML from OCEL 1.0 XML", () => {
    expect(ocelXmlFormat('<?xml version="1.0"?><log><object-types><object-type name="o"/></object-types>')).toBe("xml_standard");
    expect(ocelXmlFormat("<log><event-types/></log>")).toBe("xml_standard");
    expect(ocelXmlFormat('<log><global scope="event"><string key="ocel:activity" value="__INVALID__"/></global><events>')).toBe("xml");
  });

  it("decompresses a gzip CSV and keeps its name without .gz", async () => {
    const csv = 'case_id,activity\n"c ü&<x>","A ""q"""\n';
    const opened = await openableLogFile(new File([await gzip(csv)], "log.csv.gz"));
    expect(opened.name).toBe("log.csv");
    expect(await opened.text()).toBe(csv);
    const tsv = await openableLogFile(new File([await gzip("a\tb\n")], "log.tsv.gz"));
    expect(tsv.name).toBe("log.tsv");
    await expect(gunzipFile(new Blob(["not gzip"]), "x.csv")).rejects.toThrow("not valid gzip");
  });

  it("imports OCEL XML and SQLite into OCEL JSON and leaves other files alone", async () => {
    const xml = await openableLogFile(new File(["<log><object-types/></log>"], "orders.xmlocel"));
    expect(xml.name).toBe("orders.xmlocel.json");
    expect(xml.type).toBe("application/json");
    await openableLogFile(new File(["<log><global/></log>"], "classic.xmlocel"));
    const sqlite = await openableLogFile(new File(["SQLite format 3\0"], "orders.sqlite"));
    expect(sqlite.name).toBe("orders.sqlite.json");
    expect(ocelIo.requested).toEqual([
      { format: "xml_standard", name: "orders.xmlocel" },
      { format: "xml", name: "classic.xmlocel" },
      { format: "sqlite", name: "orders.sqlite" },
    ]);
    const json = new File(["{}"], "orders.jsonocel"), csv = new File(["a"], "log.csv");
    expect(await openableLogFile(json)).toBe(json);
    expect(await openableLogFile(csv)).toBe(csv);
    expect(ocelIo.requested).toHaveLength(3);
  });

  it("offers the pm4py extensions in the log pickers", () => {
    for (const extension of [".jsonocel", ".xmlocel", ".sqlite", ".csv.gz", ".xes.gz"]) expect(LOG_FILE_ACCEPT.split(",")).toContain(extension);
    const xmlImport = renderToStaticMarkup(createElement(OcelIoWorkspace, { file: null, busy: false, onOpen: () => {} }));
    expect(xmlImport).toContain('accept=".json,.jsonocel"');
  });
});
