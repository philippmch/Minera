// Saves a Blob as a file. The anchor is attached to the document while it is
// clicked (some browsers ignore clicks on detached anchors) and the object URL
// stays alive for a while afterwards, because revoking it synchronously can
// cancel the download before the browser has read it.

/** How long an object URL outlives the click that starts its download. */
export const DOWNLOAD_URL_LIFETIME_MS = 60_000;

export function downloadBlob(blob: Blob, filename: string): void {
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = filename;
  anchor.rel = "noopener";
  document.body?.appendChild(anchor);
  try {
    anchor.click();
  } finally {
    anchor.remove?.();
    setTimeout(() => URL.revokeObjectURL(url), DOWNLOAD_URL_LIFETIME_MS);
  }
}

/** Saves text with an explicit media type. */
export function downloadText(text: string, filename: string, type: string): void {
  downloadBlob(new Blob([text], { type }), filename);
}
