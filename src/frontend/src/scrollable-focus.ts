// Keyboard users scroll a region with the arrow keys only once it can take
// focus. Result blocks (<pre> dumps, wide matrices) overflow inside their own
// scroll box, so this keeps every such box that actually overflows in the tab
// order, and takes boxes that stop overflowing back out of it.

const CANDIDATES = "pre, .efg-scroll, .query-table-wrap, .table-scroll, .activity-table-scroll";
const MARK = "data-scroll-focus";

function scrolls(element: HTMLElement): boolean {
  const style = getComputedStyle(element);
  const x = /(auto|scroll)/.test(style.overflowX) && element.scrollWidth > element.clientWidth + 1;
  const y = /(auto|scroll)/.test(style.overflowY) && element.scrollHeight > element.clientHeight + 1;
  return x || y;
}

/** Adds or removes the tabindex this module manages; returns how many elements changed. */
export function syncScrollableFocus(root: ParentNode = document): number {
  let changed = 0;
  root.querySelectorAll<HTMLElement>(CANDIDATES).forEach((element) => {
    const managed = element.hasAttribute(MARK);
    if (!managed && element.hasAttribute("tabindex")) return;
    const needs = scrolls(element);
    if (needs && !managed) {
      element.setAttribute("tabindex", "0");
      element.setAttribute(MARK, "");
      changed++;
    } else if (!needs && managed) {
      element.removeAttribute("tabindex");
      element.removeAttribute(MARK);
      changed++;
    }
  });
  return changed;
}

export function installScrollableFocus(root: HTMLElement = document.body): () => void {
  let frame = 0;
  const schedule = () => {
    if (frame) return;
    frame = requestAnimationFrame(() => { frame = 0; syncScrollableFocus(root); });
  };
  const observer = new MutationObserver((records) => {
    // Our own attribute writes must not retrigger a scan.
    if (records.every((record) => record.type === "attributes" && (record.attributeName === "tabindex" || record.attributeName === MARK))) return;
    schedule();
  });
  observer.observe(root, { childList: true, subtree: true, characterData: true, attributes: true, attributeFilter: ["open", "class", "style", "hidden"] });
  window.addEventListener("resize", schedule);
  schedule();
  return () => { observer.disconnect(); window.removeEventListener("resize", schedule); if (frame) cancelAnimationFrame(frame); };
}
